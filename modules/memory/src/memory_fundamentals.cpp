/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement fundamental functions for the memory module.
 * 
 * These are either called directly, or through inlined new operators etc.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_OOM_EMULATION
#  include "modules/lea_malloc/lea_monoblock.h"
#endif

#include "modules/memory/src/memory_debug.h"
#include "modules/memory/src/memory_opallocinfo.h"
#include "modules/memory/src/memory_memguard.h"
#include "modules/memory/src/memory_opallocinfo.h"
#include "modules/memory/src/memory_reporting.h"

#ifdef MEMORY_USE_LOCKING
#  include "modules/pi/system/OpMemory.h"
#  define MUTEX_LOCK() OpMemory::MemdebugLock()
#  define MUTEX_UNLOCK() OpMemory::MemdebugUnlock()
#else
#  define MUTEX_LOCK() do {} while (0)
#  define MUTEX_UNLOCK() do {} while (0)
#endif


#if MEMORY_INLINED_OP_NEW || MEMORY_INLINED_NEW_ELEAVE || MEMORY_INLINED_ARRAY_ELEAVE

void* op_meminternal_malloc_leave(size_t size)
{
	void* ptr = op_meminternal_malloc(size);
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#endif

void* op_meminternal_always_leave(void)
{
	LEAVE(OpStatus::ERR_NO_MEMORY);
	return NULL;
}

#ifdef ENABLE_MEMORY_DEBUGGING

extern "C" void* OpMemDebug_OpMalloc(size_t size, const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, "bytes[]", MEMORY_FLAG_ALLOCATED_OP_MALLOC);
	return g_mem_debug->OpMalloc(size, &info);
}

extern "C" void* OpMemDebug_OpCalloc(size_t num, size_t size,
									 const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, "bytes[]", MEMORY_FLAG_ALLOCATED_OP_CALLOC);
	return g_mem_debug->OpCalloc(num, size, &info);
}

extern "C" void* OpMemDebug_OpRealloc(void* ptr, size_t size,
									  const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, 0, 0);
	return g_mem_debug->OpRealloc(ptr, size, &info);
}

extern "C" void OpMemDebug_OpFree(void* ptr, void** pptr,
								  const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, 0, MEMORY_FLAG_RELEASED_OP_FREE);
	g_mem_debug->OpFree(ptr, pptr, &info);
}

extern "C" void OpMemDebug_OpFreeExpr(void* ptr, const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, 0, MEMORY_FLAG_RELEASED_OP_FREE);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

void* OpMemDebug_NewSMOD(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOD(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOD_L(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* OpMemDebug_NewSMOD_L(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEW_SMOD);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* OpMemDebug_NewSMOT(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEWSMOT);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOT(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWSMOT);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOT_L(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEWSMOT);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* OpMemDebug_NewSMOT_L(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWSMOT);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* OpMemDebug_NewSMOP(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEWSMOP);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOP(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWSMOP);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewSMOP_L(size_t size)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_OP_NEWSMOP);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* OpMemDebug_NewSMOP_L(size_t size, const char* file,
						 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWSMOP);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void OpMemDebug_PooledDelete(void* ptr)
{
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_RELEASED_POOLED_DELETE);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

extern "C" char* OpMemDebug_OpStrdup(const char* str,
									 const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, "char", MEMORY_FLAG_ALLOCATED_OP_STRDUP);
	return g_mem_debug->OpStrdup(str, &info);
}

extern "C" uni_char* OpMemDebug_UniStrdup(const uni_char* str,
										  const char* file, int line)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, "uni_char",
				MEMORY_FLAG_ALLOCATED_UNI_STRDUP);
	return g_mem_debug->UniStrdup(str, &info);
}

extern "C" void OpMemDebug_ReportLeaks(void)
{
	if (g_mem_guard)
		g_mem_guard->ReportLeaks();
}

#if MEMORY_INLINED_OP_NEW

void* OpMemDebug_New(size_t size, const char* file,
					 int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEW);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_NewA(size_t size, const char* file, int line,
					  const char* obj, unsigned int count1,
					  unsigned int count2, size_t objsize)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWA);
	info.SetArrayElementCount(count1);
	info.SetArrayElementSize(objsize);
	OP_ASSERT(count1 == count2);
	return g_mem_debug->OpMalloc(size, &info);
}

#endif // MEMORY_INLINED_OP_NEW

#if MEMORY_INLINED_GLOBAL_NEW

