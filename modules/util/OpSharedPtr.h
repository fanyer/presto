#ifndef MODULES_UTIL_OPSHAREDPTR_H // -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
#define MODULES_UTIL_OPSHAREDPTR_H
#include "OpSharedPtr_internals.h" // For SharedBlockHeap and SharedBlockForMakeShared

/** Implementation of a reference-counted shared pointer.
 *
 * In contrast to OpSmartPointer*, this implementation is not intrusive, i.e.
 * T does not have to implement any special interfaces. T can also be a simple
 * type.
 *
 * The tracked object will be deleted (OP_DELETE) after the reference
 * count reaches zero.
 *
 * OpSharedPtr<Derived> can be implicitly converted to OpSharedPtr<Base> if Derived*
 * can be implicitly converted to Base*.
 *
 * For more information, search for shared_ptr (either boost, tr1 or C++0x standard).
 * This implementation behaves like any typical shared pointer. It's simpler though,
 * so custom allocators are not supported (OP_NEW and OP_DELETE are used).
 * There is also no weak pointer functionality.
 *
 * If you're concerned about doing twice as many allocations (one for the object
 * itself, second for the OpSharedPtr's shared reference count block) use make_shared
 * to create the object and the pointer's internals in a single allocation. This
 * is as effective as using an intrusive smart pointer.
 *
 * At the time of writing, OpSharedPtr is not thread-safe! Avoid sharing pointers
 * across thread boundaries. If a need arises to make this thread-safe, reimplement
 * the increment and decrement-and-check parts to be atomic and figure out if the
 * deallocation logic is Ok with potentially deleting something that was allocated
 * in a different thread.
 */
