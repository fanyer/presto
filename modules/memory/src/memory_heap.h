/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Declare internal structures for sbrk
 *
 * This functionality is used by e.g. lea_malloc to simplify porting
 * of lea_malloc to a platform where virtual memory is available and
 * implemented. It may also be used for other things.
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#ifndef MEMORY_HEAP_H
#define MEMORY_HEAP_H

#ifdef ENABLE_OPERA_HEAP_SEGMENT

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_manager.h"

/**
 * \brief A heap memory allocator
 *
 * The \c OpHeapSegment uses a virtual \c OpMemory::OpMemSegment to
 * implement a heap. A heap is a region of memory that stretches from
 * some address until it ends. Where it ends can be changed
 * dynamically.
 *
 * On a typical UNIX system, the "program break" would be the "top of
 * the heap", which would be modified by the brk() or sbrk() system
 * functions.
 *
 * It would typically be the memory allocator that would modify the
 * top of heap when more or less memory was needed to satisfy
 * allocations by the program.
 *
 * This emulated heap is intended used by lea_malloc to simplify
 * porting to platforms that has virtual memory capabilities
 * (e.g. smart-phones), but where the OS may not have a native "heap"
 * API, or it is already used by the native allocator (and we still
 * want to use lea_malloc).
 *
 * This class is primarily used to implement the \c op_sbrk() function
 * which is the Opera equivalent of UNIX sbrk(), used by lea_malloc.
 */
class OpHeapSegment
{
public:
#ifdef MEMORY_USE_LOCKING
	/**
	 * \brief Create an OpHeapSegment
	 *
	 * An \c OpHeapSegment must be created from a virtual \c
	 * OpMemory::OpMemSegment instance (which is unused). When
	 * created, the allocation type class needs to be specified for
	 * correct accounting operation.
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
	 * \param type The memory accounting class for the heap
	 * \param memlock Call OpMemory::MallocLock()/Unlock() when TRUE
	 * \return A OpHeapSegment or NULL
	 */
	static OpHeapSegment* Create(const OpMemory::OpMemSegment* mseg,
								 OpMemoryClass type = MEMCLS_DEFAULT,
								 BOOL memlock = TRUE);

	/**
	 * \brief Destroy an OpHeapSegment
	 *
	 * This will destroy an OpHeapSegment, releasing all the memory
	 * it has allocated (and thus causing all still lingering
	 * allocations to cause a memory access violation).
	 *
	 * The underlying \c OpMemory::OpMemSegment is not destroyed,
	 * it needs to be destroyed separately afterwards if this is
	 * desirable.
	 *
	 * \b Note: The \c memlock parameter must be treated like
	 * described for \c OpHeapSegment::Create().
	 */
	static void Destroy(const OpHeapSegment* hseg, BOOL memlock = TRUE);

	/**
	 * \brief Change data segment size
	 *
	 * This changes the data segment size, or program break, by the
	 * amount specified in the argument. Negative sizes are allowed,
	 * as long as the heap is not reduced to a negative size.
	 *
	 *\b Note: The \c memlock parameter must be treated like
	 * described for \c OpHeapSegment::Create().
	 */
	void* sbrk(long increment, BOOL memlock = TRUE);
#else
	static OpHeapSegment* Create(const OpMemory::OpMemSegment* mseg,
								 OpMemoryClass type = MEMCLS_DEFAULT);

	static void Destroy(const OpHeapSegment* hseg);

	void* sbrk(long increment);
#endif

private:
	const OpMemory::OpMemSegment* mseg; ///< Memory segment holdig the heap
	OpMemoryClass type; ///< Memory class for accounting
	char* base; ///< First address on heap
	char* top; ///< The maximum top of the heap
	char* current; ///< The current top of heap
	char* allocated; ///< Memory allocated below here
	size_t pagesize; ///< The memory page-size
};

#endif // ENABLE_OPERA_HEAP_SEGMENT

#endif // MEMORY_HEAP_H
