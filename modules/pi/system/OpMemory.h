/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_SYSTEM_OPMEMORY_H
#define PI_SYSTEM_OPMEMORY_H

#if defined(OPMEMORY_MEMORY_SEGMENT) || defined(OPMEMORY_VIRTUAL_MEMORY) || defined(OPMEMORY_EXECUTABLE_SEGMENT) || defined(OPMEMORY_STACK_SEGMENT) || defined(OPMEMORY_ALIGNED_SEGMENT)
# define OPMEMORY_SEGMENT_BASIC
#endif // OPMEMORY_STACK_SEGMENT || OPMEMORY_VIRTUAL_MEMORY || OPMEMORY_EXECUTABLE_SEGMENT || OPMEMORY_STACK_SEGMENT || OPMEMORY_ALIGNED_SEGMENT

/** @short Memory utilities.
 *
 * This is a collection of static methods needed by Opera's memory
 * infrastructure code. Depending on malloc() implementation, product
 * multi-threaded-ness, and whether or not debugging is enabled, some (or even
 * all) of these methods may need implementation.
 */
class OpMemory
{
public:
#ifdef OPMEMORY_MALLOC_LOCK
	/** @short Lock mutex for memory operations.
	 *
	 * Opera needs this method when configured to provide its own allocator,
	 * and at the same time configured to use locking to protect the allocator
	 * from multiple thread access.
	 *
	 * This method should be implemented so that it will either self-initialize
	 * upon first lock operation, or the locks should be statically initialized
	 * if possible (for performance reasons). The method should hang until the
	 * lock can be acquired. Recursive locking is not supported.
	 *
	 * Note that the mutex used here is not the same as the one used in
	 * MemdebugLock() / MemdebugUnlock().
	 */
	static void MallocLock();

	/** @short Unlock mutex for memory operations.
	 *
	 * This method unlocks the mutex previously locked by \c MallocLock(). It
	 * must be done from the same thread that locked it.
	 *
	 * Behavior when unlocking when not previously locked is undefined.
	 */
	static void MallocUnlock();
#endif // OPMEMORY_MALLOC_LOCK

#ifdef OPMEMORY_MEMDEBUG_LOCK
	/** @short Lock memory debugger mutex.
	 *
	 * The memory debugger needs a mutex to protect against multiple thread
	 * access.
	 *
	 * This method should be implemented so that it will either self-initialize
	 * upon first lock operation, or the locks should be statically initialized
	 * if possible (for performance reasons). The method should hang until the
	 * lock can be acquired. Recursive locking is not supported.
	 *
	 * Note that the mutex used here is not the same as the one used in
	 * MallocLock() / MallocUnlock().
	 */
	static void MemdebugLock();

	/** @short Unlock memory debugger mutex.
	 *
	 * This method unlocks the mutex previously locked by \c MemdebugLock(). It
	 * must be done from the same thread that locked it.
	 *
	 * Behavior when unlocking when not previously locked is undefined.
	 */
	static void MemdebugUnlock();
#endif // OPMEMORY_MEMDEBUG_LOCK

#ifdef OPMEMORY_SEGMENT_BASIC
	/** @short The different usages of memory segments.
	 *
	 * The life expectancy for the data being entered into a memory
	 * segment differentiates between different segment types, and is
	 * categorized by this enum. All segments will be created as one
	 * of these types.
	 *
	 * The virtual type indicates that the memory segment is being
	 * managed through virtual memory techniques (and the life
	 * expectancy is then individual at the virtual memory block size
	 * level).
	 *
	 * The unused type will not be requested, but it can be used by
	 * the platform implementation to signal that e.g. an OpMemSegment
	 * is unused.
	 */
	enum OpMemSegmentType
	{
#ifdef OPMEMORY_MEMORY_SEGMENT
		MEMORY_SEGMENT_TEMPORARY,
		MEMORY_SEGMENT_DOCUMENT,
		MEMORY_SEGMENT_PERSISTENT,
#endif
#ifdef OPMEMORY_VIRTUAL_MEMORY
		MEMORY_SEGMENT_VIRTUAL,
#endif
#ifdef OPMEMORY_EXECUTABLE_SEGMENT
		MEMORY_SEGMENT_EXECUTABLE,
#endif
#ifdef OPMEMORY_STACK_SEGMENT
		MEMORY_SEGMENT_STACK,
#endif
#ifdef OPMEMORY_ALIGNED_SEGMENT
		MEMORY_SEGMENT_ALIGNED,
#endif
		MEMORY_SEGMENT_UNUSED // Can be used by platform implementation
	};

