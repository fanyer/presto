/***************************************************************************/
/*                                                                         */
/*  ftstdlib.h                                                             */
/*                                                                         */
/*    ANSI-specific library and header configuration file (specification   */
/*    only).                                                               */
/*                                                                         */
/*  Copyright 2002-2007, 2009, 2011 by                                     */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* This file is used to group all #includes to the ANSI C library that   */
  /* FreeType normally requires.  It also defines macros to rename the     */
  /* standard functions within the FreeType source code.                   */
  /*                                                                       */
  /* Load a file which defines __FTSTDLIB_H__ before this one to override  */
  /* it.                                                                   */
  /*                                                                       */
  /*************************************************************************/


#ifndef __FTSTDLIB_H__
#define __FTSTDLIB_H__


/* #include <stddef.h> */

#define ft_ptrdiff_t  ptrdiff_t


  /**********************************************************************/
  /*                                                                    */
  /*                           integer limits                           */
  /*                                                                    */
  /* UINT_MAX and ULONG_MAX are used to automatically compute the size  */
  /* of `int' and `long' in bytes at compile-time.  So far, this works  */
  /* for all platforms the library has been tested on.                  */
  /*                                                                    */
  /* Note that on the extremely rare platforms that do not provide      */
  /* integer types that are _exactly_ 16 and 32 bits wide (e.g. some    */
  /* old Crays where `int' is 36 bits), we do not make any guarantee    */
  /* about the correct behaviour of FT2 with all fonts.                 */
  /*                                                                    */
  /* In these case, `ftconfig.h' will refuse to compile anyway with a   */
  /* message like `couldn't find 32-bit type' or something similar.     */
  /*                                                                    */
  /**********************************************************************/


/* #include <limits.h> */

#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_ULONG_MAX   ULONG_MAX


  /**********************************************************************/
  /*                                                                    */
  /*                 character and string processing                    */
  /*                                                                    */
  /**********************************************************************/


/* #include <string.h> */


// forward declarations since including opera_stdlib causes all kinds of havoc
#ifndef HAVE_MEMCMP
int op_memcmp(const void* a, const void* b, size_t n);
#endif
#ifndef HAVE_MEMCPY
void* op_memcpy(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_MEMMOVE
void* op_memmove(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_MEMSET
void* op_memset(void *s, int c, size_t n);
#endif
#ifndef HAVE_STRCAT
char* op_strcat(char* dest, const char* src);
#endif
#ifndef HAVE_STRCMP
int op_strcmp(const char* s1, const char* s2);
#endif
#ifndef HAVE_STRCPY
char* op_strcpy(char* dest, const char* src);
#endif
#ifndef HAVE_STRLEN
size_t op_strlen(const char* s);
#endif
#ifndef HAVE_STRNCMP
int op_strncmp(const char* s1, const char* s2, size_t n);
#endif
#ifndef HAVE_STRNCPY
char* op_strncpy(char* dest, const char* src, size_t n);
#endif
#ifndef HAVE_STRRCHR
char* op_strrchr(const char* s, int c);
#endif

#ifndef HAVE_MEMCHR
void* op_memchr(const void* s1, int c, size_t n);
#endif // HAVE_MEMCHR


#define ft_memchr   op_memchr
#define ft_memcmp   op_memcmp
#define ft_memcpy   op_memcpy
#define ft_memmove  op_memmove
#define ft_memset   op_memset
#define ft_strcat   op_strcat
#define ft_strcmp   op_strcmp
#define ft_strcpy   op_strcpy
#define ft_strlen   op_strlen
#define ft_strncmp  op_strncmp
#define ft_strncpy  op_strncpy
#define ft_strrchr  op_strrchr
#define ft_strstr   op_strstr


  /**********************************************************************/
  /*                                                                    */
  /*                           file handling                            */
  /*                                                                    */
  /**********************************************************************/


/* ----- using OpFile instead - see ftsystem.cpp ----- */

/* #include <stdio.h> */


// forward declarations since including opera_stdlib causes all kinds of havoc
#ifndef HAVE_SPRINTF
int op_sprintf(char *buffer, const char *format, ...);
#endif


/* #define FT_FILE     FILE */
/* #define ft_fclose   fclose */
/* #define ft_fopen    fopen */
/* #define ft_fread    fread */
/* #define ft_fseek    fseek */
/* #define ft_ftell    ftell */
#define ft_sprintf  op_sprintf


  /**********************************************************************/
  /*                                                                    */
  /*                             sorting                                */
  /*                                                                    */
  /**********************************************************************/


/* #include <stdlib.h> */


#ifndef HAVE_QSORT
void op_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
#endif
#ifndef HAVE_ATOI
int op_atoi(const char* p);
#endif


#define ft_qsort  op_qsort


  /**********************************************************************/
  /*                                                                    */
  /*                        memory allocation                           */
  /*                                                                    */
  /**********************************************************************/


#define ft_scalloc   op_calloc
#define ft_sfree     op_free
#define ft_smalloc   op_malloc
#define ft_srealloc  op_realloc


  /**********************************************************************/
  /*                                                                    */
  /*                          miscellaneous                             */
  /*                                                                    */
  /**********************************************************************/


#define ft_atol   atol
#define ft_labs   labs


  /**********************************************************************/
  /*                                                                    */
  /*                         execution control                          */
  /*                                                                    */
  /**********************************************************************/


#include <setjmp.h>

#define ft_jmp_buf     jmp_buf  /* note: this cannot be a typedef since */
                                /*       jmp_buf is defined as a macro  */
                                /*       on certain platforms           */

#define ft_longjmp     longjmp
#define ft_setjmp( b ) setjmp( *(jmp_buf*) &(b) )    /* same thing here */


  /* the following is only used for debugging purposes, i.e., if */
  /* FT_DEBUG_LEVEL_ERROR or FT_DEBUG_LEVEL_TRACE are defined    */

/* #include <stdarg.h> */


#endif /* __FTSTDLIB_H__ */


/* END */
