/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#ifndef _GNU_SOURCE /* pthread.h leaves out some declarations without it */
# define _GNU_SOURCE
#endif

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopThreads.h"

#include "platforms/mac/util/MacTLS.h"
#include "platforms/mac/util/macutils_mincore.h"
#include <pthread.h>

// Type used for semaphores
struct sema_struct_t
{
	unsigned		count;			///< Current count of the semaphore.
	unsigned		waiters_count;	///< Number of threads that have called DecrementSemaphore
	pthread_mutex_t lock;			///< Serialize access to count and waiters_count.
	pthread_cond_t	count_nonzero;	///< Condition variable that blocks the count 0.
};

namespace DesktopThreads
{
	struct MacThreadFunction
	{
		void *(*start_routine)(void *);
		void *arg;

		static void* Start(void* arg)
		{
			OperaNSReleasePool pool;
			MacTLS::AttachThread();
			MacThreadFunction thread_function = *static_cast<MacThreadFunction*>(arg);
			OP_DELETE(static_cast<MacThreadFunction*>(arg));
			return thread_function.start_routine(thread_function.arg);
		}
	};
}


/***********************************************************************************
 ** Create a thread, starting the specified function
 **
 ** DesktopThreads::CreateThread
 ** @param thread_id ID of the created thread will be stored here
 ** @param start_routine Function to start
 ** @param arg Argument for function to start
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::CreateThread(OpThreadId& thread_id, void *(*start_routine)(void *), void *arg)
{
	MacThreadFunction* thread_function = OP_NEW(MacThreadFunction, ());
	RETURN_OOM_IF_NULL(thread_function);
	thread_function->start_routine = start_routine;
	thread_function->arg = arg;

	int retval = pthread_create(&thread_id, NULL, &MacThreadFunction::Start, thread_function);
	if (retval != 0)
	{
		OP_DELETE(thread_function);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


/***********************************************************************************
 ** Destroy a thread, releasing all resources. Don't call when thread is still
 ** running.
 **
 ** DesktopThreads::DestroyThread
 ** @param thread_id Thread to destroy
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::DestroyThread(OpThreadId& thread_id)
{
	return OpStatus::OK;
}


/***********************************************************************************
 ** Wait for a thread to exit, blocks the calling thread until specified thread exits
 **
 ** DesktopThreads::WaitForThread
 ** @param thread_id ID of thread to wait for
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::WaitForThread(OpThreadId& thread_id)
{
	int retval = pthread_join(thread_id, NULL);

	return retval == 0 ? OpStatus::OK : OpStatus::ERR;
}


/***********************************************************************************
 ** Creates a new mutex. The mutex will only be used in this process (process-local).
 ** The mutex is recursive (i.e. can be locked by the same thread multiple times, but
 ** must be unlocked the same number of times).
 **
 ** DesktopThreads::CreateMutex
 ** @param mutex_id ID of the created mutex will be stored here
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::CreateMutex(OpMutexId& mutex_id)
{
	pthread_mutexattr_t attr;

	// Setup attributes: we explicitly set the mutex type to PTHREAD_MUTEX_RECURSIVE
	if (pthread_mutexattr_init(&attr) || pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE))
		return OpStatus::ERR;

	// Initialize the mutex
	if (pthread_mutex_init(&mutex_id, &attr))
		return OpStatus::ERR;

	// Cleanup mutex attributes
	if (pthread_mutexattr_destroy(&attr))
		return OpStatus::ERR;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Destroys a mutex. This will return an error if the mutex is locked.
 **
 ** DesktopThreads::DestroyMutex
 ** @param mutex_id ID of mutex to destroy
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::DestroyMutex(OpMutexId& mutex_id)
{
	int retval = pthread_mutex_destroy(&mutex_id);

	return retval == 0 ? OpStatus::OK : OpStatus::ERR;
}


/***********************************************************************************
 ** Locks a mutex. If the mutex is already locked, the calling thread
 ** will block until the mutex becomes available.
 **
 ** DesktopThreads::LockMutex
 ** @param mutex_id ID of mutex to lock
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::LockMutex(OpMutexId& mutex_id)
{
	int retval = pthread_mutex_lock(&mutex_id);

	return retval == 0 ? OpStatus::OK : OpStatus::ERR;
}


/***********************************************************************************
 ** Unlocks a mutex if the current thread holds a lock on that mutex
 **
 ** DesktopThreads::UnlockMutex
 ** @param mutex_id ID of mutex to unlock
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::UnlockMutex(OpMutexId& mutex_id)
{
	int retval = pthread_mutex_unlock(&mutex_id);

	return retval == 0 ? OpStatus::OK : OpStatus::ERR;
}


/***********************************************************************************
 ** Create a semaphore
 **
 ** DesktopThreads::CreateSemaphore
 ** @param semaphore_id ID of the created semaphore will be stored here
 ** @param initial_value Initial value of the semaphore (>= 0)
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::CreateSemaphore(OpSemaphoreId& semaphore_id, unsigned initial_value)
{
	semaphore_id = OP_NEW(sema_struct_t, ());
	if (!semaphore_id)
		return OpStatus::ERR_NO_MEMORY;

	// We create a non-recursive mutex
	pthread_mutexattr_t attr;

	// Setup attributes: we explicitly set the mutex type to PTHREAD_MUTEX_NORMAL
	if (pthread_mutexattr_init(&attr) || pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL))
		return OpStatus::ERR;

	// Initialize the mutex
	if (pthread_mutex_init(&semaphore_id->lock, &attr))
		return OpStatus::ERR;

	// Cleanup mutex attributes
	if (pthread_mutexattr_destroy(&attr))
		return OpStatus::ERR;

	// Create the condition
	if (pthread_cond_init(&semaphore_id->count_nonzero, NULL) != 0)
		return OpStatus::ERR;

	semaphore_id->count 		= initial_value;
	semaphore_id->waiters_count = 0;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Destroys a semaphore.
 **
 ** DesktopThreads::DestroySemaphore
 ** @param semaphore_id ID of semaphore to destroy
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::DestroySemaphore(OpSemaphoreId& semaphore_id)
{
	if (pthread_cond_destroy(&semaphore_id->count_nonzero) != 0)
		return OpStatus::ERR;
	if (pthread_mutex_destroy(&semaphore_id->lock) != 0)
		return OpStatus::ERR;

	delete semaphore_id;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Increments a semaphore by 1, alerting waiting threads
 **
 ** DesktopThreads::IncrementSemaphore
 ** @param semaphore_id ID of semaphore to increment
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::IncrementSemaphore(OpSemaphoreId& semaphore_id)
{
	if (pthread_mutex_lock(&semaphore_id->lock) != 0)
		return OpStatus::ERR;

	// Always allow one thread to continue if it is waiting.
	if (semaphore_id->waiters_count > 0 && pthread_cond_signal(&semaphore_id->count_nonzero) != 0)
		return OpStatus::ERR;

	// Increment the semaphore's count.
	semaphore_id->count++;

	if (pthread_mutex_unlock(&semaphore_id->lock) != 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Decrements a semaphore by 1, blocks if current value of semaphore is 0
 **
 ** DesktopThreads::DecrementSemaphore
 ** @param semaphore_id ID of semaphore to decrement
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::DecrementSemaphore(OpSemaphoreId& semaphore_id)
{
	// Acquire mutex to enter critical section.
	if (pthread_mutex_lock(&semaphore_id->lock) != 0)
		return OpStatus::ERR;

	// Keep track of the number of waiters so that <sema_post> works correctly.
	semaphore_id->waiters_count++;

	// Wait until the semaphore count is > 0, then atomically release
	// <lock> and wait for <count_nonzero> to be signaled.
	while (semaphore_id->count == 0)
	{
		if (pthread_cond_wait(&semaphore_id->count_nonzero, &semaphore_id->lock) != 0)
			return OpStatus::ERR;
	}

	// <lock> is now held.

	// Decrement the waiters count.
	semaphore_id->waiters_count--;

	// Decrement the semaphore's count.
	semaphore_id->count--;

	// Release mutex to leave critical section.
	if (pthread_mutex_unlock(&semaphore_id->lock) != 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Makes the current thread sleep for at least the specified amount of time
 **
 ** DesktopThreads::Sleep
 ** @param interval_ms Time to sleep in ms
 ***********************************************************************************/
