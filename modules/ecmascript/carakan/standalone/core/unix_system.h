/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef UNIX_SYSTEM_H
#define UNIX_SYSTEM_H

#define _FILE_OFFSET_BITS 64 /* Enable 64-bit file operations */
#define NO 0
#define YES 1

/* Make stdint.h define INTn_MIN and INTn_MAX macros (and some more
   that we don't need but that hopefully won't hurt either.)  We
   specificly want INT64_MIN and INT64_MAX, since we set SYSTEM_INT64
   to YES below. */
#define __STDC_LIMIT_MACROS

#include <inttypes.h>
#include <limits.h> // Hardcore module requires INT_MAX & co to be defined.
#include <float.h> // Hardcore module requires DBL_ & co to be defined.
#include <stddef.h> // Hardcore module requires ptrdiff_t to be declared.
#include <setjmp.h> // Need jmp_buf and friends.
#include <time.h> // Need time_t and friends.
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h> // Probetools needs sleep()
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

/* Part 1: Indicate which system features work on this platform.
 */

#define SYSTEM_LOCALECONV				YES
#define SYSTEM_LONGJMP_VOLATILE_LOCALS	YES
#define SYSTEM_LONGLONG					NO
#define SYSTEM_RECT						NO
#define SYSTEM_POINT					NO
#define SYSTEM_COLORREF					NO
#define SYSTEM_SIZE						NO
#define SYSTEM_MAKELONG					NO
#define SYSTEM_RGB						NO
#define SYSTEM_ALLOCA					YES
#define SYSTEM_NEWA_OVERFLOW_CHECK		NO
#if SMARTPHONE_PROFILE
#define SYSTEM_GLOBALS                  NO
#else
#define SYSTEM_GLOBALS                  YES
#endif
#define SYSTEM_COMPLEX_GLOBALS          YES
#define SYSTEM_NAN_ARITHMETIC_OK		YES
#define SYSTEM_NAN_EQUALS_EVERYTHING	NO
#define SYSTEM_QUIET_NAN_IS_ONE			YES
#define SYSTEM_DLL_NAME_LOOKUP			YES
#define SYSTEM_SETJMP					YES
#define SYSTEM_BLOCK_CON_FILE           NO
#define SYSTEM_DRIVES                   NO
#define SYSTEM_MULTIPLE_DRIVES          NO
#define SYSTEM_NETWORK_BACKSLASH_PATH   NO
#define SYSTEM_OWN_OP_STATUS_VALUES     NO

#define SYSTEM_IEEE_DENORMALS			YES

#define SYSTEM_BIG_ENDIAN                               NO

#define SYSTEM_RISC_ALIGNMENT			NO

// Library defines grouped together here

#define SYSTEM_ABS						NO
#define SYSTEM_ACOS						NO
#define SYSTEM_ASIN						NO
#define SYSTEM_ATAN2					NO
#define SYSTEM_ATAN						NO
#define SYSTEM_ATOI						NO
#define SYSTEM_BSEARCH					NO
#define SYSTEM_CEIL						NO
#define SYSTEM_COS						NO
#define SYSTEM_DTOA						NO
#define SYSTEM_ECVT						NO
#define SYSTEM_ETOA						NO
#define SYSTEM_EXP						NO
#define SYSTEM_FABS						NO
#define SYSTEM_FCVT						NO
#define SYSTEM_FLOOR					NO
#define SYSTEM_FMOD						NO
#define SYSTEM_FREXP					NO
#define SYSTEM_GCVT						NO
#define SYSTEM_GMTIME					NO
#define SYSTEM_ITOA						NO		// not in glibc
#define SYSTEM_LDEXP					NO
#define SYSTEM_LOCALTIME				NO
#define SYSTEM_LOG						NO
#define SYSTEM_LTOA						NO		// not in glibc
#define SYSTEM_MALLOC					YES
#define SYSTEM_MEMCHR					NO
#define SYSTEM_MEMCMP					NO
#define SYSTEM_MEMCPY					NO
#define SYSTEM_MEMMEM					NO
#define SYSTEM_MEMMOVE					NO
#define SYSTEM_MEMSET					NO
#define SYSTEM_MKTIME					NO
#define SYSTEM_MODF						NO
#define SYSTEM_POW						NO
#define SYSTEM_QSORT					NO
#define SYSTEM_RAND						YES
# define HAVE_RAND
#define SYSTEM_SETLOCALE				YES
# define HAVE_SETLOCALE
#define SYSTEM_SIN						NO
#define SYSTEM_SNPRINTF					NO
#define SYSTEM_SPRINTF					NO
#define SYSTEM_SQRT						NO
#define SYSTEM_SSCANF					NO
#define SYSTEM_STRCASE					NO		// not in glibc
#define SYSTEM_STRCAT					NO
#define SYSTEM_STRCHR					NO
#define SYSTEM_STRCMP					NO
#define SYSTEM_STRCPY					NO
#define SYSTEM_STRCSPN					NO
#define SYSTEM_STRDUP					NO
#define SYSTEM_STRICMP					NO
#define SYSTEM_STRLEN					NO
#define SYSTEM_STRLCAT					NO		// not in glibc
#define SYSTEM_STRLCPY					NO		// not in glibc
#define SYSTEM_STRNCAT					NO
#define SYSTEM_STRNCMP					NO
#define SYSTEM_STRNCPY					NO
#define SYSTEM_STRNICMP					NO
#define SYSTEM_STRPBRK					NO
#define SYSTEM_STRRCHR					NO
#define SYSTEM_STRREV					NO		// not in glibc
#define SYSTEM_STRSPN					NO
#define SYSTEM_STRSTR					NO
#define SYSTEM_STRTOD					NO
#define SYSTEM_STRTOL					NO
#define SYSTEM_STRTOUL					NO
#define SYSTEM_TAN						NO
#define SYSTEM_TIME						YES		// makes life easier
#define SYSTEM_VSNPRINTF				NO
#define SYSTEM_VSPRINTF					NO

