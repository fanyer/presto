/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef EXECUTABLE_MEMORY_MANAGER

#include "modules/memory/src/memory_executable.h"

OpExecMemoryManager::OpExecMemoryManager(OpMemoryClass type) :
#if defined(DEBUG_ENABLE_OPASSERT) && !defined(MEMORY_SMALL_EXEC_SEGMENTS)
	has_been_merged(FALSE),
#endif
	type(type)
{
}

OpExecMemoryManager::~OpExecMemoryManager(void)
{
#ifndef MEMORY_SMALL_EXEC_SEGMENTS
	for (;;)
	{
		OpExecMemorySegment* segment;
		segment = static_cast<OpExecMemorySegment*>(segments.First());
		if ( segment == 0 )
			break;
		OP_ASSERT(segment->allocations == 0 ); // Impossible...
		FreeSegment(segment);
	}
#endif // !MEMORY_SMALL_EXEC_SEGMENTS

	merged_with.Clear();
}

#ifndef MEMORY_SMALL_EXEC_SEGMENTS

const OpExecMemory* OpExecMemoryManager::AllocateL(size_t size)
{
#if !defined(MEMORY_SMALL_EXEC_SEGMENTS)
	// Allocations from merged instances of OpExecMemoryManager is not allowed
	OP_ASSERT(!has_been_merged);
#endif

	// Round size to a multiple of the assumed cache line size
	size = (size + 63) & ~(size_t)63;

	// Create the handle to be returned
	OpExecMemory* handle = OP_NEW_L(OpExecMemory, ());

	// Search among existing segments for free memory
	OpExecMemorySegment* segment =
		static_cast<OpExecMemorySegment*>(segments.First());

	if ( segment != 0 )
		if ( segment->free_bytes < size )
			segment = 0;
		else
			while ( segment->free_bytes > size && segment->Next() != 0 &&
					segment->Next()->free_bytes >= size )
				segment = segment->Next();

	// Allocate a new segment of executable memory since there is no space
	if ( segment == 0 )
	{
		size_t segment_size = MEMORY_EXEC_SEGMENT_SIZE;
		while ( size > segment_size )
			segment_size += segment_size;

		const OpMemory::OpMemSegment* mseg = 0;
		segment = OP_NEW(OpExecMemorySegment, ());
		if ( segment == 0 ||
			 (mseg = OpMemory::CreateExecSegment(segment_size)) == 0 )
		{
			OP_DELETE(handle);
			OP_DELETE(segment);
			LEAVE(OpStatus::ERR_NO_MEMORY);
		}

		segment->mseg = mseg;
		segment->free_ptr = (char*)segment->mseg->address;
		segment->free_bytes = segment->mseg->size;
		segment->allocations = 0;
		segment->IntoStart(&segments);
	}

	handle->address = (void*)(segment->free_ptr);
	handle->size = size;
	handle->segment = segment;

	// Open for writing
	void * ptr =
		OpMemory::WriteExec(segment->mseg, handle->address, handle->size);

	if ( ptr == 0 )
	{
		// Oops, failed to open segment for writing
		OP_DELETE(handle);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	// Succeeded - update the segment meta-data to account for the allocation
	OP_ASSERT(segment->free_bytes >= size);
	segment->free_ptr += size;
	segment->free_bytes -= size;
	segment->allocations++;

	// A different write address from the execute address is not
	// supported yet, so we assert on it for the time being.
	OP_ASSERT(ptr == handle->address);

	return handle;
}

void OpExecMemoryManager::FinalizeL(const OpExecMemory* handle)
{
	const OpMemory::OpMemSegment* mseg = handle->segment->mseg;
	OP_STATUS status =
		OpMemory::WriteExecDone(mseg, handle->address, handle->size);
	LEAVE_IF_ERROR(status);
}

void OpExecMemoryManager::Free(const OpExecMemory* handle)
{
	OpExecMemory* write_handle = const_cast<OpExecMemory*>(handle);
	OpExecMemorySegment* segment = write_handle->segment;

	if ( --segment->allocations == 0 )
		FreeSegment(segment);

	OP_DELETE(write_handle);
}

void OpExecMemoryManager::FreeSegment(OpExecMemorySegment* segment)
{
	OpMemory::DestroySegment(segment->mseg);
	segment->Out();
	OP_DELETE(segment);
}

#ifdef SELFTEST

int OpExecMemoryManager::Test_GetSegmentCount(void)
{
	return segments.Cardinal();
}

#endif // SELFTEST
#endif // MEMORY_SMALL_EXEC_SEGMENTS

void OpExecMemoryManager::MergeWith(OpExecMemoryManager *other)
{
#if defined(DEBUG_ENABLE_OPASSERT) && !defined(MEMORY_SMALL_EXEC_SEGMENTS)
	// Can't merge twise or when allready merged
	OP_ASSERT(!other->has_been_merged);
	OP_ASSERT(!has_been_merged);
	other->has_been_merged = TRUE;
#endif

	merged_with.Append(&other->merged_with);
	other->Into(&merged_with);
}


#endif // EXECUTABLE_MEMORY_MANAGER
