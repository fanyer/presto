/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_SYNC_PRIMITIVES_H
#define WINDOWS_SYNC_PRIMITIVES_H

/**
 * A wrapper-class around Critical Section functionality. Avoid using this directly, but use
 * it with the WinAutoLock class instead as this will avoid the possibility of having
 * a lingering lock.
 */
class OpCriticalSection
{
public:

	/**
	* OpCriticalSection class constructor.
	*/
	OpCriticalSection()
	{
		::InitializeCriticalSectionAndSpinCount(&m_cs, 0);
	};
 
	/**
	* OpCriticalSection class destructor
	*/
	~OpCriticalSection(void)
	{
		Enter();	// make sure some other thread doesn't hold ownership on it before we delete it
		::DeleteCriticalSection(&m_cs);
	}; 
 
	/**
	* Wait for unlock and enter the CriticalSection object.
	*/
	void Enter()
	{
		::EnterCriticalSection(&m_cs);
	};
 
	/**
	* Leaves the critical section object.
	* This function will only work if the current thread
	* holds the current lock on the OpCriticalSection object
	* called by Enter().
	* 
	* @see Enter()
	*/
	void Leave(void)
	{
		::LeaveCriticalSection(&m_cs);
	};
 
private:
	CRITICAL_SECTION m_cs;			//!< windows critical section object
};

/**
 * A wrapper-class around locking critical sections. Use this to ensure no lingering locks exist:
 * 
 * OpCriticalSection m_cs;
 * {
 *		WinAutoLock lock(m_cs);
 *
 *		// protected section of code
 * 
 * } // lock goes out of scope here
 */

class WinAutoLock
{
private:
	WinAutoLock() {}

public:
	WinAutoLock(OpCriticalSection* cs) 
	{
		m_lock = cs;

		m_lock->Enter();
	}
	~WinAutoLock()
	{
		if(m_lock)
		{
			m_lock->Leave();
		}
	}
	/*
	* No longer track the critical section with the auto lock
	*/
	void Detach()
	{
		m_lock = NULL;

	}

private:
	OpCriticalSection* m_lock;
};

#endif // WINDOWS_SYNC_PRIMITIVES_H