// glibc wchar_t is not the same as uni_char, so all SYSTEM_UNI_*
// must be set to NO
#define SYSTEM_UNI_MEMCHR				NO
#define SYSTEM_UNI_ITOA					NO
#define SYSTEM_UNI_LTOA					NO
#define SYSTEM_UNI_SNPRINTF				NO
#define SYSTEM_UNI_SPRINTF				NO
#define SYSTEM_UNI_SSCANF				NO
#define SYSTEM_UNI_STRCAT				NO
#define SYSTEM_UNI_STRCHR				NO
#define SYSTEM_UNI_STRCMP				NO
#define SYSTEM_UNI_STRCPY				NO
#define SYSTEM_UNI_STRCSPN				NO
#define SYSTEM_UNI_STRDUP				NO
#define SYSTEM_UNI_STRICMP				NO
#define SYSTEM_UNI_STRISTR				NO
#define SYSTEM_UNI_STRLCAT				NO
#define SYSTEM_UNI_STRLCPY				NO
#define SYSTEM_UNI_STRLEN				NO
#define SYSTEM_UNI_STRNCAT				NO
#define SYSTEM_UNI_STRNCMP				NO
#define SYSTEM_UNI_STRNCPY				NO
#define SYSTEM_UNI_STRNICMP				NO
#define SYSTEM_UNI_STRPBRK				NO
#define SYSTEM_UNI_STRRCHR				NO
#define SYSTEM_UNI_STRREV				NO
#define SYSTEM_UNI_STRSPN				NO
#define SYSTEM_UNI_STRSTR				NO
#define SYSTEM_UNI_STRTOK				NO
#define SYSTEM_UNI_VSNPRINTF			NO
#define SYSTEM_UNI_VSPRINTF				NO
#define SYSTEM_INT64					YES
#define HAVE_INT64
#define SYSTEM_UINT64					YES
#define HAVE_UINT64
# if defined(__alpha__) || defined(__x86_64__)
#  define SYSTEM_64BIT                                  YES
# else
#  define SYSTEM_64BIT                                  NO
# endif
#define SYSTEM_YIELD					NO
#define SYSTEM_GETPHYSMEM				NO
#define SYSTEM_NTOHS					NO
#define SYSTEM_NTOHL					NO
#define SYSTEM_HTONS					NO
#define SYSTEM_HTONL					NO
#define SYSTEM_GUID					NO


#define HAVE_SOCKLEN_T

/* Part 2: defines of various things presumed by the rest of opera:
 */

typedef int BOOL; // FIXME
#define TRUE 1
#define FALSE 0

typedef uintptr_t UINTPTR;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t UINT8;
typedef intptr_t INTPTR;
typedef int64_t INT64;
typedef int32_t INT32;
typedef int16_t INT16;
typedef int8_t INT8;

