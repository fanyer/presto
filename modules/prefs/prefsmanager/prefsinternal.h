/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSINTERNAL_H
#define PREFSINTERNAL_H

/**
 * @file prefsinternal.h
 * Internal macros for the prefs module.
 */

#ifdef PREFS_STRING_API_CASE_SENSITIVE
/** Comparison function for the string API. */
# define STRINGAPI_STRCMP op_strcmp
#else
# define STRINGAPI_STRCMP op_stricmp
#endif

#ifdef PREFSMAP_USE_CASE_SENSITIVE
# define OVERRIDE_STRCMP(a,b) op_strcmp(a,b)
#else
/** Comparison function for override matching. */
# define OVERRIDE_STRCMP(a,b) op_stricmp(a,b)
#endif

#undef RETURN_OPSTRINGC
#ifdef CAN_RETURN_IMPLICIT_OBJECTS
/** Workaround for the BREW compiler */
# define RETURN_OPSTRINGC(s) { return s; }
#else
# define RETURN_OPSTRINGC(s) { OpStringC tmp(s); return tmp; }
#endif

#ifdef HAS_COMPLEX_GLOBALS
/** Declare as static where possible */
# define PREFS_STATIC static const
/** Declare as static where possible */
# define PREFS_STATIC_CONST static const
/** Declare as const where possible */
# define PREFS_CONST const
#else
# define PREFS_STATIC
# define PREFS_STATIC_CONST const
# define PREFS_CONST
#endif

#endif // PREFSINTERNAL_H
