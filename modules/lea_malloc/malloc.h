/* -*- Mode: c; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
/** @file
 * Primary API: stdlib memory management.
 */
/* Changes made since adoption by Opera:
 * - Fix whitespace and other non-functional issues
 * - Introduce DL_MALLOC_NAME() macro; eliminates "#ifdef USE_DL_PREFIX" blocks
 * - Skip <stddef.h>
 * - Expand documentation; convert to doxygen
 * - Protect malloc.h with HAVE_DL_MALLOC
 * - Add declaration of posix_memalign() (implementation in lea_malloc.cpp)
 * - Add declaration of C++ API (new/delete) if not declared in 'memory' module
 * - Protect seldom-used API functions with LEA_MALLOC_FULL_MEMAPI
 *   See API_DLM_FULL_MEMAPI for more details
 *
 * The following comments are derived from the public domain original, but what
 * they say about copyrights and permissions is superseded by the above
 * restrictions.  Note that a version of this file without Opera copyrights may
 * be obtained from
 *     ftp://gee.cs.oswego.edu/pub/misc/malloc-2.7.2.h
 * A more recent edition may be obtained by eliding the -2.7.2 version number.
 */
/*
  Default header file for malloc-2.7.2, written by Doug Lea
  and released to the public domain.  Use, modify, and redistribute
  this code without permission or acknowledgement in any way you wish.
  Send questions, comments, complaints, performance data, etc to
  dl@cs.oswego.edu.

  last update: Sun Feb 25 18:38:11 2001  Doug Lea  (dl at gee)

  This header is for ANSI C/C++ only.  You can set either of
  the following #defines before including:

  * If USE_DL_PREFIX is defined, it is assumed that malloc.c
    was also compiled with this option, so all routines
    have names starting with "dl".

  * If HAVE_USR_INCLUDE_MALLOC_H is defined, it is assumed that this
    file will be #included AFTER <malloc.h>. This is needed only if
    your system defines a struct mallinfo that is incompatible with the
    standard one declared here.  Otherwise, you can include this file
    INSTEAD of your system system <malloc.h>.  At least on ANSI, all
    declarations should be compatible with system versions
*/

#ifndef MALLOC_270_H
#define MALLOC_270_H

#ifdef HAVE_DL_MALLOC /* not quite to end of file */

/** Packages standard library names as used by this API.
 *
 * By default the \c DL_MALLOC_NAME of a function is just its standard name;
 * however, when re-naming the API to not clash with the standard library's
 * version of it, the prefix \c dl is added to each name.
 */
#if defined(USE_DL_PREFIX) && !defined(DOXYGEN_DOCUMENTATION)
#define DL_MALLOC_NAME(nom) dl ## nom
#else
#define DL_MALLOC_NAME(nom) nom
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#include <stddef.h>   /* for size_t */
#endif

/** Allocate a new chunk of memory.
 *
 *  Returns a pointer to a newly allocated chunk of at least \c space bytes, or
 *  \c NULL if no space is available.  Additionally, on ANSI C systems, on
 *  failure, \c errno is set to \c ENOMEM.  On success, the chunk's contents
 *  are indeterminate; no assumption should be made about their initial values.
 *
 *  If \c space is zero, malloc returns a minimum-sized chunk. The minimum size
 *  is 16 bytes on most 32-bit systems, and either 24 or 32 bytes on 64-bit
 *  systems, depending on internal size and alignment restrictions.
 *
 *  On most systems, \c size_t is an unsigned type.  Calls with values of
 *  \c space that appear "negative" when signed are interpreted as requests for
 *  huge amounts of space, which will most often fail.
 *
 *  The maximum allowed value of \c space differs across systems, but is in all
 *  cases less (typically by 8K) than the maximum representable value of a \c
 *  size_t.  Requests greater than this value result in failure.
 *
 *  @param space Size of required memory buffer.
 *  @return Pointer to newly-allocated memory block.
 */
