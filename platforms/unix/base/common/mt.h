/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#ifndef MT_H
#define MT_H

#include <pthread.h>
#include <semaphore.h>


typedef pthread_t THREAD_HANDLE;

#define THREAD_HANDLE_NULL 0
#define THREAD_HANDLES_EQUAL(H1,H2) (H1 == H2)

class Thread
{
public:
	/** Create thread */
    static THREAD_HANDLE CreateThread(void* (*entry)(void*), void* argument);

	/** Return the thread identifier for the calling thread. */
    static THREAD_HANDLE Self();

	/** Wait for the given thread to terminate */
	static int Join(THREAD_HANDLE handle);

	/** Detach and terminate since nobody is interested anymore. */
	static int Detach(THREAD_HANDLE handle);

	/** Detect null thread */
	static bool IsNullThread(THREAD_HANDLE handle) { return THREAD_HANDLES_EQUAL(handle, THREAD_HANDLE_NULL); }
};


/** This class implements a regular recursive mutex.
 * The thread that holds the mutex may acquire it several times
 * (recursively).  The thread must release it as many times as
 * it has acquired it.
 */
class Mutex
{
public:
	Mutex();
	~Mutex();

	/* Acquire mutex. Block until the mutex can be acquired. */
	void Acquire();

	/* Release mutex. */
	void Release();

private:
	pthread_mutex_t	mutex;

	Mutex(const Mutex& X);
};

#endif // MT_H
