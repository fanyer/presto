/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement mmap functionality for use by e.g. lea_malloc.
 *
 * This functionality should be enabled only in order to simplify
 * porting of lea_malloc to a platform where virtual memory is
 * available and implemented.
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_OPERA_MMAP_SEGMENT

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_pilayer.h"
#include "modules/memory/src/memory_mmap.h"
#include "modules/memory/src/memory_heap.h"

#ifdef MEMORY_USE_LOCKING
void* OpMmapSegment::mmap(size_t size, OpMemoryClass type, BOOL memlock)
#else
void* OpMmapSegment::mmap(size_t size, OpMemoryClass type)
#endif
{
	unsigned int pages = (size + pagesize - 1) / pagesize;
	OP_ASSERT(pages > 0); // Zero allocations with mmap is not supported

	// We can't  handle "impossibly large" single allocations. This means
	// pages that can't be indexed by 16-bit variables.
	if ( pages > 0xffff )
		return 0;

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	unsigned int size_class = ComputeSizeClass(pages);
	char* ptr = 0;
	unsigned int cls;

	if ( max_size[OP_MMAP_UNUSED_SIZECLASS + size_class] >= pages )
	{
		// Apparently, there should be an unused allocation large
		// enough to hold this request, so search for it.

		unsigned int idx =
			page_handle[OP_MMAP_UNUSED_SIZECLASS + size_class].next;

		UINT16 found_max_size = 0; // For recomputing max_size

		while ( idx >= OP_MMAP_ANCHOR_COUNT )
		{
			unsigned int found_pages = page_handle[idx].size;

			if ( found_pages >= pages )
			{
				// We have an unused allocation that is large enough
				ptr = (char*)AllocateUnused(idx, pages, type);
				goto done;
			}

			if ( found_pages > found_max_size )
				found_max_size = found_pages;

			idx = page_handle[idx].next;
		}

		// No unused allocation large enough for our needs, even
		// though it was supposed to be one somewhere... The
		// 'max_size' must have been wrong, so update it to the correct
		// value and fall through and try bigger size-classes.

		max_size[OP_MMAP_UNUSED_SIZECLASS + size_class] = found_max_size;
	}
#ifdef DEBUG_ENABLE_OPASSERT
	else
	{
		//
		// Make sure there really isn't any blocks larger than given,
		// this would cause a corruption furhter down when reusing an
		// an unused block and extend it with fresh memory.
		//
		unsigned int idx =
			page_handle[OP_MMAP_UNUSED_SIZECLASS + size_class].next;
		unsigned int biggest = max_size[OP_MMAP_UNUSED_SIZECLASS + size_class];
			

		while ( idx >= OP_MMAP_ANCHOR_COUNT )
		{
			OP_ASSERT(page_handle[idx].size <= biggest);
			idx = page_handle[idx].next;
		}
	}
#endif // DEBUG_ENABLE_OPASSERT

	// Search higher size-classes
	for ( cls = size_class + 1; cls <= max_size_class; cls++ )
	{
		unsigned int idx = page_handle[OP_MMAP_UNUSED_SIZECLASS + cls].next;

		if ( idx > OP_MMAP_ANCHOR_COUNT )
		{
			// This is an unused allocation, and since it is of a
			// higher size-class, it will always be large enough
			ptr = (char*)AllocateUnused(idx, pages, type);
			goto done;
		}
	}

	//
	// No unused allocations of sufficient size are available, so we
	// need to request some from the operating system.
	//

	// Search for sufficiently large reserved memory space
	for ( cls = size_class; cls <= max_size_class; cls++ )
	{
		unsigned int idx = page_handle[OP_MMAP_RESERVED_SIZECLASS + cls].next;

		while ( idx > OP_MMAP_ANCHOR_COUNT )
		{
			if ( page_handle[idx].size >= pages )
			{
				//
				// Found a reserved block that is large enough
				//

				if ( page_handle[idx - 1].flag == OP_MMAP_FLAG_UNUSED )
				{
					//
					// The block below is unused: Include it into our own
					// allocation, so we don't have to request as much
					// memory as we otherwise would have.
					//
					// How this impacts fragmentation is unknown.
					//
					unsigned int size2 = page_handle[idx - 1].size;
					OP_ASSERT(size2 < pages); // We would have found it previously
					unsigned int idx2 = idx - size2;
					OP_ASSERT(page_handle[idx2].size == size2);
					OP_ASSERT(page_handle[idx2].flag == OP_MMAP_FLAG_UNUSED);

#ifdef ENABLE_MEMORY_MANAGER
					if ( mm->Alloc(type, size2 * pagesize) )
#endif
					{
						ptr = (char*)AllocateReserved(idx, pages - size2, type);

						if ( ptr != 0 )
						{
							ptr -= (size2 * pagesize);
							Unlink(idx2);
							SetDetails(idx2, pages, OP_MMAP_FLAG_ALLOCATED);
							page_handle[idx2].type = type;

							unused_pages -= size2;
							allocated_pages += size2;

#ifdef ENABLE_MEMORY_MANAGER
							// The 'size2' unused block is no longer unused
							mm->Free((OpMemoryClass)unusedtype,
									 size2 * pagesize);
						}
						else
						{
							// Allocation failed, reverse the reservation
							// on the 'size2' unused block below
							mm->Free(type, size2 * pagesize);
#endif
						}
					}

					goto done;
				}

				ptr = (char*)AllocateReserved(idx, pages, type);
				goto done;
			}

			idx = page_handle[idx].next;
		}
	}

 done:

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif

	return (void*)ptr;
}