typedef UINT32 uint32;
typedef UINT16 uint16;
typedef UINT8 uint8;
typedef INT32 int32;
typedef INT16 int16;
typedef INT8 int8;

#ifdef WCHAR_IS_UNICHAR
/* UNI_L() needs to be done in two stages, so that UNI_L(s) expressions where s
   is a macro get s translated to the string literal before 'L' is
   prepended. */
# define _UNI_L_FORCED_WIDE(s) L ## s
# define UNI_L(s) _UNI_L_FORCED_WIDE(s)
# define WCHAR_SYSTEMLIB_T unsigned int // probably not a very safe assumption to make
typedef wchar_t uni_char;
#else
# define WCHAR_SYSTEMLIB_T wchar_t
typedef UINT16 uni_char;
#endif // WCHAR_IS_UNICHAR

typedef unsigned int   UINT;
typedef int            INT;
typedef unsigned char  UCHAR;
typedef unsigned char  BYTE;
typedef long           LONG;

typedef BYTE           byte;

typedef UINT32         DWORD;
typedef unsigned short WORD;

#ifndef max
#ifdef __cplusplus
template<class T,class T2> static inline T min(T a, T2 b) { if( a < b ) return a; return (T)b; }
template<class T,class T2> static inline T max(T a, T2 b) { if( a > b ) return a; return (T)b; }
#else // __cplusplus
# define min(a,b) (((a) < (b)) ? (a) : (b))
# define max(a,b) (((a) > (b)) ? (a) : (b))
#endif // __cplusplus
#endif // max

typedef UINT64 OpFileLength;
#define FILE_LENGTH_NONE UINT64_MAX

#define IN
#define OUT
#define IO
#define OPT
#define far
#define _MAX_PATH PATH_MAX
#define MAX_PATH PATH_MAX
#define huge
#define IGNORE_SZ_PARAM	NULL

#define VER_NUM 9
#define VER_NUM_STR "9.00"
#define VER_NUM_INT_STR "900"
#define VER_NUM_STR_UNI UNI_L(VER_NUM_STR)
#define VER_NUM_INT_STR_UNI UNI_L(VER_NUM_INT_STR)

#define __export

#define PATHSEP "/"
#define PATHSEPCHAR '/'
#define CLASSPATHSEP ":"
#define CLASSPATHSEPCHAR ':'
#define SYS_CAP_WILDCARD_SEARCH "*"

