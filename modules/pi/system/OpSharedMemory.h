/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PI_SYSTEM_OPSHAREDMEMORY_H
#define PI_SYSTEM_OPSHAREDMEMORY_H
#ifdef PI_SHARED_MEMORY
#include "modules/util/opautoptr.h"

/**
 * A porting interface for handling shared memory across processes.
 *
 * Shared memory is created by one process and can be opened by many other
 * processes.
 * Example usage:
 * @code
class MySharedClass
{
	// Some content...
};

const size_t N = 10;

// Creating the memory:
OpSharedMemory* memory = NULL;
RETURN_IF_ERROR(OpSharedMemory::Create(sizeof(MySharedClass) * N, &memory));

// Construct some classes on shared memory using placement new.
MySharedClass* myClasses = new (memory->Ptr()) MySharedClass[N];

// Set cleanup functor[*]
memory->SetCleanupFunctor(
	OpAutoPtr<OpSharedMemory::CleanupFunctor>(OP_NEW(MyArrayCleanupFunctor, (N))))

// Use myClasses[x]...

// Opening the memory in a different process:
OpSharedMemory* memory = NULL;
RETURN_IF_ERROR(OpSharedMemory::Open(identifier, &memory));
MySharedClass* myClasses = (MySharedClass*)memory->Ptr();

// Set cleanup functor[*]
// Same functor set in both processes as either may need to clean up.
memory->SetCleanupFunctor(
	OpAutoPtr<OpSharedMemory::CleanupFunctor>(OP_NEW(MyArrayCleanupFunctor, (N))))

// Use myClasses[x]...

//[*] Cleanup functor:
class MyArrayCleanupFunctor : public OpSharedMemory::CleanupFunctor
{
public:
	MyArrayCleanupFunctor(size_t arraySize) :
		m_arraySize(arraySize)
	{}

	virtual void Cleanup(void* mem) const
	{
		MySharedClass* myClasses = (MySharedClass*)mem;
		size_t i = m_arraySize;
		while(i)
			myClasses[--i].~MySharedClass();
		// Note the inverse destruction order to preserve
		// canonical destruction order of C++
	}
private:
	size_t m_arraySize;
};
 * @endcode
 *
 * @note Only non-polymorphic classes may be placed within shared memory!
 * V-table pointers will be invalid in different processes.
 * @note Classes which allocate internal objects using OP_NEW or OP_ALLOC or
 * hold process-specific handles or objects (ex. pointers to objects allocated
 * within the process) should not be placed within shared memory. Process-specific
 * handles and pointers will be invalid in other processes.
 * @attention Shared memory may be a scarce resource on many platforms.
 * Performance-wise, it's better to allocate few big blocks than many small ones.
 * Try to group your shared objects into batches and share them within a single
 * OpSharedMemory object.
 */
class OpSharedMemory
{
public:
	typedef OpData Identifier;

	/** Will be called once to clean up the shared memory just before freeing it
	 * by the platform.
	 * The argument is a void* to the memory. A typical cleanup functor will cast
	 * the pointer to actual type of the object that is being shared and call the
	 * object's destructor.
	 * Note that the memory will be	freed by OpSharedMemory so do not attempt to
	 * OP_DELETE or OP_FREE it. Manually invoke the destructor instead.
	 */
	class CleanupFunctor
	{
	public:
		virtual void Cleanup(void* mem) const = 0;
		virtual ~CleanupFunctor() {}
	};

	virtual ~OpSharedMemory() { }

	/** Create/allocate a chunk of shared memory.
	 *
	 * This function must allocate at least size bytes and create a unique
	 * identifier that can be obtained with GetIdentifier().
	 *
	 * NOTE: The shared memory is reference counted, so the last instance to
	 * close it deletes the resource.
	 *
	 * @param size The minimum size of the shared memory to be allocated. This
	 * size MAY be rounded up for alignment reasons.
	 * @param[out] out A pointer to the shared memory handler. Only valid if OK
	 * is returned.
	 *
	 * @retval OK if the memory is created, and out is set to such a piece of
	 *         memory.
	 * @retval ERR_NO_MEMORY if out of memory.
	 * @retval ERR_NO_ACCESS if insufficient permissions to create the shared
	 *         memory.
	 * @retval ERR for unspecific errors.
	 */
	static OP_STATUS Create(size_t size, OpSharedMemory** out);

	/**
	 * Open a piece of memory shared by another process.
	 *
	 * NOTE: The shared memory is reference counted, so the last instance to
	 * close it deletes the resource.
	 *
	 * @param identifier An identifier for the shared memory. This must be
	 * obtained from Identifier() by the process who created the memory.
	 * @param[out] out A pointer to the shared memory handler. Only valid if OK
	 * is returned.
	 *
	 * @retval OK if the memory can be accessed and out is set to such a piece
	 * of memory.
	 * @retval ERR_NO_MEMORY if out of memory.
	 * @retval ERR_NO_ACCESS if insufficient permissions to access the shared
	 *         memory or if @a identifier has incompatible platformType.
	 * @retval ERR_FILE_NOT_FOUND if the memory identifier cannot be found.
	 *         Calling Create() would be a good idea at this point.
	 * @retval ERR for unspecific errors.
	 */
	static OP_STATUS Open(const Identifier& identifier, OpSharedMemory** out);

	/**
	 * Returns the size of the shared memory buffer.
	 *
	 * Is at least the size passed to Create(); may be bigger.
	 * All bytes ((char*)Ptr())[i] for 0 <= i < Size() are available for use.
	 * @return size of usable shared memory.
	 */
	virtual size_t Size() = 0;

	/**
	 * Returns a pointer to the shared memory buffer.
	 *
	 * The memory accessible through the pointer is considered read-write. It is
	 * NOT implicitly synchronized or guarded against concurrent modification.
	 *
	 * The pointer is always aligned to MAX(sizeof(void*), sizeof(double)). This
	 * should be enough to perform a placement new with any type.
	 *
	 * @note Only non-polymorphic classes may be placed within shared memory!
	 * @return aligned pointer to usable shared memory.
	 */
	virtual void* Ptr() = 0;

	/**
	 * Returns an identifier for this shared memory.
	 * The identifier is needed so that other processes can get access to this
	 * shared memory using Open().
	 *
	 * The identifier is typically a filename where the content is memory mapped.
	 * This is, however, not a requirement and the application shall not attempt
	 * to read or interpret contents of the identifier.
	 * @return identifier of this shared memory, can be used to Open() another
	 * reference.
	 */
	virtual const Identifier& GetIdentifier() = 0;

	/**
	 * Sets a functor that cleans up the memory before it is freed.
	 *
	 * Since OpSharedMemory doesn't know the type of object or objects being
	 * shared through it, it cannot infer the cleanup actions to perform on
	 * destruction.
	 * This method may be called multiple times, each assignment will destroy
	 * the previous functor and replace it with the new one. An empty pointer
	 * means that no cleanup action will be performed (good when sharing POD types)
	 * and it the default state of OpSharedMemory.
	 *
	 * @param cleanUpCallback A functor that cleans up the shared memory just before
	 * it is freed by the platform. @a cleanUpCallback's Cleanup() method will be
	 * called if this OpSharedMemory is the last owner of the shared resource.
	 * OpSharedMemory takes ownership of the cleanup functor and will dispose of it
	 * during destruction.
	 *
	 */
	virtual void SetCleanupFunctor(OpAutoPtr<CleanupFunctor> cleanupFunctor) = 0;
};
#endif // PI_SHARED_MEMORY
#endif // PI_SYSTEM_OPSHAREDMEMORY_H
