/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement a heap with 'sbrk' functionality used by lea_malloc.
 *
 * This functionality should be enabled only in order to simplify
 * porting of lea_malloc to a platform where virtual memory is
 * available and implemented.
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_OPERA_HEAP_SEGMENT

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_pilayer.h"
#include "modules/memory/src/memory_heap.h"

#ifdef MEMORY_USE_LOCKING
OpHeapSegment* OpHeapSegment::Create(const OpMemory::OpMemSegment* mseg,
									 OpMemoryClass type, BOOL memlock)
#else
OpHeapSegment* OpHeapSegment::Create(const OpMemory::OpMemSegment* mseg,
									 OpMemoryClass type)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	OpHeapSegment* hseg = 0;
	size_t mem_pagesize = OpMemory::GetPageSize();

	OP_ASSERT(mseg != 0);
	OP_ASSERT(mseg->type == OpMemory::MEMORY_SEGMENT_VIRTUAL);
	OP_ASSERT(mseg->size >= 3 * mem_pagesize); // Maybe only two?

	void* addr = mseg->address;
	size_t size = mem_pagesize;

	if ( OpMemory_VirtualAlloc(mseg, addr, size, type) )
	{
		hseg = reinterpret_cast<OpHeapSegment*>(addr);
		hseg->mseg = mseg;
		hseg->type = type;
		hseg->base = (char*)addr + mem_pagesize;
		hseg->top = (char*)addr + mseg->size;
		hseg->current = hseg->base;
		hseg->allocated = hseg->current;
		hseg->pagesize = mem_pagesize; 
	}

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif

	return hseg;
}

#ifdef MEMORY_USE_LOCKING
void OpHeapSegment::Destroy(const OpHeapSegment* hseg, BOOL memlock)
#else
void OpHeapSegment::Destroy(const OpHeapSegment* hseg)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	OP_ASSERT(hseg != 0);

	void* addr = hseg->mseg->address;
	size_t size = hseg->allocated - (char*)addr;

	OP_ASSERT(addr == (void*)hseg);
	OP_ASSERT(hseg->allocated > (char*)addr );

	OpMemory_VirtualFree(hseg->mseg, addr, size, hseg->type);

	// The OpMemSegment does no longer hold any data, but is not deleted

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif
}

#ifdef MEMORY_USE_LOCKING
void* OpHeapSegment::sbrk(long increment, BOOL memlock)
#else
void* OpHeapSegment::sbrk(long increment)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	void* ptr = (void*)current;

	if ( increment != 0 )
	{
		UINTPTR step = 0x10000 - 1; // Allocate in 64K chunks
		OP_ASSERT(pagesize <= 0x10000);

		char* new_current = current + increment;
		char* new_allocated = (char*)(((UINTPTR)(new_current + step)) & ~step);

		if ( new_allocated > top )
		{
			//
			// No remaining space in memory segment for the chunk, but try
			// without rounding up to the chunksize and see if that works.
			//
			step = pagesize - 1;
			new_allocated = (char*)(((UINTPTR)(new_current + step)) & ~step);
			if ( new_allocated > top )
			{
				// Still no space in memory segment for allocation
				ptr = (void*)-1; // Standard UNIX return value
				goto done;
			}
		}

		if ( new_allocated > allocated )
		{
			// We need to allocate more memory to grow the heap
			size_t size = new_allocated - allocated;
			if ( ! OpMemory_VirtualAlloc(mseg, allocated, size, type) )
			{
				// No way to satisfy allocation
				ptr = (void*)-1; // Standard UNIX return value
				goto done;
			}
		}
		else if ( new_allocated < allocated )
		{
			// We can free some memory as the heap is shrinking
			size_t size = allocated - new_allocated;
			OpMemory_VirtualFree(mseg, (void*)new_allocated, size, type);
		}

		allocated = new_allocated;
		current = new_current;
	}

 done:

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif

	return ptr;
}

#ifdef ENABLE_OPERA_SBRK

extern "C" void* op_sbrk(long increment)
{
	OpHeapSegment* heap = g_mem_sbrk_heap;

	if ( heap == 0 )
	{
		OpMemoryStateInit();

		OP_ASSERT(g_mem_sbrk_mseg == 0);
		size_t minimum = MEMORY_OPERA_SBRK_SIZE;

#ifdef MEMORY_USE_LOCKING
		// Expect calling function (e.g. lea_malloc) to grab the lock for us
		g_mem_sbrk_mseg = OpMemory_CreateVirtualSegment(minimum, FALSE);
#else
		g_mem_sbrk_mseg = OpMemory_CreateVirtualSegment(minimum);
#endif

		if ( g_mem_sbrk_mseg != 0 )
		{
#ifdef MEMORY_USE_LOCKING
			heap = OpHeapSegment::Create(g_mem_sbrk_mseg, MEMCLS_OP_SBRK,
										 FALSE);
#else
			heap = OpHeapSegment::Create(g_mem_sbrk_mseg, MEMCLS_OP_SBRK);
#endif

			g_mem_sbrk_heap = heap;
			if ( heap == 0 )
			{
#ifdef MEMORY_USE_LOCKING
				OpMemory_DestroySegment(g_mem_sbrk_mseg, FALSE);
#else
				OpMemory_DestroySegment(g_mem_sbrk_mseg);
#endif
				g_mem_sbrk_mseg = 0;
			}
		}
	}

	if ( heap != 0 )
	{
#ifdef MEMORY_USE_LOCKING
		return heap->sbrk(increment, FALSE);
#else
		return heap->sbrk(increment);
#endif
	}

	return (void*)-1;
}

#endif // ENABLE_OPERA_SBRK

#endif // ENABLE_OPERA_HEAP_SEGMENT
