#ifndef MODULES_UTIL_OPSHAREDPTR_INTERNALS // -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
#define MODULES_UTIL_OPSHAREDPTR_INTERNALS

/** @file Implementation details of OpSharedPtr can be found here, you shouldn't
 * need to look here unless you're fixing a bug or changing OpSharedPtr's implementation.
 */

/** Shared reference-count block for OpSharedPtr.
 *
 * The control block shared between all OpSharedPtr instances that manage the same
 * object.
 */
template<typename T>
class SharedBlock
{
public:
	SharedBlock() : refCount(1) {}
	virtual ~SharedBlock() {}

	virtual T* object() = 0;
	UINT32 refCount;
};

/** Shared block with a pointer to managed object allocated on heap.
 *
 * This is the block that's being used when OpSharedPtr is created with
 * a typical constructor. The managed object is allocated separately, by the
 * user, and passed to this shared block for keeping.
 */
template<typename T>
class SharedBlockHeap : public SharedBlock<T>
{
public:
	template<typename Y>
	SharedBlockHeap(Y* obj) :
		SharedBlock<T>(),
		m_objectPtr(obj)
	{}

	virtual ~SharedBlockHeap()
	{
		OP_DELETE(m_objectPtr);
	}

	virtual T* object()
	{
		return m_objectPtr;
	}

private:
	T* m_objectPtr;
};

/** Shared block with an embedded object.
 *
 * This is the block that's being used when OpSharedPtr is created with
 * make_shared. The managed object and this block are allocated together
 * in a single memory block for efficiency.
 */
template<typename T>
class SharedBlockForMakeShared : public SharedBlock<T>
{
public:
	SharedBlockForMakeShared() :
		SharedBlock<T>(),
		m_object()
	{}

	template<typename A1>
	SharedBlockForMakeShared(A1 const & a1) :
		SharedBlock<T>(),
		m_object(a1)
	{}

	template<typename A1,
			 typename A2>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2) :
		SharedBlock<T>(),
		m_object(a1, a2)
	{}

	template<typename A1,
			 typename A2,
			 typename A3>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3) :
		SharedBlock<T>(),
		m_object(a1, a2, a3)
	{}

	template<typename A1,
			 typename A2,
			 typename A3,
			 typename A4>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3,
							 A4 const & a4) :
		SharedBlock<T>(),
		m_object(a1, a2, a3, a4)
	{}

	template<typename A1,
			 typename A2,
			 typename A3,
			 typename A4,
			 typename A5>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3,
							 A4 const & a4,
							 A5 const & a5) :
		SharedBlock<T>(),
		m_object(a1, a2, a3, a4, a5)
	{}

	template<typename A1,
			 typename A2,
			 typename A3,
			 typename A4,
			 typename A5,
			 typename A6>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3,
							 A4 const & a4,
							 A5 const & a5,
							 A6 const & a6) :
		SharedBlock<T>(),
		m_object(a1, a2, a3, a4, a5, a6)
	{}

	template<typename A1,
			 typename A2,
			 typename A3,
			 typename A4,
			 typename A5,
			 typename A6,
			 typename A7>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3,
							 A4 const & a4,
							 A5 const & a5,
							 A6 const & a6,
							 A7 const & a7) :
		SharedBlock<T>(),
		m_object(a1, a2, a3, a4, a5, a6, a7)
	{}

	template<typename A1,
			 typename A2,
			 typename A3,
			 typename A4,
			 typename A5,
			 typename A6,
			 typename A7,
			 typename A8>
	SharedBlockForMakeShared(A1 const & a1,
							 A2 const & a2,
							 A3 const & a3,
							 A4 const & a4,
							 A5 const & a5,
							 A6 const & a6,
							 A7 const & a7,
							 A8 const & a8) :
		SharedBlock<T>(),
		m_object(a1, a2, a3, a4, a5, a6, a7, a8)
	{}

	virtual T* object()
	{
		return &m_object;
	}

private:
	T m_object;
};

#endif // MODULES_UTIL_OPSHAREDPTR_INTERNALS