	/** @short The memory segment handle.
	 *
	 * A memory segment is represented by this \c OpMemSegment handle which
	 * must be created, initialized and destroyed by the platform code. Core
	 * code will only read values from this handle.
	 *
	 * Platform code may extend this structure with additional data of its own,
	 * like operating system handles or linked lists, etc. if this is practical
	 * to the platforms' administration of the \c OpMemSegment handles and
	 * their associated data.
	 */
	struct OpMemSegment
	{
		void* address; ///< Pointer to the memory segment
		size_t size; ///< Size of memory segment in bytes
		OpMemSegmentType type; ///< Actual type of the memory segment
	};

	/** @short Destroy a memory segment.
	 *
	 * When a memory segment is not needed any more, it will be destroyed
	 * through this function. This may happen immediately when it becomes
	 * empty, or it may happen sometime later when consolidation detects that
	 * it is no longer used.
	 *
	 * It is the responsibility of this function to deallocate the \c
	 * OpMemSegment structure (or keep it for later re-use), and free
	 * the memory or release the virtual memory associated with the \c
	 * OpMemSegment handle.
	 *
	 * @param mseg The segment to be destroyed
	 */
	static void DestroySegment(const OpMemSegment* mseg);
#endif // OPMEMORY_SEGMENT_BASIC

#ifdef OPMEMORY_MEMORY_SEGMENT
	/** @short Create a memory segment.
	 *
	 * The desirable size of a memory segment depends on the size of the memory
	 * blocks used internally in Opera (MEMORY_BLOCKSIZE macro), but the
	 * segment should have space for 6-8 memory blocks, and there must be
	 * room for at least 2 memory blocks.
	 *
	 * The allocated memory does not have to be initialized, and users of
	 * this function must thus never assume that the memory will be e.g.
	 * all-zeroes, even if this may often seem to be the case.
	 *
	 * The size to be allocated is decided by this platform function, i.e. it
	 * is a platform responsibility to decide the segment size. The segment
	 * size may change between segments and segment types (e.g. smaller
	 * segments can be allocated if bigger ones causes OOM, or smaller segments
	 * for persistent data).
	 *
	 * The type is suggested at creation time, but is only for reference since
	 * OOM and other conditions may later distort the expected allocation
	 * pattern. The platform may decide to use or disregard the provided type
	 * during creation at its own discretion. The indicated type must however
	 * be copied into the 'type' member of the \c OpMemSegment handle, along
	 * with the details of the allocated memory segment.
	 *
	 * Note: This function will not be used to allocate a virtual memory
	 * segment. The CreateVirtualSegment() method is used for this.
	 *
	 * @param type The type of data to be allocated in the segment
	 * @return Pointer to a \c OpMemSegment structure or NULL
	 */
	static const OpMemSegment* CreateSegment(OpMemSegmentType type);
#endif

#ifdef OPMEMORY_VIRTUAL_MEMORY
	/*
	 * NOTE: This functionality is in a phase of development. Have a
	 * look at the memory module home-page on the developer wiki for
	 * updated information about the status of the virtual memory code.
	 *
	 * Please don't implement this functionality before you know that
	 * it will work for you, and provide useful functionality.
	 */

