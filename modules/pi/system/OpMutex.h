/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_SYSTEM_OPMUTEX_H
#define PI_SYSTEM_OPMUTEX_H

#ifdef PI_MUTEX

/** @brief A process-local thread mutex
 * This class can be used to create mutual exclusion across threads inside a
 * process. To use it, a mutex that can be reached by all threads that need
 * access to it has to be created. A lock on the mutex can then be acquired
 * at the start of a critical section, and released at the end of it.
 *
 * The mutex is recursive (i.e. can be locked by the same thread multiple times,
 * but must be unlocked the same number of times).
 */
class OpMutex
{
public:
	/** Create an OpMutex object */
	static OP_STATUS Create(OpMutex** mutex);

	/** Destructor */
	virtual ~OpMutex() {}

	/** Acquire a lock on the mutex.
	 * If the mutex is already locked by another thread, this function will
	 * block until the mutex becomes available. If the current thread already
	 * holds the lock, this function will not block, but @ref Release() must be
	 * called the same number of times to release the lock.
	 */
	virtual void Acquire() = 0;

	/** Release the lock on the mutex.
	 * This function should never be called more times than @ref Acquire().
	 */
	virtual void Release() = 0;

	/**
	 * An instance of this class can be used to automatically acquire
	 * and release an OpMutex. The constructor acquires the mutex
	 * (OpMutex::Acquire()) and the destructor releases the mutex
	 * (OpMutex::Release()).
	 *
	 * Thus you don't need to add code for releasing the mutex for
	 * each return statement. Example:
	 * @code
	 * int some_function(OpMutex& mutex, int some_argument)
	 * {
	 *    OpMutex::AutoLock lock(mutex);
	 *    if (some_argument > 0) return 1;
	 *    else if (some_argument == 0) return 0;
	 *    else return -1;
	 * }
	 * @endcode
	 *
	 * @note If you use an instance of this class in a method on the core-thread
	 *  that may LEAVE(), be sure to ANCHOR() the OpMutex::AutoLock instance.
	 */
	class AutoLock
	{
	public:
		/** Constructor
		 * @param mutex Mutex on which a lock should be acquired
		 */
		AutoLock(OpMutex& mutex) : m_mutex(mutex) { m_mutex.Acquire(); }

		/** Destructor
		 * Releases the lock that was acquired in the constructor
		 */
		~AutoLock() { m_mutex.Release(); }
	private:
		OpMutex& m_mutex;
	};
};

#endif // PI_MUTEX

#endif // PI_SYSTEM_OPMUTEX_H
