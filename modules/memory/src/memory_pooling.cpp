/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement aspects of the OpMemPools class.
 *
 * Call support functions to achieve the desired allocations and
 * deallocations.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#include "modules/memory/src/memory_pooling.h"
#include "modules/memory/src/memory_segment.h"
#include "modules/probetools/probepoints.h"

#if defined(USE_POOLING_MALLOC) && !defined(MEMORY_PLATFORM_POOLING)

extern "C" void* OpPooledAlloc(size_t size)
{
	return OpMemPoolManager::AllocSMO(size);
}

extern "C" void* OpPooledAlloc_L(size_t size)
{
	void* ret = OpMemPoolManager::AllocSMO(size);
	if ( ret == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ret;
}

extern "C" void* OpPooledAllocT(size_t size)
{
	return OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_TEMPORARY);
}

extern "C" void* OpPooledAllocT_L(size_t size)
{
	void* ret = OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_TEMPORARY);
	if ( ret == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ret;
}

extern "C" void* OpPooledAllocP(size_t size)
{
	return OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_PERSISTENT);
}

extern "C" void* OpPooledAllocP_L(size_t size)
{
	void* ret = OpMemPoolManager::AllocSMO(size, MEMPOOL_ALLOC_PERSISTENT);
	if ( ret == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ret;
}

extern "C" void OpPooledFree(void* ptr)
{
	OpMemPoolManager::Free(ptr);
}

#endif // USE_POOLING_MALLOC && !MEMORY_PLATFORM_POOLING

#if defined(USE_POOLING_MALLOC)

OpMemPoolManager::OpMemPoolManager(void)
{
	// Initialize management.
	OP_ASSERT(g_mem_poolmgr == 0);
	g_mem_poolmgr = this;

	// Initialize circular lists by assigning headers.
	static OpMemLink
		list_headers[MEMORY_POOL_ALLOC_TYPES * (1 + 2 * MEMORY_POOL_COUNT)];
	unsigned int index = 0;

	for ( int j = 0; j < MEMORY_POOL_ALLOC_TYPES; j++ )
	{
		free_pools[j] = &list_headers[index++];

		for ( int i = 0; i < MEMORY_POOL_COUNT; i++ )
		{
			available_pools[j][i] = &list_headers[index++];
			full_pools[j][i] = &list_headers[index++];
		}
	}
}

OpMemPoolManager::~OpMemPoolManager(void)
{
	OP_ASSERT(g_mem_poolmgr != 0);
	g_mem_poolmgr = 0;
}

void* OpMemPoolManager::AllocSMO(size_t size, OpMemPoolAlloc type)
{
	OP_ASSERT(g_mem_poolmgr && "Please call OpMemoryStateInit before performing pooled allocations.");
	OpMemPoolManager* mgr = g_mem_poolmgr;

	void* ptr = 0;
	size_t new_size = (size + (MEMORY_ALIGNMENT - 1)) & ~((UINTPTR)MEMORY_ALIGNMENT - 1);
	OP_ASSERT(new_size >= size); // catch overflow
	size = new_size;
	OP_ASSERT(size < MEMORY_POOL_MAX_SIZE);

	// Try to allocate from an available pool.
	OpMemPool* p = mgr->FirstAvailablePool(size, type);
	while ( p && ptr == 0 )
	{
		ptr = p->AllocSMO();
		if ( ptr == 0 )
		{
			OpMemPool* q = mgr->NextPool();
			p->MoveTo(MEMPOOL_LIST_FULL);
			p = q;
		}
	}

	if ( ptr == 0 )
	{
		// No suitable available pools found, obtain a free pool of
		// adequate size and appropriate type.
		p = mgr->FirstFreePool(size, type);

		if ( p == 0 )
		{
			// None exist, allocate more.
			OpMemory::OpMemSegmentType seg_type =
				(OpMemory::OpMemSegmentType)type;
			const OpMemory::OpMemSegment* seg =
				OpMemory::CreateSegment(seg_type);
			if ( seg == 0 )
				return 0; // OOM

			new OpCoreSegment(seg);

			p = mgr->FirstFreePool(size, type);
			OP_ASSERT(p != 0);
		}

		// Initialize pool.
		p->InitSMO(size);
		p->MoveTo(MEMPOOL_LIST_AVAILABLE);

		// Allocate.
		ptr = p->AllocSMO();
		OP_ASSERT(ptr != 0);
	}

	return ptr;
}

void OpMemPoolManager::Free(void* ptr)
{
	OpMemPool* pool = LocatePool(ptr);
	OP_ASSERT(pool != 0);

	pool->FreeSMO(ptr);
}

unsigned int OpMemPoolManager::GetSegmentID(void* ptr)
{
	OpMemPool* pool = LocatePool(ptr);
	OP_ASSERT(pool != 0);

	return pool->segment->GetID();
}

OpMemPool* OpMemPoolManager::LocatePool(void* ptr)
{
	UINTPTR base = ((UINTPTR)ptr) & ~((UINTPTR)MEMORY_BLOCKSIZE-1);
	int dir = ((UINTPTR)ptr) & MEMORY_ALIGNMENT;

	return *(OpMemPool**)(dir ? base : base + MEMORY_BLOCKSIZE - sizeof(OpMemPool*));
}

OpMemPool* OpMemPoolManager::FirstAvailablePool(size_t size, OpMemPoolAlloc type)
{
	current_head = available_pools[type][size / MEMORY_ALIGNMENT];
	if ( current_head->next == current_head )
		return 0;

	current_pool = static_cast<OpMemPool*>(current_head->next);
	return current_pool;
}

OpMemPool* OpMemPoolManager::FirstFreePool(size_t size, OpMemPoolAlloc type)
{
	current_head = free_pools[type];
	if ( current_head->next == current_head )
		return 0;

	current_pool = static_cast<OpMemPool*>(current_head->next);

	if ( size > 0 )
	{
		OpMemPool* p = current_pool;
		while ( p && p->size < sizeof(OpMemPool*) + size )
			p = NextPool();

		current_pool = p;
	}

	return current_pool;
}

OpMemPool* OpMemPoolManager::NextPool()
{
	if ( current_pool->next == current_head )
		return 0;

	current_pool = current_pool->GetNext();
	return current_pool;
}

OpMemLink::OpMemLink()
{
	next = prev = this;
}

void OpMemLink::Out()
{
	prev->next = next;
	next->prev = prev;
}

OpMemPool::OpMemPool(char* address, size_t size, OpMemPoolAlignment align, OpCoreSegment* seg) :
	OpMemLink(),
	segment(seg),
	type(MEMPOOL_UNALLOCATED),
	align(align),
	address(address),
	size(size)
{
	current_list = MEMPOOL_LIST_NONE;
}

OpMemPool::~OpMemPool(void)
{
}

void OpMemPool::MoveTo(OpMemPoolList list)
{
	OpMemLink* head = 0;
	int i = segment->GetType();

	switch (list)
	{
	case MEMPOOL_LIST_FREE:
		head = g_mem_poolmgr->free_pools[i];
		break;

	case MEMPOOL_LIST_AVAILABLE:
		head = g_mem_poolmgr->available_pools[i][data.smo.size / MEMORY_ALIGNMENT];
		break;

	case MEMPOOL_LIST_FULL:
		head = g_mem_poolmgr->full_pools[i][data.smo.size / MEMORY_ALIGNMENT];
		break;

	default:
		OP_ASSERT(!"Invalid argument.");
	};

	// Disentangle.
	Out();

	// Re-entangle.
	prev = head;
	next = head->next;
	next->prev = this;
	head->next = this;

	current_list = list;
}

void OpMemPool::Free()
{
	if ( type != MEMPOOL_FREE )
	{
		type = MEMPOOL_FREE;
		MoveTo(MEMPOOL_LIST_FREE);
		segment->PoolFree(this);
	}
}

#endif // USE_POOLING_MALLOC