	/** @short Request a memory segment for virtual memory management
	 *
	 * When the memory system supports virtual memory management at a
	 * block level where the block size is typically 4K, this can be
	 * utilized by Opera.
	 *
	 * This method must be implemented so that it sets aside a
	 * contiguous region of memory that can later be used. No other
	 * application or system library can lay claim to this virtual
	 * memory segment or parts of it once set aside for opera.
	 * The virtual memory segment must not have any memory allocated
	 * initially.
	 *
	 * Opera may request multiple memory segments for virtual memory
	 * management during normal operation. This may happen due to
	 * features needing private virtual memory segments, or all
	 * segments go full, there is not enough contiguous unused space in
	 * the existing segments, or if special memory debugging
	 * functionality prevents reuse of virtual memory.
	 *
	 * This request function takes one argument: The minimum size.
	 * The platform may return as much virtual memory as it sees
	 * fit (but must return at least the minimum). Opera might
	 * not use all of it. The minimum size will always be a multiple
	 * of the page size.
	 *
	 * If the minimum size given is large, e.g. 1M or larger, it is
	 * not a good idea to set aside significantly more than the
	 * minimum.
	 *
	 * The virtual memory segment reserved with this method will be
	 * released through the \c DestroySegment() method. The type of
	 * the segments created by this method must be
	 * MEMORY_SEGMENT_VIRTUAL.
	 *
	 * The implementation of this function is required to return a
	 * memory segment that is aligned on a page boundary. The size
	 * of the returned memory segment needs to be a multiple of the
	 * memory page size.
	 *
	 * @param minimum Minimum size of the virtual memory segment in bytes
	 *
	 * @return Pointer to a \c OpMemSegment structure or NULL.
	 */
	static const OpMemSegment* CreateVirtualSegment(size_t minimum);

	/** @short Query the memory block size of the virtual memory system
	 *
	 * This method must return the memory block size used by the
	 * virtual memory system. This information is used to decide how
	 * to organize allocations in the virtual memory segment, or to
	 * verify compile-time assumptions.
	 *
	 * Opera will typically only call this method once, and cache its
	 * result. It is thus important that the page size stays the same
	 * during a run. The page size must be a power of two, and be 4096
	 * or larger.
	 *
	 * @return The memory page size in bytes
	 */
	static size_t GetPageSize(void);

	/** @short Allocate virtual memory for use
	 *
	 * This method is called on an \c OpMemSegment of type
	 * MEMORY_SEGMENT_VIRTUAL, and must cause specified portion
	 * of the memory segment to be made available for reading
	 * and writing. The allocated memory does not have to be
	 * initialized, and users of this function must thus never
	 * assume that the memory will be e.g. all-zeroes, even if
	 * this may often seem to be the case.
	 *
	 * The range of memory allocated in this way was previously
	 * unallocated. This is guaranteed, and all users of this
	 * function must thus follow this design restriction, even if
	 * typical platform implementations does not have such a
	 * limitation.
	 *
	 * The argument \c ptr will be on a page boundary, and \c size
	 * will be a multiple of the page size.
	 *
	 * @param mseg Pointer to the OpMemSegment to be allocated from
	 * @param ptr Pointer to first byte to be allocated
	 * @param size Number of bytes to be allocated
	 *
	 * @return TRUE when allocation succeeds, FALSE otherwise
	 */
	static BOOL VirtualAlloc(const OpMemSegment* mseg, void* ptr, size_t size);

