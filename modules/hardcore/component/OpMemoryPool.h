/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPMEMORYPOOL_H
#define MODULES_HARDCORE_COMPONENT_OPMEMORYPOOL_H

/**
 * OpMemoryPool - Templated memory pool for objects of a constant size.
 *
 *          !!!NOTE!!! THIS IS NOT THREAD-SAFE !!!NOTE!!!
 *
 * Simple memory pool implementation for storing objects of size OBJ_SIZE with
 * alignment OBJ_ALIGN in pools of size POOL_SIZE (16 kB by default). The pool
 * uses a free list to traverse all unused objects in the allocated memory area.
 * (De)allocation adds/removed objects to/from the front of this list.
 * Objects are therefore reused in MRU order.
 *
 * The size of a pool is set by the POOL_SIZE constant. Some overhead and
 * padding is subtracted from this to get the NET_POOL_SIZE, which is the
 * actual number of bytes available to store pooled objects.
 *
 * The overhead/administrivia consists of:
 *   - m_next: Pointer to next memory pool in list of pools.
 *   - m_used: The number of currently allocated objects in this pool.
 *   - m_free: The free list.
 *
 * When a memory pool is filled, several pools can be strung together using the
 * GetNext()/SetNext() methods.
 *
 * Obviously the length of the free list + m_used should always equal POOL_OBJS.
 *
 * === An additional note on thread-safety. ===
 *
 * If you want to use OpMemoryPool in a multithreaded environment you will have
 * to use thread-local pools and ensure that they are allocated and freed in the
 * same thread.
 *
 * === Example of use ===
 *
 *  An example using the OpMemoryPool can be found with the macro @ref #OP_USE_MEMORY_POOL_DECL
 */
template<
	size_t OBJ_SIZE,
	size_t OBJ_ALIGN = sizeof(double),
	size_t POOL_SIZE = 16 * 1024 // 16 kB
>
class OpMemoryPool
{
	/// declares the type of this memory pool:
	typedef OpMemoryPool<OBJ_SIZE, OBJ_ALIGN, POOL_SIZE> OpMemoryPoolType;

public:
	/// \#bytes needed for overhead
	static const unsigned short POOL_OVERHEAD =
		sizeof(OpMemoryPool*) + sizeof(void*) + sizeof(unsigned short);

	/// \#bytes needed for padding between overhead and data area
	static const unsigned short POOL_PADDING =
		((OBJ_ALIGN*POOL_OVERHEAD - POOL_OVERHEAD) % OBJ_ALIGN);

	/// \#bytes available to store pooled objects ().
	static const size_t NET_POOL_SIZE =
		POOL_SIZE - POOL_OVERHEAD - POOL_PADDING;

	/// \#bytes used for each single object
	static const size_t OBJ_ALIGNED_SIZE =
		OBJ_SIZE + (OBJ_ALIGN*OBJ_SIZE - OBJ_SIZE) % OBJ_ALIGN;

#if defined(DEBUG_ENABLE_OPASSERT) || defined(HC_MEMORY_POOL_SELFTEST_ENABLED)
	/// Actual \#objects that can be stored in the pool
	static const size_t POOL_OBJS = NET_POOL_SIZE / OBJ_ALIGNED_SIZE;
#endif // DEBUG_ENABLE_OPASSERT or HC_MEMORY_POOL_SELFTEST_ENABLED

	/// Constructor.
	OpMemoryPool(OpMemoryPool* next = NULL)
		: m_next(next), m_used(0)
	{
		/* Note: the size of this class may be larger than POOL_SIZE if the
		 * compiler adds padding for this instance. So assume that the maximum
		 * padding is 8 bytes (64bits): */
		OP_ASSERT(sizeof(*this) <= (POOL_SIZE + (8 * POOL_SIZE-POOL_SIZE) % 8));
		OP_ASSERT(sizeof(void*) <= OBJ_ALIGNED_SIZE || !"Otherwise we can't store the free list");
		OP_ASSERT(POOL_OBJS > 100);
		OP_ASSERT(reinterpret_cast<INTPTR>(GetMem()) % OBJ_ALIGN == reinterpret_cast<INTPTR>(this) % OBJ_ALIGN);

		// Construct initial free list (chain everything together)
		char* a;
		// Loop until the next-to-last item
		for (a = GetMem(); a <= GetMem() + NET_POOL_SIZE - 2 * OBJ_ALIGNED_SIZE; a += OBJ_ALIGNED_SIZE)
			ResetMemory(reinterpret_cast<void*>(a), reinterpret_cast<void*>(a + OBJ_ALIGNED_SIZE));
		ResetMemory(a, NULL); // Reset the last item
		m_free = GetMem();

#ifdef DEBUG_ENABLE_OPASSERT
		// Assert that #objects on free list == POOL_OBJS
		size_t i = 0;
		void* p = m_free;
		while (p)
		{
			OP_ASSERT(IsDead(p));
			i++;
			p = *reinterpret_cast<void**>(p);
		}
		OP_ASSERT(i == POOL_OBJS);
#endif // DEBUG_ENABLE_OPASSERT
	}

