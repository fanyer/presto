/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Arjan van Leeuwen (arjanl)
*/

#ifndef OPQUEUE_H
#define OPQUEUE_H


/** @brief Make class T derive from OpQueueItem<T> to use it in an OpQueue
  */
template<typename T>
class OpQueueItem
{
	public:
		OpQueueItem() : m_queue_next(0) {}

		T*	 QueueGetNext() const { return m_queue_next; }

		void QueueSetNext(T* next) { m_queue_next = next; }

	private:
		T*	 m_queue_next;
};


/** @brief Queue of items: add items to the end, remove items from the front
  */
template<class T>
class OpQueue
{
public:
	OpQueue() : m_queue_first(0), m_queue_last(0), m_count(0) {}

	/** Add an item to the queue
	  * @param item Item to add to the queue - NULL-values will be silently ignored
	  */
	void Enqueue(T* item)
		{
			if (item)
			{
				if (!m_queue_first)
				{
					m_queue_first = m_queue_last = item;
					item->QueueSetNext(0);
				}
				else
				{
					m_queue_last->QueueSetNext(item);
					m_queue_last = item;
				}
				m_count++;
			}
		}

	/** Remove the first item from the queue
	  * @return The item that was removed
	  */
	T*	 Dequeue()
		{
			if (m_queue_first)
			{
				T* item = m_queue_first;
				m_queue_first = item->QueueGetNext();
				item->QueueSetNext(0);
				m_count--;
				return item;
			}
			return 0;
		}

	/** @return Whether the queue is empty
	  */
	BOOL IsEmpty() const
		{ return !m_queue_first; }

	/** @return Element at the front of the queue (i.e. least recently inserted)
	  */
	T*	 Front() const
		{ return m_queue_first; }

	/** @return Element at the back of the queue (i.e. most recently inserted)
	  */
	T*	 Back() const
		{ return m_queue_last; }

	/** @return Number of elements in the queue
	  */
	unsigned GetCount() const
		{ return m_count; }

private:
	OpQueue(const OpQueue&);
	OpQueue& operator =(const OpQueue&);

	T* m_queue_first;
	T* m_queue_last;
	unsigned m_count;
};


/** @brief Like OpQueue, but deallocates contents on destruction
  */
template<class T>
class OpAutoQueue : public OpQueue<T>
{
public:
	~OpAutoQueue()
	{
		T* item;
		while ((item = this->Dequeue()))
			OP_DELETE(item);
	}
};


#endif // OPQUEUE_H
