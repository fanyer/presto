/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DEVICE_API_LISTENABLE_H
#define DEVICE_API_LISTENABLE_H

/**
 * Base class for management of listeners. It provides the basic storage
 * of listeners and a way to lock listeners while notifications are active.
 * Use methods for AttachListener() and DetachListener() to add and remove
 * listeners.
 *
 * @note This class should not be used directly as it works on void pointers,
 *       instead use the templated OpListenable class.
 * @note It holds the listeners but doesn't own them.
 */
class OpGenericListenable
{
public:
	/**
	 * Initialize with an empty listener list and zero locks.
	 */
	OpGenericListenable()
	  : m_notyfying_event(0)
	{
	}

	/**
	 * Attach a new listener. Attaching the same listener multiple times
	 * is not allowed.
	 * @param listener The listener to add.
	 * @return OK if successful, ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AttachListener(void* listener)
	{
		OP_ASSERT(m_listeners.Find(listener) < 0 && "Trying to attach the same listener twice.");
		return m_listeners.Add(listener);
	}

	/**
	 * Detach a previously registered listener.
	 * @param listener The listener to remove.
	 * @return OK if successful, ERR_NO_SUCH_RESOURCE if listener is not attached.
	 */
	OP_STATUS DetachListener(void* listener)
	{
		INT32 index = m_listeners.Find(listener);
		if (index < 0)
			return OpStatus::ERR_NO_SUCH_RESOURCE;
		if (m_notyfying_event > 0)
			m_listeners.Replace(index, NULL);
		else
			m_listeners.Remove(index);
		return OpStatus::OK;
	}

	/**
	 * Checks if the listenable object is currently locked.
	 * @return @c TRUE if there one or more locks on the listenable object, @c FALSE otherwise.
	 */
	BOOL IsLocked() const
	{
		return m_notyfying_event > 0;
	}

	/**
	 * Increase lock counter.
	 */
	void Lock()
	{
		++m_notyfying_event;
	}

	/**
	 * Decrease lock counter.
	 * @note Make sure calls to Lock() and Unlock() are balanced.
	 */
	void Unlock()
	{
		OP_ASSERT(m_notyfying_event > 0);
		--m_notyfying_event;
	}

	/**
	 * Remove any NULL entries in the listener vector. These entries are
	 * set when a listener is detached while IsLocked() is @a TRUE.
	 */
	void CleanupListeners()
	{
		OP_ASSERT(m_notyfying_event >=0);
		if (m_notyfying_event == 0)
		{
			UINT32 i = 0;
			while(i < m_listeners.GetCount())
			{
				if (!m_listeners.Get(i))
					m_listeners.Remove(i);
				else
					++i;
			}
		}
	}

	/**
	 * Checks if there are any active listeners attached.
	 * @return TRUE if there are any active listeners attached.
	 * @note Takes into account NULL-ed listeners during event notification.
	 */
	BOOL HasListeners()
	{
		return Range(*this).Front() ? TRUE : FALSE;
	}

	/**
	 * Return all listeners attached to this listenable object.
	 * @return A vector of listener objects.
	 * @note The vector may contain NULL entries, they should be skipped.
	 */
	const OpVector<void> &GetListeners() const { return m_listeners; }

	/**
	 * This class is meant to be used on the stack to lock the listenable
	 * object while the notification process is active. When it is locked
	 * any listeners which are removed will have its entry set to NULL instead
	 * of having it removed from the list, this ensures that the listeners
	 * can be safely iterated.
	 *
	 * When all locks are removed from a listenable object it will call
	 * OpGenericListenable::CleanupListeners.
	 *
	 * Example:
	 * @code
	 * void Notify()
	 * {
	 *     CleanupGuard guard(*this);
	 *     ... Do some work
	 * } // guard destructor is called when leaving function
	 * @endcode
	 */
	class CleanupGuard
	{
	public:
		/**
		 * Initialize guard on a listenable object by locking it.
		 * @param l The listenable object to guard.
		 */
		CleanupGuard(OpGenericListenable &l)
			: listenable(l)
		{
			listenable.Lock();
		}

		/**
		 * Release lock and perform cleanup if no more locks are active.
		 */
		~CleanupGuard()
		{
			listenable.Unlock();
			if (!listenable.IsLocked())
				listenable.CleanupListeners();
		}

