/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare internal structures for mmap
 *
 * This functionality is used e.g. by lea_malloc to simplify porting
 * to a platform where virtual memory is available and implemented. It
 * may also be used for other things.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_MMAP_H
#define MEMORY_MMAP_H

#ifdef ENABLE_OPERA_MMAP_SEGMENT

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_state.h"
#include "modules/memory/src/memory_manager.h"

/**
 * \brief Structure to identify a single memory page.
 *
 * This structure is allocated in an array, so that it covers all
 * memory pages of an \c OpMmapSegment. There are a numbe of
 * "administrative handles" in the array:
 *
 * Anchors for reserved memory blocks, and anchors for unused
 * memory blocks
 *
 * The anchors are the "head" for doubly linked lists of various
 * size classes for each of allocated and reserved memory blocks.
 *
 * Reserved memory blocks are those that does not (yet) hold any
 * memory, and is thus inexpensive from a memory consumption
 * perspective.
 *
 * Unused memory blocks holds physical memory, and can thus be
 * used, but they are currently not allocated. Threshold limiters
 * will keep the ammount of unused memory blocks below certain
 * limits.
 *
 * After the ancors, there is a sentinel handle, before the first
 * handle that has an associated memory page attached to it.
 * A second sentinel is placed after all the handles. There is
 * typically 56 anchors (2 * 28 size classes) and the two sentinels
 * (See \c OP_MMAP_SIZECLASSES_COUNT and \c OP_MMAP_ANCHOR_COUNT).
 *
 * Handles will be unused when the mmap'ed allocation is two or more
 * memory pages in size, but the benefit is very quick lookup and
 * manipulation of free-lists and consolidation. Another benefit is
 * that no meta-data is allocated alongside the allocation itself,
 * which makes it possible to allocate e.g. a full 16K block that can
 * be given back entirely (except the handle overhead).
 *
 * The overhead is 8 bytes per memory page, which is 0.2% overhead
 * with a memory page size of 4K.
 */
struct OpMemPageHandle
{
	/**
	 * \brief Size in number of page blocks
	 *
	 * This allocated or reserved block of memory consists of \c size
	 * number of memory pages. This is a value between 1 and the
	 * maximum of slightly less than 65535 (16-bit value, minus the
	 * anchors and sentinels).
	 */
	UINT16 size;

	/**
	 * \brief Flags identifying this handle
	 *
	 * The state of the page handle is encoded in this variable.
	 * The state is one of: \c OP_MMAP_FLAG_ALLOCATED,
	 * \c OP_MMAP_FLAG_UNUSED, \c OP_MMAP_FLAG_RESERVED and
	 * \c OP_MMAP_FLAG_SENTINEL.
	 */
	UINT8  flag;

	/**
	 * \brief Memory allocation class type (enum OpMemoryClass)
	 *
	 * The allocation type class is encoded into the allocation.
	 * This type info is used for accounting purposes, and upon
	 * deallocation, the corresponding allocation class type
	 * is decremented with the size of the deallocation.
	 */
	UINT8  type;

	/**
	 * \brief The index of the previous page handle
	 *
	 * The first page handle of all blocks of reserved or unused
	 * memory are linked into doubly linked lists. The \c prev
	 * variable holds the index of the page handle in the list
	 * previous to this one (of the same size-class).
	 */
	UINT16 prev;

	/**
	 * \brief The index of the next page handle
	 *
	 * The first page handle of all blocks of reserved or unused
	 * memory are linked into doubly linked lists. The \c next
	 * variable holds the index of the page handle in the list
	 * that follows this one (of the same size-class).
	 */
	UINT16 next;
};

/**
 * \name Page allocation types
 *
 * The following page allocation types are used to define the
 * layout and status of the memory pages in the OpMmapSegment.
 *
 * @{
 */

/**
 * \brief Allocated and used memory
 *
 * The page(s) are allocated and handed out for use. The memory is
 * being used for holding data.
 */
#define OP_MMAP_FLAG_ALLOCATED   1