void*  DL_MALLOC_NAME(malloc)(size_t space);

/** Release previously-allocated memory.
 *
 *  Releases the chunk of memory pointed to by \c ptr, that had been previously
 *  allocated using \c malloc or a related routine such as \c realloc.  It has
 *  no effect if \c ptr is \c NULL.  It can have arbitrary (and bad!) effects
 *  if \c ptr has already been \c free()d or was not obtained via this file's
 *  API.
 *
 *  Unless disabled using \c mallopt, freeing very large spaces will, when
 *  possible, automatically trigger operations that give back unused memory to
 *  the system, thus reducing program footprint.
 *
 *  @param ptr Pointer to be de-allocated.
 */
void   DL_MALLOC_NAME(free)(void* ptr);

/** Allocate zero-filled memory.
 *
 *  @param count Number of objects.
 *  @param each Size required for each object.
 *  @return A pointer to \c count * \c each zero-initialized bytes.
 */
void*  DL_MALLOC_NAME(calloc)(size_t count, size_t each);

/** Adjust the size of prior allocation.
 *
 *  The returned pointer may or may not be the same as \c ptr.  The algorithm
 *  prefers extending \c ptr when possible, otherwise it employs the equivalent
 *  of a malloc-copy-free sequence.
 *
 *  If \c ptr is \c NULL, \c realloc is equivalent to \c malloc.
 *
 *  If space is not available, \c realloc returns \c NULL, \c errno is set (if
 *  on ANSI) and \c ptr is NOT \c free()d.
 *
 *  If \c space is for fewer bytes than already held by \c ptr, the newly
 *  unused space is lopped off; and \c free()d if possible.  Unless the @code
 *        #define REALLOC_ZERO_BYTES_FREES
 *  @endcode is set, \c realloc with a \c size argument of zero (re)allocates a
 *  minimum-sized chunk.
 *
 *  Large chunks that were internally obtained via \c mmap will always be
 *  reallocated using malloc-copy-free sequences unless the system supports \c
 *  MREMAP (currently only Linux).
 *
 *  The old Unix \c realloc convention of allowing the last-free'd chunk to be
 *  used as an argument to \c realloc is not supported.
 *
 *  @param ptr Previously allocated pointer to be re-sized.
 *  @param space New space requirement for the pointer.
 *  @return A pointer to a chunk of size \c space that contains the same data
 *          as does chunk \c ptr up to the minimum of \c space bytes and
 *          \c ptr's prior size.
 */
void*  DL_MALLOC_NAME(realloc)(void* ptr, size_t space);

#ifdef LEA_MALLOC_FULL_MEMAPI /* see API_DLM_FULL_MEMAPI */
/** Allocate memory subject to an alignment constraint.
 *
 *  Overreliance on \c memalign is a sure way to fragment space.  8-byte
 *  alignment is guaranteed by normal \c malloc calls, so don't bother calling
 *  \c memalign with an \c alignment argument of 8 or less.
 *
 *  @param alignment A power of two (else it will be rounded up to the nearest
 *         such); must be at least 8.
 *  @param space Size of chunck of memory to allocate.
 *  @return A pointer to a newly allocated chunk of \c space bytes, starting at
 *          an address that is a multiple of \c alignment (after adjustment, as
 *          above).
 */
void*  DL_MALLOC_NAME(memalign)(size_t alignment, size_t space);

#ifndef USE_DL_PREFIX /* There's never any sense in prefixing this one: */
/** Like memalign, but round alignment up to at least word size.
 *
 *  Added by Opera; implementation found in lea_malloc.cpp.
 *
 *  @param memptr Address at which to record a pointer to the start of the
 *         allocated memory.
 *  @param alignment Lower bound on desired alignment; shall be rounded up to
 *         word size, if less, and to a power of two, if it isn't one already.
 *  @param size Size of the chunk of memory to allocate.
 *  @return An \c errno value (\c errno is not set); zero on success.
 */
