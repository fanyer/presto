/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_FEATURES_FEATURES_POST_H
#define MODULES_HARDCORE_FEATURES_FEATURES_POST_H

// setjmp
#ifdef SETJMP_SUPPORTED
# define JPEG_SETJMP_SUPPORTED
# define PNG_SETJMP_SUPPORTED
#endif // SETJMP_SUPPORTED

// ZLIB
#ifdef ZLIB_COMPRESSION
# define SUPPORT_DEFLATE
# define SUPPORT_ZLIB_UTIL
#endif // ZLIB_COMPRESSION

// Image decoders
#ifdef INTERNAL_IMG_DECODERS_SUPPORT
# define INTERNAL_GIF_SUPPORT

# ifdef _JPG_SUPPORT_
#  define INTERNAL_JPG_SUPPORT
# endif // _JPG_SUPPORT_

# ifdef _PNG_SUPPORT_
#  define INTERNAL_PNG_SUPPORT
# endif // _PNG_SUPPORT_

# ifdef _BMP_SUPPORT_
#  define INTERNAL_BMP_SUPPORT
# endif // _BMP_SUPPORT_

# ifdef WBMP_SUPPORT
#  define INTERNAL_WBMP_SUPPORT
# endif // WBMP_SUPPORT

# ifdef _XBM_SUPPORT_
#  define INTERNAL_XBM_SUPPORT
# endif // _XBM_SUPPORT_

# ifdef ICO_SUPPORT
#  define INTERNAL_ICO_SUPPORT
# endif // ICO_SUPPORT
#endif // INTERNAL_IMG_DECODERS_SUPPORT

// WML
#ifdef _WML_SUPPORT_
# define _CSS_LINK_SUPPORT_
# define _WML_CARD_SUPPORT_
#endif // _WML_SUPPORT_

// DOM2
#ifdef DOM2_TRAVERSAL_AND_RANGE
# define DOM2_TRAVERSAL
# define DOM2_RANGE
#endif // DOM2_TRAVERSAL_AND_RANGE

// FTP
#if !defined NO_FTP_SUPPORT && !(defined _LOCALHOST_SUPPORT_ || defined SUPPORT_DATA_URL)
# error "FEATURE_FTP requires either FEATURE_LOCALHOST or FEATURE_DATA_URL_SCHEME"
#endif

// Left-over from FEATURE_SCRIPTS_IN_XML, to be removed when no longer used.
#define SCRIPTS_IN_XML_HACK

#endif // !MODULES_HARDCORE_FEATURES_FEATURES_POST_H
