/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on unix/base/common/mt.cpp by Morten Stenshorne).
 */
#include "core/pch.h"
#ifdef POSIX_OK_THREAD

#include "platforms/posix/posix_thread_util.h"
#include "modules/pi/OpSystemInfo.h"

# ifdef __hpux__ // HP's almost-POSIX threads:
#define pthread_attr_init(a) pthread_attr_create(a)
#define pthread_attr_destroy(x) // no-op
#define pthread_create(h, a, e, a) pthread_create(h, *a, e, a)
# endif // __hpux__

# ifdef DEBUG_ENABLE_OPASSERT
// Expression is either zero (success) or an errno value:
#define ZERO_OR_ERRNO(e) int const res = e; OP_ASSERT(!res)
# else
#define ZERO_OR_ERRNO(e) e
# endif

THREAD_HANDLE PosixThread::CreateThread(void* (*entry)(void*), void* argument)
{
	THREAD_HANDLE handle;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
#ifdef OPERA_LNXJAVA_SVALBARD
	pthread_attr_setstacksize(&attr, 128 * 1024);
#endif // OPERA_LNXJAVA_SVALBARD

	if (pthread_create(&handle, &attr, entry, argument))
		handle = THREAD_HANDLE_NULL;

	pthread_attr_destroy(&attr);
	return handle;
}

PosixCondition::PosixCondition()
	: PosixMutex(MUTEX_RECURSIVE)
#ifdef DEBUG_ENABLE_OPASSERT
	, m_locked(0)
#endif
{
	ZERO_OR_ERRNO(pthread_cond_init(&m_cond, NULL));
}

PosixCondition::~PosixCondition()
{
	ZERO_OR_ERRNO(pthread_cond_destroy(&m_cond));
	OP_ASSERT(m_locked == 0);
}

#ifdef DEBUG_ENABLE_OPASSERT
# define POSIX_COND_WAIT_START(condition)								\
	{																	\
		PosixCondition::AssertOwner assert_owner(condition);			\
		POSIX_CLEANUP_PUSH(PosixCondition::AssertOwner::CleanupHandler, &assert_owner)
# define POSIX_COND_WAIT_END(condition)			\
		POSIX_CLEANUP_POP();					\
	}
#else // !DEBUG_ENABLE_OPASSERT
# define POSIX_COND_WAIT_START(condition)
# define POSIX_COND_WAIT_END(condition)
#endif

void PosixCondition::WakeAll()
{
	POSIX_CONDITION_GRAB(this);
	ZERO_OR_ERRNO(pthread_cond_broadcast(&m_cond));
	POSIX_CONDITION_GIVE(this);
}

void PosixCondition::Wake()
{
	POSIX_CONDITION_GRAB(this);
	ZERO_OR_ERRNO(pthread_cond_signal(&m_cond));
	POSIX_CONDITION_GIVE(this);
}

void PosixCondition::Wait(bool grab)
{
	if (grab)
	{
		POSIX_CONDITION_GRAB(this);
		Wait(false);
		POSIX_CONDITION_GIVE(this);
	}
	else
	{
		OP_ASSERT(m_locked > 0);
		POSIX_COND_WAIT_START(this);
		ZERO_OR_ERRNO(pthread_cond_wait(&m_cond, &m_mutex));
		POSIX_COND_WAIT_END(this);
	}
}

bool PosixCondition::TimedWait(int delay, bool grab)
{
	double abs_msec = g_op_time_info->GetTimeUTC() + delay;

	struct timespec ts;
	ts.tv_sec = abs_msec / 1000;
	ts.tv_nsec = (abs_msec - 1000. * ts.tv_sec) * 1000000;

	int err;
	if (grab)
	{
		POSIX_CONDITION_GRAB(this);
		POSIX_COND_WAIT_START(this);
		err = pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
		POSIX_COND_WAIT_END(this);
		POSIX_CONDITION_GIVE(this);
	}
	else
	{
		OP_ASSERT(m_locked > 0);
		POSIX_COND_WAIT_START(this);
		err = pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
		POSIX_COND_WAIT_END(this);
	}

	OP_ASSERT(err == 0 || err == ETIMEDOUT); // *not* EINVAL or EPERM
	return err == 0;
}

#endif // POSIX_OK_THREAD