#ifdef MEMORY_USE_LOCKING
void OpMmapSegment::munmap(void* ptr, BOOL memlock)
#else
void OpMmapSegment::munmap(void* ptr)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	OP_ASSERT(ptr != 0);

	char* cptr = (char*)ptr;
	OP_ASSERT(address_first_usable <= cptr);
	OP_ASSERT(cptr < address_last_usable);
	OP_ASSERT((((UINTPTR)ptr) & (pagesize - 1)) == 0); // Page aligned
	unsigned int idx = (cptr - base) >> pagesize_shift;
	OP_ASSERT(page_handle[idx].flag == OP_MMAP_FLAG_ALLOCATED);
	unsigned int pages = page_handle[idx].size;

#ifdef DEBUG_ENABLE_OPASSERT
	unsigned int idx2 = idx + pages - 1;
	OP_ASSERT(page_handle[idx2].size == pages);
	OP_ASSERT(page_handle[idx2].flag == OP_MMAP_FLAG_ALLOCATED);
#endif

#ifdef ENABLE_MEMORY_MANAGER
	// Move this allocation from its real memory class type into the
	// temporary one for "avilable" but unused mmap memory
	size_t size = pages * pagesize;
	OpMemoryClass type = (OpMemoryClass)page_handle[idx].type;
	mm->ForceTransfer(type, (OpMemoryClass)unusedtype, size);
#endif

	Merge(OP_MMAP_FLAG_UNUSED, unusedtype, idx);

	allocated_pages -= pages;
	unused_pages += pages;

	// Release some unused memory if the threshold suggests we should
	ReleaseUnused();

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif
}

void* OpMmapSegment::AllocateUnused(UINT16 idx, UINT16 pages,
									OpMemoryClass type)
{
	OP_ASSERT(page_handle[idx].flag == OP_MMAP_FLAG_UNUSED);
	OP_ASSERT(page_handle[idx].size >= pages);

#ifdef DEBUG_ENABLE_OPASSERT
	unsigned int endmarker = idx + page_handle[idx].size - 1;
	OP_ASSERT(page_handle[endmarker].flag == OP_MMAP_FLAG_UNUSED);
	OP_ASSERT(page_handle[endmarker].size == page_handle[idx].size);
#endif

#ifdef ENABLE_MEMORY_MANAGER
	// Move the accounting from "available" to allocated by correct type,
	// and fail allocation if this is not possible
	if ( ! mm->Transfer((OpMemoryClass)unusedtype, type, pages * pagesize) )
		return 0;
#endif

	// Record the number of remaining pages in the block.
	// This number will always be 0 or positive.
	unsigned int pages2 = page_handle[idx].size - pages;

	// Unlink from unused chain, and set new details
	Unlink(idx);
	SetDetails(idx, pages, OP_MMAP_FLAG_ALLOCATED);
	page_handle[idx].type = type;

	// If there are any remaining pages, insert them on the unused chain
	if ( pages2 )
	{
		// Create a new unused allocation for remainder
		unsigned int idx2 = idx + pages;
		unsigned int cls2 = ComputeSizeClass(pages2);
		SetDetails(idx2, pages2, OP_MMAP_FLAG_UNUSED);
		Link(OP_MMAP_UNUSED_SIZECLASS + cls2, idx2);
	}

	allocated_pages += pages;
	unused_pages -= pages;

	return reinterpret_cast<void*>(base + idx * pagesize);
}

void* OpMmapSegment::AllocateReserved(UINT16 idx, UINT16 pages,
									  OpMemoryClass type)
{
	OP_ASSERT(page_handle[idx].flag == OP_MMAP_FLAG_RESERVED);
	OP_ASSERT(page_handle[idx].size >= pages);

#ifdef DEBUG_ENABLE_OPASSERT
	unsigned int endmarker = idx + page_handle[idx].size - 1;
	OP_ASSERT(page_handle[endmarker].flag == OP_MMAP_FLAG_RESERVED);
	OP_ASSERT(page_handle[endmarker].size == page_handle[idx].size);
#endif

	// 
	// Identify and allocate the memory area that needs to be
	// requested with VirtualAlloc()
	//
	void* ptr = (void*)(base + idx * pagesize);
	unsigned int bytes = pages * pagesize;

	if ( OpMemory_VirtualAlloc(mseg, ptr, bytes, type) )
	{
		//
		// Succeeded, but we need to make sure the two handles of the
		// new block is available, and the the first handle in the
		// following block.
		//
		if ( AllocateHandles(idx + pages) )
		{
			//
			// Main block and handles allocated ok.
			//
			// Recordthe number of remaining pages in the block.  This
			// number will always be 0 or positive.
			//
			unsigned int pages2 = page_handle[idx].size - pages;

			// Unlink from reserved chain, and set new details
			Unlink(idx);
			SetDetails(idx, pages, OP_MMAP_FLAG_ALLOCATED);
			page_handle[idx].type = type;

			// If there are any remaining pages, add them to reserved chain
			if ( pages2 )
			{
				// Create a new reserved block for remainder of block
				unsigned int idx2 = idx + pages;
				unsigned int cls2 = ComputeSizeClass(pages2);
				SetDetails(idx2, pages2, OP_MMAP_FLAG_RESERVED);
				Link(OP_MMAP_RESERVED_SIZECLASS + cls2, idx2);
			}

			allocated_pages += pages;

			goto success;
		}

		//
		// Could not allocate handles, free the previously
		// allocated memory and fall through into failure.
		//
		OpMemory_VirtualFree(mseg, ptr, bytes, type);
	}

	// Failure
	ptr = 0;

 success:
	return ptr;
}