	private:
		OpGenericListenable &listenable;
	};

	/**
	 * A range that spans all active listeners in a listenable object.
	 * Any listeners which have been set to NULL will be skipped.
	 */
	class Range
	{
	public:
		typedef void *Type;

		/**
		 * Initialize range from a listenable object containing listeners.
		 * @param l The listenable object to work on.
		 */
		Range(const OpGenericListenable &l)
			: m_listeners(l.GetListeners())
			, m_index(0)
			, m_count(m_listeners.GetCount())
		{
			// Skip any initially NULL listeners
			while (!Empty() && Front() == NULL)
				++m_index;
		}

		/**
		 * @return @c FALSE if there is more data available in the range.
		 */
		BOOL Empty() const { return m_index >= m_count; }

		/**
		 * Return the listener at the front of the range.
		 * Calling this is allowed only if Empty() returns @c FALSE.
		 * @return Listener object.
		 */
		void *Front() const { return m_listeners.Get(m_index); }

		/**
		 * Advances to the next listener in the range, any listeners which are
		 * set to NULL will be skipped.
		 * Calling this is allowed only if Empty() returns @c FALSE.
		 */
		void PopFront()
		{
			do
			{
				++m_index;
			} while (!Empty() && Front() == NULL);
		}

	private:
		const OpVector<void> &m_listeners;
		unsigned m_index, m_count;
	};
	friend class Range;

protected:
	OpVector<void> m_listeners;
	int m_notyfying_event;
};


class ST_deviceapioplistenable;
/**
 * Provides an interface for managing listeners in any class. The basic
 * management is handled by OpGenericListenable while this class handles
 * casting to the correct listener type.
 *
 * Basic usage is to inherit from OpListenable with the name of the listener
 * class as the template parameter. This will define methods AttachListener()
 * and DetachListener() for adding and removing listeners for the specific
 * listener class.
 *
 * Example:
 * @code
 * class OperationHandler
 *     : public OpListenable<OpLongOperationListener>
 * {
 *   ...
 * };
 * @endcode
 *
 * If more than one listener class is needed just inherit multiple times, once
 * for each listener class.
 * @code
 * class MultipleOperationHandler
 *     : public OpListenable<OpLongOperationListener>
 *     , public OpListenable<OpStringOperationListener>
 * {
 *   ...
 * };
 * @endcode
 *
 * To define methods for calling events on the listener classes use one of
 * these macros:
 * - @ref DECLARE_LISTENABLE_EVENT0
 * - @ref DECLARE_LISTENABLE_EVENT1
 * - @ref DECLARE_LISTENABLE_EVENT2
 *
 * The number indicates how many parameters to define for the event.
 *
 * Example:
 * @code
 *   ...
 *   DECLARE_LISTENABLE_EVENT0(OpLongOperationListener, Finished);
 *   DECLARE_LISTENABLE_EVENT1(OpLongOperationListener, Progress, int);
 *   DECLARE_LISTENABLE_EVENT1(OpStringOperationListener, RemovedKey, const OpString &);
 *   DECLARE_LISTENABLE_EVENT2(OpStringOperationListener, NewValue, const OpString &, int);
 *   ...
 * @endcode
 *
 * In the class implementation when we want to notify listeners of an event
 * we call one of the defined @i Notify<event> methods:
 * @code
 * ...
 *	NotifyProgress(m_progress); // Calls OnProgress(m_progress) on each OpLongOperationListener listener
 *	NotifyFinished(); // Calls OnFinished() on each OpLongOperationListener listener
 *	NotifyRemovedKey(name); // Calls OnRemovedKey(name) on each OpStringOperationListener listener
 *  NotifyNewValue(name, value); // Calls OnNewValue(name, value) on each OpStringOperationListener listener
 * ...
 * @endcode
 */
template<class ListenerType>
class OpListenable : protected OpGenericListenable
{
	friend class ST_deviceapioplistenable;
public:
	/**
	 * Attach a new listener. Attaching the same listener multiple times
	 * is not allowed.
	 * @param listener Listener object which will receive notifications.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	OP_STATUS AttachListener(ListenerType* listener)
	{
		return OpGenericListenable::AttachListener(reinterpret_cast<void*>(listener));
	}

	/**
	 * Detach a previously registered listener.
	 * @param listener Listener object which was previously attached.
	 * @return OK if successful, ERR_NO_SUCH_RESOURCE if listener is not part of listeners, ERR for other errors.
	 */
	OP_STATUS DetachListener(ListenerType* listener)
	{
		return OpGenericListenable::DetachListener(reinterpret_cast<void*>(listener));
	}

