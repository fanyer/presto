/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement the SMOPOOL allocator.
 * 
 * Use the block allocator to allocate blocks from which equal sized
 * small to medium sized objects are allocated.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#include "modules/memory/src/memory_pooling.h"
#include "modules/memory/src/memory_segment.h"

#ifdef USE_POOLING_MALLOC

void OpMemPool::InitSMO(size_t sz)
{
	OP_ASSERT(type == MEMPOOL_FREE || type == MEMPOOL_UNALLOCATED);
	type = MEMPOOL_SMO;

	// Initialize SMO data.
	data.smo.freelist = 0;
	data.smo.size = sz;
	data.smo.count = 0;

	//
	// Place pointer(s) to responsible OpMemPool.
	//
	bool upper_pointer = false;
	bool lower_pointer = false;

	switch ( align )
	{
	case MEMPOOL_ALIGN_FULL:
		// Use two pointers if allocations don't automatically align, and
		// allocations will need alternating upper and lower pointers.
		if ( (sz & ((MEMORY_ALIGNMENT << 1) - 1)) != 0 )
			lower_pointer = true;
		// Fall through.

	case MEMPOOL_ALIGN_LOWER:
		upper_pointer = true;
		break;

	case MEMPOOL_ALIGN_UPPER:
		lower_pointer = true;
		break;
	}

	if ( upper_pointer )
	{
		*((OpMemPool**)(address + size - sizeof(OpMemPool*))) = this;
		data.smo.free_offset = 0;
	}
	
	if ( lower_pointer )
	{
		*((OpMemPool**)address) = this;
		data.smo.free_offset = (sizeof(this) + (MEMORY_ALIGNMENT - 1))
			& ~((UINTPTR)MEMORY_ALIGNMENT - 1);
	}

	segment->PoolAllocation(this);
}

void* OpMemPool::AllocSMO(void)
{
	OP_ASSERT(type == MEMPOOL_SMO);

	if ( data.smo.freelist != 0 )
	{
		// Satisfy request from the free list.
		char* a = address + (data.smo.freelist - 1);
		void* obj = reinterpret_cast<void*>(a);
		OpMemPoolLink* k = reinterpret_cast<OpMemPoolLink*>(obj);
		data.smo.freelist = k->next;
		data.smo.count++;
		return obj;
	}

	unsigned short top = size;
	if ( align != MEMPOOL_ALIGN_UPPER )
		top -= sizeof(OpMemPool*);

	if ( data.smo.free_offset + data.smo.size <= top )
	{
		// There is enough unallocated space in this pool for another
		// object, so allocate from this free space.

		unsigned int bytes = data.smo.size;
		if ( align != MEMPOOL_ALIGN_FULL )
			bytes = (bytes + (MEMORY_ALIGNMENT << 1) - 1) &
				~(((UINTPTR)MEMORY_ALIGNMENT << 1) - 1);

		char* a = address + data.smo.free_offset;
		void* obj = reinterpret_cast<void*>(a);
		data.smo.count++;
		data.smo.free_offset += bytes;

		return obj;
	}

	// There are no objects on the freelist, and the free_offset has gone
	// all the way to the top of the pool and filled it up.

	return 0;
}

void OpMemPool::FreeSMO(void* ptr)
{
	OP_ASSERT(type == MEMPOOL_SMO);

	// Free object and pop freelist pointer.
	OpMemPoolLink* k = reinterpret_cast<OpMemPoolLink*>(ptr);
	k->next = data.smo.freelist;
	data.smo.freelist = (reinterpret_cast<char*>(ptr) - address) + 1;
	data.smo.count--;

	if ( data.smo.count == 0 )
		Free();
	else if ( current_list == MEMPOOL_LIST_FULL )
		MoveTo(MEMPOOL_LIST_AVAILABLE);
}

#endif // USE_POOLING_MALLOC
