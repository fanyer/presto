// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef DESKTOP_SEMAPHORE_H
#define DESKTOP_SEMAPHORE_H

#include "adjunct/desktop_pi/DesktopThreads.h"

/** @brief A semaphore
  * @author Arjan van Leeuwen
  */
class DesktopSemaphore
{
public:
	DesktopSemaphore() : m_is_initialised(FALSE) {}
	/** Destructor
	  */
	~DesktopSemaphore() { if (m_is_initialised) OpStatus::Ignore(DesktopThreads::DestroySemaphore(m_semaphore_id)); }

	/** Initialize - run this function before doing anything else!
	  * @param initial_value Initial value of the semaphore
	  */
	OP_STATUS Init(unsigned initial_value) {
		OP_STATUS status = DesktopThreads::CreateSemaphore(m_semaphore_id, initial_value);
		if (OpStatus::IsSuccess(status))
			m_is_initialised = TRUE;
		return status;
	}

	/** Increment this semaphore
	  */
	OP_STATUS Increment() { return m_is_initialised ? DesktopThreads::IncrementSemaphore(m_semaphore_id) : OpStatus::ERR; }

	/** Decrement this semaphore (will block if value of semaphore is 0)
	  */
	OP_STATUS Decrement() { return m_is_initialised ? DesktopThreads::DecrementSemaphore(m_semaphore_id) : OpStatus::ERR; }

private:
	BOOL m_is_initialised;
	OpSemaphoreId m_semaphore_id;
};

#endif // DESKTOP_SEMAPHORE_H
