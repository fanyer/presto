/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef POSIX_OK_THREAD
/* TODO: consider whether Mutex could usefully record which thread (identified
 * by pthread_self()) is currently holding it; only meaningful check is "is it
 * me ?" but this might suffice to support cleverer lock semantics.  Possibly
 * only for use in debug, to catch abuses.
 */

#include "platforms/posix/posix_mutex.h"
#include "platforms/posix/posix_thread_util.h"
#include "platforms/posix/src/posix_mutex_compat.h"

# ifdef DEBUG_ENABLE_OPASSERT
// Expression is either zero (success) or an errno value:
#define ZERO_OR_ERRNO(e) int const res = e; OP_ASSERT(!res)
# else
#define ZERO_OR_ERRNO(e) e
# endif

#ifdef PI_MUTEX
// static
OP_STATUS OpMutex::Create(OpMutex** mutex)
{
	OpAutoPtr<PosixMutex> posix_mutex (OP_NEW(PosixMutex, (PosixMutex::MUTEX_RECURSIVE)));

	if (!posix_mutex.get())
		return OpStatus::ERR_NO_MEMORY;

	*mutex = posix_mutex.release();
	return OpStatus::OK;
}
#endif // PI_MUTEX

PosixMutex::PosixMutex(PosixMutex::MutexType type) // MUTEX_NORMAL or MUTEX_RECURSIVE
#ifdef DEBUG_ENABLE_OPASSERT
	: m_owner(THREAD_HANDLE_NULL), m_depth(0)
#endif
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, type == MUTEX_RECURSIVE
							  ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
	ZERO_OR_ERRNO(pthread_mutex_init(&m_mutex, &attr));
	pthread_mutexattr_destroy(&attr);
}

PosixMutex::~PosixMutex()
{
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(m_depth == 0 && PosixThread::IsNullThread(m_owner));
#endif
	ZERO_OR_ERRNO(pthread_mutex_destroy(&m_mutex));
}

void PosixMutex::Acquire()
{
	ZERO_OR_ERRNO(pthread_mutex_lock(&m_mutex));
#ifdef DEBUG_ENABLE_OPASSERT
	if (!res) // NB: 35 is EDEADLK, at least on Linux
		if (m_depth++ == 0)
			m_owner = PosixThread::Self();
		else
			OP_ASSERT(m_owner == PosixThread::Self());
#endif
}

bool PosixMutex::Request()
{
	int res = pthread_mutex_trylock(&m_mutex);
	if (res)
	{
		OP_ASSERT(res == EBUSY);
		return false;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	if (m_depth++ == 0)
		m_owner = PosixThread::Self();
	else
		OP_ASSERT(m_owner == PosixThread::Self());
#endif
	return true;
}

void PosixMutex::Release()
{
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(m_owner == PosixThread::Self() && m_depth > 0);
	if (m_owner == PosixThread::Self() && m_depth > 0)
		if (--m_depth < 1)
			m_owner = THREAD_HANDLE_NULL;
#endif
	ZERO_OR_ERRNO(pthread_mutex_unlock(&m_mutex));
}

#ifdef DEBUG_ENABLE_OPASSERT
bool PosixMutex::SinglyLocked()
{
	return m_depth == 1 && PosixThread::Self() == m_owner;
}

bool PosixMutex::NotMyLock()
{
	return PosixThread::Self() != m_owner;
}
#endif

#endif // POSIX_OK_THREAD
