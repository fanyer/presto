/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef LEA_MALLOC_H
#define LEA_MALLOC_H __FILE__
/** @mainpage Doug Lea's implementation of malloc

  Projects wishing to use this module's APIs should do so, and tune it, via the
  feature system if on core-2 or later; otherwise, they should turn on relevant
  defines.  See capabilities file for details.

  Source files which directly exercise this module's primary API should simply
  @code
		#include "modules/lea_malloc/malloc.h"
  @endcode to access this module's declarations of the standard C (and C++)
  memory management functions (and operators) - its primary API.  Code
  responsible for initialization, shut-down and (in single-block mode)
  interrogating the block and free memory space - via the secondary API - need
  to @code
		#include "modules/lea_malloc/lea_malloc.h"
  @endcode

  Since these headers depend on feature (and capability) defines, the above
  inclusion must happen \e after that of the feature system, which happens after
  that of the \e system system; consequently the latter cannot reference either
  of this module's API headers.  Thus, to use this module's primary API as op_*
  functions, a \c system.h should either duplicate declarations from this API or
  declare related wrapper functions (bound to \c op_* names) whose definitions
  can duly call this module's primary API functions.
 */
/** @file
 * Declaration of the secondary API.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef LEA_MALLOC_API_GET_STATE_SIZE
/** Query size of lea_malloc's malloc_state struct
 *
 * The malloc_state struct is where lea_malloc keeps its internal state.
 * @return The size of the malloc_state struct
 */
extern size_t get_malloc_state_size(void);
#endif // LEA_MALLOC_API_GET_STATE_SIZE

#ifdef LEA_MALLOC_EXTERN_CHECK
/** Perform expensive global check of internal state sanity.
 *
 * This check is a no-op unless: either _DEBUG is defined; or
 * LEA_MALLOC_CONFIG_HEADER provides a definition of the assert macro (see
 * dummy_config.h and TWEAK_LEA_MALLOC_PLATFORM_CONFIG for details).  In these
 * cases, this function assert()s (which is OP_ASSERT by default) various
 * conditions which it expects to be true, in the course of a traversing the
 * module's global data structures (i.e. the free list).
 *
 * Available when API_DLM_CHECK is imported.
 */
void lea_malloc_check_state();
#endif // LEA_MALLOC_EXTERN_CHECK


#ifdef LEA_MALLOC_API_CONSTRAIN

/**
 * Returns true if we are linked against our lea malloc
 * (i.e. Opera didn't pick up the system allocator by accident)
 */
extern int is_using_opera_allocator(void);

/**
 * Gets the current limit on constrain allocated memory.
 * @return The limit or INT_MAX if no limit has been set.
 */
extern int get_alloc_limit(void);

/**
 * Limits constrain allocated memory.
 *
 * @param limit The new limit, INT_MAX to unset the limit.
 */
extern void set_alloc_limit(int limit);

/**
 * Gets the current heap size limit.
 * @return The limit or INT_MAX if no limit has been set.
 */
extern int get_heap_limit(void);

/**
 * Limits the heap size.
 *
 * @param limit The new limit, INT_MAX to unset the limit.
 */
extern void set_heap_limit(int limit);

/**
 * Gets the amount of memory that is currently constrain allocated.
 * @return The amount of constrain allocated memory.
 */
extern int get_constrain_allocated_mem(void);

/**
 * Gets the amount of memory currently used by the heap. This can grow
 * above the limit set by set_heap_limit when the normal, unconstrained
 * allocators are used. Includes special allocations like those made
 * with constrained_mmap.
 */
extern int get_heap_size(void);

