/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/posix_task_queue.h"
#include <pthread.h>

OP_STATUS DesktopTaskQueue::Make(DesktopTaskQueue** new_queue)
{
	*new_queue = new PosixTaskQueue;
	return *new_queue ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

PosixTaskQueue::PosixTaskQueue()
{
	exit = false;
	pthread_mutex_init(&mutex, NULL);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_cond_init(&task_added, NULL);
	pthread_cond_init(&work_completed, NULL);
	pthread_create(&thread, &attr, static_run, this);
}

PosixTaskQueue::~PosixTaskQueue()
{
	if (thread)
	{
		pthread_mutex_lock(&mutex);
		exit = true;
		pthread_cond_signal(&task_added);
		pthread_mutex_unlock(&mutex);
		pthread_join(thread, NULL);
		thread = 0;
	}
}

void PosixTaskQueue::AddTask(OpTask* task)
{
	pthread_mutex_lock(&mutex);
	tasks.Add(task);
	pthread_cond_signal(&task_added);
	pthread_mutex_unlock(&mutex);
}

void PosixTaskQueue::HandleSynchronousTask(OpTask* task)
{
}

void PosixTaskQueue::HandleSynchronousTaskWhenDone(OpTask* task)
{
}

void* PosixTaskQueue::Run()
{
	while (true)
	{
		OpTask* task = GetNextTask();
		if (task)
		{
			task->Execute();
			delete task;
		}
		else if(exit)
			break;
	}
	return NULL;

}

// static
void* PosixTaskQueue::static_run(void* data)
{
	return ((PosixTaskQueue*)data)->Run();
}

OpTask* PosixTaskQueue::GetNextTask()
{
	OpTask* task = NULL;
	pthread_mutex_lock(&mutex);
	while ((!tasks.GetCount() || ((task = tasks.Remove(0)) == NULL)) && !exit)
	{
		pthread_cond_wait(&task_added, &mutex);
	}
	pthread_mutex_unlock(&mutex);
	return task;
}
