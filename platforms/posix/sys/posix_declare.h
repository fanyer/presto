/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: none.
 */
#ifndef POSIX_SYS_DECLARE_H
#define POSIX_SYS_DECLARE_H __FILE__
/** Declare the things specified by \c SYSTEM_* variables that are \c YES.
 *
 * Once the \c SYSTEM_* variables are set to suitable values, \c \#include this
 * file to get suitable implementations of corresponding \c op_* defines.
 * Every \c op_* define here is protected in a check that the matching
 * \c SYSTEM_* is \c YES, so it should normally be sufficient to pull in
 * \c posix_choice.h, amend any \c SYSTEM_* where the native implementation
 * doesn't live up to our expectations, then pull in this file.
 *
 * Also defines standard integral types and bool, using the
 * <code><inttypes.h></code> types for the former; and defines some of the
 * miscellaneous macros specified by the system system.
 */

#include "platforms/posix/sys/declare_libc.h"	/* The op_* for ANSI C's standard library */
#include "platforms/posix/sys/declare_const.h"	/* ... plus its const-violators and ... */
#include "platforms/posix/sys/declare_posix.h"	/* other things POSIX and ANSI C promise. */
#include "platforms/posix/sys/declare_gcc.h"	/* The op_* that gcc supports */
#include "platforms/posix/sys/declare_uni.h"	/* The uni_* in case we can ever do them */
#include "platforms/posix/sys/declare_nonix.h"	/* Stray function-like system macros */
#include "platforms/posix/sys/declare_bool.h"	/* Configure BOOL */
#include "platforms/posix/sys/declare_ints.h"	/* Configure other integral types */

/* Miscellany: */

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define _MAX_PATH PATH_MAX

#define NEWLINE "\n"
#define PATHSEP "/"
#define PATHSEPCHAR '/'
#define CLASSPATHSEP ":"
#define CLASSPATHSEPCHAR ':'
#define SYS_CAP_WILDCARD_SEARCH "*"

#include "platforms/posix/sys/include_late.h"	/* Various #include we turn out to need */
#endif /* POSIX_SYS_DECLARE_H */
