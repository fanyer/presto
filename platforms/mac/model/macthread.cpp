/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MACGOGI
#include "platforms/macgogi/pi/network/macthread.h"
#else
#include "platforms/mac/model/macthread.h"
#endif

#ifdef __MWERKS__
#pragma profile off
#endif

MacThread::MacThread()
{
	isRunning = false;
#ifdef USE_PTHREADS
	thread = NULL;
#else
	notifyQueue = NULL;
#endif
}

MacThread::~MacThread()
{
	terminate();
}

OSStatus MacThread::start()
{
#ifdef USE_PTHREADS
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	typedef void *(*start_routine)(void *);

	isRunning = true;

	OSStatus err = pthread_create(&thread, &attr, (start_routine)sRun, (void *)this);

	if(err != noErr)
	{
		thread = 0;
		isRunning = false;
		return err;
	}
#else
	OSStatus err;

	// Create the default communicatiosn queue for the
	// thread to be created.
	err = MPCreateQueue(&notifyQueue);

	// Generally at this stage if there is an error, the error type is
	// kMPInsufficientResourcesErr
	if(err != noErr)
		return err;


	// Before starting the thread, set the isRunning flag to true.
	// This makes it so that if the thread immediately exits,
	// the isRunning won't be set to true after being reset to
	// false.
	isRunning = true;

	int terminationParameter1 = 0;
	int terminationParameter2 = 0;
	err = MPCreateTask(sRun,
					   (void *) this,
					   (unsigned long) 0,
					   notifyQueue,
					   &terminationParameter1,
					   &terminationParameter2,
					   0,
					   &task);

	// The error code returned from here also should be
	// kMPInsufficientResourcesErr.
	if(err != noErr)
	{
		// Checking the error code on the delete queue operation is
		// generally not a good idea. Therefore we don't bother. If
		// this fails there is no recourse we can take. It is better
		// to report the error from the MPCreateTask statement.
		MPDeleteQueue(notifyQueue);
		notifyQueue = NULL;

		isRunning = false;

		return err;
	}
#endif

	return noErr;
}

void MacThread::terminate()
{
#ifdef USE_PTHREADS
	if (!thread)
		return;
	OSStatus err = pthread_cancel(thread);
	pthread_join(thread,NULL);
	thread = 0;
#else
	OSStatus err = MPTerminateTask(task, noErr);
	MPDeleteQueue(notifyQueue);
#endif

	if(err == noErr)
		isRunning = false;
}


OSStatus MacThread::sRun(void *parameter)
{
	MacThread *mpThread = (MacThread *) parameter;
	return mpThread->run();
}

#ifdef __MWERKS__
#pragma profile reset
#endif
