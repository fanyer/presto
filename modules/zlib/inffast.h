/* inffast.h -- header to use inffast.c
 * Copyright (C) 1995-2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

#ifndef ZLIB_INFFAST_H
#define ZLIB_INFFAST_H

#if defined USE_ZLIB && !defined USE_SYSTEM_ZLIB

#ifndef ZLIB_REDUCE_FOOTPRINT
void inflate_fast OF((z_streamp strm, unsigned start));
#endif // ZLIB_REDUCE_FOOTPRINT

#endif // USE_ZLIB && !USE_SYSTEM_ZLIB

#endif // ZLIB_INFFAST_H
