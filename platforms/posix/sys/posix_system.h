/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: none.
 *
 * Note that this file and each file it pulls in must be compatible with
 * #include by a plain C compilation unit; so C++-style comments are not
 * allowed.
 */
#ifndef POSIX_SYSTEM_H
#define POSIX_SYSTEM_H __FILE__
/** @file posix_system.h A header for most of the system-system.
 *
 * This is intended to be usable by projects with reasonably full POSIX support,
 * as a way to provide a large part of its PRODUCT_SYSTEM_FILE.  Obviously,
 * departures from POSIX may require tweaking: if only small changes are needed,
 * \c \#include this file and \c \#undef the things you need to change before
 * you \c \#define them to their revised values.  See posix_choice.h for
 * configuration options that assist with the more common variations.
 *
 * For larger changes, you may want to use a hacked a copy of this file as
 * start-point for your own, instead. (It is based on what was in unix/system.h
 * in late 2009, minus deprecated parts, things not addressed by POSIX and other
 * cruft.)  To make this easier, the content of this file is spread across
 * several files in the same directory, so that you can \c \#include from here
 * the parts that you can re-use, skipping the parts that would require more
 * than trivial amendment. In particular, the files into which this is split
 * come in two phases: a hacked copy of this file should normally be able to
 * make do with correcting some \c SYSTEM_* defines in between posix_choice.h
 * and posix_declare.h, unless it has really drastic changes to make.
 *
 * Some system defines are deliberately left unhandled - you must make other
 * arrangements (e.g. make can work it out and tell the compiler) to handle
 * these: SYSTEM_BIG_ENDIAN, SYSTEM_IEEE_{8087,MC68K}.
 */

#include "platforms/posix/sys/posix_choice.h" /* Say YES or NO to each SYSTEM_* define */
/* Modified copies can adjust SYSTEM_* defines to taste.  When enabling one,
 * they can reasonably implement associated op_* defines in the same place.
 */
#include "platforms/posix/sys/posix_declare.h" /* Implement the op_* defines */
#endif /* POSIX_SYSTEM_H */