/**
 * \brief Unused memory
 *
 * The page(s) are obtained from the Operating System, and are
 * present (so they can hold data right away if needed), but is
 * currently not used for holding data.
 *
 * This typically happens when an allocation is released, but it
 * is not given back to the OS right away in case it will be needed
 * again soon. A threshold determines how much unused memory can
 * stay around before given back to the OS.
 */
#define OP_MMAP_FLAG_UNUSED      2

/**
 * \brief Reserved memory
 *
 * This is the initial state which signals that there is no memory
 * associated with the page handle. It is thus not possible to use
 * the memory for holding data.
 *
 * The Operating System will have to be asked to populate the memory
 * page before it can be used to hold data.
 */
#define OP_MMAP_FLAG_RESERVED    3

/**
 * \brief Sentinel handle for simplifying fencepost logic
 *
 * There is a sentinel page-handle before and after all
 * usable page-handles, to simplify merging of memory blocks
 * during memory release and consolidation operations.
 *
 * The sentinel will not join with any other page handle type,
 * and thus prevent joining at the upper and lower borders.
 */
#define OP_MMAP_FLAG_SENTINEL    4

/** @} */

/**
 * \brief The number of size-classes for mmap allocations
 *
 * When administrating the free-lists of the memory blocks used to
 * provide 'mmap' functionality, there are 29 size classes.  These
 * classes are 0-16 (1 to 17 memory pages, which is 4-68K in 4K
 * steps when the pagesize is 4K), and 17-28 (18 to 65536 pages in
 * increasing powers of two, which is 68K+1 to 256M when the pagesize
 * is 4K).
 *
 * Larger size-classes are not needed, since 16-bit page-indexing will
 * never provide more than 65536 accessible memory pages, minus the
 * anchors. Slightly less than 256M is thus the maximum OpMmapSegment
 * size with 4K memory pages.
 */
#define OP_MMAP_SIZECLASSES_COUNT 29

/**
 * \brief The total ammount of page-handle anchors
 *
 * The anchors are only used for "anchoring" a doubly linked list,
 * and does not have any associated memory page. The anchors are
 * located in the \c page_handle array, from index 0 onwards.
 */
#define OP_MMAP_ANCHOR_COUNT (2*OP_MMAP_SIZECLASSES_COUNT)

/**
 * \brief The UNUSED memory pages anchor index base
 *
 * The memory pages with unused status are located in the size-classes
 * from \c OP_MMAP_UNUSED_SIZECLASS and onwards in the page_handle
 * array <code>page_handle[OP_MMAP_UNUSED_SIZECLASS]</code> to
 * <code>page_handle[OP_MMAP_UNUSED_SIZECLASS +
 * OP_MMAP_SIZECLASSES_COUNT - 1]</code>
 */
#define OP_MMAP_UNUSED_SIZECLASS 0

/**
 * \brief The RESERVED memory pages anchor index base
 *
 * The memory pages with reserved status are located in the size-classes
 * from \c OP_MMAP_RESERVED_SIZECLASS and onwards in the page_handle
 * array <code>page_handle[OP_MMAP_RESERVED_SIZECLASS]</code> to
 * <code>page_handle[OP_MMAP_RESERVED_SIZECLASS +
 * OP_MMAP_SIZECLASSES_COUNT - 1]</code>
 */
#define OP_MMAP_RESERVED_SIZECLASS OP_MMAP_SIZECLASSES_COUNT

#ifdef SELFTEST
#define OP_MMAP_MAX_TESTABLE_BLOCKS 64
#endif

/**
 * \brief Class to administrate the mappings of a virtual memory segment
 *
 * This class is responsible for keeping track of MMAP-style allocations
 * inside the virtual OpMemSegment provided during construction.
 *
 */
