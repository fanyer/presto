/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

class EcmaScript_Manager;
class ES_Runtime;

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/ecmascript_manager_impl.h"
#include "modules/memory/src/memory_executable.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"

#ifdef OPPSEUDOTHREAD_THREADED
#include "modules/ecmascript/oppseudothread/oppseudothread_threaded.h"
extern OpSystemThread *g_cached_thread;
#endif // OPPSEUDOTHREAD_THREADED

#ifndef HAS_COMPLEX_GLOBALS
#  ifdef ECMASCRIPT_DISASSEMBLER
extern void init_simple_operations();
#  endif // ECMASCRIPT_DISASSEMBLER
#  ifdef ES_INLINE_ASSEMBLER
extern void init_instruction_names();
#  endif // ES_INLINE_ASSEMBLER
#endif // HAS_COMPLEX_GLOBALS

EcmaScriptModule::EcmaScriptModule()
	: manager(NULL)
	, runtime(NULL)
	, instruction_handlers(NULL)
	, sse2_block(NULL)
	, sse41_block(NULL)
	, bytecode_to_native_block(NULL)
	, reconstruct_native_stack1_block(NULL)
	, reconstruct_native_stack2_block(NULL)
	, throw_from_machine_code_block(NULL)
	, chunk_allocator(NULL)
#ifdef CPUUSAGETRACKING
	, gc_cpu_tracker(NULL)
#endif // CPUUSAGETRACKING
{
}

void
EcmaScriptModule::InitL(const OperaInitInfo& info)
{
	manager = EcmaScript_Manager::MakeL();
#if defined ES_NATIVE_SUPPORT || !defined CONSTANT_DATA_IS_EXECUTABLE
	executable_memory = OP_NEW_L(OpExecMemoryManager,(MEMCLS_ECMASCRIPT_EXEC));
#endif // ES_NATIVE_SUPPORT || !CONSTANT_DATA_IS_EXECUTABLE
#ifdef ES_USE_VIRTUAL_SEGMENTS
	chunk_allocator = OP_NEW_L(ES_AlignedChunkAllocator, ());
#else // ES_USE_VIRTUAL_SEGMENTS
	chunk_allocator = OP_NEW_L(ES_ChunkAllocator, ());
#endif // ES_USE_VIRTUAL_SEGMENTS
	// Previously ecma_runtime was initialized lazily, but this contributed to
	// bugs like CORE-34173. The initialization overhead should be negligible
	// anyway.
	LEAVE_IF_ERROR(EcmaScript_Manager_Impl::Initialise());
#ifdef CPUUSAGETRACKING
	// Tracking GCs that can't be connected to a specific tab.
	gc_cpu_tracker = OP_NEW_L(CPUUsageTracker, ());
	LEAVE_IF_ERROR(gc_cpu_tracker->SetDisplayName(UNI_L("ECMAScript GC")));
#endif // CPUUSAGETRACKING
}

void
EcmaScriptModule::Destroy()
{
	OP_DELETE(manager);
	manager = NULL;

	OP_DELETEA(instruction_handlers);

	// ecma_runtime is destroyed earlier, by the engine

#ifdef CPUUSAGETRACKING
	OP_DELETE(gc_cpu_tracker);
	gc_cpu_tracker = NULL;
#endif // CPUUSAGETRACKING

#ifdef ES_NATIVE_SUPPORT
	if (sse2_block)
		OpExecMemoryManager::Free(sse2_block);
	if (sse41_block)
		OpExecMemoryManager::Free(sse41_block);
	if (bytecode_to_native_block)
		OpExecMemoryManager::Free(bytecode_to_native_block);
	if (reconstruct_native_stack1_block)
		OpExecMemoryManager::Free(reconstruct_native_stack1_block);
	if (reconstruct_native_stack2_block)
		OpExecMemoryManager::Free(reconstruct_native_stack2_block);
	if (throw_from_machine_code_block)
		OpExecMemoryManager::Free(throw_from_machine_code_block);
#endif // ES_NATIVE_SUPPORT
#if defined ES_NATIVE_SUPPORT || !defined CONSTANT_DATA_IS_EXECUTABLE
	OP_DELETE(executable_memory);
#endif
	OP_DELETE(chunk_allocator);
#ifdef OPPSEUDOTHREAD_THREADED
	OP_DELETE(g_cached_thread);
#endif // OPPSEUDOTHREAD_THREADED
}

BOOL
EcmaScriptModule::FreeCachedData(BOOL toplevel_context)
{
#ifdef _STANDALONE
	return FALSE;
#else
	if (toplevel_context)
	{
		manager->GarbageCollect(TRUE);      // Not quite good enough
		return TRUE;
	}
	else
		return FALSE;
#endif // _STANDALONE
}
