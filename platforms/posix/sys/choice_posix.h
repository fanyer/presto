/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: O/S identification and version, other system-system defines.
 * Sort order: alphabetic in each group; system capabilities, unconditional
 * function availability, conditional functions.
 */
#ifndef POSIX_SYS_CHOICE_POSIX_H
#define POSIX_SYS_CHOICE_POSIX_H __FILE__
/** @file choice_posix.h POSIX and non-stdlib ANSI C contributions to the system system.
 *
 * See also choice_libc.h, which should normally be applicable if the rest of
 * POSIX is supported.  This adds the things ANSI C specifies to be true, along
 * with some functions specified by POSIX.
 */

/* POSIX does guarantee: */
#define SYSTEM_CAN_DELETE_OPEN_FILE		YES
/* ANSI C promises globals, even complex ones */
#define SYSTEM_COMPLEX_GLOBALS			YES
#define SYSTEM_GLOBALS					YES
# ifdef USE_CXX_EXCEPTIONS
/* See CORE-27571: the problem this system define exists to work round is a
 * non-issue when using throw/catch for LEAVE/TRAP: */
#define SYSTEM_LONGJMP_VOLATILE_LOCALS	NO
#define SYSTEM_NOTHROW					YES
# else
/* The documentation of longjmp says (ANSI C, 4.6.2.1) that "... the values of
 * objects of automatic storage duration that are local to the function
 * containing the invocation of the corresponding setjmp macro that do not have
 * volatile-qualified type and have been changed between the setjmp invocation
 * and longjmp call are indeterminate." */
#define SYSTEM_LONGJMP_VOLATILE_LOCALS	YES
#define SYSTEM_NOTHROW					NO
# endif /* USE_CXX_EXCEPTIONS */

/* Functions not in ANSI C, added by POSIX: */
#define SYSTEM_ALLOCA					YES /* no op_alloca ! */
#define SYSTEM_DLL_NAME_LOOKUP			YES
#define SYSTEM_ECVT						POSIX_SYS_USE_NATIVE /* BUG: unused ! */
#define SYSTEM_FCVT						POSIX_SYS_USE_NATIVE /* no op_fcvt ! maybe unused */
#define SYSTEM_GCVT						POSIX_SYS_USE_NATIVE /* BUG: unused ! */
#define SYSTEM_HTONL					POSIX_SYS_USE_NATIVE
#define SYSTEM_HTONS					POSIX_SYS_USE_NATIVE
#define SYSTEM_NTOHL					POSIX_SYS_USE_NATIVE
#define SYSTEM_NTOHS					POSIX_SYS_USE_NATIVE
#define SYSTEM_PUTENV					YES
#define SYSTEM_STRDUP					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRICMP					POSIX_SYS_USE_NATIVE
#define SYSTEM_STRNICMP					POSIX_SYS_USE_NATIVE

/* In the spirit of POSIX, albeit not yet in a public standard: */
# if defined(__FreeBSD__) || defined(__NetBSD__)
#define SYSTEM_STRLCAT					YES
#define SYSTEM_STRLCPY					YES
# else
#define SYSTEM_STRLCAT					NO
#define SYSTEM_STRLCPY					NO
# endif /* *BSD */
#endif /* POSIX_SYS_CHOICE_POSIX_H */
