/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYPEG_H
#define JAYPEG_H

/** This file contains various constants and features of jaypeg which are
 * not hardcoded. Most of them should however never be changed. */

#define JAYPEG_NOT_ENOUGH_DATA 1
#define JAYPEG_OK 0
#define JAYPEG_ERR -1
#define JAYPEG_ERR_NO_MEMORY -2

// FEATURES
// FIXME: jp2 support is currently not working at all
//#define JAYPEG_JP2_SUPPORT
#define JAYPEG_JFIF_SUPPORT

#if defined(JAYPEG_JFIF_SUPPORT) && defined(IMAGE_METADATA_SUPPORT)
#define JAYPEG_EXIF_SUPPORT
#endif // JAYPEG_JFIF_SUPPORT && IMAGE_METADATA_SUPPORT

// Define this on slow cpus
//#define JAYPEG_FAST_BUT_LOSSY

// Define this to use nearest interpolation when scaling up (will be linear 
// otherwise)
//#define JAYPEG_LOW_QUALITY_SCALE

#define JAYPEG_MAX_COMPONENTS 4

#if 0
#include <stdarg.h>
static void odprintf(const char *format, ...)
{
	char buf[4096], *p = buf;
	va_list args;

	va_start(args, format);
	p += _vsnprintf(p, sizeof buf - 1, format, args);
	va_end(args);

	while ( p > buf  &&  isspace(p[-1]) )
		*--p = '\0';

	*p++ = '\r';
	*p++ = '\n';
	*p   = '\0';

	OutputDebugStringA(buf);
}
#define JAYPEG_PRINTF(x) odprintf x
#else
#define JAYPEG_PRINTF(x)
#endif // 0

#endif

