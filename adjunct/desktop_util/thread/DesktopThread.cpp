/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/thread/DesktopThread.h"

/***********************************************************************************
 ** Destructor
 **
 ** DesktopThread::~DesktopThread
 ***********************************************************************************/
DesktopThread::~DesktopThread()
{
	// Stop the thread if it's currently running
	OpStatus::Ignore(Stop());

	// Cleanup the queues
	OP_DELETEA(m_queues);
}


/***********************************************************************************
 ** Initialize - run this before running any other functions!
 **
 ** DesktopThread::Init
 ***********************************************************************************/
OP_STATUS DesktopThread::Init()
{
	// Initialize queues
	m_queues = OP_NEWA(ThreadMessageQueue, m_max_priority + 1);
	if (!m_queues)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned i = 0; i <= m_max_priority; i++)
		RETURN_IF_ERROR(m_queues[i].Init());

	// Initialize queue semaphore
	return m_queue_semaphore.Init(0);
}


/***********************************************************************************
 ** Start the thread
 **
 ** DesktopThread::Start
 ***********************************************************************************/
OP_STATUS DesktopThread::Start()
{
	if (m_thread_running)
		return OpStatus::ERR;

	// Start the thread!
	OP_STATUS status = DesktopThreads::CreateThread(m_thread_id, MainLoop, this);
	if (OpStatus::IsSuccess(status))
		m_thread_running = TRUE;

	return status;
}


/***********************************************************************************
 ** Stop the thread (blocks until thread is stopped)
 **
 ** DesktopThread::Stop
 ** @param asap Whether to stop as soon as possible (with highest priority)
 ***********************************************************************************/
OP_STATUS DesktopThread::Stop(BOOL asap)
{
	if (!m_thread_running)
		return OpStatus::ERR;

	// Post a stop message
	RETURN_IF_ERROR(PostMessage(MSG_STOP_THREAD, 0, 0, asap ? m_max_priority : 0));

	// Wait until the thread finishes
	OpStatus::Ignore(DesktopThreads::WaitForThread(m_thread_id));
	m_thread_running = FALSE;

	return DesktopThreads::DestroyThread(m_thread_id);
}


/***********************************************************************************
 ** Post a message to this thread
 **
 ** DesktopThread::PostMessage
 ** @param msg Message to post
 ** @param par1 Free first parameter
 ** @param par2 Free second parameter
 ** @param priority Priority for this message, higher priority gets handled first.
 **                 At most m_max_priority.
 ***********************************************************************************/
OP_STATUS DesktopThread::PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned priority)
{
	OP_ASSERT(priority <= m_max_priority);

	// Create the new message
	RETURN_IF_ERROR(m_queues[priority].Add(msg, par1, par2));

	// Increase the semaphore
	return m_queue_semaphore.Increment();
}


/***********************************************************************************
 ** Main loop of the thread, handles messages
 **
 ** DesktopThread::MainLoop
 ***********************************************************************************/
void* DesktopThread::MainLoop(void* arg)
{
	DesktopThread* thread = (DesktopThread*)arg;

	while (TRUE)
	{
		// Wait until there is a message
		if (OpStatus::IsError(thread->m_queue_semaphore.Decrement()))
			break;

		// Get one message with the highest priority
		OpAutoPtr<ThreadMessage> message;

		for (int i = thread->m_max_priority; i >= 0 && !message.get(); i--)
			message = thread->m_queues[i].Get();

		OP_ASSERT(message.get());

		// Stop if necessary, else handle the message
		if (message->msg == MSG_STOP_THREAD)
			break;
		else
			thread->HandleMessage(message->msg, message->par1, message->par2);
	}

	thread->OnThreadDestruct();

	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////
//                            ThreadMessageQueue                                   //
/////////////////////////////////////////////////////////////////////////////////////

/***********************************************************************************
 ** Destructor
 **
 ** DesktopThread::ThreadMessageQueue::~ThreadMessageQueue
 ***********************************************************************************/
DesktopThread::ThreadMessageQueue::~ThreadMessageQueue()
{
	ThreadMessage* msg = m_queue;
	while (msg)
	{
		ThreadMessage* next_msg = msg->next;

		OP_DELETE(msg);
		msg = next_msg;
	}
}


/***********************************************************************************
 ** Initialize - run this before running other functions on the queue
 **
 ** DesktopThread::ThreadMessageQueue::Init
 ***********************************************************************************/
OP_STATUS DesktopThread::ThreadMessageQueue::Init()
{
	m_queue = OP_NEW(ThreadMessage, ());
	if (!m_queue)
		return OpStatus::ERR_NO_MEMORY;

	m_queue_next = m_queue;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add a message to the queue
 **
 ** DesktopThread::ThreadMessageQueue::Add
 ***********************************************************************************/
OP_STATUS DesktopThread::ThreadMessageQueue::Add(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Create the new message (actually the 'next' message)
	ThreadMessage* new_msg = OP_NEW(ThreadMessage, ());
	if (!new_msg)
		return OpStatus::ERR_NO_MEMORY;

	// Populate the message
	m_queue_next->msg  = msg;
	m_queue_next->par1 = par1;
	m_queue_next->par2 = par2;
	m_queue_next->next = new_msg;

	// Next message is now the one that we created
	m_queue_next = m_queue_next->next;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get the first message from the queue (and remove it) if it's there
 **
 ** DesktopThread::ThreadMessageQueue::Get
 ***********************************************************************************/
DesktopThread::ThreadMessage* DesktopThread::ThreadMessageQueue::Get()
{
	if (m_queue->next)
	{
		ThreadMessage* message = m_queue;
		m_queue = m_queue->next;
		return message;
	}

	return NULL;
}
