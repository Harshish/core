/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/config.h>

#include "jpeg.h"
#include <jpeglib.h>
#include <jerror.h>

#include "JpegReader.hxx"
#include <vcl/bitmapaccess.hxx>
#include <vcl/FilterConfigItem.hxx>
#include <vcl/graphicfilter.hxx>
#include <tools/fract.hxx>
#include <memory>

#define JPEG_MIN_READ 512
#define BUFFER_SIZE  4096

/* Expanded data source object for stdio input */

struct SourceManagerStruct {
    jpeg_source_mgr pub;                /* public fields */
    SvStream*   stream;                 /* source stream */
    JOCTET*     buffer;                 /* start of buffer */
    boolean     start_of_file;          /* have we gotten any data yet? */
};

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */
extern "C" void init_source (j_decompress_ptr cinfo)
{
    SourceManagerStruct * source = reinterpret_cast<SourceManagerStruct *>(cinfo->src);

    /* We reset the empty-input-file flag for each image,
     * but we don't clear the input buffer.
     * This is correct behavior for reading a series of images from one source.
     */
    source->start_of_file = TRUE;
}

long StreamRead( SvStream* pStream, void* pBuffer, long nBufferSize )
{
    long nRead = 0;

    if( pStream->GetError() != ERRCODE_IO_PENDING )
    {
        long nInitialPosition = pStream->Tell();

        nRead = static_cast<long>(pStream->ReadBytes(pBuffer, nBufferSize));

        if( pStream->GetError() == ERRCODE_IO_PENDING )
        {
            // in order to search from the old position
            // we temporarily reset the error
            pStream->ResetError();
            pStream->Seek( nInitialPosition );
            pStream->SetError( ERRCODE_IO_PENDING );
        }
    }

    return nRead;
}

extern "C" boolean fill_input_buffer (j_decompress_ptr cinfo)
{
    SourceManagerStruct * source = reinterpret_cast<SourceManagerStruct *>(cinfo->src);
    size_t nbytes;

    nbytes = StreamRead(source->stream, source->buffer, BUFFER_SIZE);

    if (!nbytes)
    {
        if (source->start_of_file)     /* Treat empty input file as fatal error */
        {
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        }
        WARNMS(cinfo, JWRN_JPEG_EOF);
        /* Insert a fake EOI marker */
        source->buffer[0] = (JOCTET) 0xFF;
        source->buffer[1] = (JOCTET) JPEG_EOI;
        nbytes = 2;
    }

    source->pub.next_input_byte = source->buffer;
    source->pub.bytes_in_buffer = nbytes;
    source->start_of_file = FALSE;

    return TRUE;
}

extern "C" void skip_input_data (j_decompress_ptr cinfo, long numberOfBytes)
{
    SourceManagerStruct * source = reinterpret_cast<SourceManagerStruct *>(cinfo->src);

    /* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (numberOfBytes > 0)
    {
        while (numberOfBytes > (long) source->pub.bytes_in_buffer)
        {
            numberOfBytes -= (long) source->pub.bytes_in_buffer;
            (void) fill_input_buffer(cinfo);

            /* note we assume that fill_input_buffer will never return false,
             * so suspension need not be handled.
             */
        }
        source->pub.next_input_byte += (size_t) numberOfBytes;
        source->pub.bytes_in_buffer -= (size_t) numberOfBytes;
    }
}

extern "C" void term_source (j_decompress_ptr)
{
    /* no work necessary here */
}

void jpeg_svstream_src (j_decompress_ptr cinfo, void* input)
{
    SourceManagerStruct * source;
    SvStream* stream = static_cast<SvStream*>(input);

    /* The source object and input buffer are made permanent so that a series
     * of JPEG images can be read from the same file by calling jpeg_stdio_src
     * only before the first one.  (If we discarded the buffer at the end of
     * one image, we'd likely lose the start of the next one.)
     * This makes it unsafe to use this manager and a different source
     * manager serially with the same JPEG object.  Caveat programmer.
     */

    if (cinfo->src == nullptr)
    { /* first time for this JPEG object? */
        cinfo->src = static_cast<jpeg_source_mgr *>(
            (*cinfo->mem->alloc_small) (reinterpret_cast<j_common_ptr>(cinfo), JPOOL_PERMANENT, sizeof(SourceManagerStruct)));
        source = reinterpret_cast<SourceManagerStruct *>(cinfo->src);
        source->buffer = static_cast<JOCTET *>(
            (*cinfo->mem->alloc_small) (reinterpret_cast<j_common_ptr>(cinfo), JPOOL_PERMANENT, BUFFER_SIZE * sizeof(JOCTET)));
    }

    source = reinterpret_cast<SourceManagerStruct *>(cinfo->src);
    source->pub.init_source = init_source;
    source->pub.fill_input_buffer = fill_input_buffer;
    source->pub.skip_input_data = skip_input_data;
    source->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    source->pub.term_source = term_source;
    source->stream = stream;
    source->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
    source->pub.next_input_byte = nullptr; /* until buffer loaded */
}

