/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

/** @file stdlib_math.cpp Floating point math library.
 *
 * None of these are currently implemented by Opera, but are instead
 * in a thirdparty math lbirary. Make sure we fail before the linker
 * step if the platform does not provide a math library, but we have
 * neglected to use the one from this module. Using the math library
 * provided by the system is almost always recommended.
 */

#ifndef HAVE_LOG
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_LOG == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_ASIN
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_ASIN == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_ACOS
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_ACOS == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_ATAN
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_ATAN == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_ATAN2
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_ATAN2 == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_EXP
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_EXP == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_LDEXP
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_LDEXP == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_LOG
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_LOG == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_POW
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_POW == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_SIN
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_SIN == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_COS
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_COS == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_SQRT
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_SQRT == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif

#ifndef HAVE_TAN
# ifndef ACK_MATH_LIBRARY
#  error "SYSTEM_TAN == NO, but FEATURE_3P_MATHLIB == NO"
# endif
#endif