template<typename T> class OpSharedPtr
{
private:
	// These are used in the safe bool idiom. See description of operator bool_t() const.
	typedef void (OpSharedPtr::*bool_t)() const;
	void bool_conversion() const {}
public:

	/** Create an empty OpSharedPtr.
	 *
	 * An empty OpSharedPtr doesn't manage any object's lifetime. It can later receive
	 * an object to manage through an assignment operator or a reset, but it's
	 * pretty useless at this point.
	 */
	OpSharedPtr() : m_sharedBlock(NULL)
	{}

	/** Create an OpSharedPtr and have it manage the lifetime of @a p.
	 *
	 * @a p's lifetime will now be managed by this OpSharedPtr.
	 *
	 * A single extra allocation occurs for the shared reference counter.
	 * If this allocation fails, @a p is deleted and the OpSharedPtr becomes empty.
	 *
	 * @note Do not attempt to use @a p directly after constructing OpSharedPtr
	 * even if construction fails.
	 *
	 * For a more efficient way of creating OpSharedPtrs, see make_shared.
	 *
	 * @a p may be a class derived from T.
	 */
	template<typename Y>
	explicit OpSharedPtr(Y * p) : m_sharedBlock(OP_NEW(SharedBlockHeap<T>, (p)))
	{
		if(!m_sharedBlock)
			OP_DELETE(p);
	}

	/** Decrement reference count and optionally delete the managed object.
	 *
	 * Decrements the reference count and, if it reaches zero, deletes the managed
	 * object.
	 */
	~OpSharedPtr()
	{
		if(m_sharedBlock && --m_sharedBlock->refCount == 0)
			OP_DELETE(m_sharedBlock);
	}

	/** Copy constructor.
	 *
	 * After calling, both @a r and this will share management over lifetime of
	 * the object. @a r can be of different type than T as long as it's convertible
	 * through static_cast.
	 */
	OpSharedPtr(OpSharedPtr const & r) : m_sharedBlock(r.m_sharedBlock)
	{
		incrementRefCount();
	}

	/** @overload OpSharedPtr(OpSharedPtr const & r) */
	template<typename Y>
	OpSharedPtr(OpSharedPtr<Y> const & r)
	{
		if(T* a = static_cast<Y*>(0)) a = 0; // Will not compile if Y is not a subtype of T
		m_sharedBlock = reinterpret_cast<SharedBlock<T>*>(r.m_sharedBlock);
		incrementRefCount();
	}

	/** Assign different OpSharedPtr to this.
	 *
	 * After calling, both @a r and this will share management over lifetime of
	 * the object. @a r can be of different type than T as long as it's convertible
	 * through static_cast.
	 */
	OpSharedPtr & operator=(OpSharedPtr const & r)
	{
		OpSharedPtr(r).swap(*this);
		return *this;
	}

	/** @overload OpSharedPtr & operator=(OpSharedPtr const & r) */
	template<typename Y>
	OpSharedPtr & operator=(OpSharedPtr<Y> const & r)
	{
		OpSharedPtr(r).swap(*this);
		return *this;
	}

	/** Drop the managed object.
	 *
	 * This may call OP_DELETE on the object if this is the last owner.
	 * After calling, the OpSharedPtr is empty.
	 */
	void reset()
	{
		OpSharedPtr().swap(*this);
	}

	/** Manage the lifetime of a different object now.
	 *
	 * The new object cannot be the same one as current. Nor can it be under
	 * control of any OpSharedPtr.
	 * Will call OP_DELETE on the old object if needed.
	 *
	 * @param p Pointer to new object to manage. If NULL, behaves as reset().
	 */
	template<typename Y>
	void reset(Y * p)
	{
		// Cannot reset to self but can reset to NULL
		OP_ASSERT( !m_sharedBlock || p != m_sharedBlock->object() || p == NULL);
		OpSharedPtr(p).swap(*this);
	}

	/** Dereference the managed object.
	 *
	 * The caller is responsible for assuring the OpSharedPtr is not empty (ex.
	 * by calling get()), as a null-dereference may occur just like with a normal
	 * pointer. Do not attempt to store the address of the result in a scope
	 * that may outlive the OpSharedPtr().
	 *
	 * Use get() to check.
	 * @return Reference to the managed object.
	 */
	T & operator*() const
	{
		OP_ASSERT(m_sharedBlock && m_sharedBlock->object());
		return *(m_sharedBlock->object());
	}

	/** Dereference the managed object to get a field.
	 *
	 * Works as the arrow operator on an ordinary pointer.
	 * The caller is responsible for assuring the OpSharedPtr is not empty (ex.
	 * by calling get()), as a null-dereference may occur just like with a normal
	 * pointer.
	 * @return Reference to the managed object.
	 */
	T * operator->() const
	{
		OP_ASSERT(m_sharedBlock && m_sharedBlock->object());
		return m_sharedBlock->object();
	}

	/** Get the address of the managed object.
	 *
	 * Do not attempt to delete the result or store it in a scope that may
	 * outlive the OpSharedPtr.
	 * @return Address of the managed object, NULL if the OpSharedPtr is empty.
	 */
	T * get() const
	{
		return m_sharedBlock ? m_sharedBlock->object() : NULL;
	}

	/** Swap pointers.
	 *
	 * No copy or assignment is performed on the managed objects.
	 *
	 * @param b OpSharedPtr with which the object pointer and reference count
	 * is exchanged.
	 */
	void swap(OpSharedPtr & b)
	{
		SharedBlock<T>* temp = m_sharedBlock;
		m_sharedBlock = b.m_sharedBlock;
		b.m_sharedBlock = temp;
	}

	/** Conversion operator for use in conditional statements.
	 *
	 * This allows using an OpSharedPtr in a conditional statement like
	 * a normal pointer, ex.:
	 * @code
	 * OpSharedPtr<T> sp;
	 * ...
	 * if(sp)
	 *	// sp can be dereferenced
	 * if (!sp)
	 *	// sp is empty
	 * @endcode
	 *
	 * @note This is an implementatino of the Safe Bool Idiom -
	 * http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Safe_bool. It may seem
	 * non-intuitive but is an accepted way of providing boolean tests without
	 * compromising type-safety and introducing unwanted side effects.
	 *
	 * @return An quivalent of true if this OpSharedPtr manages an object and
	 * false if it's empty.
	 */
	operator bool_t() const
	{
		return get() ? &OpSharedPtr::bool_conversion : 0;
	}

private:
	/** Special constructor for make_shared.
	 *
	 * Creates an OpSharedPtr and sets its shared block to @a sharedBlock.
	 * No allocation is done by OpSharedPtr. SharedBlockForMakeShared is
	 * a type of shared block that contains the reference counter and the object
	 * itself in a single memory block created by one allocation.
	 * @param sharedBlock Shared block to use. May be null, in which case this
	 * OpSharedPtr will be empty.
	 */
	template<typename Y>
	explicit OpSharedPtr(SharedBlockForMakeShared<Y> * sharedBlock) :
		m_sharedBlock(sharedBlock)
	{}

	SharedBlock<T>* m_sharedBlock;

	void incrementRefCount()
	{
		if(m_sharedBlock)
		{
			m_sharedBlock->refCount++;
		}
	}

	// For implicit conversion.
	template<typename Y> friend class OpSharedPtr;

	// make_shared accesses our special constructor.
	template<typename Y> friend
	OpSharedPtr<Y> make_shared();

	template<typename Y, typename A1> friend
	OpSharedPtr<Y> make_shared(A1 const & a1);

	template<typename Y, typename A1, typename A2> friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2);

	template<typename Y, typename A1, typename A2, typename A3> friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3);

	template<typename Y, typename A1, typename A2, typename A3, typename A4>
	friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
							   A4 const & a4);

	template<typename Y, typename A1, typename A2, typename A3, typename A4,
			 typename A5>
	friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
							   A4 const & a4, A5 const & a5);

	template<typename Y, typename A1, typename A2, typename A3, typename A4,
			 typename A5, typename A6>
	friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
							   A4 const & a4, A5 const & a5, A6 const & a6);

	template<typename Y, typename A1, typename A2, typename A3, typename A4,
			 typename A5, typename A6, typename A7>
	friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
							   A4 const & a4, A5 const & a5, A6 const & a6,
							   A7 const & a7);

	template<typename Y, typename A1, typename A2, typename A3, typename A4,
			 typename A5, typename A6, typename A7, typename A8>
	friend
	OpSharedPtr<Y> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
							   A4 const & a4, A5 const & a5, A6 const & a6,
							   A7 const & a7, A8 const & a8);
};

