/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_EXECUTABLE_H
#define MEMORY_EXECUTABLE_H

#ifdef EXECUTABLE_MEMORY_MANAGER

#include "modules/pi/system/OpMemory.h"
#include "modules/memory/src/memory_manager.h"
#include "modules/util/simset.h"

#ifdef MEMORY_SMALL_EXEC_SEGMENTS
/** \brief Handle for a chunk of executable memory
 *
 * Whenever a chunk of executable memory is allocated, this handle is
 * returned which must be used for all manipulations of the executable
 * memory.
 *
 * This handle is just an opaque handle for the low level OpMemSegment
 * handle since TWEAK_MEMORY_SMALL_EXEC_SEGMENTS is applied, and we can
 * use the memory segments of the porting interface directly.
 */
class OpExecMemory : public OpMemory::OpMemSegment
{
};

#else // MEMORY_SMALL_EXEC_SEGMENTS
//
// Allocate the OpExecMemory "chunks" out of a "segment" of memory that
// comes out of the porting interface.
//

class OpExecMemorySegment;

/** \brief Handle for a chunk of executable memory
 *
 * Whenever a chunk of executable memory is allocated, this handle is
 * returned which must be used for all manipulations of the executable
 * memory.
 */
class OpExecMemory
{
public:
	void* address; ///< The address where the instructions will be executed
	size_t size; ///< The size of the chunk of executable memory
	OpExecMemorySegment* segment; ///< The segment where allocation belongs
};

/** \brief Management object for a single executable OpMemSegment
 *
 * The OpExecMemoryManager class organizes the allocated OpMemSegment
 * instances through this class. It contains the OpMemSegment handle,
 * pointers for doing allocations and allocation-counter for doing
 * deallocation.
 *
 * Note: This class is no used if the platform can handle allocation
 * of small executable segments directly (signalled by
 * TWEAK_MEMORY_SMALL_EXEC_SEGMENTS).
 */
class OpExecMemorySegment : public Link
{
public:
	const OpMemory::OpMemSegment* mseg; ///< The underlying segment handle
	char* free_ptr; ///< The pointer where the next allocation will come from
	size_t free_bytes; ///< The remaining number of free bytes to be allocated
	int allocations; ///< The count on current number of allocations

	OpExecMemorySegment* Next(void)
	{
		return static_cast<OpExecMemorySegment*>(Suc());
	}
};

#endif // MEMORY_SMALL_EXEC_SEGMENTS

/** \brief A manager class for allocating executable memory
 *
 * The OpExecMemoryManager class provides management functionality for
 * executable memory (for JIT compiling Ecmascript, regexps etc.)
 *
 * The management class is needed on systems where executable memory
 * needs to be allocated in larger blocks, like 4K, 32K etc. which is
 * much larger than the typical allocation for executable memory (a
 * few hundred bytes).  On such systems, OpExecMemoryManager will
 * organize the bigger chunks of memory, and make it possible to
 * allocate smaller parts of it at a time.
 *
 * On systems where TWEAK_MEMORY_SMALL_EXEC_SEGMENTS is applied, this
 * class will only be a shallow interface, calling directly into the
 * porting interface. When the tweak is applied, it is important that
 * the platform implementation is capable of handling small allocations
 * efficiently.
 *
 * Several OpExecMemoryManager classes can co-exist and provide
 * allocations to different documents, different kinds of executable
 * memory etc. (E.g. Native EcmaScript vs. RegExp acceleration).
 */
class OpExecMemoryManager : public Link
{
public:
	OpExecMemoryManager(OpMemoryClass type);
	~OpExecMemoryManager();

#ifdef MEMORY_SMALL_EXEC_SEGMENTS
	//
	// Use the porting interface directly, since it is capable of
	// allocating all sorts of sizes without an additional management
	// layer.
	//
	inline const OpExecMemory* AllocateL(size_t size)
	{
		const OpExecMemory* em =
			static_cast<const OpExecMemory*>(OpMemory::CreateExecSegment(size));
		if ( em == 0 || OpMemory::WriteExec(em, em->address, em->size) == 0 )
			LEAVE(OpStatus::ERR_NO_MEMORY);
		return em;
	}

#if 0
	//
	// Not used in current API, since Carakan/regexp is not able to make
	// use of this (and the API may eventually be different).
	//
	static inline void* BeginWrite(const OpExecMemory* handle)
	{
		return OpMemory::WriteExec(handle, handle->address, handle->size);
	}
#endif

