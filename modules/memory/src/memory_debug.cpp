/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the OpMemDebug class.
 * 
 * Call support functions to achieve the desired allocations and
 * deallocations, and keep track of the allocations.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_debug.h"
#include "modules/memory/src/memory_memguard.h"

void* OpMemDebug::OpMalloc(size_t size, OpAllocInfo* info)
{
	return g_mem_guard->OpMalloc(size, info);
}

void* OpMemDebug::OpMalloc_L(size_t size, OpAllocInfo* info)
{
	void* ptr = g_mem_guard->OpMalloc(size, info);
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

void* OpMemDebug::OpCalloc(size_t num, size_t size, OpAllocInfo* info)
{
	size_t bytes = num * size;
	void* ptr = g_mem_guard->OpMalloc(bytes, info);
	if ( ptr != 0 )
		op_memset(ptr, 0, bytes);
	return ptr;
}

void* OpMemDebug::OpRealloc(void* ptr, size_t size, OpAllocInfo* info)
{
	return g_mem_guard->OpRealloc(ptr, size, info);
}

void OpMemDebug::OpFree(void* ptr, void** pptr, OpAllocInfo* info)
{
	g_mem_guard->OpFree(ptr, info);
	//
	// FIXME: This can't be done yet, as a lot of op_free operations
	// do things like casting the pointer given to op_free.
	// For these places, use op_free_expr(x) instead, which will not
	// zap the memory pointer.
	//
	// *pptr = (void*)MEMORY_INVALID_POINTER_VALUE;
}

void OpMemDebug::OpFreeExpr(void* ptr, OpAllocInfo* info)
{
	g_mem_guard->OpFree(ptr, info);
}

char* OpMemDebug::OpStrdup(const char* str, OpAllocInfo* info)
{
	size_t size = op_strlen(str) + 1;
	char* copy = (char*)g_mem_guard->OpMalloc(size, info);
	if ( copy != 0 )
		op_strcpy(copy, str);
	return copy;
}

uni_char* OpMemDebug::UniStrdup(const uni_char* str, OpAllocInfo* info)
{
	size_t size = uni_strlen(str) + 1;
	uni_char* copy = (uni_char*)g_mem_guard->OpMalloc(sizeof(uni_char)*size,
													 info);
	if ( copy != 0 )
		uni_strcpy(copy, str);
	return copy;
}

int OpMemDebug::Action(OpMemDebugActionInteger action, int value)
{
	return g_mem_guard->Action(action, value);
}

int OpMemDebug::Action(OpMemDebugAction action, void* misc)
{
	return g_mem_guard->Action(action, misc);
}

void* OpMemDebug::NewSMO(size_t size, OpAllocInfo* info)
{
#ifdef MEMORY_SMOPOOL_ALLOCATOR
	return g_mem_guard->ShadowAlloc(g_mem_poolmgr->AllocSMO(size), size, info);
#else
	return g_mem_guard->OpMalloc(size, info);
#endif
}

void* OpMemDebug::NewELO(size_t size, OpMemLife& life, OpAllocInfo* info)
{
#ifdef MEMORY_ELOPOOL_ALLOCATOR
	return g_mem_guard->ShadowAlloc(life.IntAllocELO(size, MEMORY_ALIGNMENT-1),
									size, info);
#else
	return g_mem_guard->OpMalloc(size, info);
#endif
}

void* OpMemDebug::NewELO_L(size_t size, OpMemLife& life, OpAllocInfo* info)
{
#ifdef MEMORY_ELOPOOL_ALLOCATOR
	void* ptr = life.IntAllocELO_L(size, MEMORY_ALIGNMENT-1);
	return g_mem_guard->ShadowAlloc(ptr, size, info);
#else
	return g_mem_guard->OpMalloc(size, info);
#endif
}

void* OpMemDebug::NewELSA(size_t size, OpMemLife& life, OpAllocInfo* info)
{
#ifdef MEMORY_ELOPOOL_ALLOCATOR
	return g_mem_guard->ShadowAlloc(life.IntAllocELO(size, 1), size, info);
#else
	return g_mem_guard->OpMalloc(size, info);
#endif
}

void* OpMemDebug::NewELSA_L(size_t size, OpMemLife& life, OpAllocInfo* info)
{
#ifdef MEMORY_ELOPOOL_ALLOCATOR
	return g_mem_guard->ShadowAlloc(life.IntAllocELO_L(size, 1), size, info);
#else
	return g_mem_guard->OpMalloc(size, info);
#endif
}

BOOL OpMemDebug::GetRandomOOM(void)
{
	return g_mem_guard->GetRandomOOM();
}

void OpMemDebug::SetRandomOOM(BOOL value)
{
	g_mem_guard->SetRandomOOM(value);
}

#endif // ENABLE_MEMORY_DEBUGGING