void* OpMemDebug_GlobalNew(size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEW);
	return g_mem_debug->OpMalloc(size, &info);
}

void* OpMemDebug_GlobalNewA(size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA);
	return g_mem_debug->OpMalloc(size, &info);
}

#endif // MEMORY_INLINED_GLOBAL_NEW

#if MEMORY_INLINED_GLOBAL_DELETE

void OpMemDebug_GlobalDelete(void* ptr)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_RELEASED_DELETE);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

void OpMemDebug_GlobalDeleteA(void* ptr)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_RELEASED_DELETEA);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

#endif // MEMORY_INLINED_GLOBAL_DELETE

#if MEMORY_INLINED_NEW_ELEAVE

void* OpMemDebug_GlobalNew_L(size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEW);
	return g_mem_debug->OpMalloc_L(size, &info);
}

#endif // MEMORY_INLINED_NEW_ELEAVE

#if MEMORY_INLINED_ARRAY_ELEAVE

void* OpMemDebug_GlobalNewA_L(size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA);
	return g_mem_debug->OpMalloc_L(size, &info);
}

#endif // MEMORY_INLINED_ARRAY_ELEAVE

void* OpMemDebug_NewELO(size_t size, OpMemLife &life,
						const char* file, int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWELO);
	return g_mem_debug->NewELO(size, life, &info);
}

void* OpMemDebug_NewELO_L(size_t size, OpMemLife &life,
						  const char* file, int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWELO);
	return g_mem_debug->NewELO_L(size, life, &info);
}

void* OpMemDebug_NewELSA(size_t size, OpMemLife &life, const char* file,
						 int line, const char* obj, unsigned int count1,
						 unsigned int count2, size_t objsize)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWELSA);
	info.SetArrayElementCount(count1);
	info.SetArrayElementSize(objsize);
	OP_ASSERT(count1 == count2);
	return g_mem_debug->NewELSA(size, life, &info);
}

void OpMemDebug_TrackDelete(const void* ptr, const char* file, int line)
{
#if MEMORY_KEEP_FREESITE
	g_mem_guard->TrackDelete(ptr, file, line);
#endif
}

void OpMemDebug_TrackDeleteA(const void* ptr, const char* file, int line)
{
#if MEMORY_KEEP_FREESITE
	g_mem_guard->TrackDeleteA(ptr, file, line);
#endif
}


#if MEMORY_IMPLEMENT_OP_NEW

void* operator new(size_t size, TOpAllocNewDefault,
				   const char* file, int line, const char* obj)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEW);
	return g_mem_debug->OpMalloc(size, &info);
}

void* operator new(size_t size, TOpAllocNewDefault_L,
				   const char* file, int line, const char* obj)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEW);
	return g_mem_debug->OpMalloc_L(size, &info);
}

void* operator new[](size_t size, TOpAllocNewDefaultA,
					 const char* file, int line, const char* obj,
					 unsigned int count1, unsigned int count2,
					 size_t objsize)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWA);
	info.SetArrayElementCount(count1);
	info.SetArrayElementSize(objsize);
	OP_ASSERT(count1 == count2);
	return g_mem_debug->OpMalloc(size, &info);
}

void* operator new[](size_t size, TOpAllocNewDefaultA_L,
					 const char* file, int line, const char* obj,
					 unsigned int count1, unsigned int count2,
					 size_t objsize)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWA);
	info.SetArrayElementCount(count1);
	info.SetArrayElementSize(objsize);
	OP_ASSERT(count1 == count2);
	return g_mem_debug->OpMalloc_L(size, &info);
}

#if 0

#ifdef MEMORY_SMOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewSMO,
				   const char* file, int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWSMO);
	return g_mem_debug->NewSMO(size, &info);
}

#endif // MEMORY_SMOPOOL_ALLOCATOR

#ifdef MEMORY_ELOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life,
				   const char* file, int line, const char* obj)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWELO);
	return g_mem_debug->NewELO(size, life, &info);
}

void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life,
					 const char* file, int line, const char* obj,
					 unsigned int count1, unsigned int count2,
					 unsigned int objsize)
{
	OPALLOCINFO(info, file, line, obj, MEMORY_FLAG_ALLOCATED_OP_NEWELSA);
	info.SetArrayElementCount(count1);
	info.SetArrayElementSize(objsize);
	OP_ASSERT(count1 == count2);
	return g_mem_debug->NewELSA(size, life, &info);
}

