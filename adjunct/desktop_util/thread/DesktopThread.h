// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef DESKTOP_THREAD_H
#define DESKTOP_THREAD_H

#include "adjunct/desktop_pi/DesktopThreads.h"
#include "adjunct/desktop_util/thread/DesktopMutex.h"
#include "adjunct/desktop_util/thread/DesktopSemaphore.h"

/** @brief Abstract class for creating a thread
  * @author Arjan van Leeuwen
  *
  * Derive your own class from this one to make a thread. Use wisely, and use DesktopMutex where necessary!
  *
  * To implement, do the following:
  * 0) Make a class that inherits (preferably privately, to hide the details) from DesktopThread
  * 1) Implement HandleMessage() to execute functions inside your thread
  * 2) Use PostMessage() to communicate with your thread from the outside. Easiest way: implement
  *    public functions in your derived class that call PostMessage() with the appropriate parameters.
  *
  * There's an example in adjunct/m2/src/engine/selexicon.{h,cpp}.
  */
class DesktopThread
{
public:
	/** Constructor
	  * @param max_priority Specify the possible priorities for messages (0 for only one priority)
	  */
	DesktopThread(unsigned max_priority = 0)
		: m_thread_running(FALSE), m_max_priority(max_priority), m_queues(0) {}

	/** Destructor
	  */
	virtual ~DesktopThread();

	/** Initialize - run this before running any other functions, in particular before starting the thread!
	  */
	virtual OP_STATUS Init();

	/** Start the thread
	  */
	virtual OP_STATUS Start();

	/** Stop the thread (blocks until thread is stopped)
	  * @param asap Whether to stop as soon as possible (with highest priority)
	  */
	virtual OP_STATUS Stop(BOOL asap = FALSE);

	/** Post a message from the main thread to this thread
	  * @param msg Message to post
	  * @param par1 Free first parameter
	  * @param par2 Free second parameter
	  * @param priority Priority for this message, higher priority gets handled first. At most m_max_priority.
	  */
	virtual OP_STATUS PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned priority = 0);

protected:
	/** Handle an incoming message for this thread
	  * @param msg Message to be handled
	  * @param par1 Free first parameter
	  * @param par2 Free second parameter
	  */
	virtual void HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;

	/** Actions to execute when this thread is destroyed
	  */
	virtual void OnThreadDestruct() {}

private:
	struct ThreadMessage
	{
		ThreadMessage() : msg(MSG_NO_MESSAGE), next(NULL) {}

		OpMessage		  msg;
		MH_PARAM_1		  par1;
		MH_PARAM_2		  par2;
		ThreadMessage*	  next;
	};

	class ThreadMessageQueue
	{
	public:
		ThreadMessageQueue() : m_queue(0), m_queue_next(0) {}
		~ThreadMessageQueue();

		OP_STATUS	   Init();
		OP_STATUS	   Add(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		ThreadMessage* Get();

	private:
		ThreadMessage* m_queue;
		ThreadMessage* m_queue_next;
	};

	static	void*		MainLoop(void* arg);

	BOOL				m_thread_running;
	unsigned			m_max_priority;
	OpThreadId			m_thread_id;
	DesktopSemaphore	m_queue_semaphore;	///< This semaphore keeps the number of messages in the queues
	ThreadMessageQueue* m_queues;
};

#endif // DESKTOP_THREAD_H
