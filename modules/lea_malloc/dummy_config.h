/* -*- Mode: c; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef LEA_MALLOC_DUMMY_CONFIG_H
#define LEA_MALLOC_DUMMY_CONFIG_H __FILE__
/** \file LEA_MALLOC_CONFIG_HEADER
 *
 * Platforms which need to configure Doug Lea's malloc to behave differently
 * than its default should specify their configuration in a header file in
 * platform code and compile with LEA_MALLOC_CONFIG_HEADER defined to be a
 * string (i.e. *with* enclosing quotes) naming that file, relative to the Opera
 * root directory (or suitable platform-specific include path entry); this
 * macro, if defined, shall be the subject of a #include directive.  For
 * projects using the tweaks system (i.e. core-2 and later) this macro is
 * controlled via the tweak TWEAK_LEA_MALLOC_PLATFORM_CONFIG (q.v.)
 *
 * This module supports several features (see hardcore/features/features.txt),
 * tweaks (see module.tweaks) and APIs (see module.export).  These should be
 * controlled in the usual way (see documentation in hardcore), not via
 * LEA_MALLOC_CONFIG_HEADER; this file is only seen by the single compilation
 * unit, lea_malloc.cpp, which actually implements malloc.
 *
 * The upstream original code (see malloc.c) provides detailed documentation for
 * the defines it supports, which you may wish to configure.  Some of these are
 * controlled by features and tweaks (see above) or by exploiting defines from
 * the system system: {HAV,US}E_MEMCPY, USE_MALLOC_LOCK, USE_DL_PREFIX, DEBUG.
 * These should be configured via the relevant system: all others may be
 * configured in LEA_MALLOC_CONFIG_HEADER.  Defining the MORECORE macros (q.v.)
 * is particularly often important for projects with unusual needs.
 *
 * There are also some extra defines controlling the hacks Opera has added to
 * this implementation of malloc.
 *
 * If INITIAL_HEAP_MIN is defined, on WIN32 builds, then the first call to
 * sbrk() shall round the size of the lump of memory it's asked for up to
 * INITIAL_HEAP_MIN, if it's less.
 *
 * The upstream code supports assorted LACKS_various_H defines which it tests
 * before trying to #include <various.h>; we've added LACKS_SYS_TYPES_H for
 * platforms without <sys/types.h>.
 *
 * If you use a recent core-2, the memory module will take care of this module's
 * data structures automatically, within the OpMemoryState infrastructure.
 *
 * If you are NOT using OpMemoryState (please do, it'll make your life easier),
 * you should make sure the get_malloc_state() macro is #defined (you must
 * #define it, not just declare it as an extern variable) so as to resolve to a
 * pointer variable which points to a suitable lump of memory for holding this
 * module's internal state (aka. malloc_state). The size of this lump of memory
 * must meet or exceed the value returned by get_malloc_state_size() (see
 * lea_malloc.h), and must be initially (i.e. BEFORE the first call to malloc()
 * or any other function in the primary API) filled with zeroes. This ensures
 * that this module can bootstrap its internal structures into said lump of
 * memory upon the first call to malloc().
 *
 * By default, assert is defined to OP_ASSERT(); however, your configuration
 * header can provide another (compatible) #define for assert if you want.
 * Doing this (or defining _DEBUG) turns on assorted checking (expect to take a
 * performance hit) which uses assert() to test various expectations.  This
 * makes API_DLM_CHECK (see module.export) useful even without _DEBUG.
 *
 * For WIN32, to make use of sbrk() for MORECORE, NEED_WIN32_SBRK needs to be
 * defined. If NEED_WIN32_SBRK is not defined, make sure MORECORE is set up
 * correctly.
 *
 * Other things you may want to over-ride (see lea_malloc.cpp for how they're
 * used): assert (default is OP_ASSERT, probably sensible), and DEBUG (must be
 * 0 or 1, defaults to 1 if _DEBUG is defined, else 0).
 */

#endif // LEA_MALLOC_DUMMY_CONFIG_H
