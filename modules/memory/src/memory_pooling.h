/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Include file containing memory pooling functionality.
 * 
 * The pooling classes OpMemPool and OpMemPoolManager and related support
 * structures are declared in this file.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_POOLING_H
#define MEMORY_POOLING_H

#ifdef USE_POOLING_MALLOC

/*
 * Upper bound on the maximum size of an object that can be pooled.
 * It is directly linked to static overhead of pooling code.
 */
#define MEMORY_POOL_MAX_SIZE 2048

/*
 * Derived constants and misc. definitions.
 */
#define MEMORY_POOL_COUNT (MEMORY_POOL_MAX_SIZE / MEMORY_ALIGNMENT)
#define MEMORY_POOL_ALLOC_TYPES 3

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_smo.h"

class OpCoreSegment;
class OpMemPoolManager;

extern "C" OpMemPoolManager* g_mem_pools_ptr;

/**
 * \brief Elementary chain construction used in SMO pools.
 */
struct OpMemPoolLink
{
	short unsigned int next;
};

/*
 * \brief Doubly linked construction for SMOPool lists.
 */
struct OpMemLink
{
	OpMemLink();
	void Out();

	OpMemLink* next;
	OpMemLink* prev;
};

/**
 * \brief List identifiers for OpMemPool objects.
 *
 * NONE marks an object whose circular list is a singleton.
 * FREE signifies the list of unallocated pools.
 * AVAILABLE signifies the list of pools with available space.
 * FULL signifies the list of pools with no available space.
 */
enum OpMemPoolList
{
	MEMPOOL_LIST_NONE,
	MEMPOOL_LIST_FREE,
	MEMPOOL_LIST_AVAILABLE,
	MEMPOOL_LIST_FULL
};

/**
 * \brief Allocation type identifiers.
 *
 * TEMPORARY is meant for short-lived allocations.
 * PERSISTENT are those that last for many a page load.
 * NORMAL denote those that fall in between.
 */
enum OpMemPoolAlloc
{
	MEMPOOL_ALLOC_TEMPORARY = OpMemory::MEMORY_SEGMENT_TEMPORARY,
	MEMPOOL_ALLOC_DOCUMENT = OpMemory::MEMORY_SEGMENT_DOCUMENT,
	MEMPOOL_ALLOC_PERSISTENT = OpMemory::MEMORY_SEGMENT_PERSISTENT
};

/**
 * \brief Alignment notes for SMO variants of OpMemPool.
 *
 * FULL indicates a fully aligned block of MEMORY_BLOCKSIZE 
 *      with pointers at the start and end of the block.
 * LOWER indicates that there exists a pointer only at the next
 *       align mark from where the pool space begins.
 * UPPER indicatates that there exists a pointer only at the start
 *       of the pool space.
 */
enum OpMemPoolAlignment
{
	MEMPOOL_ALIGN_FULL,
	MEMPOOL_ALIGN_LOWER,
	MEMPOOL_ALIGN_UPPER
};

/**
 * \brief Type of OpMemPool.
 */
enum OpMemPoolType
{
	MEMPOOL_UNALLOCATED,	//< This pool is unavailable
	MEMPOOL_FREE,			//< This pool is available and unused
	MEMPOOL_SMO,			//< This pool is used for SMOPOOL
#ifdef MEMORY_ELOPOOL_ALLOCATOR
	MEMPOOL_ELO,			//< This pool is used for ELOPOOL
#endif
	MEMPOOL_BLK,			//< This pool is used for BLKPOOL
	MEMPOOL_MAN				//< This pool is used for MANPOOL
};

/**
 * \brief Manager object for a single pool
 *
 * This class would have been suitable as a base class with different
 * implementations for the various allocators, but we want to allocate
 * these objects in an array, so we need to have all the functionality
 * in a single object.
 *
 * The declaration and implementation of the various pool allocators
 * themselves are done in different files for clarity.
 */
class OpMemPool : public OpMemLink
{
public:
	/**
	 * \brief Initialize the pool
	 *
	 * All pools are initialized to the \e unallocated state.  Before
	 * the pool can be used, a \c BLKPOOL needs to be allocated first
	 * for this pool to use.
	 *
	 * \param address Address of memory to use.
	 */
	OpMemPool(char* address = 0, size_t size = 0,
			  OpMemPoolAlignment alignment = MEMPOOL_ALIGN_FULL,
			  OpCoreSegment* segment = 0);
		
