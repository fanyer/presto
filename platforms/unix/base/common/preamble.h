/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef COMMON_PREAMBLE_H
#define COMMON_PREAMBLE_H __FILE__
/* This file should be included as early as possible by all Unix builds.
 *
 * It should contain preprocessor directives that don't depend on anything Opera
 * does; it replaces various things we used to do in our make files.
 *
 * All comments in it must be C-compatible.
 */

#ifndef HAVE_BASENAME
# define HAVE_BASENAME
#endif
#ifndef HAVE_MALLINFO
# define HAVE_MALLINFO
#endif
#ifndef HAVE_STRINGS_H
# define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_UNISTD_H
# define HAVE_UNISTD_H 1
#endif
#ifndef NEEDS_STDARG
# define NEEDS_STDARG
#endif
#ifndef NEEDS_STDDEF
# define NEEDS_STDDEF
#endif
#ifndef NEEDS_STDLIB
# define NEEDS_STDLIB
#endif
#ifndef NEEDS_TIMEH
# define NEEDS_TIMEH
#endif
#if !defined(_PTHREADS_) && !defined(_NO_THREADS_)
# define _PTHREADS_
#endif

#if !defined(__FreeBSD__) && !defined(__NetBSD__)
# ifndef HAVE_MALLOC_H
#  define HAVE_MALLOC_H 1
# endif
#endif
#ifdef __CYGWIN__
# define HAVE_STRCASE
#endif
#ifndef HAVE_NL_LANGINFO
/* Present in glibc, known OK on: Linux, FreeBSD. */
# define HAVE_NL_LANGINFO
#endif

#ifdef HAVE_NL_LANGINFO
# include <langinfo.h>
/* Defined on FreeBSD (at least 5.3, maybe others); conflicts with a member of
 * OpWindowCommander's LayoutMode enum: */
# undef ERA
#endif

#define HAVE_SOCKLEN_T

#if defined(__x86_64__) || (__WORDSIZE + 0 == 64)
# define HAVE_64BIT_LONG
# define HAVE_64BIT_POINTERS
#endif

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#endif /* COMMON_PREAMBLE_H */