#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#endif // MEMORY_IMPLEMENT_OP_NEW


#if MEMORY_IMPLEMENT_GLOBAL_NEW

void* operator new(size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEW);
	return g_mem_debug->OpMalloc(size, &info);
}

void* operator new[](size_t size)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA);
	return g_mem_debug->OpMalloc(size, &info);
}

#endif // MEMORY_IMPLEMENT_GLOBAL_NEW


#if MEMORY_IMPLEMENT_NEW_ELEAVE

void* operator new(size_t size, TLeave /* cookie */)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEW);
	return g_mem_debug->OpMalloc_L(size, &info);
}

#endif // MEMORY_IMPLEMENT_NEW_ELEAVE


#if MEMORY_IMPLEMENT_ARRAY_ELEAVE

void* operator new[](size_t size, TLeave /* cookie */)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_ALLOCATED_GLOBAL_NEWA);
	info.SetArrayElementCount(size);
	info.SetArrayElementSize(1);
	return g_mem_debug->OpMalloc_L(size, &info);
}

#endif // MEMORY_IMPLEMENT_ARRAY_ELEAVE


#if MEMORY_IMPLEMENT_GLOBAL_DELETE

void operator delete(void* ptr)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_RELEASED_DELETE);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

void operator delete[](void* ptr)
{
	OpMemoryStateInit();
	OPALLOCINFO(info, 0, 0, 0, MEMORY_FLAG_RELEASED_DELETEA);
	g_mem_debug->OpFreeExpr(ptr, &info);
}

#endif // MEMORY_IMPLEMENT_GLOBAL_DELETE

extern "C" void OpMemDebug_OOM(int countdown)
{
	g_mem_debug->Action(MEMORY_ACTION_SET_OOM_COUNT, countdown);
}

#else // ENABLE_MEMORY_DEBUGGING


#if MEMORY_IMPLEMENT_OP_NEW

#if MEMORY_NAMESPACE_OP_NEW

void* operator new(size_t size, TOpAllocNewDefault)
{
	return op_meminternal_malloc(size);
}

void* operator new[](size_t size, TOpAllocNewDefaultA)
{
	return op_meminternal_malloc(size);
}

