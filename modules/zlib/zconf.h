/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2005 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#ifndef _ZCONF_H
#define _ZCONF_H

#if defined USE_ZLIB && !defined USE_SYSTEM_ZLIB

#define Z_PREFIX

/*
 * If you *really* need a unique prefix for all types and library functions,
 * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
 */
#ifdef Z_PREFIX
#  define deflateInit_          opera_z_deflateInit_
#  define deflate               opera_z_deflate
#  define deflateEnd            opera_z_deflateEnd
#  define inflateInit_          opera_z_inflateInit_
#  define inflate               opera_z_inflate
#  define inflateEnd            opera_z_inflateEnd
#  define deflateInit2_         opera_z_deflateInit2_
#  define inflate_fast          opera_z_inflate_fast
#  define inflateCopy           opera_z_inflateCopy
#  define inflate_table         opera_z_inflate_table
#  define deflateSetDictionary  opera_z_deflateSetDictionary
#  define deflateCopy           opera_z_deflateCopy
#  define deflateReset          opera_z_deflateReset
#  define deflateParams         opera_z_deflateParams
#  define deflatePrime          opera_z_deflatePrime
#  define inflateInit2_         opera_z_inflateInit2_
#  define inflateSetDictionary  opera_z_inflateSetDictionary
#  define inflateSync           opera_z_inflateSync
#  define inflateSyncPoint      opera_z_inflateSyncPoint
#  define inflateCopy           opera_z_inflateCopy
#  define inflateReset          opera_z_inflateReset
#  define inflateBack           opera_z_inflateBack
#  define inflateBackEnd        opera_z_inflateBackEnd
#  define compress              opera_z_compress
#  define compress2             opera_z_compress2
#  define compressBound         opera_z_compressBound
#  define uncompress            opera_z_uncompress
#  define adler32               opera_z_adler32
#  define crc32                 opera_z_crc32
#  define get_crc_table         opera_z_get_crc_table

#  define alloc_func            opera_z_alloc_func
#  define free_func             opera_z_free_func
#  define in_func               opera_z_in_func
#  define out_func              opera_z_out_func
#endif

/* Maximum value for memLevel in deflateInit2 */
#define MAX_MEM_LEVEL 9

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#define OF(args)  args

#define ZEXPORT
#define ZEXPORTVA
#define ZEXTERN extern
#ifndef FAR
#   define FAR
#endif

typedef unsigned char  Byte;  /* 8 bits */
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

typedef Byte  FAR Bytef;
typedef char  FAR charf;
typedef int   FAR intf;
typedef uInt  FAR uIntf;
typedef uLong FAR uLongf;
typedef void FAR *voidpf;
typedef void     *voidp;
typedef long      z_off_t;

#endif // USE_ZLIB && !USE_SYSTEM_ZLIB

#endif /* _ZCONF_H */
