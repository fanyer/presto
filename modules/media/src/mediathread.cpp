/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/media/src/mediathread.h"

MediaThread::MediaThread(MediaThreadListener* listener, ES_Thread* thread)
	: m_proxy_listener(this)
	, m_listener(listener)
	, m_thread(thread)
{
	thread->AddListener(&m_proxy_listener);
}

BOOL
MediaThread::IsTimeUpdateThread() const
{
	DOM_EventType actual = m_thread->GetInfo().data.event.type;

	return (m_thread->Type() == ES_THREAD_EVENT && actual == MEDIATIMEUPDATE);
}

OP_STATUS
MediaThread::Cancel()
{
	return m_thread->GetScheduler()->CancelThread(m_thread);
}

void
MediaThread::OnThreadSignal()
{
	m_listener->OnThreadFinished(this);
}

MediaThread::Listener::Listener(MediaThread* owner)
	: m_owner(owner)
{
	OP_ASSERT(owner);
}

/* virtual */ OP_STATUS
MediaThread::Listener::Signal(ES_Thread* /*thread*/, ES_ThreadSignal /*signal*/)
{
	m_owner->OnThreadSignal();
	return OpStatus::OK;
}

MediaThreadQueue::MediaThreadQueue(MediaThreadQueueListener* listener)
	: m_listener(listener)
	, m_timeupdate_queued(0)
{
	OP_ASSERT(listener);
}

OP_STATUS
MediaThreadQueue::QueueEvent(DOM_Environment* environment, const DOM_Environment::EventData &data)
{
	OP_ASSERT(environment);

	ES_Thread* event_thread;

	OP_BOOLEAN status = environment->HandleEvent(data, NULL, &event_thread);
	RETURN_IF_MEMORY_ERROR(status);

	if (status == OpBoolean::IS_TRUE && event_thread)
	{
		MediaThread* media_thread = OP_NEW(MediaThread, (this, event_thread));
		RETURN_OOM_IF_NULL(media_thread);
		media_thread->Into(&m_threads);

		if (media_thread->IsTimeUpdateThread())
			m_timeupdate_queued++;
	}

	return OpStatus::OK;
}

void
MediaThreadQueue::CancelAll()
{
	MediaThread* thread = m_threads.First();

	while (thread)
	{
		MediaThread* next = thread->Suc();
		Cancel(thread);
		thread = next;
	}
}

void
MediaThreadQueue::Cancel(MediaThread* thread)
{
	if (!thread->IsStarted() && OpStatus::IsError(thread->Cancel()))
		Remove(thread);
}

void
MediaThreadQueue::Remove(MediaThread* thread)
{
	if (thread->IsTimeUpdateThread())
	{
		OP_ASSERT(m_timeupdate_queued > 0);
		m_timeupdate_queued--;
	}

	thread->Out();
	OP_DELETE(thread);
}

/* virtual */ void
MediaThreadQueue::OnThreadFinished(MediaThread* thread)
{
	Remove(thread);

	if (!IsTimeUpdateQueued())
		m_listener->OnTimeUpdateThreadsFinished();
}

#endif //MEDIA_HTML_SUPPORT
