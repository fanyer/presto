// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef DESKTOP_MUTEX_H
#define DESKTOP_MUTEX_H

#include "adjunct/desktop_pi/DesktopThreads.h"

/** @brief A recursive mutex
  * @author Arjan van Leeuwen
  */
class DesktopMutex
{
public:
	DesktopMutex() : m_is_initialised(FALSE) {}
	/** Destructor
	  */
	~DesktopMutex() { if (m_is_initialised) OpStatus::Ignore(DesktopThreads::DestroyMutex(m_mutex_id)); }

	/** Initialize - run this function before doing anything else!
	  */
	OP_STATUS Init()
	{
		OP_STATUS status = DesktopThreads::CreateMutex(m_mutex_id);
		if (OpStatus::IsSuccess(status))
			m_is_initialised = TRUE;
		return status;
	}

	/** Acquire this mutex
	  */
	OP_STATUS Acquire() { return m_is_initialised ? DesktopThreads::LockMutex(m_mutex_id) : OpStatus::ERR; }

	/** Release this mutex
	  */
	OP_STATUS Release() { return m_is_initialised ? DesktopThreads::UnlockMutex(m_mutex_id) : OpStatus::ERR; }

private:
	BOOL m_is_initialised;
	OpMutexId m_mutex_id;
};



/** @brief A helper class that acquires a mutex on construction and releases it when out of scope
  * @author Arjan van Leeuwen
  */
class DesktopMutexLock
{
public:
	/** Constructor
	  * @param mutex Mutex to acquire
	  */
	DesktopMutexLock(DesktopMutex& mutex) : m_mutex(mutex), m_locked(TRUE) { OpStatus::Ignore(m_mutex.Acquire()); }

	/** Destructor
	  */
	~DesktopMutexLock() { if (m_locked) OpStatus::Ignore(m_mutex.Release()); }

	/** Release this mutex
	  */
	OP_STATUS Release() { m_locked = FALSE; return m_mutex.Release(); }

private:
	DesktopMutex& m_mutex;
	BOOL		  m_locked;
};

#endif // DESKTOP_MUTEX_H