	/*
	 * \brief Shut down the pool
	 *
	 * For performance reasons, nothing is done on shutdown, except
	 * possibly using \c OP_ASSERT to notify of pools that are not
	 * empty.
	 */
	~OpMemPool(void);

	/**
	 * \brief Initialize a pool for use by SMOPOOL allocator.
	 *
	 * This method needs to be called on a pool before any
	 * SMOPOOL allocations can be made from the pool.
	 *
	 * The pool needs to be free when this method is called.
	 *
	 * \param size The (preconditioned) object size for the pool in bytes
	 */
	void InitSMO(size_t size);

	/**
	 * \brief Allocate an object from an SMOPOOL.
	 *
	 * This function assumes the pool is either free (type == MEMPOOL_FREE),
	 * or is allready an SMOPOOL contining correct sized objects.
	 */
	void* AllocSMO(void);

	/**
	 * \brief Deallocate an object from an SMOPOOL.
	 *
	 * This function assumes the pool is of type MEMPOOL_FREE, and places
	 * the object to be deallocated on the freelist of the pool.
	 *
	 * \param ptr Pointer to the object to be freed
	 */
	void FreeSMO(void* ptr);

	/**
	 * \brief Get the allocation size for an SMOPOOL.
	 *
	 * All allocations of an SMOPOOL are of the same size.  This method
	 * can be used to query the size of an existing SMOPOOL.
	 */
	size_t GetSizeSMO(void)
	{
		// OP_ASSERT(type == MEMPOOL_SMO); // FIXME: Needs debug_fundamentals.h
		return data.smo.size;
	}

#ifdef MEMORY_ELOPOOL_ALLOCATOR
	/**
	 * \brief Unlock an ELOPOOL from its owning OpMemLife handle
	 *
	 * This method is called when the bond between the governing
	 * OpMemLife class and the ELOPOOL is broken.  This bond is
	 * created when a pool is allocated for use by the OpMemLife
	 * handle, and is broken (Unlocked) when the pool is no longer
	 * needed (because it has filled up), or because the OpMemLife
	 * handle is destroyed.
	 *
	 * The ELOPOOL can't be released for other uses until it is
	 * both unlocked, and all objects in it has been released.
	 *
	 * Once the ELOPOOL is unlocked, it can't be allocated from
	 * until it gets reassigned (this to prevent allocations of
	 * objects with different life expectancy).
	 */
	void UnlockELO(void);

	/**
	 * \brief Check if an ELOPOOL is still locked
	 *
	 * Check if the UnlockELO() method has been called on this ELOPOOL
	 * pool.  If not, the pool can't be released for other uses.
	 *
	 * \returns Nonzero if ELOPOOL is still locked
	 */
	BOOL IsLockedELO(void);

	void InitELO();
	void* AllocELO(size_t size, unsigned int alignment);
	void FreeELO(void);
#endif // MEMORY_ELOPOOL_ALLOCATOR

	/**
	 * \brief Move pool to an enumerated list.
	 */
	void MoveTo(OpMemPoolList list);

	/**
	 * \brief Free pool.
	 */
	void Free(void);

	/**
	 * \brief Misc accessors.
	 */
	inline OpMemPool* GetNext() { return static_cast<OpMemPool*>(next); }
	inline OpMemPool* GetPrev() { return static_cast<OpMemPool*>(prev); }

	OpMemPoolList current_list;
	OpCoreSegment* segment;

private:
	friend class OpMemPoolManager;
	friend class OpMemLife;

	OpMemPoolType type;
	OpMemPoolAlignment align;
	char* address;
	size_t size;

	union
	{
		OpMemPoolSMO smo;		//< For use by the \c SMOPOOL allocator

#ifdef MEMORY_ELOPOOL_ALLOCATOR
		OpMemPoolELO elo;		//< For use by \c ELOPOOL and \c ELSPOOL
#endif
	} data;
};

/**
 * \brief Manager class for a collection of OpMemPool resources
 *
 * The \c OpMemPoolManager class contains the interfaces to the pooling
 * allocators, and holds resources related to overall pooling.
 */
class OpMemPoolManager
{
public:
	OpMemPoolManager(void);
	~OpMemPoolManager(void);

