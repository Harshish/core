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

#ifndef INCLUDED_SCADDINS_SOURCE_ANALYSIS_BESSEL_HXX
#define INCLUDED_SCADDINS_SOURCE_ANALYSIS_BESSEL_HXX

#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/sheet/NoConvergenceException.hpp>
#include <sal/types.h>

namespace sca {
namespace analysis {


//     BESSEL functions


/** Returns the result for the unmodified BESSEL function of first kind (J), n-th order, at point x.

    @throws css::lang::IllegalArgumentException
    @throws css::sheet::NoConvergenceException
*/
double BesselJ( double x, sal_Int32 n ) throw( css::lang::IllegalArgumentException, css::sheet::NoConvergenceException );

/** Returns the result for the modified BESSEL function of first kind (I), n-th order, at point x.

    @throws css::lang::IllegalArgumentException
    @throws css::sheet::NoConvergenceException
*/
double BesselI( double x, sal_Int32 n ) throw( css::lang::IllegalArgumentException, css::sheet::NoConvergenceException );

/** Returns the result for the unmodified BESSEL function of second kind (Y), n-th order, at point x.

    @throws css::lang::IllegalArgumentException
    @throws css::sheet::NoConvergenceException
*/
double BesselY( double x, sal_Int32 n ) throw( css::lang::IllegalArgumentException, css::sheet::NoConvergenceException );

/** Returns the result for the modified BESSEL function of second kind (K), n-th order, at point x.

    @throws css::lang::IllegalArgumentException
    @throws css::sheet::NoConvergenceException
*/
double BesselK( double x, sal_Int32 n ) throw( css::lang::IllegalArgumentException, css::sheet::NoConvergenceException );

} // namespace analysis
} // namespace sca

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
