/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: only initialization of the POSIX_SYS_USE_*.
 */
#ifndef POSIX_SYS_CHOICE_H
#define POSIX_SYS_CHOICE_H __FILE__
#include "platforms/posix/sys/include_early.h"	/* Various #include <...> always needed */

/** @file posix_choice.h The chose-what-to-enable phase.
 *
 * This sets the SYSTEM_* control variables to YES or NO to match what common
 * POSIX-supporting systems use; a platform can use it, then \#undef and
 * re-\#define any for which it needs a different value.  Alternatively, it can
 * use such of the choice_*.h as it agrees with to build its own replacement for
 * this file.  There are also some POSIX_SYS_USE_* defines you can use to make
 * the more obvious major adjustments to what's supported.
 *
 * By default, we prefer system-provided functions as far as possible; they are
 * generally more efficient.  If you want to test the stdlib module
 * (e.g. because you're core, so should), pre-\#define POSIX_SYS_USE_NATIVE to
 * NO (it defaults to YES).  When our stdlib implementation changes what it's
 * able to support, relevant system defines in choice_*.h should be updated to
 * either POSIX_SYS_USE_NATIVE (when stdlib adds support for the relevant
 * function) or YES (if removing).
 *
 * Generally, on known Unices, the system libc is built using (and may be coded
 * assuming) a UTF32 wchar_t; it is consequently incompatible with our uni_char,
 * even when compiling with WCHAR_IS_UNICHAR (which lets us use wchar_t as
 * uni_char at compile-time, but won't fix libc at link-time).  However, in the
 * event of a platform being built using a UTF16 wchar_t, as MS-Windows does,
 * define POSIX_SYS_USE_UNI to YES to enable a selection of the SYSTEM_UNI_*;
 * the current selection is based on what's specified in POSIX but is open to
 * negotiation (i.e. the first team to send us a patch for *_nonix.h will
 * probably get its way).  See CORE-36220 for further discussion of this.
 */

#ifndef POSIX_SYS_USE_NATIVE
#define POSIX_SYS_USE_NATIVE YES
#endif

#ifdef POSIX_SYS_USE_UNI
# if POSIX_SYS_USE_UNI == YES && !defined(WCHAR_IS_UNICHAR)
#error "Read the comment above: no way is this going to work !"
# endif // ... even *with* WCHAR_IS_UNICHAR, we know of nowhere it can work.
#else
#define POSIX_SYS_USE_UNI NO
#endif

/* NB: order matters; later headers may conditionally #undef and redefine things
 * from ealier ones. */
#include "platforms/posix/sys/choice_libc.h"	/* Say YES to ANSI C's standard library */
#include "platforms/posix/sys/choice_posix.h"	/* other things POSIX and ANSI C promise */
#include "platforms/posix/sys/choice_gcc.h"		/* Say YES to what gcc supports */
#include "platforms/posix/sys/choice_nonix.h"	/* Say NO to (most of) the rest */
#include "platforms/posix/sys/choice_chipset.h"	/* Architecture and FPU layout */

#endif /* POSIX_SYS_CHOICE_H */
