/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPSHAREDMEMORYMUTEX_H
#define OPSHAREDMEMORYMUTEX_H
#ifdef PI_SHARED_MEMORY
/** A mutex for synchronizing access to shared memory.
 * OpSharedMemoryMutex is placed within a block of shared memory
 * (see OpSharedMemory). The mutex is non-recursive, ie. calling Acquire() by
 * a process that has already acquired the lock produces a deadlock.
 *
 * @note This PI is different from most others in that it's not an abstract class.
 * One cannot use polymorphic classes within shared memory because pointers
 * to vtables would point to bogus locations in different processes. Instead,
 * the implementer is responsible for providing a class named OpSharedMemoryMutex
 * which has an interface identical to the one specified in
 * OpSharedMemoryMutexInterface. This class must be visible to Core code.
 * DO NOT attempt to derive from OpSharedMemoryMutexInterface, provide your own
 * non-polymorphic implementation.
 *
 * Example usage:
 * @code
class MySharedClass
{
public:
	OP_STATUS Init()
	{
		return mutex.Construct();
	}

	OP_STATUS AccessData()
	{
		RETURN_IF_ERROR(mutex.Acquire());
		someSharedData.Access();
		RETURN_IF_ERROR(mutex.Release());
	}
private:
	OpSharedMemoryMutex mutex; // Note the value semantics, this is not a pointer.
	Data someSharedData;
};

...
// Creating the memory and a mutex:
OpSharedMemory* memory = NULL;
RETURN_IF_ERROR(OpSharedMemory::Create(sizeof(MySharedClass), &memory));
// Construct MySharedClass on shared memory. This calls OpSharedMemoryMutex's c-tor.
MySharedClass* myClass = new (memory->Ptr()) MySharedClass();
RETURN_IF_ERROR(myClass->Init()); // OpSharedMemoryMutex::Construct() called.
RETURN_IF_ERROR(myClass->AccessData()); // Acquires and Releases mutex.

// Opening the memory in a different process:
OpSharedMemory* memory = NULL;
RETURN_IF_ERROR(OpSharedMemory::Open(identifier, &memory));
MySharedClass* myClass = (MySharedClass*)memory->Ptr();
RETURN_IF_ERROR(myClass->AccessData()); // Acquires and Releases mutex.
 *@endcode
 *
 */

class OpSharedMemoryMutexInterface
{
public:
	/// OpSharedMemoryMutex must be default-constructible.
	OpSharedMemoryMutexInterface();


	/** Destroy the object.
	 *
	 * Must be called when in unlocked state. Destroying a locked mutex results
	 * in undefined behavior.
	 * May be called from a different process/thread than Construct().
	 */
	~OpSharedMemoryMutexInterface();

	/** Initialize this object.
	 *
	 * Will be called once during the lifetime of the mutex, before calls to
	 * other methods. Will NOT be called again by processes other than the one
	 * that creates the mutex.
	 *
	 * @retval OpStatus::OK if the object is valid and ready to use.
	 * @retval OpStatus::ERR_NO_MEMORY if the platform ran out of memory.
	 * @retval OpStatus::ERR_NO_ACCESS if the caller has insufficient permissions.
	 * @retval OpStatus::ERR for other unspecified errors.
	 */
	OP_STATUS Construct();

	/** Acquire the lock.
	 *
	 * Will attempt to lock the mutex. Note that since this is a non-recursive
	 * mutex, an attempt to Acquire a lock that has already been acquired by the
	 * same thread in the same process will result in an OpStatus:ERR result.
	 *
	 * @retval OpStatus::OK if the lock has been acquired.
	 * @retval OpStatus::ERR if an error has occured - assume the state of the mutex
	 * was not changed.
	 */
	OP_STATUS Acquire();

	/** Release the lock.
	 *
	 * Will release the lock. Releasing a lock that is not Acquired by the same
	 * thread or process is undefined behavior.
	 *
	 * @retval OpStatus::OK if the lock has been released.
	 * @retval OpStatus::ERR if an error has occured - assume the state of the mutex
	 * was not changed.
	 */
	OP_STATUS Release();

	/** Try to acquire the lock.
	 *
	 * Will attempt to lock the mutex and return immediately if it's impossible.
	 * Calling TryAcquire is safe even if the current thread or process has the
	 * lock (the platform must return 'false' in such situation). If acquisition
	 * was possible, the method should behave as Acquire and grant exclusive
	 * ownership of the mutex to the caller.
	 * @retval true if the lock has been acquired.
	 * @retval false if it was impossible for
	 * any reason - ex. it was acquired by a different thread or process
	 * at the time or an unspecified error has occured.
	 */
	bool TryAcquire();
};

#endif // PI_SHARED_MEMORY
#endif // OPSHAREDMEMORYMUTEX_H
