/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPERA_VERSION_H
#define OPERA_VERSION_H

/* Include this header to get VER_BUILD_IDENTIFIER. Or for
 * compatibility try to get BUILD_NUMBER or OPERA_GOGI_BUILD_NUMBER. */
#include ABOUT_BUILD_NUMBER_HEADER

#ifndef VER_BUILD_IDENTIFIER
# define OperaVersion_Stringify(s) OperaVersion_XStringify(s)
# define OperaVersion_XStringify(s) #s
# if defined(BUILD_NUMBER)
#  define VER_BUILD_IDENTIFIER OperaVersion_Stringify(BUILD_NUMBER)
# elif defined(OPERA_GOGI_BUILD_NUMBER)
#  define VER_BUILD_IDENTIFIER OperaVersion_Stringify(OPERA_GOGI_BUILD_NUMBER)
# else //
#  error "You need to define a build identifier in VER_BUILD_IDENTIFIER"
# endif // BUILD_NUMBER
#endif // VER_BUILD_IDENTIFIER
#ifndef VER_BUILD_IDENTIFIER_UNI
# define VER_BUILD_IDENTIFIER_UNI UNI_L(VER_BUILD_IDENTIFIER)
#endif // VER_BUILD_IDENTIFIER_UNI

#endif // OPERA_VERSION_H