#if SYSTEM_ACOS == YES
# define op_acos acos
#endif
#if SYSTEM_ASIN == YES
# define op_asin asin
#endif
#if SYSTEM_ATAN2 == YES
# define op_atan2 atan2
#endif
#if SYSTEM_ATAN == YES
# define op_atan atan
#endif
#if SYSTEM_ATOI == YES
# define op_atoi atoi
#endif
#if SYSTEM_BSEARCH == YES
# define op_bsearch bsearch
#endif
#if SYSTEM_CEIL == YES
# define op_ceil ceil
#endif
#if SYSTEM_COS == YES
# define op_cos cos
#endif
#if SYSTEM_EXP == YES
# define op_exp exp
#endif
#if SYSTEM_FABS == YES
# define op_fabs fabs
#endif
#if SYSTEM_FLOOR == YES
# define op_floor floor
#endif
#if SYSTEM_FMOD == YES
# define op_fmod fmod
#endif
#if SYSTEM_FREXP == YES
# define op_frexp frexp
#endif
#define op_getenv getenv
#if SYSTEM_GMTIME == YES
# define op_gmtime gmtime
#endif
#if SYSTEM_ITOA == YES
# define op_itoa op_itoa
#endif
#if SYSTEM_LDEXP == YES
# define op_ldexp ldexp
#endif
#if SYSTEM_LOCALTIME == YES
# define op_localtime localtime
#endif
#if SYSTEM_LOG == YES
# define op_log log
#endif
#if SYSTEM_MEMCHR == YES
# define op_memchr memchr
#endif
#if SYSTEM_UNI_MEMCHR == YES
# define uni_memchr wmemchr
#endif
#if SYSTEM_MEMCMP == YES
# define op_memcmp memcmp
#endif
#if SYSTEM_MEMCPY == YES
# define op_memcpy memcpy
#endif
#if SYSTEM_MEMMEM == YES
# define op_memmem memmem
#endif
#if SYSTEM_MEMMOVE == YES
# define op_memmove memmove
#endif
#if SYSTEM_MEMSET == YES
# define op_memset memset
#endif
#if SYSTEM_MKTIME == YES
# define op_mktime mktime
#endif
#if SYSTEM_MODF == YES
# define op_modf modf
#endif
#if SYSTEM_POW == YES
# define op_pow pow
#endif
#define op_putenv putenv
#if SYSTEM_QSORT == YES
# define op_qsort qsort
#endif
#if SYSTEM_RAND == YES
# define op_rand rand
# define op_srand srand
#endif
#if SYSTEM_SETLOCALE == YES
# define op_setlocale setlocale
#endif
#if SYSTEM_SIN == YES
# define op_sin sin
#endif
//#if SYSTEM_SLEEP == YES
# define op_sleep sleep
//#endif
#if SYSTEM_SNPRINTF == YES
# define op_snprintf snprintf
#endif
#if SYSTEM_SPRINTF == YES
# define op_sprintf sprintf
#endif
#if SYSTEM_SQRT == YES
# define op_sqrt sqrt
#endif
#if SYSTEM_SSCANF == YES
# define op_sscanf sscanf
#endif
#if SYSTEM_STRCAT == YES
# define op_strcat strcat
#endif
#if SYSTEM_STRCHR == YES
# define op_strchr strchr
#endif
#if SYSTEM_STRCMP == YES
# define op_strcmp strcmp
#endif
#if SYSTEM_STRCPY == YES
# define op_strcpy strcpy
#endif
#if SYSTEM_STRCSPN == YES
# define op_strcspn strcspn
#endif
#if SYSTEM_STRICMP == YES
# define op_stricmp strcasecmp
#endif
#if SYSTEM_STRLEN == YES
# define op_strlen strlen
#endif
#if SYSTEM_STRNCAT == YES
# define op_strncat strncat
#endif
#if SYSTEM_STRNCMP == YES
# define op_strncmp strncmp
#endif
#if SYSTEM_STRNCPY == YES
# define op_strncpy strncpy
#endif
#if SYSTEM_STRNICMP == YES
# define op_strnicmp strncasecmp
#endif
#if SYSTEM_STRPBRK == YES
# define op_strpbrk strpbrk
#endif
#if SYSTEM_STRRCHR == YES
# define op_strrchr strrchr
#endif
#if SYSTEM_STRREV == YES
# define op_strrev strrev
#endif
#if SYSTEM_STRSPN == YES
# define op_strspn strspn
#endif
#if SYSTEM_STRSTR == YES
# define op_strstr strstr
#endif
#if SYSTEM_STRTOD == YES
# define op_strtod strtod
#endif
#if SYSTEM_STRTOL == YES
# define op_strtol strtol
#endif
#if SYSTEM_STRTOUL == YES
# define op_strtoul strtoul
#endif
#if SYSTEM_TAN == YES
# define op_tan tan
#endif
#if SYSTEM_TIME == YES
# define op_time time
#endif
#if SYSTEM_VSNPRINTF == YES
# define op_vsnprintf vsnprintf
#endif
#if SYSTEM_VSPRINTF == YES
# define op_vsprintf vsprintf
#endif

#if SYSTEM_NTOHS == YES || SYSTEM_NTOHL == YES || SYSTEM_HTONS == YES || SYSTEM_HTONL == YES
# include <arpa/inet.h>
# if SYSTEM_NTOHS == YES
#  define op_ntohs ntohs
# endif
# if SYSTEM_NTOHL == YES
#  define op_ntohl ntohl
# endif
# if SYSTEM_HTONS == YES
#  define op_htons htons
# endif
# if SYSTEM_HTONL == YES
#  define op_htonl htonl
# endif
#endif

/* Delete when hardcore gets fixed (owned by stdlib) */
#define op_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* Temporary compile hack (peter 2005-11-30) */
char *StrToLocaleEncoding(const uni_char *str);
uni_char *StrFromLocaleEncoding(const char *str);


#if defined(__cplusplus) && SYSTEM_GLOBALS == NO
// We have a global Opera object anyway.
extern class Opera* g_opera;
#endif // __cplusplus && SYSTEM_GLOBALS == NO

#undef NO
#undef YES

#endif // !UNIX_SYSTEM_H
