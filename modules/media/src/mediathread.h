/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIATHREAD_H
#define MEDIATHREAD_H

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/domenvironment.h"
#include "modules/dom/domeventtypes.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/simset.h"

class MediaThread;

/** An interface for observing events on MediaThreads. */
class MediaThreadListener
{
public:
	virtual ~MediaThreadListener() {}

	/** Called when a thread has finished.
	 *
	 * @param thread The thread that finished.
	 */
	virtual void OnThreadFinished(MediaThread* thread) = 0;
};

/** Represents a thread which handles a media event. */
class MediaThread
	: public ListElement<MediaThread>
{
public:
	/** Construct a new MediaThread.
	 *
	 * @param listener An object that will observe events on the thread.
	 * @param thread The actual ECMAScript event thread.
	 */
	MediaThread(MediaThreadListener* listener, ES_Thread* thread);

	/** Is this a timeupdate event handler thread? */
	BOOL IsTimeUpdateThread() const;

	/** Has this thread started?
	 *
	 * @return TRUE if the thread has started, FALSE if it only is scheduled
	 *         to start.
	 */
	BOOL IsStarted() const { return m_thread->IsStarted(); }

	/** Cancel a thread (even if the thread has started).
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. Note that the
	 *         cancelling itself will always succeed; only thread signalling
	 *         can fail.
	 */
	OP_STATUS Cancel();

private:
	/** Called when a signal is received from @c m_thread. */
	void OnThreadSignal();

	/** This class is an implementation of ES_ThreadListener.
	 *
	 * Because both ListElement and ES_ThreadListener inherit from Link,
	 * we use a nested class to resolve ambiguities.
	 */
	class Listener
		: public ES_ThreadListener
	{
	public:
		Listener(MediaThread* owner);
		// Implements ES_ThreadListener.
		virtual OP_STATUS Signal(ES_Thread* thread, ES_ThreadSignal signal);
	private:
		MediaThread* m_owner;
	};

	// This object listens for events on @c m_thread.
	Listener m_proxy_listener;

	// An object which receives notifications when something significant
	// happens to the MediaThread.
	MediaThreadListener* m_listener;

	// The actual ECMAScript thread.
	ES_Thread* m_thread;

	// For access to OnThreadSignal.
	friend class Listener;
};

/** An interface for observing events on MediaThreadQueues.
 *
 * A class may implement this interface if it needs to know when
 * there are no more queued threads of a certain type. This can
 * for example be used to ensure that events are fired one after
 * another.
 */
class MediaThreadQueueListener
{
public:
	virtual ~MediaThreadQueueListener() {}

	/** The last timeupdate event handler has finished.
	 *
	 * In other words, this is called when the observed MediaThreadQueue
	 * transitions from a state of one (or more) timeupdate event
	 * handlers, to a state of zero timeupdate event handlers.
	 */
	virtual void OnTimeUpdateThreadsFinished() = 0;
};

/** Create and keep track of a set of related event handler threads.
 *
 * One feature of this class is the ability to cancel the queued event
 * threads, or "tasks" as it were in HTML5. It can also keep track of the
 * number of running (and scheduled) event handler threads of a certain event
 * type. This is useful for clients which do not wish to queue an event handler
 * thread of a certain type if one such event handler is already running, or
 * scheduled to run.
 *
 * @see http://www.whatwg.org/html#queue-a-task
 */
class MediaThreadQueue
	: public MediaThreadListener
{
public:
	/** Construct a new MediaThreadQueue.
	 *
	 * @param listener An observer of significant events in the MediaThreadQueue.
	 */
	MediaThreadQueue(MediaThreadQueueListener* listener);

	/** Queue an event in the specified DOM_Environment.
	 *
	 * @param environment The DOM_Environment to fire the event in.
	 * @param data The event data for DOM_Environment::HandleEvent.
	 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS QueueEvent(DOM_Environment* environment, const DOM_Environment::EventData &data);

	/** Cancel all MediaThreads which have not yet started.
	 *
	 * MediaThreads which have already started will be allowed to finish, and
	 * the MediaThreadQueue will continue to keep track of them until they do
	 * finish.
	 */
	void CancelAll();

	/** Is a timeupdate event thread is running, or scheduled to run? */
	BOOL IsTimeUpdateQueued() const { return m_timeupdate_queued > 0; }

private:
	/** Cancel a scheduled MediaThread.
	 *
	 * If the provided thread is already started, the thread will be allowed
	 * to finish. If the thread is merely scheduled, it will be cancelled.
	 *
	 * @param thread The MediaThread to cancel (if possible).
	 */
	void Cancel(MediaThread* thread);

	/** Remove a MediaThread from the list of threads.
	 *
	 * Calling this simply causes the MediaThreadQueue to stop listening
	 * for threads signals from the specified thread, and delete all
	 * references to it. It does not cancel the thread.
	 *
	 * @param thread The MediaThread to remove from the list.
	 */
	void Remove(MediaThread* thread);

	// Implements MediaThreadListener.
	virtual void OnThreadFinished(MediaThread* thread);

	// An object which receives notifications when something significant
	// happens to the MediaThreadQueue.
	MediaThreadQueueListener* m_listener;

	// The number of queued timeupdate event threads.
	unsigned m_timeupdate_queued;

	// The list of queued threads. Some of these may be in progress, others
	// may simply be scheduled to run.
	AutoDeleteList<MediaThread> m_threads;
};

#endif // MEDIA_HTML_SUPPORT

#endif // MEDIATHREAD_H