/**
 * Does the same thing as malloc, but doesn't actually allocate the
 * memory. It is used to ensure that there is enough free memory before
 * calling a library function that is known to allocate memory.
 *
 * It tries if an allocation of the specified size would succeed
 * without increasing the heap past the limit. It does not take the
 * Opera allocation limit into account.
 *
 * Normally you should use fake_alloc() and fake_free() within Opera
 * code rather than try_alloc(), so that the Opera allocation count is
 * updated and the allocation limit is taken into account.
 *
 * @param nbytes The number of bytes you think the libary function is
 * going to allocate.
 *
 * @return 1 if the allocation would succeed, 0 if not.
 */
extern int try_alloc(size_t nbytes);

/**
 * The same as try_alloc(), but also take the Opera allocation limit
 * into account and increases the allocation count if the allocation
 * would succeed.
 *
 * This function is used to (roughly) account for memory that is
 * allocated by library functions, or other functions that uses the
 * non-constrained alloc functions.  The idea is that if Opera calls a
 * function that allocates non-constrained memory that memory should
 * also be counted as Opera allocated memory, otherwise Opera may end
 * up using a lot more memory than the alloc count reflects.
 *
 * The way to use this function is to first estimate how much memory
 * the library function will allocate (it can be difficult at times,
 * but for example for images it is often possible to do fairly
 * accurate estimates).  Then fake_alloc() that much memory. If
 * fake_alloc() succeeds (and the estimate is about right) it should
 * be safe to call the library function without the heap growing too
 * big. If fake_alloc() fails, on the other hand, you should not call
 * the library function and instead act is if you got an
 * OOM-situation.
 *
 * As with other allocations you must also free the fake allocated
 * memory at some point. To free the fake allocated memory you call
 * fake_free() with the same amount of memory that was fake
 * allocated. Only call fake_free() if fake_alloc() succeeded in the
 * first place.
 *
 * @param nbytes The number of bytes to fake allocate.
 *
 * @return 1 on success, 0 on failure.
 */
extern int fake_alloc(size_t nbytes);

/**
 * "Frees" fake allocated memory by adjusting the allocation count by
 * the given number of bytes.
 *
 * First fake_allocate() some "memory" then, when the memory that is
 * accounted for by the fake allocation has been freed, fake_free()
 * the same amount.
 *
 * You should only fake free "memory" that has been previously
 * successfully fake allocated, or the allocation count will be wrong.
 */
extern void fake_free(size_t nbytes);

/**
 * Similar to fake_alloc(), this function accounts for memory *outside*
 * the heap (e.g. mmap) that should be counted as Opera allocated memory.
 *
 * The call will succeed if the memory can be safely allocated without
 * exceeding either the allocation limit or the heap limit, increasing
 * the allocation counts appropriately. After this, you can proceed with
 * the actual allocation.
 *
 * If the call fails, you should act as if you encountered OOM and not
 * attempt the actual allocation.
 *
 * Note that you need to call external_free() for every successful
 * call to external_alloc() (e.g. even if the actual allocation fails).
 *
 * @param nbytes The number of bytes to fake allocate externally,
 *     including any required overhead.
 *
 * @return 1 on success, 0 on failure.
 */
extern int external_alloc(size_t nbytes);

/**
 * Reports that Opera memory outside the heap has been freed, allowing
 * Opera to adjust the allocation count.
 *
 * As with fake_free(), you should call this function if and only if
 * corresponding call to external_alloc() succeeds, or the allocation
 * count will be wrong.
 */
extern void external_free(size_t nbytes);

#ifdef VALGRIND
void* constrained_valgrind_new(size_t nbytes);
void* constrained_valgrind_anew(size_t nbytes);
#endif

/**
 * Type signatures of the constrained versions of malloc and friends.
 *
 * @see FEATURE_LEA_MALLOC_CONSTRAINED.
 */
void* constrained_malloc(size_t nbytes);
void* constrained_calloc(size_t nobj, size_t objsize);
void* constrained_realloc(void *obj, size_t nbytes);
void* constrained_memalign(size_t alignment, size_t n);
char* constrained_strdup(const char* s);

#endif /* LEA_MALLOC_API_CONSTRAIN */

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* LEA_MALLOC_H */