	static inline void FinalizeL(const OpExecMemory* handle)
	{
		if ( OpStatus::IsError(OpMemory::WriteExecDone(handle, handle->address, handle->size)) )
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	static inline void Free(const OpExecMemory* handle)
	{
		OpMemory::DestroySegment(handle);
	}

#else // MEMORY_SMALL_EXEC_SEGMENTS
	//
	// The proper management layer for the executable memory chunks
	// are more complicated, and can't be inlined.
	//

	/** \brief Allocate memory for holding and running machine instructions
	 *
	 * Allocate the given number of bytes, and make the memory
	 * writeable so that fresh instructions can be written into
	 * it. While this metod is running, and until the Finalize()
	 * method has returned, no execution of instructions allocated
	 * from the same OpExecMemoryManager can take place.
	 *
	 * This limitation is needed since some architectures has hardware
	 * limitations that prevents writing and executing at the same
	 * time.  Such a limitation could also result from a security
	 * decision of not allowing writeable and executable memory pages
	 * to exist.
	 *
	 * \b NOTE: This method will LEAVE on error. No other status code
	 * than OpStatus::ERR_NO_MEMORY will be given upon error.
	 *
	 * \param size Number of bytes to be allocated for machine instructions
	 *
	 * \return A read-only handle that identifies the allocated memory
	 */
	const OpExecMemory* AllocateL(size_t size);

#if 0
	/** \brief Open up an allocation for writing
	 *
	 * \b WARNING: This method is currently not being used, and
	 * it is currently not implemented.
	 *
	 * This method will set the allocation up so that writing into
	 * the allocation can take place at the address returned.
	 *
	 * The address returned may be different from the address where
	 * the machine instructions will be executed.
	 *
	 * \b NOTE: Currently, the memory is made writeable by AllocateL()
	 * at the same address as where it is to be executed, to stay
	 * compatible with old behaviour, but this may change at some
	 * point.
	 *
	 * \param handle A handle to an allocation of executable memory
	 *
	 * \return A pointer to where the instructions must be written
	 */
	static void* BeginWrite(const OpExecMemory* handle);
#endif

	/** \brief Signal that the allocation is ready for execution
	 *
	 * When an allocation has had its instructions written into it,
	 * this method will set everything up so that the instructions
	 * can be called and executed.
	 *
	 * The calling of this method will typically flush CPU caches, make
	 * the memory executable or similar necessary operations.
	 *
	 * It is an error to call this method on a handle that is not
	 * open for writing.
	 *
	 * \b NOTE: This method will LEAVE on error.
	 *
	 * \param handle A handle to an allocation of executable memory
	 */
	static void FinalizeL(const OpExecMemory* handle);

	/** \brief Deallocate an allocation of executable memory
	 *
	 * When this method is called, the executable memory of the allocation
	 * will be freed. No instructions can be executed from this allocation
	 * after calling this method. Doing so is undefined.
	 *
	 * \param handle A handle to an allocation of executable memory
	 */
	static void Free(const OpExecMemory* handle);

#ifdef SELFTEST
	/** \brief Selftest function to return number of segments for manager
	 *
	 * In selftest enabled builds, this method will be available, and it
	 * counts the number of segments that are in use by the manager.
	 * This can be used to verify that segments are given back, for
	 * instance.
	 */
	int Test_GetSegmentCount(void);
#endif // SELFTEST

private:
	Head segments; ///< All OpExecMemorySegments for this OpExecMemoryManager

	/** \brief Free an OpExecMemorySegment
	 *
	 * Release the segment and the underlying allocated memory.
	 */
	static void FreeSegment(OpExecMemorySegment* segment);

public:
#endif // MEMORY_SMALL_EXEC_SEGMENTS

	/** \brief Merge two OpExecMemoryManager classes
	 *
	 * After calling MergeWith() on an object 'this', the resources
	 * previously administrated by the 'other' instance is transfered
	 * to 'this', and 'other' becomes owned by 'this'.
	 *
	 * When 'this' is deleted, the 'other' instance will also be
	 * deleted as part of the operation.
	 *
	 * After 'other' has been merged into 'this', no allocations can
	 * take place from 'other'. Also; 'other' can not merge with
	 * another manager.
	 *
	 * \param other The manager that gives up its resources
	 */
	void MergeWith(OpExecMemoryManager *other);

private:
	Head merged_with; ///< All OpExecMemoryManager that have been merged in

#if defined(DEBUG_ENABLE_OPASSERT) && !defined(MEMORY_SMALL_EXEC_SEGMENTS)
	BOOL has_been_merged; ///< This OpExecMemoryManager has been merged
#endif

	OpMemoryClass type; ///< The type of memory allocated by this manager
};

#endif // EXECUTABLE_MEMORY_MANAGER

#endif // MEMORY_EXECUTABLE_H