void OpMmapSegment::Merge(UINT8 flag, UINT8 type, UINT16 idx)
{
	OP_ASSERT(flag == OP_MMAP_FLAG_UNUSED ||
			  flag == OP_MMAP_FLAG_RESERVED);

	UINT16 pages = page_handle[idx].size;

	if ( page_handle[idx - 1].flag == flag )
	{
		UINT16 size2 = page_handle[idx - 1].size;
		idx -= size2;
		OP_ASSERT(page_handle[idx].size == size2);
		OP_ASSERT(page_handle[idx].flag == flag);
		pages += size2;
		Unlink(idx);
	}

	UINT16 idx2 = idx + pages;

	if ( page_handle[idx2].flag == flag )
	{
		UINT16 size2 = page_handle[idx2].size;
		OP_ASSERT(page_handle[idx2 + size2 - 1].size == size2);
		OP_ASSERT(page_handle[idx2 + size2 - 1].flag == flag);
		pages += size2;
		Unlink(idx2);
	}

	unsigned int cls;
	unsigned int size_class = ComputeSizeClass(pages);
	SetDetails(idx, pages, flag);
	page_handle[idx].type = type;

	switch ( flag )
	{
	case OP_MMAP_FLAG_UNUSED:
		cls = OP_MMAP_UNUSED_SIZECLASS + size_class;
		break;

	case OP_MMAP_FLAG_RESERVED:
		cls = OP_MMAP_RESERVED_SIZECLASS + size_class;
		break;

	default:
		OP_ASSERT(!"Critical - Illegal merge operation");
		return;
	}

	Link(cls, idx);
}

#ifdef MEMORY_USE_LOCKING
void OpMmapSegment::ReleaseAllUnused(BOOL memlock)
#else
void OpMmapSegment::ReleaseAllUnused(void)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	unsigned int tmp = unused_pages_threshold;
	unused_pages_threshold = 0;
	ReleaseUnused();
	unused_pages_threshold = tmp;

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif
}

void OpMmapSegment::ReleaseUnused(void)
{
	if ( unused_pages > unused_pages_threshold )
	{
		unsigned int half_unused_pages_threshold = unused_pages_threshold / 2;

		for ( int cls = max_size_class; cls >= 0; cls-- )
		{
			unsigned int anchor = OP_MMAP_UNUSED_SIZECLASS + cls;

			for (;;)
			{
				if ( unused_pages <= half_unused_pages_threshold )
					return;

				unsigned int idx = page_handle[anchor].prev;

				if ( idx < OP_MMAP_ANCHOR_COUNT )
					break;

				OP_ASSERT(page_handle[idx].flag == OP_MMAP_FLAG_UNUSED);
				unsigned int pages = page_handle[idx].size;
				void* ptr = (void*)(base + idx * pagesize);
				OpMemory_VirtualFree(mseg, ptr, pages * pagesize,
									 (OpMemoryClass)unusedtype);

				Unlink(idx);
				Merge(OP_MMAP_FLAG_RESERVED, 0, idx);

				unused_pages -= pages;
			}
		}
	}
}

#ifdef MEMORY_USE_LOCKING
OpMmapSegment* OpMmapSegment::Create(const OpMemory::OpMemSegment* mseg,
									 OpMemoryClass hdr, OpMemoryClass unused,
									 BOOL memlock)
#else
OpMmapSegment* OpMmapSegment::Create(const OpMemory::OpMemSegment* mseg,
									 OpMemoryClass hdr, OpMemoryClass unused)