int posix_memalign(void **memptr, size_t alignment, size_t size);
#endif /* USE_DL_PREFIX */

/** Allocates a page-aligned chunk of at least \c space bytes.
 *
 *  Equivalent to <tt>memalign(pagesize, space)</tt>, where \c pagesize is
 *  the page size of the system.  If the page size is unknown, 4096 is used.
 *
 *  @param space Size of chunk to allocate.
 *  @return A pointer to a newly allocated chunk of \c space bytes, starting on
 *          a page boundary.
 */
void*  DL_MALLOC_NAME(valloc)(size_t space);

/** Several contiguous \c calloc calls of equal size.
 *
 *  Similar to \c calloc, but instead of returning a single cleared space, it
 *  returns an array of pointers to \c count independent elements, each of
 *  which can hold contents of size \c each.  Each element starts out cleared,
 *  and can be independently \c free()d, \c realloc()ed etc. The elements are
 *  guaranteed to be adjacently allocated (this is not guaranteed to occur with
 *  multiple \c calloc() or \c malloc() calls), which may also improve cache
 *  locality in some applications.
 *
 *  The \c chunks argument is optional (i.e., may be \c NULL, which is probably
 *  the most typical usage).  If it is \c NULL, the returned array is itself
 *  dynamically allocated and should also be \c free()d when it is no longer
 *  needed.  Otherwise, the \c chunks array must be of length at least
 *  \c count. It is filled in with the pointers to the chunks allocated.
 *
 *  In either case, \c independent_calloc returns this pointer array, or
 *  \c NULL if the allocation failed.  If \c count is zero and \c chunks is
 *  \c NULL, it returns a chunk representing an array with zero elements
 *  (which should be \c free()d if not wanted).
 *
 *  Each element must be individually \c free()d when it is no longer needed.
 *  If you'd like to instead be able to free all at once, you should instead
 *  use regular \c calloc and assign pointers into this space to represent
 *  elements. (In this case though, you \e cannot independently \c free
 *  elements.)
 *
 *  Using \c independent_calloc simplifies and speeds up implementations of
 *  many kinds of pools.  It may also be useful when constructing large data
 *  structures that initially have a fixed number of fixed-sized nodes, but the
 *  number is not known at compile time, and some of the nodes may later need
 *  to be \c free()d.
 *  For example: @code
 *
 *  struct Node { int item; struct Node* next; };
 *
 *  struct Node* build_list() {
 *    struct Node** pool;
 *    int n = read_number_of_nodes_needed();
 *    if (n <= 0) return 0;
 *    pool = (struct Node**)(independent_calloc(n, sizeof(struct Node), 0);
 *    if (pool == 0) return 0; // failure
 *    // organize into a linked list...
 *    struct Node* first = pool[0];
 *    for (i = 0; i < n-1; ++i)
 *      pool[i]->next = pool[i+1];
 *    free(pool);     // Can now free the array (or not, if it is needed later)
 *    return first;
 *  }
 *
 *  @endcode
 *
 *  @param count Number of cleared objects to allocate.
 *  @param each Size of each cleared object.
 *  @param[out] chunks Optional array in which to return pointers to
 *              newly-allocated objects; if \c NULL, an array large enough to
 *              hold \c count pointers shall be allocated for it.
 *  @return \c NULL on failure, else \c chunks (or the start of the array
 *          allocated to serve in its place).
 */
void** DL_MALLOC_NAME(independent_calloc)(size_t count, size_t each, void*chunks[]);