void DesktopThreads::Sleep(unsigned interval_ms)
{
	struct timespec ts, tm;

	ts.tv_sec  = interval_ms / 1000;
	ts.tv_nsec = (interval_ms % 1000) * 1000000;

	// nanosleep is run in a loop, because it might return if signals are delivered.
	// In that case, a non-zero value will be returned, and we run again with the
	// time that is left (saved in tm)
	while (nanosleep(&ts, &tm))
		ts = tm;
}

/***********************************************************************************
 ** Performs an atomic compare-and-exchange operation on the specified values. 
 **
 ** DesktopThreads::AtomicCompareAndSwap
 ** @param A pointer to the destination value.
 ** @param The value to compare to destination
 ** @param The exchange value
 ***********************************************************************************/
INT32 DesktopThreads::AtomicCompareAndSwap(INT32 volatile *destination, INT32 oldval, INT32 newval)
{
#if defined(GCC4) && __GNUC_MINOR__ >= 1
	return __sync_val_compare_and_swap(destination, oldval, newval);
#else // defined(GCC4) && __GNUC_MINOR__ >= 1
    __asm__ __volatile__("lock cmpxchgl %3, %1"
                          : "=a"(oldval), "=m"(*(destination))
                          : "0"(oldval), "r"(newval), "m"(*(destination)));
	return oldval;
#endif // defined(GCC4) && __GNUC_MINOR__ >= 1
}
