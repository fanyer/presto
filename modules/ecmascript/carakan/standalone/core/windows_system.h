/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

/*
 * Options and tweaks for Windows platform targets.
 *
 * Kept here in preference to VS project files (and elsewhere.)
 */
#ifndef ES_WINDOWS_SYSTEM_H
#define ES_WINDOWS_SYSTEM_H

#define ARCHITECTURE_IA32

#ifdef SIXTY_FOUR_BIT
# define ARCHITECTURE_AMD64
# define ARCHITECTURE_AMD64_WINDOWS
# define ARCHITECTURE_SSE
# define ES_NATIVE_SUPPORT
# define ECMASCRIPT_NATIVE_SUPPORT
#else
# define ARCHITECTURE_SSE
# define ES_NATIVE_SUPPORT
# define ECMASCRIPT_NATIVE_SUPPORT
#endif // SIXTY_FOUR_BIT

#ifdef _DEBUG
# define DEBUG_ENABLE_OPASSERT
# define ES_BYTECODE_LOGGER
# define ES_DISASSEMBLER_SUPPORT
//#define ES_HARDCORE_GC_MODE
#endif // _DEBUG

#define ECMASCRIPT_STANDALONE_COMPILER
#define ES_SEGMENTED_STRINGS
#define ES_COMBINED_ADD_SUPPORT
#define ES_COMPILER_OPTIONS
#define ES_USE_INT32_FOR_INTEGERS
#define ES_MULTIPLE_HEAPS
#define ES_POLYMORPHIC_PROPERTY_CACHE
#define OPMEMORY_EXECUTABLE_SEGMENT
#define EXECUTABLE_MEMORY_MANAGER
#define MEMORY_EXEC_SEGMENT_SIZE 32768
#define MEMORY_SMALL_EXEC_SEGMENTS
#define ES_CARAKAN_PARM_MAX_PARSER_STACK (900*1024)

#endif //ES_WINDOWS_SYSTEM_H