/** Several contiguous \c malloc calls of various sizes.
 *
 *  Allocates, all at once, a set of \c count chunks with sizes indicated in
 *  the \c sizes array.  It returns an array of pointers to these elements,
 *  each of which can be independently \c free()d, \c realloc()ed etc.
 *  The elements are guaranteed to be adjacently allocated (this is not
 *  guaranteed to occur with multiple \c calloc() or \c malloc() calls), which
 *  may also improve cache locality in some applications.
 *
 *  The \c chunks argument is optional (i.e., may be \c NULL). If it is \c NULL
 *  the returned array is itself dynamically allocated and should also be
 *  \c free()d when it is no longer needed.  Otherwise, the \c chunks array
 *  must be of length at least \c count.  It is filled in with the pointers to
 *  the chunks.
 *
 *  In either case, \c independent_comalloc returns this pointer array, or
 *  \c NULL if the allocation failed.  If \c count is zero and chunks is
 *  \c NULL, it returns a \c chunk representing an array with zero elements
 *  (which should be \c free()d if not wanted).
 *
 *  Each element must be individually \c free()d when it is no longer needed.
 *  If you'd like to instead be able to free all at once, you should instead
 *  use a single regular \c malloc, and assign pointers at particular offsets
 *  in the aggregate space. (In this case, though, you cannot independently
 *  \c free elements.)
 *
 *  Use of \c independent_comallac differs from \c independent_calloc in that
 *  each element may have a different size, and also that it does not
 *  automatically clear elements.  It can speed up allocation in cases where
 *  several structs or objects must always be allocated at the same time.
 *  For example: @code
 *
 *  struct Head { ... }
 *  struct Foot { ... }
 *
 *  void send_message(char* msg) {
 *    int msglen = strlen(msg);
 *    size_t sizes[3] = { sizeof(struct Head), msglen, sizeof(struct Foot) };
 *    void* chunks[3];
 *    if (independent_comalloc(3, sizes, chunks) == 0)
 *      die();
 *    struct Head* head = (struct Head*)(chunks[0]);
 *    char*        body = (char*)(chunks[1]);
 *    struct Foot* foot = (struct Foot*)(chunks[2]);
 *    // ...
 *  }
 *
 *  @endcode
 *  In general though, \c independent_comalloc is worth using only for larger
 *  values of \c count.  For small values, you probably won't detect enough
 *  difference from series of \c malloc calls to bother.
 *
 *  Overuse of \c independent_comalloc can increase overall memory usage, since
 *  it cannot reuse existing noncontiguous small chunks that might be available
 *  for some of the elements.
 *
 *  @param count Number of blocks of memory to allocate.
 *  @param sizes Array of sizes for those blocks.
 *  @param[out] chunks Optional array in which to return the allocated
 *              pointers; if \c NULL, an array of \c count pointers will be
 *              allocated and used for it; otherwise, its size must be at least
 *              \c count.
 *  @return \c NULL on failure, else \c chunks (or the pointer array allocated
 *          for it).
 */
void** DL_MALLOC_NAME(independent_comalloc)(size_t count, size_t sizes[], void* chunks[]);

/** Allocate enough pages to hold an object of size \c space.
 *
 *  Equivalent to valloc(minimum-page-that-holds(space)), that is, round space
 *  up to nearest pagesize.
 *
 *  @param space Size of desired block of memory.
 *  @return Pointer to a newly allocated block of memory comprising enough
 *          pages to hold \c space bytes.
 */
void*  DL_MALLOC_NAME(pvalloc)(size_t space);

/** Release allocated memory.
 *
 *  Equivalent to <tt>free(ptr)</tt>
 *
 *  Needed/defined on some systems that pair it with \c calloc, for odd
 *  historical reasons (such as: \c cfree is used in example code in the first
 *  edition of K&R).
 *
 *  @param ptr Pointer to be deallocated.
 */
void   DL_MALLOC_NAME(cfree)(void* ptr);

/* These next two, malloc_trim, malloc_usable_size, are used by test code only: */