class OpMmapSegment
{
public:

#ifdef MEMORY_USE_LOCKING
	/**
	 * \brief Create an OpMmapSegment from an OpMemSegment
	 *
	 * The only way to create an \c OpMmapSegment is to have a virtual
	 * \c OpMemory::OpMemSegment created by \c
	 * OpMemory_CreateVirtualSegment() (which in turn calls the \c
	 * OpMemory::CreateVirtualSegment() porting interface to get the
	 * job done).
	 *
	 * \b Note: The memory segment \c mseg must not be used for
	 * anything else.
	 *
	 * \b Note: This method does only have the \c memlock parameter
	 * for builds where \c MEMORY_USE_LOCKING is defined, so any code
	 * that calls this method should either not specify the memlock
	 * parameter (and use locking if enabled through the default TRUE
	 * value), or test on \c MEMORY_USE_LOCKING to provide \c FALSE
	 * for the \c memlock parameter when memory locking is enabled.
	 *
	 * \b Note: The \c memlock parameter should \b only be specified
	 * as FALSE when the memory-lock is already held, for instance
	 * when the call-chain goes through lea_malloc, which grabs this
	 * lock for itself.
	 *
	 * \param mseg A virtual memory segment
	 * \param hdr The memory accounting class for headers/metadata
	 * \param unused The memory accounting class for unused memory
	 * \param memlock Call OpMemory::MallocLock()/Unlock() when TRUE
	 * \return A OpMmapSegment or NULL
	 */
	static OpMmapSegment* Create(const OpMemory::OpMemSegment* mseg,
								 OpMemoryClass hdr = MEMCLS_MMAP_HEADER,
								 OpMemoryClass unused = MEMCLS_MMAP_UNUSED,
								 BOOL memlock = TRUE);

	/**
	 * \brief Allocate memory in an OpMmapSegment
	 *
	 * Allocate a number of memory pages in an \c OpMmapSegment. The
	 * allocation will be placed on a memory page boundary.  The size
	 * will be rounded up to the nearest page size to accomodate the
	 * allocation.
	 *
	 * \b Note: The \c memlock parameter must be treated like
	 * described for \c OpMmapSegment::Create().
	 *
	 * \param size The number of bytes to be allocated
	 * \param type The memory type class for allocation accounting purposes
	 * \param memlock Call OpMemory::MallocLock()/Unlock() when TRUE
	 * \return Pointer to allocated memory or NULL
	 */
	void* mmap(size_t size, OpMemoryClass type, BOOL memlock = TRUE);

	/**
	 * \brief Deallocate memory in an OpMmapSegment
	 *
	 * Deallocate a previous allocation. The \c ptr parameter must be
	 * the non-NULL result of a previous call to
	 * OpMmapSegment::mmap().
	 *
	 * \b Note: The \c memlock parameter must be treated like
	 * described for \c OpMmapSegment::Create().
	 *
	 * \b Note: This API differs from the typical UNIX 'mmap' API in
	 * that deallocations does not have to specify the size of the
	 * region to be deallocated. This API is thus more of an
	 * allocator, than a memory page administrator API. If
	 * administrating individual memory pages is needed, the porting
	 * interface in OpMemory can be used directly (Or preferably the thin
	 * layer in \c modules/memory/src/memory_pilayer.h)
	 *
	 * \param ptr Pointer to allocation
	 * \param memlock Call OpMemory::MallocLock()/Unlock() when TRUE
	 */
	void  munmap(void* ptr, BOOL memlock = TRUE);

	/**
	 * \brief Release all unused memory from an OpMmapSegment
	 *
	 * Unused memory is memory that is present physically, awaiting
	 * possible reuse or release back to the Operating System
	 * (Depending on thresholds and usage pattern).
	 *
	 * This method will release all unused memory regardless of
	 * any thresholds. This method can be used when we want to
	 * be nice and give back as much as possible, or when we expect
	 * no large quantities of allocations to happen for a while.
	 */
	void ReleaseAllUnused(BOOL memlock = TRUE);

#else
	static OpMmapSegment* Create(const OpMemory::OpMemSegment* mseg,
								 OpMemoryClass hdr = MEMCLS_MMAP_HEADER,
								 OpMemoryClass unused = MEMCLS_MMAP_UNUSED);
	void* mmap(size_t size, OpMemoryClass type);
	void  munmap(void* ptr);
	void ReleaseAllUnused(void);
#endif

