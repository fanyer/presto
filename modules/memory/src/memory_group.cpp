/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef MEMORY_GROPOOL_ALLOCATOR

#define MEMORY_GROUP_SIZE 8192
#define MEMORY_GROUP_UNUSABLE_SIZE 128

#include "modules/memory/src/memory_group.h"

OpMemGroup::OpMemGroup(void)
{
	Reset();
}

OpMemGroup::~OpMemGroup(void)
{
	ReleaseAll();
}

void* OpMemGroup::NewGRO(size_t size)
{
	// This allocator will not accept allocations of zero byte sizes
	OP_ASSERT(size > 0);

	// This is assumed below
	OP_ASSERT(MEMORY_GROUP_SIZE > MEMORY_GROUP_UNUSABLE_SIZE);

	//
	// Round up to nearest MEMORY_ALIGNMENT boundary to assure correct
	// alignment of objects.
	//
	size_t mask = (~(size_t)0) - (MEMORY_ALIGNMENT - 1);
	size = (size + MEMORY_ALIGNMENT - 1) & mask;

	if ( primary_size >= size )
	{
		// Satisfy request from primary memory area
		void* ptr = (void*)primary_ptr;
		primary_ptr += size;
		primary_size -= size;
		return ptr;
	}

	if ( secondary_size >= size )
	{
		// Satisfy request from secondary memory area
		void* ptr = (void*)secondary_ptr;
		secondary_ptr += size;
		secondary_size -= size;
		return ptr;
	}

	//
	// None of the primary or secondary memory areas where large enough to
	// satisfy the request, so we have to allocate a chunk of memory from
	// somewhere else first.
	//
	// Since we need to allocate something, determine size of the
	// header for chaining the allocations, since this will be needed
	// later:
	//
	const int header_size = 
		(sizeof(void*) > MEMORY_ALIGNMENT) ? sizeof(void*) : MEMORY_ALIGNMENT;

	//
	// Maybe the request failed because the size was very large? If
	// so, create a "private" allocation for the request. The limit is
	// set at 1/6th of the memory group size (chunk size), which is
	// really just an arbitrary number:
	//
	if ( size >= (MEMORY_GROUP_SIZE/6) )
	{
		void* ptr = op_malloc(size + header_size);
		if ( ptr == 0 )
			return 0;

		// Add this allocation to the chain of all allocations
		*(void**)ptr = all;
		all = ptr;
		return (void*)(((char*)ptr) + header_size);
	}

	//
	// The request was not large, so the two areas are either full, or
	// they have not been created yet, so create a new one:
	//
	void* fresh_area = op_malloc(MEMORY_GROUP_SIZE);
	if ( fresh_area == 0 )
		return 0;

	// Add this allocation to the chain of all allocations
	*(void**)fresh_area = all;
	all = fresh_area;

	//
	// Make sure the primary area is the one with the most bytes
	// free (since this is the one to be kept, and is the one to
	// be filled up first).
	//
	if ( primary_size < secondary_size )
	{
		// Swap primary and secondary

		char* tmp_ptr = primary_ptr;
		int tmp_size = primary_size;

		primary_ptr = secondary_ptr;
		primary_size = secondary_size;

		secondary_ptr = tmp_ptr;
		secondary_size = tmp_size;
	}

	void* ptr;

	if ( primary_size < MEMORY_GROUP_UNUSABLE_SIZE )
	{
		//
		// The primary, which is the one with the most free bytes, has
		// less free bytes than the threshold for what is considered
		// useful. Keep it for later none the less just in case, but
		// only as the secondary to speed up allocations from a new
		// fresh primary.
		//
		// This strategy will gain us something when allocations are
		// generally small in size. Profiling may reveal that this
		// optimization is not necessary.
		//
		secondary_ptr = primary_ptr;
		secondary_size = primary_size;

		// Give the fresh chunk of memory to the primary:
		primary_ptr = ((char*)fresh_area) + header_size;
		primary_size = MEMORY_GROUP_SIZE - header_size;

		// Satisfy the allocation from the fresh primary (this will
		// always work; the size is small)
		ptr = (void*)primary_ptr;
		primary_ptr += size;
		primary_size -= size;
	}
	else
	{
		//
		// The primary still has some useful free bytes, so keep it as
		// primary to increase its chance of contributing when a small
		// allocation comes along.
		//
		secondary_ptr = ((char*)fresh_area) + header_size;
		secondary_size = MEMORY_GROUP_SIZE - header_size;

		// Satisfy the allocation from the fresh secondary memory ares
		// (this will always work; the size is small)
		ptr = (void*)secondary_ptr;
		secondary_ptr += size;
		secondary_size -= size;
	}

	return ptr;
}

void* OpMemGroup::NewGRO_L(size_t size)
{
	void* ptr = NewGRO(size);
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#ifdef ENABLE_MEMORY_DEBUGGING

void* OpMemGroup::NewGRO(size_t size, const char* file, int line,
						 const char* obj)
{
	return NewGRO(size); // FIXME: Support proper debugging
}

void* OpMemGroup::NewGRO_L(size_t size, const char* file, int line,
						   const char* obj)
{
	void* ptr = NewGRO(size); // FIXME: Support proper debugging
	if ( ptr == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return ptr;
}

#endif // ENABLE_MEMORY_DEBUGGING

void OpMemGroup::ReleaseAll(void)
{
	void* ptr = all;

	while ( ptr != 0 )
	{
		void* next = *(void**)ptr;
		op_free(ptr);
		ptr = next;
	}

	Reset();	
}

#endif // MEMORY_GROPOOL_ALLOCATOR