/** Return free memory to the operating system.
 *
 *  If possible, gives memory back to the system (via negative arguments to
 *  \c sbrk) if there is unused memory at the `high' end of the \c heap memory
 *  pool.  You can call this after \c free()ing large blocks of memory to
 *  potentially reduce the system-level memory requirements of a program.
 *  However, it cannot guarantee to reduce memory.  Under some allocation
 *  patterns, some large free blocks of memory will be locked between two used
 *  chunks, so they cannot be given back to the system.
 *
 *  The \c pad argument represents the amount of free trailing space to leave
 *  untrimmed.  If this argument is zero, only the minimum amount of memory to
 *  maintain internal data structures will be left (one page or less).
 *  Non-zero arguments can be supplied to maintain enough trailing space to
 *  service future expected allocations without having to re-obtain memory from
 *  the system.
 *
 *  On systems that do not support "negative \c sbrk calls", it will always
 *  return 0.
 *
 *  @param pad Size of trailing space to retain (see above).
 *  @return A true value (1) if any memory is returned to they system, else a
 *          false value (0).
 */
int    DL_MALLOC_NAME(malloc_trim)(size_t pad);

/** Report effective size of allocated block.
 *
 *  Returns the number of bytes you can actually use in an allocated chunk,
 *  which (albeit seldom) may be more than you requested, due to alignment and
 *  minimum size constraints.  You can use this many bytes without worrying
 *  about overwriting other allocated objects. This is not a particularly great
 *  programming practice.  But \c malloc_usable_size can be more useful in
 *  debugging and assertions, for example: @code
 *
 *          p = malloc(n);
 *          assert(malloc_usable_size(p) >= 256);
 *
 *  @endcode
 *
 *  @param ptr Allocated pointer.
 *  @return Size (in bytes) of latest allocation starting at \c ptr.
 */
size_t DL_MALLOC_NAME(malloc_usable_size)(void* ptr);

/** Report heap activity statistics to \c stderr
 *
 *  Prints on \c stderr the amount of space obtained from the system (via both
 *  \c sbrk and \c mmap), the maximum amount (which may be more than current if
 *  \c malloc_trim and/or \c munmap got called), and the current number of
 *  bytes allocated via \c malloc, \c realloc, etc. but not yet \c free()d.
 *  Note that this is the number of bytes allocated, not the number requested.
 *  It will be larger than the number requested because of alignment and
 *  bookkeeping overhead.  Because it includes alignment wastage as being in
 *  use, this figure may be greater than zero even when no user-level chunks
 *  are allocated.
 *
 *  The reported current and maximum system memory can be inaccurate if a
 *  program makes other calls to system memory allocation functions (normally
 *  \c sbrk) outside of \c malloc, \c realloc, etc.
 *
 *  Only prints only the most commonly interesting statistics.
 *  More information can be obtained by calling \c mallinfo.
 */
void   DL_MALLOC_NAME(malloc_stats)(void);

/* mallinfo: not much used, but used */
#ifndef _MALLOC_H
#ifdef HAVE_USR_INCLUDE_MALLOC_H
#include "/usr/include/malloc.h"
#else
/** Heap activity summary structure.
 *
 *  The names of some of these fields don't bear much relation with their
 *  contents because this \c struct was defined as standard in SVID/XPG so
 *  reflects the \c malloc implementation that was then used in SystemV Unix.
 *
 *  The original SVID version of this \c struct, defined on most systems with
 *  \c mallinfo, declares all fields as of type \c int.  But some others use
 *  type \c unsigned long.  If your system defines the fields using a type of
 *  different width than listed here, you should \#\c include your system
 *  version before including this file.  The \c struct declaration is
 *  suppressed if \c _MALLOC_H is defined (which is done in most system
 *  \c malloc.h files).  You can also suppress it by defining
 *  \c HAVE_USR_INCLUDE_MALLOC_H.
 *
 *  Because these fields are of type \c int, but internal bookkeeping is done
 *  with values of type \c unsigned \c long, the reported values may appear
 *  negative, and may wrap around zero and thus be inaccurate.
 */
