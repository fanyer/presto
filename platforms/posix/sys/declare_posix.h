/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines.
 */
#ifndef POSIX_SYS_DECLARE_POSIX_H
#define POSIX_SYS_DECLARE_POSIX_H __FILE__
/** @file declare_posix.h POSIX and non-stdlib ANSI C contributions to the system system.
 *
 * See also declare_libc.h and declare_const.h, which should normally be
 * applicable if the rest of POSIX is supported.  This adds some functions
 * specified by POSIX.
 */

# if SYSTEM_HTONL == YES
#define op_htonl(l)				htonl(l)
#define POSIX_SYS_NEED_NETINET
# endif
# if SYSTEM_HTONS == YES
#define op_htons(h)				htons(h)
#define POSIX_SYS_NEED_NETINET
# endif
# if SYSTEM_NTOHL == YES
#define op_ntohl(l)				ntohl(l)
#define POSIX_SYS_NEED_NETINET
# endif
# if SYSTEM_NTOHS == YES
#define op_ntohs(h)				ntohs(h)
#define POSIX_SYS_NEED_NETINET
# endif

# if SYSTEM_PUTENV == YES
#define op_putenv(string)		putenv(string)
# endif
# if SYSTEM_STRDUP == YES
#define op_lowlevel_strdup(s)	strdup(s)
# endif
# if SYSTEM_STRICMP == YES
#define op_stricmp(s,t)			strcasecmp(s,t)
# endif
# if SYSTEM_STRNICMP == YES
#define op_strnicmp(s,t,n)		strncasecmp(s,t,n)
# endif
/* In the spirit of POSIX, albeit not yet in a public standard: */
# if SYSTEM_STRLCAT == YES
#define op_strlcat(t,f,n)		strlcat(t,f,n)
# endif
# if SYSTEM_STRLCPY == YES
#define op_strlcpy(t,f,n)		strlcpy(t,f,n)
# endif

#endif /* POSIX_SYS_DECLARE_POSIX_H */