	~OpMemoryPool(void)
	{
		OP_ASSERT(GetUsed() == 0);
#ifdef DEBUG_ENABLE_OPASSERT
		size_t i = 0;
		void* p = m_free;
		while (p)
		{
			i++;
			p = *reinterpret_cast<void**>(p);
		}
		OP_ASSERT(i == POOL_OBJS);
#endif // DEBUG_ENABLE_OPASSERT
	}

	OpMemoryPool* GetNext(void) const { return m_next; }
	void SetNext(OpMemoryPool* next) { m_next = next; }

	size_t GetUsed(void) const { return m_used; }
	size_t GetObjSize(void) const { return OBJ_ALIGNED_SIZE; }

	bool Contains(void* ptr) const
	{
		return (GetMem() <= ptr && ptr < GetMem() + NET_POOL_SIZE);
	}

	void* Allocate(bool allow_growing=false)
	{
		void* ret = NULL;
		if (m_free)
		{
			ret = m_free;
			OP_ASSERT(IsDead(ret));
			m_free = *reinterpret_cast<void**>(ret);
			m_used++;
		}
		else if (m_next)
			ret = m_next->Allocate(allow_growing);
		else if (allow_growing)
		{
			/* FIXME: Use memory module functionality for allocating a new
			 * memory pool? It may be better to allocated memory pools using
			 * mmap() (or similar functionality), and not OP_NEW() on the
			 * heap. */
			m_next = OP_NEW(OpMemoryPoolType, ());
			if (m_next)
				ret = m_next->Allocate();
		}
		OP_ASSERT(m_used > 0 && m_used <= POOL_OBJS);
		return ret;
	}

	void* AllocateL(bool allow_growing=false)
	{
		void* ret = Allocate(allow_growing);
		LEAVE_IF_NULL(ret);
		return ret;
	}

	bool Deallocate(void* ptr)
	{
		if (!ptr) return true;
		else if (Contains(ptr))
		{
			OP_ASSERT(!IsFree(ptr));
			ResetMemory(ptr, m_free);
			m_free = ptr;
			m_used--;
			OP_ASSERT(m_used < POOL_OBJS);
			return true;
		}
		else if (m_next && m_next->Deallocate(ptr))
		{
			if (m_next->GetUsed() == 0)
			{
				OpMemoryPoolType* empty_pool = m_next;
				m_next = m_next->m_next;
				OP_DELETE(empty_pool);
			}
			return true;
		}
		OP_ASSERT(!"The specified pointer was not allocated from this pool!");
		return false;
	}

private:
#ifdef DEBUG_ENABLE_OPASSERT
	/** Returns true if the specified ptr is in the list of free pointers. */
	bool IsFree(void* ptr) const
	{
		for (void* p = m_free; p; p = *reinterpret_cast<void**>(p))
		{
			OP_ASSERT(Contains(p));
			if (p == ptr) return true;
		}
		return false;
	}

	/** Returns true if the specified pointer is dead, i.e. if it was either not
	 * yet allocated, or de-allocated and if the memory was not overwritten.
	 * Initialising the memory-pool and de-allocating memory writes the bytes
	 * 0xdead into the memory that is not used by the next free pointer and this
	 * function tests if those bytes are still there. This makes only sense if
	 * OBJ_ALIGNED_SIZE > sizeof(void*). */
	bool IsDead(void* ptr) const
	{
		if (!Contains(ptr)) return false;
		unsigned char* x = reinterpret_cast<unsigned char*>(ptr);
		for (size_t i = sizeof(void*); i < OBJ_ALIGNED_SIZE; ++i)
			if (x[i] != ((i % 2) ? 0xad : 0xde))
				return false;
		return true;
	}
#endif // DEBUG_ENABLE_OPASSERT

