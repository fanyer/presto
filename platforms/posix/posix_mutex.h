/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef POSIX_MUTEX_H
#define POSIX_MUTEX_H __FILE__
# ifdef POSIX_OK_THREAD

#ifdef PI_MUTEX
# include "modules/pi/system/OpMutex.h"
#endif // PI_MUTEX

#include <pthread.h>

/** Implement a mutex.
 *
 * A normal mutex can be locked only once; attempts to lock it while locked,
 * even from the same thread, shall block.  By default we make a recursive
 * mutex: the thread that holds it may acquire it several times (recursively).
 * Calling Request() counts as acquiring when it returns true.  The thread must
 * release the mutex as many times as it acquires it.
 */
class PosixMutex
#ifdef PI_MUTEX
  : public OpMutex
#endif
{
	PosixMutex(const PosixMutex&); // suppress copy-construction

protected: // so PosixCondition's methods can access it:
#ifdef DEBUG_ENABLE_OPASSERT
	pthread_t m_owner;
	int m_depth;
#endif
	pthread_mutex_t	m_mutex;
public:
	enum MutexType { MUTEX_NORMAL, MUTEX_RECURSIVE };
	PosixMutex(MutexType type=MUTEX_RECURSIVE);
	virtual ~PosixMutex();

	void Acquire(); //< Acquire mutex.  Block until the mutex can be acquired.
	void Release(); //< Release mutex.

	/** Try to acquire mutex; does not block.
	 * @return True on success; else, we didn't acquire the mutex.
	 */
	bool Request();

#ifdef DEBUG_ENABLE_OPASSERT
	/** Assert just before a Release() that you're sure leaves this unlocked. */
	bool SinglyLocked();
	/** Assert when you think you're not holding this lock. */
	bool NotMyLock();
#endif

public:
#ifndef PI_MUTEX
	/**
	 * An instance of this class can be used to automatically acquire
	 * and release a PosixMutex. The constructor acquires the mutex
	 * (PosixMutex::Acquire()) and the destructor releases the mutex
	 * (PosixMutex::Release()).
	 *
	 * Thus you don't need to add code for releasing the mutex for
	 * each return statement. Example:
	 * @code
	 * int some_function(PosixMutex& mutex, int some_argument)
	 * {
	 *    PosixMutex::AutoLock lock(mutex);
	 *    if (some_argument > 0) return 1;
	 *    else if (some_argument == 0) return 0;
	 *    else return -1;
	 * }
	 * @endcode
	 *
	 * @note If you use an instance of this class in a method on the core-thread
	 *  that may LEAVE(), be sure to ANCHOR() the AutoLock instance.
	 */
	class AutoLock {
		PosixMutex& m_mutex;
	public:
		AutoLock(PosixMutex& mutex) : m_mutex(mutex) { m_mutex.Acquire(); }
		~AutoLock() { m_mutex.Release(); }
	};
#endif // !PI_MUTEX

#ifdef POSIX_ASYNC_CANCELLATION
	/**
	 * When you use a PosixMutex on a cancelable thread you should push this
	 * cleanup handler using pthread_cleanup_push() to the thread's stack of
	 * thread-cancellation clean-up handlers. Thus the PosixMutex can be
	 * released if the thread is cancelled.
	 *
	 * Example:
	 * @code
	 * PosixMutex* mutex = ...;
	 * mutex->Acquire();
	 * pthread_cleanup_push(PosixMutex::CleanupRelease, mutex);
	 * ...
	 * // do something, if this thread is cancelled, the cleanup handler
	 * // will be executed and release the mutex.
	 * ...
	 * pthread_cleanup_pop(0); // don't execute the handler
	 * mutex->Release();
	 * @endcode
	 *
	 * @param mutex is the PosixMutex instance to release.
	 * @see PosixThread::Cancel()
	 */
	static void CleanupRelease(void* mutex)
	{
		if (mutex) static_cast<PosixMutex*>(mutex)->Release();
	}
#endif // POSIX_ASYNC_CANCELLATION
};

# endif // POSIX_OK_THREAD
#endif // POSIX_MUTEX_H
