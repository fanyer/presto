/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement OpCoreSegment object
 * 
 * The \c OpCoreSegment object represents a memory segment \c OpMemSegment
 * allocated by platform code.  The \c OpCoreSegment object holds details
 * on how the segment is used, freelists, counters etc.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef USE_POOLING_MALLOC

#if defined (LINGOGI)
// FIXME: Should be in platform code, e.g. lingogi
#include <sys/mman.h>
#endif // defined (LINGOGI)

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_segment.h"
#include "modules/memory/src/memory_pooling.h"
#include "modules/memory/src/memory_memguard.h"

// This *must* be larger than sizeof(OpMemPool) for the code below to work.
// No pools with less than this number of bytes will ever be created.
#define MINIMUM_BLOCK_SIZE	128

OpCoreSegment::OpCoreSegment(const OpMemory::OpMemSegment* memseg) :
	memory_segment(memseg),
	mempools_total_count(0),
	mempools_in_use_count(0),
	type(memseg->type)
{
	OP_ASSERT(MINIMUM_BLOCK_SIZE > sizeof(OpMemPool));

#ifdef MEMORY_LOG_ALLOCATIONS
	if ( g_mem_guard->IsMemoryLogging() )
		log_printf("MEM: 0x%x segment 0x%x %d %d\n", this, memseg->address, 
				   memseg->size, memseg->type);
#endif

	char* addr = reinterpret_cast<char*>(memseg->address);
	UINTPTR lowbits = reinterpret_cast<UINTPTR>(addr) & (MEMORY_BLOCKSIZE-1);
	unsigned int size = memseg->size;

	unsigned int partial_low_bytes = 0;
	char* low_pool_ptr = addr;

	// Check if there is a partial pool at the beginning
	if ( lowbits != 0 )
	{
		// The low pool will not be properly aligned
		partial_low_bytes = MEMORY_BLOCKSIZE - lowbits;
		addr += partial_low_bytes;
		size -= partial_low_bytes;
	}

	// Check if there is a partial pool at the end
	unsigned int partial_high_bytes = size & (MEMORY_BLOCKSIZE-1);
	char* high_pool_ptr = addr + size;

	for (;;)
	{
		// Find a suitable OpMemPool somewhere
		void* pool = 0;
		if ( partial_high_bytes >= sizeof(OpMemPool) )
		{
			// Allocate from above, in partial pool
			high_pool_ptr -= sizeof(OpMemPool);
			partial_high_bytes -= sizeof(OpMemPool);
			pool = reinterpret_cast<void*>(high_pool_ptr);
		}
		else if ( partial_low_bytes >= sizeof(OpMemPool) )
		{
			// Allocate from below, in partial pool
			pool = reinterpret_cast<void*>(low_pool_ptr);
			low_pool_ptr += sizeof(OpMemPool);
			partial_low_bytes -= sizeof(OpMemPool);
		}
		else
		{
			// No space in any of the partial high or low pools, so go back
			// to eating from above by creating a new partial pool.
			partial_high_bytes += (MEMORY_BLOCKSIZE - sizeof(OpMemPool));
			high_pool_ptr -= sizeof(OpMemPool);
			pool = reinterpret_cast<void*>(high_pool_ptr);
		}

		int avail = high_pool_ptr - addr;

		if ( avail >= MINIMUM_BLOCK_SIZE )
		{
			if ( avail >= MEMORY_BLOCKSIZE )
			{
				// There is room for a complete pool
				CreatePool(pool, addr, MEMORY_BLOCKSIZE, MEMPOOL_ALIGN_FULL);
				addr += MEMORY_BLOCKSIZE;
			}
			else
			{
				// There is only room for a partial high block
				CreatePool(pool, addr, avail, MEMPOOL_ALIGN_UPPER);
				addr += avail;
				partial_high_bytes = 0;  // All spent high up
			}
		}
		else
		{
			// Make sure the low partial block is properly aligned.
			unsigned int adjust = (partial_low_bytes & (2*MEMORY_ALIGNMENT-1));
			partial_low_bytes -= adjust;
			low_pool_ptr += adjust;

			// Create a pool in the lower partial block if large enough.
			if ( partial_low_bytes >= MINIMUM_BLOCK_SIZE )
				CreatePool(pool, low_pool_ptr, partial_low_bytes,
						   MEMPOOL_ALIGN_LOWER);

			// This is the end of the road - no more room for pools
			break;
		}
	}
}

OpCoreSegment::~OpCoreSegment()
{
}

void OpCoreSegment::PoolAllocation(OpMemPool*)
{
	mempools_in_use_count++;
}

void OpCoreSegment::PoolFree(OpMemPool* pool)
{
	mempools_in_use_count--;

	if ( mempools_in_use_count == 0 )
	{
		// Disentangle pools from lists. No need to delete, as they live
		// in the segment.
		OpMemPool* p = g_mem_poolmgr->FirstFreePool(0, (OpMemPoolAlloc)type);
		while ( p )
		{
			OpMemPool* q = g_mem_poolmgr->NextPool();
			if ( p->segment == this )
			{
				OP_ASSERT(p->current_list == MEMPOOL_LIST_FREE);
				p->Out();
				p->~OpMemPool();
			}
			p = q;
		}

		OpMemory::DestroySegment(memory_segment);
		delete this;
	}
}

void OpCoreSegment::CreatePool(void* pool, char* addr,
							   unsigned int size, OpMemPoolAlignment flag)
{
	OpMemPool* p = new (pool) OpMemPool(addr, size, flag, this);
	mempools_total_count++;
	p->MoveTo(MEMPOOL_LIST_FREE);
}

#endif // USE_POOLING_MALLOC
