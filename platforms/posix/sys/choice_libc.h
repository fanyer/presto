/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: none.
 * Sort order: alphabetic.
 */
#ifndef POSIX_SYS_CHOICE_LIBC_H
#define POSIX_SYS_CHOICE_LIBC_H __FILE__
/** @file choice_libc.h Opera system-system configuration for C standard library support.
 *
 * This simply says yes to everything in ANSI C's standard library (except, when
 * POSIX_SYS_USE_NATIVE is defined to NO, the things our own stdlib implements);
 * declare_libc.h provides the default implementation.  It is to be expected
 * that it can be generally used by most platforms in practice, due to the
 * ubiquity of ANSI C support.
 */

#define SYSTEM_ABS						POSIX_SYS_USE_NATIVE
#define SYSTEM_ACOS						YES
#define SYSTEM_ASIN						YES
#define SYSTEM_ATAN						YES
#define SYSTEM_ATAN2					YES
#define SYSTEM_ATOI						POSIX_SYS_USE_NATIVE
#define SYSTEM_BSEARCH					POSIX_SYS_USE_NATIVE
#define SYSTEM_CEIL						POSIX_SYS_USE_NATIVE
#define SYSTEM_COS						YES
#define SYSTEM_ERRNO					YES
#define SYSTEM_EXP						YES
#define SYSTEM_FABS						POSIX_SYS_USE_NATIVE
#define SYSTEM_FLOOR					POSIX_SYS_USE_NATIVE
#define SYSTEM_FMOD						YES
#define SYSTEM_FREXP					POSIX_SYS_USE_NATIVE
#define SYSTEM_GETENV					YES
#define SYSTEM_GMTIME					POSIX_SYS_USE_NATIVE
#define SYSTEM_LDEXP					YES
#define SYSTEM_LEAVE_WITH_JMP			YES
#define SYSTEM_LOCALECONV				YES
#define SYSTEM_LOCALTIME				POSIX_SYS_USE_NATIVE
#define SYSTEM_LOG						YES
#define SYSTEM_MALLOC					YES
#define SYSTEM_MEMCHR					POSIX_SYS_USE_NATIVE
#define SYSTEM_MEMCMP					POSIX_SYS_USE_NATIVE
#define SYSTEM_MEMCPY					POSIX_SYS_USE_NATIVE
#define SYSTEM_MEMMOVE					POSIX_SYS_USE_NATIVE
#define SYSTEM_MEMSET					POSIX_SYS_USE_NATIVE
#define SYSTEM_MKTIME					POSIX_SYS_USE_NATIVE
#define SYSTEM_MODF						POSIX_SYS_USE_NATIVE
#define SYSTEM_OPFILELENGTH_IS_SIGNED	YES
#define SYSTEM_POW						YES
#define SYSTEM_QSORT					POSIX_SYS_USE_NATIVE
#define SYSTEM_QSORT_S					POSIX_SYS_USE_NATIVE
#define SYSTEM_RAND						POSIX_SYS_USE_NATIVE
#define SYSTEM_SETJMP					YES
#define SYSTEM_SETLOCALE				YES
#define SYSTEM_SIN						YES
#define SYSTEM_SNPRINTF					POSIX_SYS_USE_NATIVE
#define SYSTEM_SPRINTF					POSIX_SYS_USE_NATIVE
#define SYSTEM_SQRT						YES
#define SYSTEM_SSCANF					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRCAT					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRCHR					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRCMP					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRCPY					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRCSPN					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRLEN					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRNCAT					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRNCMP					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRNCPY					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRPBRK					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRRCHR					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRSPN					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRSTR					POSIX_SYS_USE_NATIVE
#define SYSTEM_TAN						YES
#define SYSTEM_TIME						YES
#define SYSTEM_STRTOD					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRTOL					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRTOUL					POSIX_SYS_USE_NATIVE
#define SYSTEM_VSNPRINTF				POSIX_SYS_USE_NATIVE
#define SYSTEM_VSPRINTF					POSIX_SYS_USE_NATIVE
#define SYSTEM_VSSCANF					POSIX_SYS_USE_NATIVE

#endif /* POSIX_SYS_CHOICE_LIBC_H */