struct mallinfo {
	/** Current total non-\c mmap()ped bytes allocated from system */
	int arena;

	/** The number of free chunks */
	int ordblks;

	/** The number of fastbin blocks (i.e., small chunks that have been
	 *  \c free()d but not re-used or consolidated).
	 */
	int smblks;

	/** Current number of \c mmap()ped regions */
	int hblks;

	/** Total bytes held in \c mmap()ped regions */
	int hblkhd;

	/** The maximum total allocated space.  This will be greater than
	 *  current total if trimming has occurred.
	 */
	int usmblks;

	/** Total bytes held in fastbin blocks */
	int fsmblks;

	/** Current total allocated space (normal or \c mmap()ped) */
	int uordblks;

	/** total free space */
	int fordblks;

	/** The maximum number of bytes that could ideally be released back to
	 *  system via \c malloc_trim.  ("Ideally" means that it ignores page
	 *  restrictions etc.)
	 */
	int keepcost;
};
#endif /* HAVE_USR_INCLUDE_MALLOC_H */
#endif /* !_MALLOC_H */

/** Heap activity summary query.
 *
 *  @return Heap activity summary structure of type \c struct mallinfo (q.v.).
 */
struct mallinfo DL_MALLOC_NAME(mallinfo)(void);

/** Set tunable parameters.
 *
 *  Provide an (id, value) pair describing the parameter to set and the value
 *  to give it; \c mallopt then sets the corresponding parameter to the given
 *  value if it can (i.e., so long as the value is meaningful), and returns 1
 *  if successful else 0.
 *
 *  SVID/XPG defines four standard param numbers for \c mallopt, normally
 *  defined in \c malloc.h.  Only one of these (\c M_MXFAST) is used in this
 *  \c malloc.  The others (\c M_NLBLKS, \c M_GRAIN, \c M_KEEP) don't apply, so
 *  setting them has no effect.  But this \c malloc also supports four other
 *  options in \c mallopt. See below for details. Briefly, supported parameters
 *  are as follows (listed defaults are for "typical" configurations):
 *
 *  <table>
 *  <tr><th>Symbol           <th>id <th> default <th>allowed param values </tr>
 *  <tr><td>M_MXFAST         <td> 1 <td>      64 <td>0-80 (0 disables fastbins) </tr>
 *  <tr><td>M_TRIM_THRESHOLD <td>-1 <td>128*1024 <td> any (-1U disables trimming) </tr>
 *  <tr><td>M_TOP_PAD        <td>-2 <td>       0 <td> any </tr>
 *  <tr><td>M_MMAP_THRESHOLD <td>-3 <td>128*1024 <td> any (or 0 if no MMAP support) </tr>
 *  <tr><td>M_MMAP_MAX       <td>-4 <td>   65536 <td> any (0 disables use of mmap) </tr>
 *  </table>
 *
 *  @param id Number identifying the parameter to be set.
 *  @param value Value to give to the parameter.
 *  @return A true value on success, a false one on failure.
 */
int    DL_MALLOC_NAME(mallopt)(int id, int value);

/* Descriptions of tuning options */

/** Size limit for "fastbins".
 *
 *  \c M_MXFAST is the maximum request size used for "fastbins", special bins
 *  that hold returned chunks without consolidating their spaces.  This enables
 *  future requests for chunks of the same size to be handled very quickly, but
 *  can increase fragmentation, and thus increase the overall memory footprint
 *  of a program.
 *
 *  This malloc manages fastbins very conservatively yet still efficiently, so
 *  fragmentation is rarely a problem for values less than or equal to the
 *  default.  The maximum supported value of \c MXFAST is 80. You wouldn't want
 *  it any higher than this anyway.  Fastbins are designed especially for use
 *  with many small structs, objects or strings -- the default handles
 *  structs/objects/arrays with sizes up to 8 4-byte fields, or small strings
 *  representing words, tokens, etc.  Using fastbins for larger objects
 *  normally worsens fragmentation without improving speed.
 *
 *  You can reduce \c M_MXFAST to 0 to disable all use of fastbins. This causes
 *  the \c malloc algorithm to be a closer approximation of FIFO-best-fit in
 *  all cases, not just for larger requests, but will generally cause it to be
 *  slower.
 */