void* operator new(size_t size, TOpAllocNewDefault_L)
{
	void* ptr = op_meminternal_malloc(size);
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

void* operator new[](size_t size, TOpAllocNewDefaultA_L)
{
	void* ptr = op_meminternal_malloc(size);
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#endif // MEMORY_NAMESPACE_OP_NEW

#if 0

#ifdef MEMORY_SMOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewSMO)
{
	return g_mem_pools.AllocSMO(size);
}

#endif // MEMORY_SMOPOOL_ALLOCATOR

#ifdef MEMORY_ELOPOOL_ALLOCATOR

void* operator new(size_t size, TOpAllocNewELO, OpMemLife &life)
{
	return life.IntAllocELO(size, MEMORY_ALIGNMENT-1);
}

void* operator new[](size_t size, TOpAllocNewELSA, OpMemLife &life)
{
	return life.IntAllocELO(size, 1);
}

#endif // MEMORY_ELOPOOL_ALLOCATOR

#endif // 0

#endif // MEMORY_IMPLEMENT_OP_NEW


#if MEMORY_IMPLEMENT_GLOBAL_NEW

void* operator new(size_t size)
{
	return op_meminternal_malloc(size);
}

void* operator new[](size_t size)
{
	return op_meminternal_malloc(size);
}

#endif // MEMORY_IMPLEMENT_GLOBAL_NEW


#if MEMORY_IMPLEMENT_NEW_ELEAVE

void* operator new(size_t size, TLeave /* cookie */)
{
#ifdef VALGRIND
	void* ptr = operator new(size);
#else
	void* ptr = op_meminternal_malloc(size);
#endif
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#endif // MEMORY_IMPLEMENT_NEW_ELEAVE


#if MEMORY_IMPLEMENT_ARRAY_ELEAVE

void* operator new[](size_t size, TLeave /* cookie */)
{
#ifdef VALGRIND
	void* ptr = operator new[](size);
#else
	void* ptr = op_meminternal_malloc(size);
#endif
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#endif // MEMORY_IMPLEMENT_ARRAY_ELEAVE


#if MEMORY_IMPLEMENT_GLOBAL_DELETE

void operator delete(void* ptr)
{
	op_meminternal_free(ptr);
}

void operator delete[](void* ptr)
{
	op_meminternal_free(ptr);
}

#endif // MEMORY_IMPLEMENT_GLOBAL_DELETE

#endif // ENABLE_MEMORY_DEBUGGING

#if defined(ENABLE_MEMORY_OOM_EMULATION) && !defined(ENABLE_MEMORY_DEBUGGING)

//
// When memory debugging is not enabled, but OOM emulation is enabled, this
// set of functions are used to allocate on the general heap.  If the limited
// heap is not available, the platform allocator is used (when not configured
// for Desktop with -mem option, or not yet available)
//

void* op_limited_heap_malloc(size_t size)
{
	if ( g_mem_limited_heap == 0 )
		return op_lowlevel_malloc(size);

	void* mem = g_mem_limited_heap->malloc(size);
#ifdef MEMORY_ASSERT_ON_OOM
	if(!mem) { OP_ASSERT(!"oom_assert"); }
#endif // MEMORY_ASSERT_ON_OOM
	return mem;
}

void* op_limited_heap_calloc(size_t num, size_t size)
{
	if ( g_mem_limited_heap == 0 )
		return op_lowlevel_calloc(num, size);

	void* mem = g_mem_limited_heap->calloc(num, size);
#ifdef MEMORY_ASSERT_ON_OOM
	if(!mem) { OP_ASSERT(!"oom_assert"); }
#endif // MEMORY_ASSERT_ON_OOM
	return mem;
}

void* op_limited_heap_realloc(void* ptr, size_t size)
{
	if ( g_mem_limited_heap == 0 || ! g_mem_limited_heap->Contains(ptr) )
		return op_lowlevel_realloc(ptr, size);

	void* mem = g_mem_limited_heap->realloc(ptr, size);
#ifdef MEMORY_ASSERT_ON_OOM
	if(!mem) { OP_ASSERT(!"oom_assert"); }
#endif // MEMORY_ASSERT_ON_OOM
	return mem;
}

void  op_limited_heap_free(void* ptr)
{
	if ( g_mem_limited_heap == 0 || ! g_mem_limited_heap->Contains(ptr) )
		op_lowlevel_free(ptr);
	else
		g_mem_limited_heap->free(ptr);
}

#endif // ENABLE_MEMORY_OOM_EMULATION && !ENABLE_MEMORY_DEBUGGING

#ifdef SELFTEST
void* OpMemGuard_Real2Shadow(void* ptr)
{
#ifdef ENABLE_MEMORY_DEBUGGING
	return g_mem_guard->Real2Shadow(ptr);
#else
	return ptr;
#endif
}
#endif // SELFTEST

#ifdef MEMORY_DEBUGGING_ALLOCATOR

extern "C" void* op_debug_malloc(size_t size)
{
	return op_lowlevel_malloc(size);
}

extern "C" void  op_debug_free(void* ptr)
{
	op_lowlevel_free(ptr);
}

char* op_debug_strdup(const char* str)
{
	int len = op_strlen(str);
	char* copy = (char*)op_debug_malloc(len + 1);
	if ( copy != 0 )
	{
		op_strncpy(copy, str, len);
		copy[len] = 0;
	}

	return copy;
}

#endif // MEMORY_DEBUGGING_ALLOCATOR


#ifdef OPMEMORY_VIRTUAL_MEMORY

size_t op_get_mempagesize(void)
{
	return OpMemory::GetPageSize();
}

#endif // OPMEMORY_VIRTUAL_MEMORY

#ifdef MEMORY_FAIL_ALLOCATION_ONCE
BOOL OpMemDebug_SetOOMFailEnabled(BOOL b)
{
	return (g_mem_guard->SetOOMFailEnabled(b!=FALSE)?TRUE:FALSE);
}

void OpMemDebug_SetOOMFailThreshold(int oom_threshold)
{
	g_mem_guard->SetOOMFailThreshold(oom_threshold);
}

int OpMemDebug_GetOOMFailThreshold()
{
	return g_mem_guard->GetOOMFailThreshold();
}

void OpMemDebug_SetTagNumber(int tag)
{
	g_mem_guard->SetCurrentTag(tag);
}

void OpMemDebug_ClearCallCounts()
{
	g_mem_guard->ClearCallCounts();
}

void OpMemDebug_SetBreakAtCallNumber(int callnumber)
{
	g_mem_guard->SetBreakAtCallNumber(callnumber);
}

#endif // MEMORY_FAIL_ALLOCATION_ONCE
