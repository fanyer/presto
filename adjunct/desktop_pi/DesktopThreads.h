// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef DESKTOP_THREADS_H
#define DESKTOP_THREADS_H

namespace DesktopThreads
{
	/**
	 * Create a thread, starting the specified function
	 * @param thread_id ID of the created thread will be stored here
	 * @param start_routine Function to start
	 * @param arg Argument for function to start
	 * @return OK on success
	 */
	OP_STATUS CreateThread(OpThreadId& thread_id, void *(*start_routine)(void *), void *arg);

	/**
	 * Destroy a thread, releasing all resources. Don't call when thread is still running.
	 * @param thread_id Thread to destroy
	 * @return OK on success
	 */
	OP_STATUS DestroyThread(OpThreadId& thread_id);

	/**
	 * Wait for a thread to exit, blocks the calling thread until specified thread exits
	 * @param thread_id ID of thread to wait for
	 * @return OK on success
	 */
	OP_STATUS WaitForThread(OpThreadId& thread_id);

	/**
	 * Creates a new mutex. The mutex will only be used in this process (process-local).
	 * The mutex is recursive (i.e. can be locked by the same thread multiple times, but
	 * must be unlocked the same number of times).
	 * @param mutex_id ID of the created mutex will be stored here
	 * @return OK on success
	 */
	OP_STATUS CreateMutex(OpMutexId& mutex_id);

	/**
	 * Destroys a mutex. This will return an error if the mutex is locked.
	 * @param mutex_id ID of mutex to destroy
	 * @return OK on success
	 */
	OP_STATUS DestroyMutex(OpMutexId& mutex_id);

	/**
	 * Locks a mutex. If the mutex is already locked, the calling thread
	 * will block until the mutex is unlocked.
	 * @param mutex_id ID of mutex to lock
	 * @return OK on success
	 */
	OP_STATUS LockMutex(OpMutexId& mutex_id);

	/**
	 * Unlocks a mutex if the current thread holds a lock on that mutex
	 * @param mutex_id ID of mutex to unlock
	 * @return OK on success
	 */
	OP_STATUS UnlockMutex(OpMutexId& mutex_id);

	/**
	 * Create a semaphore. The semaphore will only be used in this process (process-local).
	 * @param semaphore_id ID of the created semaphore will be stored here
	 * @param initial_value Initial value of the semaphore (>= 0)
	 * @return OK on success
	 */
	OP_STATUS CreateSemaphore(OpSemaphoreId& semaphore_id, unsigned initial_value);

	/**
	 * Destroys a semaphore.
	 * @param semaphore_id ID of semaphore to destroy
	 */
	OP_STATUS DestroySemaphore(OpSemaphoreId& semaphore_id);

	/**
	 * Increments a semaphore by 1, alerting waiting threads if the current value of semaphore is 0
	 * @param semaphore_id ID of semaphore to increment
	 */
	OP_STATUS IncrementSemaphore(OpSemaphoreId& semaphore_id);

	/**
	 * Decrements a semaphore by 1, blocks if current value of semaphore is 0
	 * @param semaphore_id ID of semaphore to decrement
	 */
	OP_STATUS DecrementSemaphore(OpSemaphoreId& semaphore_id);

	/**
	 * Suspends the current thread for at least the specified amount of time
	 * @param interval_ms Time to sleep in ms
	 */
	void	  Sleep(unsigned interval_ms);

	/**
	 * Performs an atomic compare-and-exchange operation on the specified values. 
	 * The function compares the value at the destination address to a given value (oldval) and changes it to a new value (newval) if they are equal
	 * @param A pointer to the destination value.
	 * @param The value to compare to destination
	 * @param The exchange value
	 */
	INT32	  AtomicCompareAndSwap(INT32 volatile *destination, INT32 oldval, INT32 newval);
};


#endif // DESKTOP_THREADS_H