JPEGReader::JPEGReader( SvStream& rStream, void* /*pCallData*/, bool bSetLogSize ) :
    mrStream         ( rStream ),
    mnLastPos        ( rStream.Tell() ),
    mnLastLines      ( 0 ),
    mbSetLogSize     ( bSetLogSize )
{
    maUpperName = "SVIJPEG";
    mnFormerPos = mnLastPos;
}

JPEGReader::~JPEGReader()
{
}

bool JPEGReader::CreateBitmap(JPEGCreateBitmapParam& rParam)
{
    if (rParam.nWidth > SAL_MAX_INT32 / 8 || rParam.nHeight > SAL_MAX_INT32 / 8)
        return false; // avoid overflows later

    if (rParam.nWidth == 0 || rParam.nHeight == 0)
        return false;

    Size aSize(rParam.nWidth, rParam.nHeight);
    bool bGray = rParam.bGray;

    maBitmap = Bitmap();

    sal_uInt64 nSize = aSize.Width() * aSize.Height();

    if (nSize > SAL_MAX_INT32 / (bGray?1:3))
        return false;

    if( bGray )
    {
        BitmapPalette aGrayPal( 256 );

        for( sal_uInt16 n = 0; n < 256; n++ )
        {
            const sal_uInt8 cGray = (sal_uInt8) n;
            aGrayPal[ n ] = BitmapColor( cGray, cGray, cGray );
        }

        maBitmap = Bitmap(aSize, 8, &aGrayPal);
    }
    else
    {
        maBitmap = Bitmap(aSize, 24);
    }

    if (mbSetLogSize)
    {
        unsigned long nUnit = rParam.density_unit;

        if (((1 == nUnit) || (2 == nUnit)) && rParam.X_density && rParam.Y_density )
        {
            Point       aEmptyPoint;
            Fraction    aFractX( 1, rParam.X_density );
            Fraction    aFractY( 1, rParam.Y_density );
            MapMode     aMapMode( nUnit == 1 ? MapUnit::MapInch : MapUnit::MapCM, aEmptyPoint, aFractX, aFractY );
            Size        aPrefSize = OutputDevice::LogicToLogic( aSize, aMapMode, MapUnit::Map100thMM );

            maBitmap.SetPrefSize(aPrefSize);
            maBitmap.SetPrefMapMode(MapMode(MapUnit::Map100thMM));
        }
    }

    return true;
}

Graphic JPEGReader::CreateIntermediateGraphic(long nLines)
{
    Graphic aGraphic;
    const Size aSizePixel(maBitmap.GetSizePixel());

    if (!mnLastLines)
    {
        maIncompleteAlpha = Bitmap(aSizePixel, 1);
        maIncompleteAlpha.Erase(Color(COL_WHITE));
    }

    if (nLines && (nLines < aSizePixel.Height()))
    {
        const long nNewLines = nLines - mnLastLines;

        if (nNewLines > 0)
        {
            {
                Bitmap::ScopedWriteAccess pAccess(maIncompleteAlpha);
                pAccess->SetFillColor(Color(COL_BLACK));
                pAccess->FillRect(Rectangle(Point(0, mnLastLines), Size(pAccess->Width(), nNewLines)));
            }

            aGraphic = BitmapEx(maBitmap, maIncompleteAlpha);
        }
        else
        {
            aGraphic = maBitmap;
        }
    }
    else
    {
        aGraphic = maBitmap;
    }

    mnLastLines = nLines;

    return aGraphic;
}

ReadState JPEGReader::Read( Graphic& rGraphic )
{
    long        nEndPosition;
    long        nLines;
    ReadState   eReadState;
    bool        bRet = false;
    sal_uInt8   cDummy;

    // TODO: is it possible to get rid of this seek to the end?
    // check if the stream's end is already available
    mrStream.Seek( STREAM_SEEK_TO_END );
    mrStream.ReadUChar( cDummy );
    nEndPosition = mrStream.Tell();

    // else check if at least JPEG_MIN_READ bytes can be read
    if( mrStream.GetError() == ERRCODE_IO_PENDING )
    {
        mrStream.ResetError();
        if( ( nEndPosition  - mnFormerPos ) < JPEG_MIN_READ )
        {
            mrStream.Seek( mnLastPos );
            return JPEGREAD_NEED_MORE;
        }
    }

    // seek back to the original position
    mrStream.Seek( mnLastPos );

    // read the (partial) image
    ReadJPEG( this, &mrStream, &nLines, GetPreviewSize() );

    if (!maBitmap.IsEmpty())
    {
        if( mrStream.GetError() == ERRCODE_IO_PENDING )
        {
            rGraphic = CreateIntermediateGraphic(nLines);
        }
        else
        {
            rGraphic = maBitmap;
        }

        bRet = true;
    }
    else if( mrStream.GetError() == ERRCODE_IO_PENDING )
    {
        bRet = true;
    }

    // Set status ( Pending has priority )
    if( mrStream.GetError() == ERRCODE_IO_PENDING )
    {
        eReadState = JPEGREAD_NEED_MORE;
        mrStream.ResetError();
        mnFormerPos = mrStream.Tell();
    }
    else
    {
        eReadState = bRet ? JPEGREAD_OK : JPEGREAD_ERROR;
    }

    return eReadState;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