	/**
	 * Checks if there are any active listeners attached.
	 * @return TRUE if there are any active listeners attached.
	 * @note Takes into account NULL-ed listeners during event notification.
	 */
	BOOL HasListeners()
	{
		return OpGenericListenable::HasListeners();
	}

	/**
	 * A range that spans all active listeners in a listenable object.
	 * This will also use the OpGenericListenable::CleanupGuard class to make
	 * sure any listeners that are detached while the notification process
	 * is active will are cleaned up.
	 *
	 * Any listeners which have been set to NULL will be skipped.
	 */
	class ListenerRange
		: public Range
		, public CleanupGuard
	{
	public:
		typedef ListenerType *Type;

		/**
		 * Initialize range from a specific listenable object and lock it.
		 * @param l The listenable object to work on.
		 */
		ListenerRange(OpListenable<ListenerType> &l)
			: Range(l)
			, CleanupGuard(l)
		{
		}

		/**
		 * @copydoc OpGenericListener::Range
		 */
		ListenerType *Front() const { return static_cast<ListenerType *>(OpGenericListenable::Range::Front()); }
	};
	friend class ListenerRange;
};

/**
 * Defines a method called Notify{event_name}, e.g. @a NotifyProgress. These
 * methods must be called when all listeners must be notified about events.
 * The listener type must be specified in the first parameter. This is needed
 * in case a class inherits from OpListenable multiple times.
 *
 * For instance to define a @c Finished and @c Progress event one would do:
 * @code
 *   DECLARE_LISTENABLE_EVENT0(OpLongOperationListener, Finished);
 *   DECLARE_LISTENABLE_EVENT1(OpLongOperationListener, Progress, int);
 * @endcode
 *
 * This generates methods like this:
 * @code
 *	void NotifyFinished();
 *  void NotifyProgress(int arg1);
 * @endcode
 *
 * If the callback needs arguments use either @ref DECLARE_LISTENABLE_EVENT1
 * or @ref DECLARE_LISTENABLE_EVENT2.
 *
 * @param listener_type The listener class used for this notification event, e.g. @c MyListener
 * @param event_name Name of the event callback, e.g. Progress, used for generated method name and callback.
 * @see OpListenable for more details.
 */

#define DECLARE_LISTENABLE_EVENT0(listener_type, event_name) \
void Notify##event_name()									\
{															\
	OpListenable<listener_type>::ListenerRange r(*this);	\
	for (; !r.Empty(); r.PopFront())						\
		r.Front()->On##event_name();						\
}

/**
 * Similar to @ref DECLARE_LISTENABLE_EVENT0 but supports one argument.
 *
 * @param listener_type The listener class used for this notification event, e.g. @c MyListener
 * @param event_name Name of the event callback, e.g. Progress, used for generated method name and callback.
 * @param arg1type The type for first argument.
 * @see DECLARE_LISTENABLE_EVENT0 for more details
 */
#define DECLARE_LISTENABLE_EVENT1(listener_type, event_name, arg1type) \
void Notify##event_name(arg1type arg1)						\
{															\
	OpListenable<listener_type>::ListenerRange r(*this);	\
	for (; !r.Empty(); r.PopFront())						\
		r.Front()->On##event_name(arg1);					\
}

/**
 * Similar to @ref DECLARE_LISTENABLE_EVENT0 but supports two arguments.
 *
 * @param listener_type The listener class used for this notification event, e.g. @c MyListener
 * @param event_name Name of the event callback, e.g. Progress, used for generated method name and callback.
 * @param arg1type The type for first argument.
 * @param arg2type The type for second argument.
 * @see DECLARE_LISTENABLE_EVENT0 for more details
 */
#define DECLARE_LISTENABLE_EVENT2(listener_type, event_name, arg1type, arg2type) \
void Notify##event_name(arg1type arg1, arg2type arg2)		\
{															\
	OpListenable<listener_type>::ListenerRange r(*this);	\
	for (; !r.Empty(); r.PopFront())						\
		r.Front()->On##event_name(arg1, arg2);				\
}

#endif // DEVICE_API_LISTENABLE_H
