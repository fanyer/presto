/* -*- Mode: c++; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef RE_CONFIG_H
#define RE_CONFIG_H

//#ifndef _STANDALONE
#  ifdef _DEBUG
   /* Define to include a simple disassembler. */
//#    define RE_FEATURE__DISASSEMBLER
#  endif // _DEBUG

#  define RE_FEATURE__NAMED_CAPTURES
//#endif // STANDALONE

#define RE_FEATURE__CHARACTER_CLASS_INTERSECTION
//#define RE_FEATURE__COMMENTS
#define RE_FEATURE__BRACED_HEXADECIMAL_ESCAPES
#define RE_FEATURE__EXTENDED_SYNTAX
//#define RE_FEATURE__NEL_IS_LINE_TERMINATOR

#include "modules/ecmascript/carakan/src/es_pch.h"

#if defined ECMASCRIPT_NATIVE_SUPPORT && (defined ARCHITECTURE_IA32 || defined ARCHITECTURE_ARM || defined ARCHITECTURE_MIPS)
#  define RE_FEATURE__MACHINE_CODED
#  define RE_CALLING_CONVENTION ES_CALLING_CONVENTION
#else // ECMASCRIPT_NATIVE_SUPPORT
#  define RE_CALLING_CONVENTION
#endif // ECMASCRIPT_NATIVE_SUPPORT && (ARCHITECTURE_IA32 || ARCHITECTURE_ARM)

#define RE_CONFIG__LOOP_BACKTRACKING_LIMIT 1024

/** How much stack the code generator is allowed to use while
    compiling. A generous limit for everything but ill-formed
    or ill-intended regular expressions. */

#define RE_CONFIG_PARM_MAX_STACK (900 * 1024)
#ifdef RE_FEATURE__CHARACTER_CLASS_INTERSECTION
# define RE_FEATURE__CHARACTER_CLASS_INTERSECTION_MAX_DEPTH 128
#endif // RE_FEATURE__CHARACTER_CLASS_INTERSECTION


#endif /* RE_CONFIG_H */