	/** @short Deallocate virtual memory
	 *
	 * This method is called on an \c OpMemSegment of type
	 * MEMORY_SEGMENT_VIRTUAL, and must cause the specified portion
	 * of the memory segment to be released and made available for
	 * other uses (released back to the OS preferably).
	 *
	 * The memory released by this method should preferably be made
	 * inaccessible, so any future accesses to this memory will
	 * provoke a segmentation fault or general protection fault.
	 * Some memory debugging functionality relies on this behaviour.
	 *
	 * The arguments \c ptr and \c size will have the same alignment
	 * properties as for \c VirtualAlloc(), but they will not
	 * necessarily correspond to a single previous allocation. E.g.
	 * contiguous memory may be allocated in two calls to
	 * \c VirtualAlloc(), but the memory may be freed through a single
	 * call to \c VirtualFree() with a \c ptr and \c size spanning the
	 * two original allocations.
	 *
	 * The memory that is deallocated will previously have been
	 * allocated, this is a guarantee and a limitation like for
	 * \c VirtualAlloc() that must be observed by all users of
	 * this function.
	 *
	 * @param mseg Pointer to the OpMemSegment to be deallocated from
	 * @param ptr Pointer to first byte to be deallocated
	 * @param size Number of bytes to be deallocated
	 */
	static void VirtualFree(const OpMemSegment* mseg, void* ptr, size_t size);
#endif // OPMEMORY_VIRTUAL_MEMORY

#ifdef OPMEMORY_EXECUTABLE_SEGMENT
	/** @short Create an executable memory segment
	 *
	 * A memory segment created using this method can hold machine
	 * instructions to be executed. This is typically useful for
	 * compiling scripts to native machine instructions, or to speed
	 * up general computations where the computation itself is
	 * dependant on dynamic input (like searches) through native code
	 * generation.
	 *
	 * This method only creates the memory segment in such a fashion
	 * that it can later be used for holding native machine
	 * instructions. When using this functionality, the requirements
	 * and limitations of the executable memory segments described
	 * below must be followed very carefully.
	 *
	 * The memory segment allocated must be of at least the size given
	 * as an argument. The memory segment returned must be aligned
	 * such that machine instructions can be executed from the first
	 * byte of the segment, and preferably align efficiently with CPU
	 * cache line size if practical and appropriate.
	 *
	 * This method can be implemented in one of two flavors:
	 *
	 * - The segment created has a size very close to the size given
	 *   as argument. For this implementation, the 'minimum' argument
	 *   can be small, e.g. 96 bytes, and the platform implementation
	 *   will still make an efficient allocation. This is typically
	 *   achieved by the platform by using a general heap allocator
	 *   when allocating the executable memory segments.
	 *
	 * - The segment created may be significantly larger than
	 *   requested.  This might be caused by using mmap() or similar
	 *   operating system functionality to perform the
	 *   allocation. Page size granularity of 4K or 8K will typically
	 *   constrain the allocation size, and force a (significant)
	 *   rounding up. In order to avoid wasting memory, users of this
	 *   API should do allocations within the bigger blocks
	 *   themselves. The users of this API should be prepared to adapt
	 *   to the bigger sizes returned by this method as indicated by
	 *   OpMemSegment::size (due to e.g. rounding up to nearest 64K).
	 *   Platforms are advised to avoid rounding up too liberally to
	 *   avoid wasting memory. Trade-offs to be made here may be due to
	 *   e.g. severe limitations on the platform (big page-size, slow
	 *   operation, risk of fragmentation).
	 *
	 * When the macro MEMORY_SMALL_EXEC_SEGMENTS is defined, the
	 * platform must support small segments efficiently. This macro is
	 * controlled through TWEAK_MEMORY_SMALL_EXEC_SEGMENTS.
	 *
	 * Protocol that must be observed when using an executable memory segment:
	 *
	 * -# Create the executable memory segment by calling CreateExecSegment()
	 * -# Call WriteExec() on a range of bytes in the segment to be written
	 * -# Write the instructions into the memory returned by WriteExec()
	 * -# Call WriteExecDone() on the same range of bytes
	 * -# Use the previously written instructions by calling into the segment
	 * -# Destroy the segment by calling DestroySegment()
	 *
	 * Steps 2-5 can be repeated, but observe the following limitations:
	 *
	 * - The memory must be considered execute-only except when being
	 *   written
	 * - No part of an OpMemSegment can be executed while any range
	 *   within it is open for writing
	 * - Failing to call WriteExecDone() may cause a crash on execution.
	 *
	 * These limitations must be observed by those using this API,
	 * i.e. Core, while platforms are free to implement the API in any
	 * compatible manner.  WriteExecDone() may for instance be a
	 * no-op, but keeping executable memory write protected would be a
	 * security enhancement (at a likely performance penalty).
	 *
	 * The allocated OpMemSegment is deallocated by DestroySegment().
	 *
	 * @param minimum Minimum number of bytes to be allocated for the segment
	 *
	 * @return Pointer to an OpMemSegment, or NULL on error
	 */
	static const OpMemSegment* CreateExecSegment(size_t minimum);

