/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Eirik Byrkjeflot Anonsen
*/


/* This file is a common include file for including the correct
 * jni.h.  The point of this file is to keep features.h relatively
 * clean, and put the platform ifdefs in here instead.
 *
 * IMPORTANT NOTE: Do not modify the files included from this file
 * unless you have made sure that we are allowed to modify them.
 * In particular, the license for Sun's header files explicitly
 * disallows modifications.
 */



/* Fallback "default" version.  Sun JDK 1.4.2_01 has identical
 * files for linux/x86 and solaris/sparc, so we'll use that as
 * the current "default" version.  --eba 2003-10-14
 */

#include "sun/default/jni.h"
