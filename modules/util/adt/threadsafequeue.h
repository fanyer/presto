/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#ifdef UTIL_THREADSAFEQUEUE

#include "modules/pi/system/OpMutex.h"
#include "modules/otl/list.h"

/** @brief A thread-safe queue
  */
template<class T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue() : m_mutex(NULL) {}

	~ThreadSafeQueue() { OP_DELETE(m_mutex); }

	/** Initialize this queue. Needs to be run before invoking other functions.
	 */
	OP_STATUS Init() { return OpMutex::Create(&m_mutex); }

	/** @return Whether this queue is empty
	 */
	bool IsEmpty() const { OpMutex::AutoLock lock(*m_mutex); return m_queue.IsEmpty(); }

	/** Add an item to the end of the queue
	  * @param item The item to add
	  * @return OpStatus::OK on success, error values otherwise
	  */
	OP_STATUS Append(const T& item) { OpMutex::AutoLock lock(*m_mutex); return m_queue.Append(item); }

	/** Remove the first item from the queue and return it
	  * @return The removed item
	  */
	T PopFirst() { OpMutex::AutoLock lock(*m_mutex); return m_queue.PopFirst(); }

	/** Remove the last item from the queue and return it
	  * @return The removed item
	  */
	T PopLast() { OpMutex::AutoLock lock(*m_mutex); return m_queue.PopLast(); }

private:
	OtlList<T> m_queue;
	mutable OpMutex* m_mutex;
};

#endif // UTIL_THREADSAFEQUEUE

#endif // THREAD_SAFE_QUEUE_H
