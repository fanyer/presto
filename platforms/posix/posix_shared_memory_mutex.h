/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef POSIX_SHARED_MEMORY_MUTEX_H
#define POSIX_SHARED_MEMORY_MUTEX_H
#ifdef POSIX_OK_SHARED_MEMORY
#include <pthread.h>

/** Implementation of OpSharedMemoryMutex PI.
 *
 * Uses a pthread_mutex, pretty trivial.
 */
class OpSharedMemoryMutex
{
public:
	~OpSharedMemoryMutex();
	OP_STATUS Construct();
	OP_STATUS Acquire();
	OP_STATUS Release();
	bool TryAcquire();
private:
	pthread_mutex_t m_mutex;
};
#endif // POSIX_OK_SHARED_MEMORY
#endif // POSIX_SHARED_MEMORY_MUTEX_H
