/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, WCHAR_IS_UNICHAR.
 */
#ifndef POSIX_SYS_DECLARE_INTS_H
#define POSIX_SYS_DECLARE_INTS_H __FILE__

/** @file declare_ints.h Miscellaneous integral types.
 *
 * All the associated *_MAX and *_MIN defines are presumed to have been supplied
 * by <limits.h>, as specified in ANSI X3.159-1989 and later editions of the C
 * language standard.
 */

typedef uintptr_t		UINTPTR;
# if SYSTEM_UINT64 == YES
typedef uint64_t		UINT64;
# endif
typedef uint32_t		UINT32;
typedef uint16_t		UINT16;
typedef uint8_t			UINT8;

typedef intptr_t		INTPTR;
# if SYSTEM_INT64 == YES
typedef int64_t			INT64;
# endif
typedef int32_t			INT32;
typedef int16_t			INT16;
typedef int8_t			INT8; /* Not strictly in the system system ! See CORE-38087. */

typedef unsigned int	UINT;
typedef int				INT;
typedef unsigned char	UCHAR;

/* <avoid> Deprecated types: */
typedef UINT32			DWORD;
typedef unsigned short	WORD;
typedef unsigned char	BYTE;
typedef BYTE			byte;
typedef long			LONG;

typedef UINT32			uint32;
typedef UINT16			uint16;
typedef UINT8			uint8;
typedef INT32			int32;
typedef INT16			int16;
typedef INT8			int8;
/* </avoid> */

/* OpFileLength: */
# if SYSTEM_OPFILELENGTH_IS_SIGNED == YES
#  if SYSTEM_INT64 == YES
typedef INT64			OpFileLength;
#define FILE_LENGTH_NONE INT64_MIN
#define FILE_LENGTH_MAX	 INT64_MAX
#  else /* SYSTEM_INT64 is NO */
typedef INT32			OpFileLength;
#define FILE_LENGTH_NONE INT32_MIN
#define FILE_LENGTH_MAX	 INT32_MAX
#  endif /* SYSTEM_INT64 */
# else /* SYSTEM_OPFILELENGTH_IS_SIGNED is NO */
#  if SYSTEM_UINT64 == YES
typedef UINT64			OpFileLength;
#define FILE_LENGTH_NONE UINT64_MAX
#define FILE_LENGTH_MAX  UINT64_MAX
#  else /* SYSTEM_UINT64 is NO */
typedef UINT32			OpFileLength;
#define FILE_LENGTH_NONE UINT32_MAX
#define FILE_LENGTH_MAX  UINT32_MAX
#  endif /* SYSTEM_UINT64 */
# endif /* SYSTEM_OPFILELENGTH_IS_SIGNED */

/* UNI_L() and uni_char: */
# ifdef WCHAR_IS_UNICHAR
/* wchar_t is the same as uni_char, which also means that wchar_t is
 * incompatible with the wchar_t used in system libraries.  Special attention
 * (OK, you may call it a hack if you like) is therefore needed when calling
 * system library functions that take wchar_t arguments. */

/* UNI_L() needs to be done in two stages, so that UNI_L(s) expressions where s
 * is a macro get s translated to the string literal before 'L' is prepended. */
#define _UNI_L_FORCED_WIDE(s) L ## s
#define UNI_L(s) _UNI_L_FORCED_WIDE(s)
#define WCHAR_SYSTEMLIB_T UINT32 /* not necessarily an entirely safe assumption */
typedef wchar_t uni_char;
# else /* Use unil_convert to handle UNI_L(): */
#define WCHAR_SYSTEMLIB_T wchar_t
typedef UINT16 uni_char;
# endif /* WCHAR_IS_UNICHAR */
#endif /* POSIX_SYS_DECLARE_INTS_H */