#endif
{
#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocLock();
#endif

	OpMmapSegment* mmap = 0;
	size_t mem_pagesize = OpMemory::GetPageSize();

	OP_ASSERT(mem_pagesize > 0); // This is an absolute requirement
	OP_ASSERT(mem_pagesize >= 4096); // While this is a sensible one...

	// We assume this to be true in this function (must be power of two)
	OP_ASSERT(sizeof(OpMemPageHandle) == 8);

	// This simplifies and saves us a single page, potentially
	OP_ASSERT((sizeof(OpMmapSegment) & 7) == 0);

	// Memory segment must exist
	OP_ASSERT(mseg != 0);

	// Memory segment must be virtual
	OP_ASSERT(mseg->type == OpMemory::MEMORY_SEGMENT_VIRTUAL);

	// The OpMmapSegment and the first two handles must fit in the first page
	OP_ASSERT(sizeof(OpMmapSegment)+2*sizeof(OpMemPageHandle) <= mem_pagesize);

	void* addr = mseg->address;

	if ( OpMemory_VirtualAlloc(mseg, addr, mem_pagesize, hdr) )
	{
		mmap = reinterpret_cast<OpMmapSegment*>(addr);
		unsigned int available = mseg->size / mem_pagesize;
		unsigned int usable;

		usable = ComputeUsablePages(available, mem_pagesize);
		OP_ASSERT(usable > 1); // Code below assumes this

		mmap->mseg = mseg;
		mmap->hdrtype = hdr;
		mmap->unusedtype = unused;
		mmap->pagesize = mem_pagesize;
		mmap->highest_index = 0;
		mmap->header_pages = 1; // One page created for 'OpMmapSegment' itself

		mmap->allocated_pages = 0;
		mmap->unused_pages = 0;

		mmap->unused_pages_threshold = MEMORY_MMAP_DEFAULT_MAX_UNUSED;

#ifdef ENABLE_MEMORY_MANAGER
		mmap->mm = g_mem_manager; // Cached for performance
#endif

		// Compute the pagesize_shift variable (for optimizing computations)
		mmap->pagesize_shift = 0;
		for ( size_t psx = mem_pagesize; psx != 1; psx >>= 1 )
		{
			mmap->pagesize_shift++;
			OP_ASSERT((psx & 1) == 0); // Must be power of two
		}

		// Initialize the anchor handles to empty doubly linked list
		for ( int k = 0; k < OP_MMAP_ANCHOR_COUNT; k++ )
		{
			mmap->page_handle[k].next = k;
			mmap->page_handle[k].prev = k;
			mmap->max_size[k] = 0;
		}

		//
		// Allocate page(s) for the topmost usable handle, since it
		// will be updated when the topmost reserved space is chopped
		// into smaller pieces, and at the same time we don't want to
		// populate all the handles all the way up to the very last
		// handle (to avoid using a fixed, large, ammount of memory for
		// the handles, even when there are few or no allocations).
		//
		// The sentinels are there to simplify fencepost handling code.
		// No blocks will ever be merged with any of the sentinels,
		// due to differing types, so we don't have to worry about
		// the upper and lower blocks when merging.
		// The sentinels only have a representation in the page_handle
		// table - they don't have any corresponding memory pages.
		//
		unsigned int low_sentinel_handle = OP_MMAP_ANCHOR_COUNT;
		unsigned int low_usable_handle = low_sentinel_handle + 1;

		unsigned int top_usable_handle = low_usable_handle + usable - 1;
		unsigned int top_sentinel_handle = top_usable_handle + 1;

		unsigned int top_usable_page =
			mmap->CalculatePageOfHandle(top_usable_handle);
		unsigned int top_sentinel_page =
			mmap->CalculatePageOfHandle(top_sentinel_handle);

		mmap->header_max_pages = top_usable_page;
		mmap->address_upper_handles = 0;

		if ( top_sentinel_page > 0 )
		{
			//
			// The sentinel does not fall within the first page; so at
			// least on allocation is needed for the sentinel, and
			// possibly one for the top usable handle as well (if they
			// are in different pages). Make sure not to re-allocate
			// the first page, since this one is allready allocated.
			//
			unsigned int first = top_usable_page;
			if ( first == 0 )
				first = 1;

			char* ptr = (char*)mseg->address + first * mem_pagesize;
			size_t size = (top_sentinel_page - first + 1) * mem_pagesize;
			if ( ! OpMemory_VirtualAlloc(mseg, (void*)ptr, size, hdr) )
			{
				// Failed allocation for top page(s) - abort since we need that
				OpMemory_VirtualFree(mseg, mseg->address, mem_pagesize, hdr);
				mmap = 0;
				goto done;
			}

			mmap->header_max_pages = first;
			mmap->address_upper_handles = (void*)ptr;
			mmap->size_upper_handles = size;
		}

		// Install the sentinels
		mmap->SetDetails(low_sentinel_handle, 1, OP_MMAP_FLAG_SENTINEL);
		mmap->SetDetails(top_sentinel_handle, 1, OP_MMAP_FLAG_SENTINEL);

		// Install the initial all-reserved block
		unsigned int cls = ComputeSizeClass(usable);
		unsigned int anchor = OP_MMAP_RESERVED_SIZECLASS + cls;
		mmap->SetDetails(low_usable_handle, usable, OP_MMAP_FLAG_RESERVED);
		mmap->Link(anchor, low_usable_handle);
		mmap->max_size_class = cls;

		// Compute the 'base' address of (virtual) handle with index == 0.
		mmap->base = reinterpret_cast<char*>(mseg->address);
		mmap->base += ((top_sentinel_page + 1) * mem_pagesize); // Skip header
		mmap->base -= (low_usable_handle * mem_pagesize); // Adjust for bias

		// Compute the "bounding box" for allocation addresses
		mmap->address_first_usable =
			mmap->base + low_usable_handle * mem_pagesize;
		mmap->address_last_usable =
			mmap->base + top_sentinel_handle * mem_pagesize;
	}

 done:

#ifdef MEMORY_USE_LOCKING
	if ( memlock )
		OpMemory::MallocUnlock();
#endif

	return mmap;
}

unsigned int OpMmapSegment::CalculatePageOfHandle(UINT16 index)
{
	unsigned int bytes = sizeof(OpMmapSegment)
		+ (index - OP_MMAP_ANCHOR_COUNT + 1) * sizeof(OpMemPageHandle);

	unsigned int index_page = bytes / pagesize;

	OP_ASSERT(index_page <= 0xffff);

	return index_page;
}

