/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: O/S, __cplusplus.
 */
#ifndef POSIX_SYS_INCLUDE_EARLY_H
#define POSIX_SYS_INCLUDE_EARLY_H __FILE__
/** @file include_early.h Preamble to \#include standard system headers.
 *
 * These headers are generally available and needed, although some platforms may
 * have complications to deal with, e.g. if a system header lacks \c __cplusplus
 * <code>\#if</code>-ery its \c \#include may need to be enclosed in a
 * <code>\#ifdef</code> of that.
 *
 * Please let the module owner know if any of the headers required below or in
 * include_late.h are not generally available; we may be able to do without
 * them.
 *
 * This file provides the pre-configuration (any defines system headers need to
 * see to provide everything we need) and pulls in the headers that define types
 * that are the subject of typedefs in declare_*.h; further headers may be
 * pulled in by include_late.h, if we enable SYSTEM_* defines that call for
 * them.
 */
#define __STDC_LIMIT_MACROS		/* We need INT64_M{IN,AX} from <stdint.h> */
#define _LARGEFILE64_SOURCE 1	/* Enable large file support on (at least) Linux. */
#define _FILE_OFFSET_BITS 64	/* Enable 64-bit file operations on (at least) Linux. */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE			/* Enable qsort_r() on Linux. */
#endif

/* Definitions of types referenced in typedefs: */
#include <inttypes.h>	/* Needed by declare_ints.h */
#include <limits.h>		/* INT_MAX &c. */
#include <float.h>		/* DBL_* &c. */
#include <stddef.h>		/* ptrdiff_t */
#include <setjmp.h>		/* jmp_buf &c. */
#include <time.h>		/* time_t &c. */

/* Declarations and macro definitions referenced during choice: */
#include <stdarg.h> /* va_copy in choice_gcc.h */

/* Functions called by inlines in declare_const.h: */
#include <stdlib.h>
#include <string.h>

/* Defines controlling later headers, see include_late.h: */
#undef POSIX_SYS_NEED_NETINET /* <arpa/inet.h> or <netinet/in.h>, see declare_posix.h */

#endif /* POSIX_SYS_INCLUDE_EARLY_H */