	/**
	 * \brief Brutally release all memory allocated in OpMmapSegment
	 *
	 * If, for some reason, an OpMmapSegment needs to be "cleaned"
	 * of all its allocations (possibly for reuse), this method
	 * will do just that.
	 *
	 * \b Note: Any lingering allocations would be forcibly deallocated,
	 * causing memory access errors until the page (by chance) was
	 * reallocated again, at which point a corruption is more likely.
	 *
	 * So, don't use this method unless you know what you are doing,
	 * like when using OpMmapSegment for "Arena" type of allocations
	 * (where bulk releasing is needed at the end).
	 */
	void ForceReleaseAll(void);

	/**
	 * \brief Set the threshold for how much can be cached when deallocating
	 *
	 * Whenever a deallocation is performed, the deallocated memory is
	 * kept around in case it is needed again soon. The ammount of memory
	 * to be kept around is controlled through the unused threshold
	 * setting, which can be changed through this method.
	 *
	 * Setting it to '0' will cause all released memory to be freed back
	 * to the operating system immediately.
	 *
	 * \param pgs The maximum number of memory pages to keep around
	 */
	void SetUnusedThreshold(unsigned int pgs) { unused_pages_threshold = pgs; }

	/**
	 * \brief Get the underlying memory segment handle
	 *
	 * Can be useful at times.
	 */
	const OpMemory::OpMemSegment* GetOpMemSegment(void) { return mseg; }

	/**
	 * \brief Check if an allocation is from within this segment
	 */
	BOOL Contains(void* ptr)
	{
		return (char*)ptr >= address_first_usable
			&& (char*)ptr < address_last_usable;
	}

#if defined(DEBUG_ENABLE_OPASSERT) || defined(SELFTEST)
	size_t SizeofAlloc(void* ptr);
#endif

#ifdef SELFTEST
	void* Test_mmap(size_t size, OpMemoryClass type);
	BOOL  Test_munmap(void* ptr);

	void* TestGetAddressFirstUsable(void) { return address_first_usable; }
	void* TestGetAddressLastUsable(void) { return address_last_usable; }
	int TestGetUnusedPagesCount(void) { return unused_pages; }
	int TestGetUnusedThreshold(void) { return unused_pages_threshold; }
	int TestGetAllocatedPagesCount(void) { return allocated_pages; }
	OpMemPageHandle* TestGetPageHandle(UINT16 idx) { return page_handle + idx; }
	static unsigned int TestComputeSizeClass(unsigned int pages);
	static unsigned int TestComputeUsablePages(unsigned int pages,
											   size_t pagesize);

	int TestState(int flag, ...);
	static int Corruption(int id);
	int TestCollectBlockinfo(void);
#endif

private:
	OpMmapSegment(OpMemory::OpMemSegment* mseg);
	void Link(UINT16 anchor, UINT16 page);
	void Unlink(UINT16 page);
	void SetDetails(UINT16 page, UINT16 pages, UINT8 flags);
	BOOL AllocateHandles(UINT16 index);
	unsigned int CalculatePageOfHandle(UINT16 index);
	void* AllocateUnused(UINT16 idx, UINT16 pages, OpMemoryClass type);
	void* AllocateReserved(UINT16 idx, UINT16 pages, OpMemoryClass type);
	void Merge(UINT8 flag, UINT8 type, UINT16 idx);
	void ReleaseUnused(void);

	static unsigned int ComputeSizeClass(UINT16 pages);
	static unsigned int ComputeUsablePages(unsigned int pages, size_t pagesize);

	//
	// The variables. NOTE: The size of this class must be a
	// multiple of 8, as long as sizeof(OpMemPageHandle) == 8.
	//
	// If this is not the case, we run the risk of having the
	// last handle straddle a memory page boundary.
	//