BOOL OpMmapSegment::AllocateHandles(UINT16 index)
{
	BOOL rc = TRUE;

	OP_ASSERT(mseg->address == (void*)this);

	if ( index > highest_index )
	{
		//
		// We need to allocate the handles up to and including the index
		// to cover all allocations in this region.  Note: This allocation
		// only includes allocating memory for the handles, initialization
		// is done when actually needed.
		//
		unsigned int index_page = CalculatePageOfHandle(index);

		//
		// Make sure we don't overwrite the previously allocated 'upper'
		// index (and sentinel) when expanding the number of handles.
		//
		if ( index_page >= header_max_pages )
			index_page = header_max_pages - 1;

		if ( index_page >= header_pages )
		{
			//
			// Allocation of a page to the (possibly adjusted) handle
			// is needed.
			//
			size_t size = (index_page - header_pages + 1) * pagesize;
			char* ptr = (char*)mseg->address + header_pages * pagesize;
			OpMemoryClass type = (OpMemoryClass)hdrtype;

			if ( OpMemory_VirtualAlloc(mseg, (void*)ptr, size, type) )
			{
				highest_index = index;
				header_pages = index_page + 1;
			}
			else
			{
				// Failed to allocate the handles needed
				rc = FALSE;
			}
		}
	}

	return rc;
}

void OpMmapSegment::ForceReleaseAll(void)
{
#ifdef MEMORY_USE_LOCKING
	OpMemory::MallocLock();
#endif

	unsigned int idx = OP_MMAP_ANCHOR_COUNT + 1;

	while ( page_handle[idx].flag != OP_MMAP_FLAG_SENTINEL )
	{
		UINT16 size = page_handle[idx].size;
		UINT16 flag = page_handle[idx].flag;

		OP_ASSERT(page_handle[idx + size - 1].size == size);
		OP_ASSERT(page_handle[idx + size - 1].flag == flag);

		if ( flag == OP_MMAP_FLAG_ALLOCATED )
		{
			unsigned int idx2 = idx;

			while ( page_handle[idx2].flag == OP_MMAP_FLAG_ALLOCATED )
			{
				UINT16 size2 = page_handle[idx2].size;

#ifdef DEBUG_ENABLE_OPASSERT
				UINT16 flag2 = page_handle[idx2].flag;
				OP_ASSERT(page_handle[idx2 + size2 - 1].size == size2);
				OP_ASSERT(page_handle[idx2 + size2 - 1].flag == flag2);
#endif

#ifdef ENABLE_MEMORY_MANAGER
				OpMemoryClass type = (OpMemoryClass)page_handle[idx2].type;
				mm->Free(type, size2 * pagesize);
#endif

				idx2 += size2;
			}

			unsigned int pages = idx2 - idx;
			unsigned int cls = ComputeSizeClass(pages);
			SetDetails(idx, pages, OP_MMAP_FLAG_UNUSED);

#ifdef ENABLE_MEMORY_MANAGER
			mm->ForceAlloc((OpMemoryClass)unusedtype, pages * pagesize);
#endif

			Link(OP_MMAP_UNUSED_SIZECLASS + cls, idx);

			allocated_pages -= pages;
			unused_pages += pages;

			size = pages;
		}

		idx += size;
	}

	// We should have no allocated pages any more
	OP_ASSERT(allocated_pages == 0);

	// The previously allocated ones should now be unused; release them all
#ifdef MEMORY_USE_LOCKING
	ReleaseAllUnused(FALSE);
#else
	ReleaseAllUnused();
#endif

	OP_ASSERT(unused_pages == 0);

	//
	// The last action is to release the memory used for the header and
	// page handles. After this operation, 'this' is no longer accessible.
	//
	OpMemoryClass type = (OpMemoryClass)hdrtype;

	if ( address_upper_handles != 0 )
	{
		// Remove the upper handles, allocated on its own
		OpMemory_VirtualFree(mseg, address_upper_handles,
							 size_upper_handles, type);
	}

	OpMemory_VirtualFree(mseg, mseg->address, pagesize * header_pages, type);

#ifdef MEMORY_USE_LOCKING
	OpMemory::MallocUnlock();
#endif
}

void OpMmapSegment::Unlink(UINT16 page)
{
	unsigned int prev = page_handle[page].prev;
	unsigned int next = page_handle[page].next;

	page_handle[prev].next = next;
	page_handle[next].prev = prev;
}

void OpMmapSegment::Link(UINT16 anchor, UINT16 page)
{
	unsigned int prev = anchor;
	unsigned int next = page_handle[anchor].next;

	page_handle[prev].next = page;
	page_handle[next].prev = page;
	page_handle[page].next = next;
	page_handle[page].prev = prev;

	//
	// Increase max_size[] when increasing. Decreasing it is done when
	// a linear search fails to find a large enough block.
	//
	unsigned int pages = page_handle[page].size;
	if ( max_size[anchor] < pages )
		max_size[anchor] = pages;
}

void OpMmapSegment::SetDetails(UINT16 page, UINT16 size, UINT8 flag)
{
	unsigned int lower = page;
	unsigned int upper = lower + size - 1;

	page_handle[lower].flag = flag;
	page_handle[lower].size = size;
	page_handle[upper].flag = flag;
	page_handle[upper].size = size;

#ifdef _DEBUG
	// The upper links are unused and should never be followed.
	// This initialization may provoke crash if followed anyway.
	page_handle[upper].prev = 0xffff;
	page_handle[upper].next = 0xffff;
#endif
}

