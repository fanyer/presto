/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_SYSTEM_OPMEMORY_H
#define PI_SYSTEM_OPMEMORY_H

class OpMemory
{
public:
#ifdef OPMEMORY_EXECUTABLE_SEGMENT

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

	struct OpMemSegment
	{
		void* address; ///< Pointer to the memory segment
		size_t size; ///< Size of memory segment in bytes
		OpMemSegmentType type; ///< Actual type of the memory segment
	};

	static void DestroySegment(const OpMemSegment* mseg);
	static const OpMemSegment* CreateExecSegment(size_t minimum);
	static void* WriteExec(const OpMemSegment* mseg, void* ptr, size_t size);
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
