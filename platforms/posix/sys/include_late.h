/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: O/S, __cplusplus.
 */
#ifndef POSIX_SYS_INCLUDE_LATE_H
#define POSIX_SYS_INCLUDE_LATE_H __FILE__
/** @file include_late.h Final tidy-up and \#include of needed system headers.
 *
 * See include_early.h for general comments also applicable here.
 */

#include <math.h>
#include <stdio.h>
#include <locale.h>
#ifdef __cplusplus
#include <new>
#endif

/* libopeay/crypto/, all on its own, accounts for several: */
#include <unistd.h>		/* get[pu]id, read, write, close */
#include <sys/stat.h>	/* struct stat */
#include <fcntl.h>		/* open */

#if SYSTEM_ERRNO == YES
#include <errno.h>
#endif

#ifdef POSIX_SYS_NEED_NETINET
# ifdef sun
#include <netinet/in.h>
# else
#include <arpa/inet.h>
# endif
#endif /* POSIX_SYS_NEED_NETINET */

/* #undef any preprocessor defines that conflict with our source: */
#ifdef sun
# ifdef i386
#undef ERR /* <sys/regset.h>'s vs OpStatus::ERR */
# endif
#undef QEND /* <sys/stream.h> vs logdoc/html.h's enum TagEnd */
#endif /* Solaris API: #define conflicts with our enums */

#endif /* POSIX_SYS_INCLUDE_LATE_H */