#ifndef M_MXFAST
#define M_MXFAST  1
#endif

/** Threshold at which to release free memory back to system.
 *
 *  \c M_TRIM_THRESHOLD is the maximum amount of unused top-most memory to keep
 *  before releasing via \c malloc_trim in \c free().
 *
 *  Automatic trimming is mainly useful in long-lived programs.  Because
 *  trimming via \c sbrk can be slow on some systems, and can sometimes be
 *  wasteful (in cases where programs immediately afterward allocate more large
 *  chunks) the value should be high enough so that your overall system
 *  performance would improve by releasing this much memory.
 *
 *  The trim threshold and the \c mmap control parameters (see below) can be
 *  traded off with one another.  Trimming and \c mmap()ping are two different
 *  ways of releasing unused memory back to the system.  Between these two, it
 *  is often possible to keep system-level demands of a long-lived program down
 *  to a bare minimum.  For example, in one test suite of sessions measuring
 *  the XF86 X server on Linux, using a trim threshold of 128K and a \c mmap
 *  threshold of 192K led to near-minimal long term resource consumption.
 *
 *  If you are using this \c malloc in a long-lived program, it should pay to
 *  experiment with these values.  As a rough guide, you might set to a value
 *  close to the average size of a process (program) running on your system.
 *  Releasing this much memory would allow such a process to run in memory.
 *  Generally, it's worth it to tune for trimming rather tham memory mapping
 *  when a program undergoes phases where several large chunks are allocated
 *  and released in ways that can reuse each other's storage, perhaps mixed
 *  with phases where there are no such chunks at all.  And in well-behaved
 *  long-lived programs, controlling release of large blocks via trimming
 *  versus mapping is usually faster.
 *
 *  However, in most programs, these parameters serve mainly as protection
 *  against the system-level effects of carrying around massive amounts of
 *  unneeded memory.  Since frequent calls to \c sbrk, \c mmap, and \c munmap
 *  otherwise degrade performance, the default parameters are set to relatively
 *  high values that serve only as safeguards.
 *
 *  The trim value must be greater than the page size to have any useful
 *  effect.  To disable trimming completely, you can set it to
 *  <tt>(unsigned long)(-1)</tt>
 *
 *  Trim settings interact with fastbin (\c MXFAST) settings: unless compiled
 *  with \c TRIM_FASTBINS defined, automatic trimming never takes place upon
 *  freeing a chunk with size less than or equal to \c MXFAST.  Trimming is
 *  instead delayed until subsequent \c free()ing of larger chunks.  However,
 *  you can still force an attempted trim by calling \c malloc_trim.
 *
 *  Also, trimming is not generally possible in cases where the main arena is
 *  obtained via \c mmap.
 *
 *  Note that the trick some people use of \c malloc()ing a huge space and then
 *  \c free()ing it at program startup, in an attempt to reserve system memory,
 *  doesn't have the intended effect under automatic trimming, since that
 *  memory will immediately be returned to the system.
 */
#define M_TRIM_THRESHOLD    -1