unsigned int OpMmapSegment::ComputeUsablePages(unsigned int pages,
											   size_t pagesize)
{
	unsigned int bytes = pages * pagesize;
	bytes -= sizeof(OpMmapSegment); // Subtract header & its anchors
	bytes -= (2 * sizeof(OpMemPageHandle)); // Subtract two sentinels

	unsigned int provided = bytes / (pagesize + sizeof(OpMemPageHandle));
	unsigned int max = 0x10000 - (OP_MMAP_ANCHOR_COUNT + 2);
	
	if ( provided > max )
	{
		// With 4K pages, the limit is slightly less than 256 MB
		// OP_ASSERT(!"MMAP can't use all of virtual memory segment");
		provided = max;
	}

	return provided;
}

//
// Compute the "size-class" of an mmap'ed allocation.  The size-class
// is a number 0-28 which classifies the size.
//
// The smallest 17 classes each holds an exact number of memory pages:
//
// 0 = 1 page
// 1 = 2 pages
// 2 = 3 pages
//  ...
// 14 = 15 pages
// 15 = 16 pages (64K for 4K memory pages)
// 16 = 17 pages (68K for 4K memory pages - 64K "pluss a little overhead")
//
// The biggest 12 classes each holds a progressively larger number of pages:
//
// 16 = 18-31 pages (68K+1 => 124K)
// 17 = 33-64 pages (124K+1 => 252K)
// 18 = 65-128 pages (252K+1 => 508K)
// 19 = 129-256 pages (508K+1 => 1020K)
// 20 = 257-512 pages (1020K+1 => 2044K)
// 21 = 513-1024 pages (2044K+1 => 4092K)
// 22 = 1025-2048 pages (4092K+1 => 8188K)
// 23 = 2049-4096 pages (8188K+1 => 16380K)
// 24 = 4097-8192 pages (16380K+1 => 32764K)
// 25 = 8193-16384 pages (32764K+1 => 65532K)
// 26 = 16385-32768 pages (65532K+1 => 131068K)
// 27 = 32769-65536 pages (131068K+1 => 262144K)
//
// The classes are made so that an exact power of two size is the smallest
// number of pages in its size class. A power of two allocation will thus
// never have to traverse a list to find a large enough free block: All
// free blocks in the sizeclass will be large enough, so the first entry
// in the list will always be large enough).
//

unsigned int OpMmapSegment::ComputeSizeClass(UINT16 pages)
{
	OP_ASSERT(pages > 0);  // Assuming 1 page or more

	if ( pages <= 17 )
	{
		// Sizes of 1-17 memory pages (1 to 68K bytes with 4K pages)
		return pages - 1;
	}

	unsigned int cls;

	// Nasty binary tree to decide the class with 4 tests

	if ( pages < 4096 )
		if ( pages < 256 )
			if ( pages < 64 )
				if ( pages < 32 )
					cls = 17;
				else
					cls = 18;
			else
				if ( pages < 128 )
					cls = 19;
				else
					cls = 20;
		else
			if ( pages < 1024 )
				if ( pages < 512 )
					cls = 21;
				else
					cls = 22;
			else
				if ( pages < 2048 )
					cls = 23;
				else
					cls = 24;
	else
		if ( pages < 16384 )
			if ( pages < 8192 )
				cls = 25;
			else
				cls = 26;
		else
			if ( pages < 32768 )
				cls = 27;
			else
				cls = 28;

	return cls;
}

#ifdef ENABLE_OPERA_MMAP

extern "C" void* op_mmap(void* ptr, size_t size, int prot, int flags)
{
	OP_ASSERT(ptr == 0); // Only fresh allocations are allowed
	OP_ASSERT(size > 0);

	OpMmapSegment* mmap = g_mem_mmapemu_mmap;

	if ( mmap == 0 )
	{
		OP_ASSERT(g_mem_mmapemu_mseg == 0);
		size_t minimum = MEMORY_OPERA_MMAP_SIZE;
		OpMemoryClass type = MEMCLS_OP_MMAP;

#ifdef MEMORY_USE_LOCKING
		g_mem_mmapemu_mseg = OpMemory_CreateVirtualSegment(minimum, FALSE);
#else
		g_mem_mmapemu_mseg = OpMemory_CreateVirtualSegment(minimum);
#endif

		if ( g_mem_mmapemu_mseg != 0 )
		{
#ifdef MEMORY_USE_LOCKING
			mmap = OpMmapSegment::Create(g_mem_mmapemu_mseg, type,
										 MEMCLS_MMAP_UNUSED, FALSE);
#else
			mmap = OpMmapSegment::Create(g_mem_mmapemu_mseg, type,
										 MEMCLS_MMAP_UNUSED);
#endif
			g_mem_mmapemu_mmap = mmap;
			if ( mmap == 0 )
			{
#ifdef MEMORY_USE_LOCKING
				OpMemory_DestroySegment(g_mem_mmapemu_mseg, FALSE);
#else
				OpMemory_DestroySegment(g_mem_mmapemu_mseg);
#endif
				g_mem_mmapemu_mseg = 0;
			}
		}
	}

	void* rc = (void*)-1;

	if ( mmap != 0 )
	{
#ifdef MEMORY_USE_LOCKING
		//
		// op_mmap() must always be called with OpMemory::MallocLock() held,
		// so we don't have to ask the 'mmap' method to acquire it also:
		//
		void* alloc = mmap->mmap(size, MEMCLS_OP_MMAP, FALSE);
#else
		//
		// No memory locking is needed, so there is no argument to
		// not use it:
		//
		void* alloc = mmap->mmap(size, MEMCLS_OP_MMAP);
#endif

		if ( alloc  != 0 )
			rc = alloc;
	}

	return rc;
}