	void ResetMemory(void* ptr, void* next)
	{
		OP_ASSERT(Contains(ptr) && (next == NULL || Contains(next)));
		*reinterpret_cast<void**>(ptr) = next;
#ifdef DEBUG_ENABLE_OPASSERT
		unsigned char* x = reinterpret_cast<unsigned char*>(ptr);
		for (size_t i = sizeof(void*); i < OBJ_ALIGNED_SIZE; ++i)
			x[i] = ((i % 2) ? 0xad : 0xde);
#endif // DEBUG_ENABLE_OPASSERT
	}

protected:
	char* GetMem(void) { return m_mem + POOL_PADDING; }
	const char* GetMem(void) const { return m_mem + POOL_PADDING; }
	OpMemoryPool* m_next;
	void* m_free;
	unsigned short m_used;
	char m_mem[POOL_PADDING + NET_POOL_SIZE];	// ARRAY OK 2011-09-05 markuso
};

/**
 * Declaration of operator new/delete to let a class use an OpMemoryPool.
 *
 * This macro can be used inside a class declaration to declare the operator new
 * and delete. These operators are implemented by the macro
 * OP_USE_MEMORY_POOL_IMPL() to use an OpMemoryPool instance for allocation and
 * de-allocation. With these operators a class-instance can be created by using
 * OP_NEW() or OP_NEW_L(). The instance can be deleted by using OP_DELETE().
 *
 * The array operators are declared as private, i.e. it will not be possible to
 * create an array of class instances because the OpMemoryPool does not support
 * this.
 *
 * Example:
 * @code
 * class SomeElement {
 *     OP_USE_MEMORY_POOL_DECL;
 * public:
 *    SomeElement(...) ...;
 * };
 *
 * class MyModule {
 * private:
 *     OpMemoryPool<sizeof(SomeElement)>* some_pool;
 *     ...
 * public:
 *     OpMemoryPool<sizeof(SomeElement)>* GetSomePool() { return some_pool; }
 * };
 *
 * OP_USE_MEMORY_POOL_IMPL(g_opera->my_module->GetSomePool(), SomeElement);
 *
 * ...
 * SomeElement* element = OP_NEW(SomeElement, ());
 * OP_ASSERT(g_opera->my_module->GetSomePool()->Contains(element));
 * ...
 * OP_DELETE(element);
 * @endcode
 */
#define OP_USE_MEMORY_POOL_DECL											\
private:																\
	/* Don't use array allocator on pooled objects: */					\
	void* operator new[](size_t) OP_NOTHROW;							\
	void* operator new[](size_t, TLeave);								\
	void* operator new[](size_t size, TOpAllocNewDefaultA,				\
						 const char* file, int line, const char* obj,	\
						 unsigned int count1, unsigned int count2,		\
						 size_t objsize) OP_NOTHROW;					\
	void* operator new[](size_t size, TOpAllocNewDefaultA_L,			\
						 const char* file, int line, const char* obj,	\
						 unsigned int count1, unsigned int count2,		\
						 size_t objsize);								\
public:																	\
	void* MEMORY_SIGNATURE_NEW;											\
	void* MEMORY_SIGNATURE_NEW_L;										\
	void operator delete(void* ptr)

/**
 * Implementation of operator new/delete to let a class use an OpMemoryPool.
 *
 * Use this macro to implement the operator new/delete. These operator will use
 * the specified OpMemoryPool instance to allocate or deallocate instances of
 * the specified ObjType. This macro must be used together with
 * OP_USE_MEMORY_POOL_DECL.
 *
 * @param pool Pointer to an OpMemoryPool which uses an object size that is big
 *  enough for the specified class.
 * @param ObjType is a struct or class name in which the OP_USE_MEMORY_POOL_DECL
 *  was used to declare the operator new/delete.
 */
#define OP_USE_MEMORY_POOL_IMPL(pool, ObjType)							\
	void* ObjType::MEMORY_SIGNATURE_NEW									\
	{																	\
		OP_ASSERT(pool);												\
		OP_ASSERT(size <= pool->GetObjSize());							\
		return pool ? pool->Allocate(true) : NULL;						\
	}																	\
	void* ObjType::MEMORY_SIGNATURE_NEW_L								\
	{																	\
		OP_ASSERT(pool);												\
		OP_ASSERT(size <= pool->GetObjSize());							\
		LEAVE_IF_NULL(pool);											\
		return pool->AllocateL(true);									\
	}																	\
	void ObjType::operator delete(void* ptr)							\
	{																	\
		if (ptr != 0)													\
		{																\
			if (!pool || !pool->Deallocate(ptr))						\
				OP_ASSERT(!"This instance is not allocated by the pool"); \
		}																\
	}

#endif /* MODULES_HARDCORE_COMPONENT_OPMEMORYPOOL_H */