	/**
	 * \name Raw memory block allocations
	 *
	 * \brief Management of memory blocks of fixed size MEMORY_BLOCKSIZE.
	 *
	 * This interface is intended for Opera modules which manages their
	 * own memory through memory blocks of fixed size.  This interface
	 * is most efficient for modules that (may) consume a lot of memory
	 * and are prepared to do internal memory management inside the
	 * memory blocks to make more efficient usage of available memory.
	 *
	 * Only a single block can be allocated at a time, so this interface
	 * can't be used if a larger (contigous) block of memory is needed.
	 *
	 * @{
	 */

	/**
	 * \brief Allocate a single fixed sized block of memory
	 *
	 * This method will allocate a single block of memory.  The size will
	 * be MEMORY_BLOCKSIZE bytes.
	 *
	 * \returns Address of allocated block, or NULL on failure.
	 */
	void* AllocBLK(void);

	/**
	 * \brief Free a memory block previously allocated with AllocBLK
	 *
	 * The specified memory block will be released and made available for
	 * reuse.  Freeing a NULL pointer is a safe action that does nothing.
	 *
	 * \param ptr Ponter to the block of memory to be released
	 */
	void FreeBLK(void* ptr);

	/** @} */

	static void* AllocSMO(size_t size, OpMemPoolAlloc type = MEMPOOL_ALLOC_DOCUMENT);
	static void* AllocSMO_L(size_t size, OpMemPoolAlloc type = MEMPOOL_ALLOC_DOCUMENT);

	void* AllocELO(size_t size);
	void* AllocELO_L(size_t size);

	static void Free(void* ptr);
	static unsigned int GetSegmentID(void* ptr);
	static OpMemPool* LocatePool(void* ptr);

	/**
	 * \brief Misc accessors.
	 */
	OpMemPool* FirstAvailablePool(size_t size, OpMemPoolAlloc type = MEMPOOL_ALLOC_DOCUMENT);
	OpMemPool* FirstFreePool(size_t size = 0, OpMemPoolAlloc type = MEMPOOL_ALLOC_DOCUMENT);
	OpMemPool* NextPool();

private:
	friend class OpMemPool;

	OpMemLink* available_pools[MEMORY_POOL_ALLOC_TYPES][MEMORY_POOL_COUNT];
	OpMemLink* full_pools[MEMORY_POOL_ALLOC_TYPES][MEMORY_POOL_COUNT];
	OpMemLink* free_pools[MEMORY_POOL_ALLOC_TYPES];

	/* Iterator */
	OpMemPool* current_pool;
	OpMemLink* current_head;
};

#endif // USE_POOLING_MALLOC

/**
 * \brief Life expectancy handle for memory allocations
 *
 * For all ELO or ELS allocations, a handle is needed to help
 * place objects of equal life expectancy together.  The handle is simly
 * an instance of OpMemLifetime, where all objects allocated with the
 * same instance is expected to live equally long.
 *
 * For code that has objects with different life expectancies, several
 * different OpMemLifetime handles should be used; one for each expected
 * lifetime.
 *
 * A single handle can be re-assigned to a new life-expectancy when a
 * new "batch" of allocations start (with a different life expectancy
 * from the previous batch).
 */
class OpMemLife
{
public:
	OpMemLife(void);
	~OpMemLife(void);

	/**
	 * \brief Allocator for equal lifetime objects
	 *
	 * Allocate an object for this given lifetime object.  This allocator
	 * takes an additional alignment argument, so strings with relaxed
	 * alignment restrictions can have a denser packing.
	 *
	 * \param size Size of allocation in bytes
	 * \param alignment Required alignment in bytes minus one
	 * \returns Pointer to allocated memory, or NULL
	 */
	void* IntAllocELO(size_t size, unsigned int alignment);

	/**
	 * \brief Allocator for equal lifetime objects with leave
	 *
	 * Same as \c IntAllocELO() but leaves on failure to allocate.
	 *
	 * \param size Size of allocation in bytes
	 * \param alignment Required alignment in bytes minus one
	 * \returns Pointer to allocated memory, or LEAVEs on failure
	 */
	void* IntAllocELO_L(size_t size, unsigned int alignment);

private:
#ifdef USE_POOLING_MALLOC
	OpMemPool* pool_a;	
	OpMemPool* pool_b;
#endif
};

#endif // MEMORY_POOLING_H
