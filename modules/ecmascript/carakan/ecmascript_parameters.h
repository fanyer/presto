/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2001-2004
 */

/* ECMAScript engine tunable parameters.

   Underlying memory allocator.

   ES_PARM_ARENA_BLOCK_SIZE (deprecated)

     This is the preferred chunk size (measured in bytes) to allocate from
	 the underlying memory allocator.

	 The value must be divisible by ES_LIM_ARENA_ALIGN, and it is helpful
	 if it is selected such that ES_PARM_ARENA_BLOCK_SIZE + per-block overhead
	 in the underlying allocator allows the underlying allocator to perform
	 well, typically, to allocate the block to an integral number of pages.
	 This is a bit of a black art.

     A good value seems to be 4KB-c for some small c: it is a compromise
	 between low overhead and reasonably swift return of blocks to the
	 rest of the system.

   ES_PARM_SMALL_PAGE_SIZE

     Size of pages where small objects are allocated.

   ES_PARM_CHUNK_SIZE

     Size of chunks allocated for the small object pages. Should be at least 256
     KB to be sure it is mmaped. And be a multiple of ES_PARM_SMALL_PAGE_SIZE.

   ES_PARM_MARKSTACK_SEGMENTS_SIZE

	 Garbage collector: This is the number of entries in each mark stack segment.
	 It does not need to be very large.

   ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE

	 Garbage collector: This is the number of entries in each element segment for
	 the dynamic-roots hash table.  It does not need to be very large.

   ES_PARM_DYNAMIC_ROOTS_LOAD_FACTOR

	 Garbage collector: This is the trigger for when to grow the dynamic roots hash
	 table: its population must exceed the hash table size times the load factor.
	 Larger numbers imply less space and worse performance.

   ES_PARM_DYNAMIC_ROOTS_GROWTH_FACTOR

	 Garbage collector: This is the factor by which to grow the dynamic roots hash
	 table, when it is grown.
*/

/* ECMAScript engine non-tunable parameters and implementation limits.


   ES_LIM_OBJECT_SIZE	= 4294967288

     The largest size (measured in bytes) of an object in the ECMAScript heap.
	 The object header uses a 32-bit field for the object size, so objects
	 can be as large as 2^32-8 bytes.

     Strings as large as 4MB have been seen in the wild, so it is nice to have
	 a large figure here.

   ES_LIM_ARENA_ALIGN	= 8

	 This is the allocation alignment (in bytes) in the ECMAScript heap; it is also
	 the smallest allocation unit.  The value 8 is dictated by the need to align
	 IEEE double values on 8-byte boundaries on some architectures (notably SPARC).
	 On some platforms this could be 4, and that would save a little space, but it
	 would reduce the maximum object size by half and probably impact other things
	 as well.

   ES_LIM_ARENA_ALIGN_SHIFT = 3

	 This is log2(ES_LIM_ARENA_ALIGN).
*/

#ifndef ECMASCRIPT_PARAMETERS_H
#define ECMASCRIPT_PARAMETERS_H

#define ES_PARM_ARENA_BLOCK_SIZE				(32*1024-32)
#define ES_PARM_SMALL_PAGE_SIZE                 (64*1024)
#define ES_PARM_PAGES_PER_CHUNK                 8
#define ES_PARM_MARKSTACK_SEGMENT_SIZE			1000
#define ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE		128
#define ES_PARM_DYNAMIC_ROOTS_LOAD_FACTOR		2.0
#define ES_PARM_DYNAMIC_ROOTS_GROWTH_FACTOR		1.5
#define ES_PARM_SMALLEST_YOUNG_HEAP_SIZE        (256*1024)
#define ES_PARM_LARGEST_YOUNG_HEAP_SIZE         (4*1024*1024)

#define ES_LIM_OBJECT_SIZE						4294967288U
#define ES_LIM_ARENA_ALIGN						8
#define ES_LIM_ARENA_ALIGN_SHIFT				3


#endif /* ECMASCRIPT_PARAMETERS_H */