// Equality and comparison operators.

template<typename T, typename U>
bool operator==(OpSharedPtr<T> const & a, OpSharedPtr<U> const & b)
{
	return a.get() == b.get();
}

template<typename T, typename U>
bool operator!=(OpSharedPtr<T> const & a, OpSharedPtr<U> const & b)
{
	return a.get() != b.get();
}

// For sorted containers and the equivalence relation !(a < b) && !(b < a)
template<typename T, typename U>
bool operator<(OpSharedPtr<T> const & a, OpSharedPtr<U> const & b)
{
	return a.get() < b.get();
}



// Reference wrapper for forwarding non-const references to make_shared

template<typename T>
struct ReferenceWrapper
{
	ReferenceWrapper(T& t) : ref(t) {}
	operator T& () const { return ref; }
	T& ref;
};

template<class T> inline ReferenceWrapper<T> const Ref(T & t)
{
	return ReferenceWrapper<T>(t);
}

template<class T> inline ReferenceWrapper<T const> const CRef(T const & t)
{
	return ReferenceWrapper<T const>(t);
}

// make_shared implementations.
// I wish we had variadic templates...

template<typename T>
OpSharedPtr<T> make_shared()
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>, ()));
}

template<typename T, typename A1>
OpSharedPtr<T> make_shared(A1 const & a1)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>, (a1)));
}

template<typename T, typename A1, typename A2>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>, (a1, a2)));
}

template<typename T, typename A1, typename A2, typename A3>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>, (a1, a2, a3)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
						   A4 const & a4)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>, (a1, a2, a3, a4)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4,
		 typename A5>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
						   A4 const & a4, A5 const & a5)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>,
								 (a1, a2, a3, a4, a5)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4,
		 typename A5, typename A6>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
						   A4 const & a4, A5 const & a5, A6 const & a6)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>,
								 (a1, a2, a3, a4, a5, a6)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4,
		 typename A5, typename A6, typename A7>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
						   A4 const & a4, A5 const & a5, A6 const & a6,
						   A7 const & a7)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>,
								 (a1, a2, a3, a4, a5, a6, a7)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4,
		 typename A5, typename A6, typename A7, typename A8>
OpSharedPtr<T> make_shared(A1 const & a1, A2 const & a2, A3 const & a3,
						   A4 const & a4, A5 const & a5, A6 const & a6,
						   A7 const & a7, A8 const & a8)
{
	return OpSharedPtr<T>(OP_NEW(SharedBlockForMakeShared<T>,
								 (a1, a2, a3, a4, a5, a6, a7, a8)));
}


#endif // MODULES_UTIL_OPSHAREDPTR_H

