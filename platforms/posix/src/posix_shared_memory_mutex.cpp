/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#ifdef POSIX_OK_SHARED_MEMORY
#include "platforms/posix/src/posix_mutex_compat.h"
#include "platforms/posix/posix_shared_memory_mutex.h"
#include "modules/hardcore/base/opstatus.h"
#include <sys/types.h>
#include <unistd.h>

OP_STATUS OpSharedMemoryMutex::Construct()
{
	// Introduced primarily for return-safety, destroys the mutexattr in d-tor.
	struct AttrWrapper
	{
		int Init() { return pthread_mutexattr_init(&attr); }
		int SetSpShared()
		{
			return pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		}
		int SetType()
		{
			return pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		}
		~AttrWrapper()
		{
			pthread_mutexattr_destroy(&attr);
		}

		pthread_mutexattr_t attr;
	};

	AttrWrapper attrWrapped;
	if(attrWrapped.Init()) return OpStatus::ERR_NO_MEMORY;
	if(attrWrapped.SetSpShared() || attrWrapped.SetType()) return OpStatus::ERR;

	if(int error = pthread_mutex_init(&m_mutex, &attrWrapped.attr))
		switch(error)
		{
		case ENOMEM: return OpStatus::ERR_NO_MEMORY;
		case EPERM: return OpStatus::ERR_NO_ACCESS;
		default: return OpStatus::ERR;
		}

	return OpStatus::OK;
}

OpSharedMemoryMutex::~OpSharedMemoryMutex()
{
	pthread_mutex_destroy(&m_mutex);
}

OP_STATUS OpSharedMemoryMutex::Acquire()
{
	return pthread_mutex_lock(&m_mutex) ? OpStatus::ERR : OpStatus::OK;
}

OP_STATUS OpSharedMemoryMutex::Release()
{
	return pthread_mutex_unlock(&m_mutex) ? OpStatus::ERR : OpStatus::OK;
}

bool OpSharedMemoryMutex::TryAcquire()
{
	return pthread_mutex_trylock(&m_mutex) == 0;
}
#endif //POSIX_OK_SHARED_MEMORY
