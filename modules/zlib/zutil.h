/* zutil.h -- internal interface and configuration of the compression library
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

/* @(#) $Id$ */

#ifndef _Z_UTIL_H
#define _Z_UTIL_H

#if defined USE_ZLIB && !defined USE_SYSTEM_ZLIB

#include "modules/zlib/zlib.h"

#define local static
#define SET_MSG(X)

typedef unsigned char  uch;
typedef uch FAR uchf;
typedef unsigned short ush;
typedef ush FAR ushf;
typedef unsigned long  ulg;

#define ERR_RETURN(strm,err) return err
/* To be used only when the state is known to be valid */

        /* common constants */

#ifndef DEF_WBITS
#  define DEF_WBITS MAX_WBITS
#endif
/* default windowBits for decompression. MAX_WBITS is for compression only */

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
/* default memLevel */

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

extern int  zmemcmp  (const void* s1, const void* s2, uInt len);
extern void zmemcpy  (void* s1, const void* s2, uInt len);
extern void zmemzero (void* dest, uInt len);

#define Assert(cond,msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c,x)
#define Tracecv(c,x)


typedef uLong (ZEXPORT *check_func)(uLong check, const Bytef *buf, uInt len);
/* voidpf zcalloc(voidpf opaque, unsigned items, unsigned size); */
/* void   zcfree(voidpf opaque, voidpf ptr); */
voidpf _zalloc(unsigned items);
void   _zfree(voidpf ptr);

#define ZALLOC(strm, items, size) _zalloc((items)*(size))
#define ZFREE(strm, addr)  _zfree(addr)
#define TRY_FREE(s, p) _zfree(p)

#endif // USE_ZLIB && !USE_SYSTEM_ZLIB

#endif /* _Z_UTIL_H */
