/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#ifndef _NO_THREADS_

#include "mt.h"


THREAD_HANDLE Thread::CreateThread(void* (*entry)(void*), void* argument)
{
	THREAD_HANDLE handle;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
# ifdef OPERA_LNXJAVA_SVALBARD
	pthread_attr_setstacksize(&attr, 128 * 1024);
# endif // OPERA_LNXJAVA_SVALBARD
	if (pthread_create(&handle, &attr, entry, argument))
		handle = 0;

	pthread_attr_destroy(&attr);

	return handle;
}

THREAD_HANDLE Thread::Self()
{
	return pthread_self();
}

int Thread::Join(THREAD_HANDLE handle)
{
	return pthread_join(handle, NULL);
}

int Thread::Detach(THREAD_HANDLE handle)
{
	return pthread_detach(handle);
}

Mutex::Mutex()
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#ifdef _DEBUG
	int res =
#endif
		pthread_mutex_init(&mutex, &attr);
	OP_ASSERT(!res);
	pthread_mutexattr_destroy(&attr);
}

Mutex::~Mutex()
{
#ifdef _DEBUG
	int res =
#endif
		pthread_mutex_destroy(&mutex);
	OP_ASSERT(!res);
}

void Mutex::Acquire()
{
#ifdef _DEBUG
	int res =
#endif
		pthread_mutex_lock(&mutex);
	OP_ASSERT(!res);
}

void Mutex::Release()
{
#ifdef _DEBUG
	int res =
#endif
		pthread_mutex_unlock(&mutex);
	OP_ASSERT(!res);
}

#endif // _NO_THREADS_

