/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef LEA_MONOBLOCK_H
#define LEA_MONOBLOCK_H __FILE__
# ifdef LEA_MALLOC_MONOBLOCK // API_DLM_SINGLE_BLOCK

// TWEAK_MEMORY_USE_LOCKING (MEMORY_USE_LOCKING) is equivalent to
// TWEAK_LEA_MALLOC_LOCK (USE_MALLOC_LOCK)
#if defined(MEMORY_USE_LOCKING) && !defined(USE_MALLOC_LOCK)
# define USE_MALLOC_LOCK
#endif // MEMORY_USE_LOCKING && !USE_MALLOC_LOCK

/** Single-block allocation manager.
 *
 * You need to obtain a lump of memory of known size and proper alignment,
 * from wherever you will; then new (lump) SingleBlockHeap(size) shall return
 * an object of this class, which shall manage your lump of memory for you.
 * The object itself, and its internal malloc state structure, are "allocated"
 * out of the lump during construction; the remainder constitutes the single
 * block of memory managed by the resulting object (and whose address and size
 * are reported by GetBlock{Start,Size}() if you import API_DLM_MEMORY_QUERY).
 *
 * Methods malloc, free, calloc and realloc behave exactly like the eponymous
 * functions of the standard C library, managing the block.
 *
 * Note that it is NOT a good idea to inherit from this class. Because of its
 * peculiar initialization, a subclass whose instance is not exactly the same
 * size as a SingleBlockHeap instance will likely have insufficient space
 * allocated at construction time (see the SingleBlockHeap constructor -
 * especially its initializer list - for the details). Furthermore, adding
 * virtuals will probably cause an unbearable performance hit due to accessing
 * the vtable on every method call.
 */
class SingleBlockHeap
{
	struct malloc_state *const m_state;
	void *const m_block;
	size_t const m_block_size;

	bool m_oom;

	// Internal functions wrapped by internal wrappers:
#ifdef USE_MALLOC_LOCK
	void* mALLOc(size_t space);
	void fREe(void* ptr);
	void* cALLOc(size_t count, size_t each);
	void* rEALLOc(void* ptr, size_t space);
#endif // USE_MALLOC_LOCK
	void* sYSMALLOc(size_t, struct malloc_state *);

#ifdef LEA_MALLOC_EXTERN_CHECK
	void do_check_chunk(struct malloc_chunk *);
	void do_check_free_chunk(struct malloc_chunk *);
	void do_check_inuse_chunk(struct malloc_chunk *);
	void do_check_remalloced_chunk(struct malloc_chunk *, size_t);
	void do_check_malloced_chunk(struct malloc_chunk *, size_t);
	void do_check_malloc_state();
#endif // #if-ery must match conditioning on LEA_MALLOC_CHECKING in .cpp

public:
	/** Replacement operator new for placing the SingleBlockHeap object at
	 *  the given pointer. Since no actual allocation is done, the
	 *  corresponding operator delete must not deallocate any memory.
	 */
	void *operator new (size_t size, void *chunk);
	void operator delete (void *chunk) {/* deliberate no-op */}

	SingleBlockHeap(size_t size);
	~SingleBlockHeap();

	void* malloc(size_t space);
	void free(void* ptr);
	void* calloc(size_t count, size_t each);
	void* realloc(void* ptr, size_t space);

	void ClearError() { m_oom = false; }
	bool GetError() const { return m_oom; }

	/** Query memory overhead associated with an instance of this class.
	 *
	 * Return the overhead associated an instance of this class. This
	 * includes the size of the object itself and the size of lea_malloc's
	 * internal malloc_state struct.
	 *
	 * Formally, the value returned is the difference in size between the
	 * block of memory given to the constructor, and the resulting sub-block
	 * of memory that is managed by the embedded instance of lea_malloc.
	 *
	 * Note that the overhead associated with allocations made _within_ the
	 * embedded lea_malloc is _not_ calculated by this method.
	 *
	 * @return The memory overhead used by an instance of this class.
	 */
	static size_t GetOverhead();

	/** Query total amount of memory in this heap.
	 *
	 * @return The total amount of memory in this heap.
	 */
	size_t GetCapacity() const;

	/** Query free memory available.
	 *
	 * @return The amount of memory currently free.
	 */
	size_t GetFreeMemory() const;

	/** Query whether the given pointer belongs on this heap.
	 *
	 * @return true iff the given pointer is within the memory area managed
	 *         by this heap.
	 */
	bool Contains(void *ptr) const;

#ifdef LEA_MALLOC_EXTERN_CHECK
	/** Perform expensive global check of internal state sanity.
	 *
	 * This function OP_ASSERT()s various conditions which it expects to be
	 * true, in the course of traversing the module's global data structures
	 * (i.e. the free list).
	 *
	 * This check is only fully effective if you enable OP_ASSERT, since it
	 * checks each detail. However, the mere fact of traversing the memory
	 * data structures does also serve as a check, since it is apt to turn
	 * any memory abuse into a crash relatively soon after the abuse, rather
	 * than at a random later time. (Please see the memory module for more
	 * sophisticated memory debugging tools.)
	 *
	 * Available when API_DLM_CHECK is imported.
	 */
	void CheckState() { do_check_malloc_state(); }
#endif // LEA_MALLOC_EXTERN_CHECK
};

# endif // LEA_MALLOC_MONOBLOCK
#endif // LEA_MONOBLOCK_H