	/** @short Enable writing instructions to an executable memory segment
	 *
	 * This method will "open up" an executable memory segment (or
	 * parts of it) so that fresh instructions can be written to
	 * it. This step is needed since writing directly into an
	 * executable memory segment may not always be possible (out of
	 * security concerns, platform limitations or other reasons). One
	 * such limitation would be platforms where a memory page can't be
	 * executable and writable at the same time due to how the CPU and
	 * MMU is designed.
	 *
	 * When calling this method to enable writing instructions to the
	 * memory segment, the byte range to be written inside the segment
	 * must be provided. The byte range must lie within the segment
	 * given, or else the result is undefined.
	 *
	 * The write-enabled memory, into which you can write fresh
	 * instructions, starts at the address returned; this corresponds
	 * to the @c ptr argument but may be a different address. All
	 * writes must be at non-negative byte offsets less than size from
	 * the returned pointer; when WriteExecDone() is called, the
	 * contents of this area of memory shall become available starting
	 * at @c ptr.
	 *
	 * The requirement of using the return value of WriteExec() when
	 * writing instructions is to provide a way to achieve correct
	 * operation on platforms where the executable and writable memory
	 * pages are separated in the memory layout. On many
	 * architectures, the WriteExec() method will just return the
	 * address in @c ptr directly (but this behaviour should not be
	 * relied upon).
	 *
	 * While this method is being called and until the subsequent call
	 * to WriteExecDone() has returned, no instructions in the memory
	 * segment described by mseg may be executed. Care must be taken,
	 * esp. in multithreaded environments, to prevent simultaneous
	 * execution and updating of instructions inside the same
	 * executable memory segment.
	 *
	 * Only a single range of bytes can be enabled for writing at a
	 * time.
	 *
	 * The result is undefined if this method is called on a memory
	 * segment that is not an executable memory segment.
	 *
	 * @param mseg The memory segment to be written into
	 * @param ptr The first byte inside the segment to be written
	 * @param size The number of bytes to be written
	 *
	 * @return Pointer to where instructions must be written, or NULL on error
	 */
	static void* WriteExec(const OpMemSegment* mseg, void* ptr, size_t size);

	/** @short Finish writing instructions to an executable memory segment
	 *
	 * This method will "close down" the memory region previously
	 * "opened" by WriteExec().  This method must be called to signal
	 * that the process of writing instructions to the segment has
	 * completed, and that the instructions in the segment must be
	 * made ready for execution.
	 *
	 * The reason for the existence of this method is that some
	 * architectures may require extra steps like:
	 *
	 * - Make the affected memory pages executable (e.g. if they
	 *   can't be executable and writable at the same time).
	 * - Flush the CPU instruction cache for the affected region
	 *   of memory, to prevent running stale instructions.
	 *
	 * If this method fails, the range of bytes in question will have
	 * an undefined state, and no assumptions should be made whether
	 * the byte range given can be executed or written.
	 *
	 * @b NOTE: The arguments provided to this method must have the
	 * exact same values as those provided to the WriteExec() method,
	 * or else undefined behaviour would result.
	 *
	 * @param mseg The memory segment that has been written into
	 * @param ptr Pointer to byte range inside segment
	 * @param size The size in bytes of the byte range
	 *
	 * @return OpStatus::OK on success
	 */
	static OP_STATUS WriteExecDone(const OpMemSegment* mseg, void* ptr, size_t size);

#endif // OPMEMORY_EXECUTABLE_SEGMENT

#ifdef OPMEMORY_STACK_SEGMENT
	/** @short Create a segment usable as a machine stack.
	 *
	 * A memory segment created by this function can be used as a
	 * custom machine stack, for instance via Carakan's OpPseudoThread
	 * stack switching mechanism.  If the CPU architecture and/or
	 * operating system has no special requirements, a block of memory
	 * allocated by malloc() is enough.
	 *
	 * For debugging purposes it may be useful to allocate the block
	 * using mmap()/VirtualAlloc() or similar and make the memory page
	 * immediately preceding the allocated block completely
	 * inaccessible, which makes stack overflows trigger immediate
	 * crashes instead of overwriting random parts of the heap.  But
	 * this is an optional feature.
	 *
	 * The allocated OpMemSegment is deallocated by DestroySegment().
	 *
	 * @param size The size of the block to allocate.  Must be a power
	 *             of two, and at least 4096.
	 *
	 * @return Pointer to an OpMemSegment, or NULL on error.
	 */
	static const OpMemSegment* CreateStackSegment(size_t size);

