/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_ECMASCRIPT_CARAKAN_ECMASCRIPT_MODULE_H
#define MODULES_ECMASCRIPT_CARAKAN_ECMASCRIPT_MODULE_H

class  EcmaScript_Manager;
struct ESRT_Data;
class  ES_Runtime;
//#ifdef ES_NATIVE_SUPPORT
class  OpExecMemory;
class  OpExecMemoryManager;
//#endif // ES_NATIVE_SUPPORT
class ES_ChunkAllocator;
#ifdef CPUUSAGETRACKING
class CPUUsageTracker;
#endif // CPUUSAGETRACKING

#ifdef ES_HEAP_DEBUGGER
# include "modules/util/simset.h"
#endif // ES_HEAP_DEBUGGER

#include "modules/hardcore/opera/module.h"

class EcmaScriptModule : public OperaModule
{
public:
	EcmaScriptModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();
    BOOL FreeCachedData(BOOL toplevel_context);

	EcmaScript_Manager*	manager;			// Public
	ESRT_Data*			runtime;			// Private to the module

	void**				instruction_handlers;
    void*               opt_meta_methods[6];

#ifndef HAS_COMPLEX_GLOBALS
#  ifdef ECMASCRIPT_DISASSEMBLER
	const char* simple_operations[256];
#  endif // ECMASCRIPT_DISASSEMBLER
#  if defined ECMASCRIPT_STANDALONE_COMPILER || defined ECMASCRIPT_DISASSEMBLER
	const char* instruction_names[55];
#  endif
#endif // HAS_COMPLEX_GLOBALS

//#ifdef ES_NATIVE_SUPPORT
	OpExecMemoryManager *executable_memory;
	const OpExecMemory *sse2_block;
	const OpExecMemory *sse41_block;
	const OpExecMemory *bytecode_to_native_block;
	const OpExecMemory *reconstruct_native_stack1_block;
	const OpExecMemory *reconstruct_native_stack2_block;
	const OpExecMemory *throw_from_machine_code_block;
//#endif // ES_NATIVE_SUPPORT
	ES_ChunkAllocator *chunk_allocator;

#ifdef CPUUSAGETRACKING
	CPUUsageTracker *gc_cpu_tracker;
#endif // CPUUSAGETRACKING

#ifdef ES_HEAP_DEBUGGER
	Head page_allocator_list;
#endif // ES_HEAP_DEBUGGER
};

#ifndef HAS_COMPLEX_GLOBALS
#  ifdef ECMASCRIPT_DISASSEMBLER
#    define simple_operations			g_opera->ecmascript_module.simple_operations
#  endif // ECMASCRIPT_DISASSEMBLER
#if defined ECMASCRIPT_STANDALONE_COMPILER || defined ECMASCRIPT_DISASSEMBLER
#  define instruction_names			g_opera->ecmascript_module.instruction_names
#endif // ECMASCRIPT_STANDALONE_COMPILER || ECMASCRIPT_DISASSEMBLER
#endif // HAS_COMPLEX_GLOBALS

#define g_ecmaManager				g_opera->ecmascript_module.manager
#define g_esrt						g_opera->ecmascript_module.runtime
//#ifdef ES_NATIVE_SUPPORT
#define g_executableMemory          g_opera->ecmascript_module.executable_memory
#define g_ecma_sse2_block           g_opera->ecmascript_module.sse2_block
#define g_ecma_sse41_block          g_opera->ecmascript_module.sse41_block
#define g_ecma_bytecode_to_native_block g_opera->ecmascript_module.bytecode_to_native_block
#define g_ecma_reconstruct_native_stack1_block g_opera->ecmascript_module.reconstruct_native_stack1_block
#define g_ecma_reconstruct_native_stack2_block g_opera->ecmascript_module.reconstruct_native_stack2_block
#define g_ecma_throw_from_machine_code_block g_opera->ecmascript_module.throw_from_machine_code_block
//#endif // ES_NATIVE_SUPPORT
#define g_ecma_chunk_allocator      g_opera->ecmascript_module.chunk_allocator

#define g_ecma_instruction_handlers g_opera->ecmascript_module.instruction_handlers

#ifdef ES_HEAP_DEBUGGER
#  define g_ecmaPageAllocatorList       (&g_opera->ecmascript_module.page_allocator_list)
#endif // ES_HEAP_DEBUGGER

#ifdef CPUUSAGETRACKING
# define g_ecma_gc_cputracker       g_opera->ecmascript_module.gc_cpu_tracker
#endif // CPUUSAGETRACKING

#define ECMASCRIPT_MODULE_REQUIRED

#endif // !MODULES_ECMASCRIPT_CARAKAN_ECMASCRIPT_MODULE_H
