/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*/
#ifndef  UNIX_DESKTOP_PRODUCT_FEATURES_H
#define  UNIX_DESKTOP_PRODUCT_FEATURES_H

#include "platforms/unix/base/common/preamble.h" // as early as possible

// Identifies desktop builds everywhere. It may be used in quick-features.h
#define  _UNIX_DESKTOP_
#define  _X11_

#include "adjunct/quick/quick-features.h"


/***********************************************************************************
 ** Feature overrides
 ***********************************************************************************/

#undef   FEATURE_3P_BITSTREAM_VERA
#define  FEATURE_3P_BITSTREAM_VERA                                         YES

#undef   FEATURE_3P_FREETYPE
#define  FEATURE_3P_FREETYPE                                               YES

#if defined(__linux__)
# undef   FEATURE_3P_GLIBC
# define  FEATURE_3P_GLIBC                                                 YES
#endif

#undef   FEATURE_INTERNAL_TEXTSHAPER
#define  FEATURE_INTERNAL_TEXTSHAPER                                       YES

#if defined(_DEBUG) && !defined(__PIC__) && (defined(linux) || defined(__FreeBSD__) || defined(__NetBSD__))
# undef   FEATURE_MEMORY_DEBUGGING
# define  FEATURE_MEMORY_DEBUGGING                                         YES
#endif

#undef   FEATURE_SVG_EMBEDDED_FONTS
#define  FEATURE_SVG_EMBEDDED_FONTS                                        YES

#ifdef   _DEBUG
# undef   FEATURE_VALGRIND
# define  FEATURE_VALGRIND                                                 YES
#endif

/* A mouse selection will copy text directly to clipboard */
#undef   FEATURE_X11_SELECTION_POLICY
#define  FEATURE_X11_SELECTION_POLICY                                      YES



/***********************************************************************************
 ** Misc mess
 ***********************************************************************************/

/********** TODO Should be Quick feature/capability **********/
#define  ALLOW_MENU_SHARE_SHORTCUT


/***********************************************************************************
 ** Further overrides
 ***********************************************************************************/

// DSK-357686
#include "platforms/unix/product/config.h"

// Must be included at the end to make it possible to make a custom build
// using the ubs build system:
#include "adjunct/quick/custombuild-features.h"


#endif   // !UNIX_DESKTOP_PRODUCT_FEATURES_H