	/** Type of stack switch taking place. */
	enum StackSwitchOperation
	{
		/** Switch from system stack to a custom stack.  The new stack
		    pointer will be within a block of memory previously
		    allocated via CreateStackSegment().  Signalled immediate
		    before the actual switch. */
		STACK_SWITCH_SYSTEM_TO_CUSTOM,
		/** Switch from one custom stack to another custom stack.
		    Both the current and the new stack pointers will be within
		    blocks of memory previously allocated via
		    CreateStackSegment(), and never the same block.  Signalled
		    immediately before the actual switch. */
		STACK_SWITCH_BEFORE_CUSTOM_TO_CUSTOM,
		/** Switch from one custom stack to another custom stack.
		    Both the current and the new stack pointers will be within
		    blocks of memory previously allocated via
		    CreateStackSegment(), and never the same block.  Signalled
		    immediately after the actual switch. */
		STACK_SWITCH_AFTER_CUSTOM_TO_CUSTOM,
		/** Switch from custom stack back to system stack.  The
		    current stack pointer will be within a block of memory
		    previously allocated via CreateStackSegment().  Signalled
		    immediately after the actual switch. */
		STACK_SWITCH_CUSTOM_TO_SYSTEM
	};

	/** @short Called just before stack switch.
	 *
	 * This function is called immediately before any stack switch to
	 * a custom stack previously allocated via CreateStackSegment()
	 * and immediately after any stack switch from such a custom
	 * stack.  When switching between two such custom stacks, this
	 * function is called both immediately before and immediately
	 * after the switch.
	 *
	 * The type of switch specified by the 'operation' argument, and
	 * the custom stacks involved are specified by the 'current_stack'
	 * and 'new_stack' arguments.  If either the current stack or the
	 * new stack is the system stack (that is, the normal stack of the
	 * process/thread provided by the operating system) then the
	 * corresponding argument will be NULL.
	 *
	 * For most platforms this function can be empty.
	 *
	 * NOTE: This function should *NOT* actually switch stack!
	 *
	 * @param operation Type of stack switch.
	 * @param current_stack The segment of the current stack, if it's
	 *                      a custom stack; NULL if it's the system
	 *                      stack.
	 * @param new_stack The segment of the new stack, if it's a custom
	 *                  stack; NULL if it's the system stack.
	 */
	static void SignalStackSwitch(StackSwitchOperation operation, const OpMemSegment* current_stack, const OpMemSegment* new_stack);
#endif // OPMEMORY_STACK_SEGMENT

#ifdef OPMEMORY_ALIGNED_SEGMENT
	/** @short Create an aligned memory segment.
	 *
	 * Used to allocate a large block of memory aligned at the
	 * specified boundary.  Block size and alignment should both be at
	 * least as large as the hardware page size, and both will
	 * typically be larger than it.  Alignment must be a power of two.
	 * This function is not meant to be usable for allocating small
	 * blocks of memory.
	 *
	 * The allocated segment is released using DestroySegment().
	 *
	 * @param size Size to allocate.
	 * @param alignment Alignment requirement.
	 *
	 * @return Pointer to an OpMemSegment, or NULL on error.
	 */
	static const OpMemSegment* CreateAlignedSegment(size_t size, size_t alignment);
#endif // OPMEMORY_ALIGNED_SEGMENT

};

#endif // PI_SYSTEM_OPMEMORY_H
