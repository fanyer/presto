/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: SYSTEM_* defines, #ifndef symbol to be defined.
 * Sort-order: alphabetic.
 */
#ifndef POSIX_SYS_DECLARE_NONIX_H
#define POSIX_SYS_DECLARE_NONIX_H __FILE__
/** @file declare_nonix.h Opera system-system configuration strays.
 *
 * The following are not normally available in POSIX systems.  Each macro is
 * simply implemented in the way tacitly assumed by the system system.  If you
 * define it for yourself, the version here is skipped.
 */
/* TODO: trawl the system system for anything else not covered by the other
 * declare_*.h, for completeness's sake.
 */

# if SYSTEM_ITOA == YES && !defined(op_itoa)
#define op_itoa(v, s, r)			itoa(v, s, r)
# endif
# if SYSTEM_LTOA == YES && !defined(op_ltoa)
#define op_ltoa(v, s, r)			ltoa(v, s, r)
# endif
# if SYSTEM_STRREV == YES && !defined(op_strrev)
#define op_strrev(b)				strrev(b)
# endif
# if SYSTEM_UNI_ITOA == YES && !defined(uni_itoa)
#define uni_itoa(v, s, r)			itow(v, s, r)
# endif
# if SYSTEM_UNI_LTOA == YES && !defined(uni_ltoa)
#define uni_ltoa(v, s, r)			ltow(v, s, r)
# endif
# if SYSTEM_UNI_STRREV == YES && !defined(uni_strrev)
#define uni_strrev(b)				wcsrev(b)
# endif
#endif /* POSIX_SYS_DECLARE_NONIX_H */