	char* base; ///< Address of (imaginary) page whose handle-index == 0
	const OpMemory::OpMemSegment* mseg; ///< The underlying memory segment

	char* address_first_usable; ///< Address of first usable page
	char* address_last_usable; ///< First byte past the very last usable page

	void* address_upper_handles; ///< Address of first page with upper handles
#ifdef ENABLE_MEMORY_MANAGER
	OpMemoryManager* mm; ///< Cached pointer to memory manager
#else
	void* unused1; ///< Padding to get size to multiple of 8
#endif

	unsigned int pagesize; ///< Memory page size in bytes
	unsigned int pagesize_shift; ///< Cached log2(pagesize)

	unsigned int highest_index; ///< Highest indexed page seen so far (*)
	unsigned int header_pages; ///< Number of pages for header

	unsigned int header_max_pages; /// Size of dynamically allocatable header
	unsigned int size_upper_handles; ///< Size of upper handles

	unsigned int hdrtype; ///< Memory class type for header data only
	unsigned int unusedtype; ///< Memory class type for unused data

	unsigned int max_size_class; ///< Highest possible size-class
	unsigned int allocated_pages; ///< Number of allocated pages

	unsigned int unused_pages_threshold; ///< Limit unused pages
	unsigned int unused_pages; ///< Number of unused pages

	/**
	 * \brief Try keep track of the maximum sized block in a sizeclass
	 *
	 * This information is not always correct, but it can be used to
	 * improve searching performance. It will not be correct in that
	 * it may hold a too large number, being too optimistic about what
	 * allocations can be made. When this happens, a search will fail
	 * for the sizeclass. The maximum sizeed block info will be updated
	 * on a failed search, so future searches will have more accurate
	 * information.
	 *
	 * When a free operation makes more data available than what is
	 * currently logged, the max-size is updated. An allocation should
	 * thus never fail because of incorrect max-size information.
	 *
	 * \b Note: This array *must* be a multiple of 4 elements in size
	 * to satisfy the size requirements outlined above.
	 */
	UINT16 max_size[OP_MMAP_ANCHOR_COUNT];

#ifdef SELFTEST
	/**
	 * \brief Helper array used by the TestState() method
	 *
	 * The TestState() method will collect the individual blocks
	 * in the segment (regardless of type), and compare them
	 * with a given "original" as part of selftests.
	 *
	 * The blockinfo[] array holds the distilled information
	 * on the blocks while the TestState() metod is running.
	 */
	UINT16 blockinfo[2*OP_MMAP_MAX_TESTABLE_BLOCKS+4];
#endif

	/**
	 * \brief Padding to get sizeof(OpMmapSegment) % 8 == 0
	 *
	 * Each \c OpMemPageHandle in the \c page_handle needs to
	 * fit entirely inside a memory page, to simplify allocation
	 * logic.
	 *
	 * This padding makes sure this is always the case.
	 */
	UINT16 padding[2];

	/**
	 * \brief The array of OpMemPageHandle handles
	 *
	 * This array holds the first few handles of the OpMmapSegment.
	 * The handles are the ones used as "anchors" for the various
	 * allocation size classes (for both unused and reserved memory
	 * regions).
	 *
	 * The low-address sentinel will follow right after this class,
	 * and then the page handle for the first usable memory page will
	 * follow and then so on, until the page handle for the last
	 * usable memory page, and finally the high-address sentinel.
	 *
	 * \b Note: This array must be last in this class declaration
	 * since it will extend past the end of the contining class, and
	 * continue into the dynamically allocated memory in the virtual
	 * \c OpMemSegment.
	 */
	OpMemPageHandle page_handle[OP_MMAP_ANCHOR_COUNT]; ///< The handles
};

#if 0

// This is TBD

class OpMmapManager
{
public:
	OpMmapManager(void);

	void* mmap(size_t size, BOOL executable);
	void munmap(void* ptr);

private:
	OpMmapSegment* mmapseg;
};

#endif // 0

#endif // ENABLE_OPERA_MMAP_SEGMENT

#endif // MEMORY_MMAP_H