/** Default padding for calls to \c sbrk
 *
 *  \c M_TOP_PAD is the amount of extra `padding' space to allocate or
 *  retain whenever \c sbrk is called.  It is used in two ways internally:
 *
 *  @li When \c sbrk is called to extend the top of the arena to satisfy a new
 *  malloc request, this much padding is added to the \c sbrk request.
 *
 *  @li When \c malloc_trim is called automatically from \c free(), it is used
 *  as the \c pad argument.
 *
 *  In both cases, the actual amount of padding is rounded so that the end of
 *  the arena is always a system page boundary.
 *
 *  The main reason for using padding is to avoid calling \c sbrk so often.
 *  Having even a small \c pad greatly reduces the likelihood that nearly every
 *  \c malloc request during program start-up (or after trimming) will invoke
 *  \c sbrk, which needlessly wastes time.
 *
 *  Automatic rounding-up to page-size units is normally sufficient
 *  to avoid measurable overhead, so the default is 0.  However, in
 *  systems where \c sbrk is relatively slow, it can pay to increase
 *  this value, at the expense of carrying around more memory than
 *  the program needs.
 */
#define M_TOP_PAD           -2

/** Size threshold beyond which to use \c mmap for new allocations.
 *
 *  \c M_MMAP_THRESHOLD is the request size threshold for using \c mmap()
 *  to service a request.  Requests of at least this size, that cannot
 *  be allocated using already-existing space, will be serviced via \c mmap.
 *  (If enough normal \c free()d space already exists it is used instead.)
 *
 *  Using \c mmap segregates relatively large chunks of memory so that they can
 *  be individually obtained and released from the host system.  A request
 *  serviced through \c mmap is never reused by any other request (at least not
 *  directly; the system may just so happen to remap successive requests to the
 *  same locations).
 *
 *  Segregating space in this way has the benefits that:
 *
 *   @li \c mmap()ped space can ALWAYS be individually released back to the
 *       system, which helps keep the system level memory demands of a
 *       long-lived program low.
 *
 *   @li Mapped memory can never become `locked' between other chunks, as can
 *       happen with normally allocated chunks, which means that even trimming
 *       via malloc_trim would not release them.
 *
 *   @li On some systems with "holes" in address spaces, \c mmap can obtain
 *       memory that \c sbrk cannot.
 *
 *  However, it has the disadvantages that:
 *
 *   @li The space cannot be reclaimed, consolidated, and then used to service
 *       later requests, as happens with normal chunks.
 *
 *   @li It can lead to more wastage because of \c mmap page alignment
 *       requirements
 *
 *   @li It causes \c malloc performance to be more dependent on host system
 *       memory management support routines.
 *
 *  The advantages of \c mmap nearly always outweigh disadvantages for "large"
 *  chunks, but the value of "large" varies across systems.  The default is an
 *  empirically derived value that works well in most systems.
 */
#define M_MMAP_THRESHOLD    -3

/** Limit on number of live \c mmap allocations.
 *
 *  \c M_MMAP_MAX is the maximum number of requests to simultaneously service
 *  using \c mmap.  This parameter exists because some systems have a limited
 *  number of internal tables for use by \c mmap, and using more than a few of
 *  them may degrade performance.
 *
 *  The default is set to a value that serves only as a safeguard.  Setting to
 *  0 disables use of \c mmap for servicing large requests.  If \c mmap is not
 *  supported on a system, the default value is 0, and attempts to set it to
 *  non-zero values in \c mallopt will fail.
 */
#define M_MMAP_MAX          -4

/* Unused SVID2/XPG mallopt options, listed for completeness */

#ifndef M_NLBLKS    /** UNUSED in this \c mallopt */
#define M_NLBLKS  2
#endif
#ifndef M_GRAIN     /** UNUSED in this \c mallopt */
#define M_GRAIN   3
#endif
#ifndef M_KEEP      /** UNUSED in this \c mallopt */
#define M_KEEP    4
#endif

#endif /* LEA_MALLOC_FULL_MEMAPI */

/*
  Some malloc.h's declare alloca, even though it is not part of malloc.
*/
#ifndef _ALLOCA_H
extern void* alloca(size_t);
#endif

#ifdef __cplusplus
}  /* end of extern "C" */
#endif

#endif /* HAVE_DL_MALLOC */

#endif /* MALLOC_270_H */
