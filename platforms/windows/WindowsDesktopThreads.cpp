/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include <process.h>

#include "adjunct/desktop_pi/DesktopThreads.h"

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
	unsigned thread_address ;

	thread_id = (HANDLE) _beginthreadex(NULL, 0, (unsigned int (__stdcall *)(void *))start_routine, arg, 0, &thread_address);

	return thread_id ? OpStatus::OK : OpStatus::ERR;
}


/***********************************************************************************
 ** Destroy a thread, releasing all resources. Don't call when thread is still running.
 **
 ** DesktopThreads::DestroyThread
 ** @param thread_id Thread to destroy
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::DestroyThread(OpThreadId& thread_id)
{
	BOOL retval = CloseHandle(thread_id);

	return retval ? OpStatus::OK : OpStatus::ERR;
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
	DWORD retval = WaitForSingleObject(thread_id, INFINITE);

	return retval == WAIT_FAILED ? OpStatus::ERR : OpStatus::OK;
}


/***********************************************************************************
 ** Creates a new mutex. The mutex will only be used in this process (process-local).
 ** The mutex is recursive (i.e. can be locked by the same thread multiple times).
 **
 ** DesktopThreads::CreateMutex
 ** @param mutex_id ID of the created mutex will be stored here
 ** @return OK on success
 ***********************************************************************************/
OP_STATUS DesktopThreads::CreateMutex(OpMutexId& mutex_id)
{
	InitializeCriticalSection(&mutex_id);

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
	DeleteCriticalSection(&mutex_id);

	return OpStatus::OK;
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
	EnterCriticalSection(&mutex_id);

	return OpStatus::OK;
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
	LeaveCriticalSection(&mutex_id);

	return OpStatus::OK;
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
	semaphore_id = ::CreateSemaphoreA(NULL, (LONG)initial_value, INT_MAX, NULL);

	return semaphore_id ? OpStatus::OK : OpStatus::ERR;
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
	BOOL retval = CloseHandle(semaphore_id);

	return retval ? OpStatus::OK : OpStatus::ERR;
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
	BOOL retval = ReleaseSemaphore(semaphore_id, 1, NULL);

	return retval ? OpStatus::OK : OpStatus::ERR;
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
	DWORD retval = WaitForSingleObject(semaphore_id, INFINITE);

	return retval == WAIT_FAILED ? OpStatus::ERR : OpStatus::OK;
}

/***********************************************************************************
 ** Suspends the current thread for at least the specified amount of time
 **
 ** DesktopThreads::Sleep
 ** @param interval_ms Time to sleep in ms
 ***********************************************************************************/
void DesktopThreads::Sleep(unsigned interval_ms)
{
	::Sleep(interval_ms);
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
	return InterlockedCompareExchange((LONG*)destination, newval, oldval);
}