extern "C" int op_munmap(void* ptr, size_t size)
{
	OpMmapSegment* mmap = g_mem_mmapemu_mmap;
	OP_ASSERT(mmap != 0);
	OP_ASSERT(mmap->SizeofAlloc(ptr) == size);

#ifdef MEMORY_USE_LOCKING
	mmap->munmap(ptr, FALSE);
#else
	mmap->munmap(ptr);
#endif

	return 0;
}

#endif // ENABLE_OPERA_MMAP

#if defined(DEBUG_ENABLE_OPASSERT) || defined(SELFTEST)
//
// Helper function for OP_ASSERT on correct munmap() size
//
size_t OpMmapSegment::SizeofAlloc(void* ptr)
{
	OP_ASSERT(ptr != 0);

	char* cptr = (char*)ptr;
	OP_ASSERT(address_first_usable <= cptr);
	OP_ASSERT(cptr < address_last_usable);
	OP_ASSERT((((UINTPTR)ptr) & (pagesize - 1)) == 0); // Page aligned
	unsigned int idx = (cptr - base) >> pagesize_shift;
	OP_ASSERT(page_handle[idx].flag == OP_MMAP_FLAG_ALLOCATED);

	return page_handle[idx].size * pagesize;
}

#endif // DEBUG_ENABLE_OPASSERT || SELFTEST

#ifdef SELFTEST
//
// In order to facilitate better selftests, there are a number of
// support and consistency checking functions that only gets included
// in selftest enabled builds.
//

unsigned int OpMmapSegment::TestComputeSizeClass(unsigned int pages)
{
	UINT16 arg = pages;
	return ComputeSizeClass(arg);
}

unsigned int OpMmapSegment::TestComputeUsablePages(unsigned int pages,
												   size_t pagesize)
{
	return ComputeUsablePages(pages, pagesize);
}

void* OpMmapSegment::Test_mmap(size_t size, OpMemoryClass type)
{
	void* ptr = mmap(size, type);

	if ( ptr != 0 )
	{
		char* addr = (char*)ptr;
		char* end = addr + size;
		while ( addr < end )
		{
			*(void**)addr = ptr;
			addr += sizeof(void*);
		}
	}

	return ptr;
}

BOOL OpMmapSegment::Test_munmap(void* ptr)
{
	size_t size = SizeofAlloc(ptr);

	char* addr = (char*)ptr;
	char* end = addr + size;
	while ( addr < end )
	{
		if ( *(void**)addr != ptr )
			return FALSE;
		addr += sizeof(void*);
	}

	munmap(ptr);

	return TRUE;
}

//
// This method is also used by the selftests only; it inspects the current
// set of handles, sorts them on address, and compares their type and size
// with those given to the function. An internal consistency test is also
// performed.
//
// The return value is 0 when all is well, if not, the return value is
// the argument number that fails verification.
//
// The arguments are paired, flag value first, then the size. The argument
// list is terminated with a '0', example:
//
//   TestState(OP_MMAP_FLAG_ALLOCATED, 10, OP_MMAP_FLAG_UNUSED, 20,
//             OP_MMAP_FLAG_RESERVED, 8130, 0);
//
// This would return '4' if the second block is indeed unused, but its
// size (20) is incorrect.
//
// If the internal state is corrupted, a negative value is returned.
//
// Note: There is a max limit of 64 on the number of blocks that can
// be tested with this method.
//
int OpMmapSegment::TestState(int flag, ...)
{
	va_list args;

	UINT16 expected[2*OP_MMAP_MAX_TESTABLE_BLOCKS+4];

	int rc = TestCollectBlockinfo(); // Populate blockinfo array

	if ( rc == 0 && flag != 0 )
	{
		// No internal state corruption

		expected[0] = flag;

		va_start(args, flag);
		for ( int k = 1; k < 2*OP_MMAP_MAX_TESTABLE_BLOCKS; k++ )
		{
			UINT16 val = va_arg(args, int);
			expected[k] = val;
			expected[k + 1] = 0;
			if ( val == 0 )
				break;
		}
		va_end(args);

		for ( int m = 0; m < 2*OP_MMAP_MAX_TESTABLE_BLOCKS + 2; m++ )
		{
			if ( expected[m] != blockinfo[m] )
			{
				rc = m + 1;
				break;
			}

			if ( expected[m] == 0 )
				break;
		}
	}

	return rc;
}

int OpMmapSegment::Corruption(int id)
{
	return id;
}

