/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the ELOPOOL/ELSPOOL allocators.
 * 
 * Use the block allocator to allocate blocks from which we hand out
 * memory for various small to medium sized strings and objects.
 * These strings and objects allocated from the same pool are expected
 * to live equally long.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMORY_POOLING_ALLOCATORS

#include "modules/memory/src/memory_pooling.h"

#define FREE_OFFSET_UNLOCKED_VALUE  (MEMORY_BLOCKSIZE + 1)

void OpMemPool::InitELO(void)
{
	OP_ASSERT(type == MEMPOOL_FREE || type == MEMPOOL_ELO && !data.elo.count);

	type = MEMPOOL_ELO;
	data.elo.free_offset = 0;
	data.elo.count = 0;

	segment->PoolAllocation(this);
}

void* OpMemPool::AllocELO(size_t size, unsigned int align)
{
	OP_ASSERT(type == MEMPOOL_ELO);
	OP_ASSERT(align == 1 || align == 3 || align == 7);
	OP_ASSERT(size > 0);

	unsigned int alloc_offset = (data.elo.free_offset + align) & ~align;

	unsigned new_offset = alloc_offset + size;

	if ( new_offset > MEMORY_BLOCKSIZE )
		return 0;

	data.elo.free_offset = new_offset;
	data.elo.count++;

	return reinterpret_cast<void*>(pooladdr + alloc_offset);
}

void OpMemPool::FreeELO(void)
{
	OP_ASSERT(type == MEMPOOL_ELO);
	OP_ASSERT(data.elo.count > 0);

	data.elo.count--;

	if ( data.elo.count == 0 )
	{
		data.elo.free_offset = 0;
		
		if ( !IsLockedELO() )
			Free();
	}
}

void OpMemPool::UnlockELO(void)
{
	OP_ASSERT(type == MEMPOOL_ELO);
	data.elo.free_offset = FREE_OFFSET_UNLOCKED_VALUE;
	
	if ( data.elo.count == 0 )
		Free();
}

BOOL OpMemPool::IsLockedELO(void)
{
	OP_ASSERT(type == MEMPOOL_ELO);
	return data.elo.free_offset < FREE_OFFSET_UNLOCKED_VALUE;
}

OpMemLife::OpMemLife(void) :
	pools(0)
{
}

OpMemLife::~OpMemLife(void)
{
	OpMemPool* p = pools;
	while ( p != 0 )
	{
		OpMemPool* next = p->next;
		p->UnlockELO();
	}
}

void* OpMemLife::IntAllocELO(size_t size, unsigned int align)
{
	void* a;

	// Check for invariant: Pool B can't exist without A
	OP_ASSERT(pool_a != 0 || pool_b == 0);

	if ( pool_a != 0 )
	{
		// Try to allocate from pool A first
		a = pool_a->AllocELO(size, align);
		if ( a != 0 )
			return a;

		if ( pool_b != 0 )
		{
			// Then try to allocate from pool B
			a = pool_b->AllocELO(size, align);
			if ( a != 0 )
				return a;

			//
			// If the previous two attempts at pool allocation fails, and
			// the allocation is "large", fall back to regular heap
			// allocation for this request.
			//
			if ( size > MEMORY_ELO_HIGH_THRESHOLD )
				return op_lowlevel_malloc(size);

			//
			// Allocation is "small", but could not be satisfied from any
			// of the pools.  This means it is time to move on; keep the
			// pool with the most free space as pool A, and restart the
			// operation.
			//
			if ( pool_a->data.elo.free_offset > pool_b->data.elo.free_offset )
			{
				// Move pool B into "pole position"
				OpMemPool* tmp = pool_a;
				pool_a = pool_b;
				pool_b = tmp;
			}

			// Start with a fresh pool B
			pool_b->UnlockELO();
			pool_b = 0;
		}

		//
		// Pool A exists, but can't satisfy the request, and there is
		// no pool B.  Create pool B and try to allocate from it.
		//
		pool_b = g_mem_pools.NewPoolELO();
		if ( pool_b != 0 )
		{
			a = pool_b->AllocELO(size, align);
			if ( a != 0 )
				return a;
		}
	}
	else
	{
		//
		// There is no pool A, so create it and allocate from it
		//
		pool_a = g_mem_pools.NewPoolELO();
		if ( pool_a != 0 )
		{
			a = pool_a->AllocELO(size, align);
			if ( a != 0 )
				return a;
		}
	}

	//
	// No, didn't work.  Try to fallback to the general heap.
	//
	return op_lowlevel_malloc(size);
}

void* OpMemLife::IntAllocELO_L(size_t size, unsigned int align)
{
	void* a = IntAllocELO(size, align);
	if ( a == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return a;
}

#endif // MEMORY_POOLING_ALLOCATORS