int OpMmapSegment::TestCollectBlockinfo(void)
{
	unsigned int k;
	unsigned int idx;

	//
	// First make a consistency check of the doubly linked lists that
	// holds all unused/reserved blocks.
	//
	for ( k = 0; k < OP_MMAP_ANCHOR_COUNT; k++ )
	{
		idx = k;

		// Decide characteristics of this chain of blocks
		unsigned size_class = k % OP_MMAP_SIZECLASSES_COUNT;
		UINT16 flag = OP_MMAP_FLAG_UNUSED;
		if ( k >= OP_MMAP_RESERVED_SIZECLASS )
			flag = OP_MMAP_FLAG_RESERVED;

		for (;;)
		{
			// This loop can't run forever, since an infinite loop
			// that does not include an anchor will at some point
			// have an inconsistency where its next and previous
			// pointers will be inconsistent. And there is only a
			// limited number of handles, so there is an upper bound
			// on how many there can possibly be.

			UINT16 next = page_handle[idx].next;
			UINT16 size = page_handle[idx].size;

			if ( idx > OP_MMAP_ANCHOR_COUNT )
			{
				// Not an anchor - check if its size-class matches that of
				// the chain it is in
				unsigned int cls = ComputeSizeClass(size);
				OP_ASSERT(cls == size_class);
				if ( cls != size_class )
					return Corruption(-100);

				// Check that the block's type is correct
				OP_ASSERT(page_handle[idx].flag == flag);
				if ( page_handle[idx].flag != flag )
					return Corruption(-101);
			}

			OP_ASSERT(page_handle[next].prev == idx);
			if ( page_handle[next].prev != idx )
				return Corruption(-102);

			if ( next < OP_MMAP_ANCHOR_COUNT )
			{
				OP_ASSERT(next == k); // Back where we started
				if ( next != k )
					return Corruption(-103);
				break;
			}

			idx = next;
		}
	}

	//
	// Go through the blocks, record them, and do some additional checks
	//
	k = 0;
	idx = OP_MMAP_ANCHOR_COUNT + 1; // First usable handle

	unsigned int calculated_allocated_pages = 0;
	unsigned int calculated_unused_pages = 0;

	blockinfo[0] = 0;

	for (;;)
	{
		UINT16 size = page_handle[idx].size;
		UINT16 flag = page_handle[idx].flag;

		//
		// Loop until we run into the high-sentinel which marks the end
		// of the handles.
		//
		if ( flag == OP_MMAP_FLAG_SENTINEL )
			break;

		//
		// Check some basic characteristics of the block
		//
		OP_ASSERT(size > 0);
		if ( size == 0 )
			return Corruption(-200);

		OP_ASSERT(page_handle[idx + size - 1].size == size);
		if ( page_handle[idx + size - 1].size != size )
			return Corruption(-201);

		OP_ASSERT(page_handle[idx + size - 1].flag == flag);
		if ( page_handle[idx + size - 1].flag != flag )
			return Corruption(-202);

		//
		// Record how much is allocated and unused, and verify the
		// type of the block.
		//
		switch ( flag )
		{
		case OP_MMAP_FLAG_ALLOCATED:
			calculated_allocated_pages += size;
			break;

		case OP_MMAP_FLAG_UNUSED:
			calculated_unused_pages += size;
			break;

		case OP_MMAP_FLAG_RESERVED:
			// This is an acceptable block, but it is not counted
			break;

		default:
			OP_ASSERT(!"Invalid flag for block");
			return Corruption(-203);
		}

		//
		// Determine that the block is linked into the anchors correctly
		// if it is not an allocated block (which is not anchored).
		//
		if ( flag != OP_MMAP_FLAG_ALLOCATED )
		{
			UINT16 walk = idx;

			for ( int m = 0; m < 0x10000; m++ )
			{
				UINT16 next = page_handle[walk].next;

				// Make sure the next handle points back at us
				OP_ASSERT(page_handle[next].prev == walk);
				if ( page_handle[next].prev != walk )
					return Corruption(-300);

				walk = next;

				// Terminate loop when we reach an anchor
				if ( walk < OP_MMAP_ANCHOR_COUNT )
					goto anchored_ok;
			}

			OP_ASSERT(!"Next-chain seems to be looping for block");
			Corruption(-301);

		anchored_ok: ;
		}

		if ( k < (2*OP_MMAP_MAX_TESTABLE_BLOCKS - 4) )
		{
			blockinfo[k++] = flag;
			blockinfo[k++] = size;
			blockinfo[k] = 0;
		}

		idx += size;
	}

	OP_ASSERT(calculated_allocated_pages == allocated_pages);
	if ( calculated_allocated_pages != allocated_pages )
		return Corruption(-400);

	OP_ASSERT(calculated_unused_pages == unused_pages);
	if ( calculated_unused_pages != unused_pages )
		return Corruption(-401);

	return 0;
}

#endif // SELFTEST

#endif // ENABLE_OPERA_MMAP_SEGMENT

#if defined(ENABLE_OPERA_MMAP) || defined(ENABLE_OPERA_SBRK)

void op_sbrk_and_mmap_destroy(void)
{
#ifdef ENABLE_OPERA_MMAP
	if ( g_mem_mmapemu_mmap != 0 )
		g_mem_mmapemu_mmap->ForceReleaseAll();
	if ( g_mem_mmapemu_mseg != 0 )
		OpMemory_DestroySegment(g_mem_mmapemu_mseg);
	g_mem_mmapemu_mseg = 0;
	g_mem_mmapemu_mseg = 0;
#endif // ENABLE_OPERA_MMAP

#ifdef ENABLE_OPERA_SBRK
	if ( g_mem_sbrk_heap != 0 )
		OpHeapSegment::Destroy(g_mem_sbrk_heap);
	if ( g_mem_sbrk_mseg != 0 )
		OpMemory_DestroySegment(g_mem_sbrk_mseg);
	g_mem_sbrk_heap = 0;
	g_mem_sbrk_mseg = 0;
#endif
}

#endif // ENABLE_OPERA_MMAP || ENABLE_OPERA_SBRK
