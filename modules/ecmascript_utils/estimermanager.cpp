/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/ecmascript_utils/estimerevent.h"
#include "modules/ecmascript/ecmascript.h"

ES_TimerManager::ES_TimerManager()
	: msg_handler(NULL),
	  id(++g_opera->ecmascript_utils_module.timer_manager_id),
	  timer_event_id(0),
#ifdef SCOPE_PROFILER
	  scope_event_id(0),
#endif // SCOPE_PROFILER
	  has_set_callbacks(FALSE),
	  has_posted_timeout(FALSE),
	  is_active(TRUE)
{
}

ES_TimerManager::~ES_TimerManager()
{
	OP_ASSERT(expired.Empty());
	waiting.Clear();
	RemoveMessage();
	UnsetCallbacks();
}

OP_STATUS
ES_TimerManager::RepeatEvent(ES_TimerEvent *event)
{
	OP_ASSERT(event->GetTimerManager() == this);
	OP_ASSERT(expired.HasLink(event));
	event->Out();
	return AddEventInternal(event);
}

OP_STATUS
ES_TimerManager::AddEvent(ES_TimerEvent *new_event)
{
	new_event->SetTimerManager(this);
	new_event->SetId(++timer_event_id);
#ifdef SCOPE_PROFILER
	new_event->SetScopeThreadId(++scope_event_id);
#endif // SCOPE_PROFILER

	return AddEventInternal(new_event);
}

OP_STATUS
ES_TimerManager::AddEventInternal(ES_TimerEvent *new_event)
{
	new_event->SetTimeout();

	ES_TimerEvent *waiting_event = waiting.First();
	BOOL changed_earliest_timeout = FALSE;

	if (!waiting_event || new_event->TimeoutBefore(waiting_event))
	{
		/* Add event first and make sure to post a delayed MSG_ES_TIMEOUT if
		   necessary. */
		new_event->IntoStart(&waiting);
		changed_earliest_timeout = TRUE;
	}
	else
	{
		/* Add event sorted by timeout. */
		while (waiting_event && !new_event->TimeoutBefore(waiting_event))
			waiting_event = waiting_event->Suc();

		if (waiting_event)
			new_event->Precede(waiting_event);
		else
			new_event->Into(&waiting);
	}

	if (changed_earliest_timeout)
		RETURN_IF_ERROR(FireExpiredEvents());

	return OpStatus::OK;
}

void
ES_TimerManager::Deactivate()
{
	RemoveMessage();
	UnsetCallbacks();
	is_active = FALSE;
}

OP_STATUS
ES_TimerManager::Activate()
{
	is_active = TRUE;
	/* Calling the asynchronous PostMessage() here (instead of
	   FireExpiredEvents() which might add threads synchronous) since the
	   scheduler will not accept threads on not-yet-current documents. */
	return PostMessage();
}

BOOL
ES_TimerManager::HasWaiting()
{
	return !waiting.Empty();
}

OP_STATUS
ES_TimerManager::RemoveEvent(unsigned id)
{
	ES_TimerEvent *old_first = waiting.First();
	for (ES_TimerEvent *timer_event = waiting.First(); timer_event; timer_event = timer_event->Suc())
		if (timer_event->GetId() == id)
		{
			timer_event->Out();
			OP_DELETE(timer_event);
			break;
		}

	for (ES_TimerEvent *timer_event = expired.First(); timer_event; timer_event = timer_event->Suc())
		if (timer_event->GetId() == id)
		{
			timer_event->StopRepeating();
			break;
		}

	if (old_first != waiting.First())
		/* We have removed the first (or only) event and might
		   still have a timeout message posted for the event we removed. */
		return PostMessage();
	else
		return OpStatus::OK;
}

void
ES_TimerManager::RemoveAllEvents()
{
	waiting.Clear();
	expired.Clear();
	RemoveMessage();
	UnsetCallbacks();
}

void
ES_TimerManager::RemoveExpiredEvent(ES_TimerEvent *event)
{
	OP_ASSERT(expired.HasLink(event));
	event->Out();
	OP_DELETE(event);
}

/* virtual */ void
ES_TimerManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_ES_TIMEOUT);
	OP_ASSERT(has_posted_timeout);

	has_posted_timeout = FALSE;
	OP_STATUS ret = FireExpiredEvents();

	if (OpStatus::IsMemoryError(ret))
		g_memory_manager->RaiseCondition(ret);
	else if (OpStatus::IsError(ret))
		OP_ASSERT(!"Unhandled error.");
}

OP_STATUS
ES_TimerManager::FireExpiredEvents()
{
	if (!is_active)
		return OpStatus::OK;

	ES_TimerEvent *event = waiting.First();

	if (!event)
	{
		/* This should be impossible. */
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	/* Fire all expired events. */
	List<ES_TimerEvent> expired_events;
	while (event && event->TimeoutExpired())
	{
		OP_ASSERT(event->GetTimerManager() == this);
		event->Out();
		event->Into(&expired_events);
		event = waiting.First();
	}

	ES_TimerEvent *first_non_expired = waiting.First();

	while (ES_TimerEvent *expired_event = expired_events.First())
	{
		expired_event->Out();
		expired_event->Into(&expired);
		// This will queue a thread or readd the event to the waiting list if it
		// was a bad time to start a new thread.
		OP_BOOLEAN keep = expired_event->Expire();
		if (keep != OpBoolean::IS_TRUE)
		{
			expired_event->Out();
			OP_DELETE(expired_event);
			if (OpStatus::IsMemoryError(keep))
			{
				// Reinsert the remaining events in the waiting events list,
				// carefully keeping the list sorted.
				if (first_non_expired)
				{
					while (ES_TimerEvent *remaining_event = expired_events.First())
					{
						remaining_event->Out();
						remaining_event->Precede(first_non_expired);
					}
				}
				else
					waiting.Append(&expired_events);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return PostMessage();
}

OP_STATUS
ES_TimerManager::PostMessage()
{
	/* First remove any currently posted messages. */
	RemoveMessage();

	/* Check if we really should post a message. */
	if (waiting.Empty())
		return OpStatus::OK;

	ES_TimerEvent *waiting_event = waiting.First();
	double timeout = waiting_event->GetTimeout();

	RETURN_IF_ERROR(SetCallbacks());

	unsigned timeout_approx = static_cast<unsigned>(timeout);
	if (timeout_approx == 0)
		timeout_approx = 1; // To avoid busy looping until it's time
	if (!msg_handler->PostDelayedMessage(MSG_ES_TIMEOUT, id, 0, timeout_approx))
		return OpStatus::ERR_NO_MEMORY;

	has_posted_timeout = TRUE;

	return OpStatus::OK;
}

void
ES_TimerManager::RemoveMessage()
{
	if (has_posted_timeout)
		msg_handler->RemoveDelayedMessage(MSG_ES_TIMEOUT, id, 0);

	has_posted_timeout = FALSE;
}

OP_STATUS
ES_TimerManager::SetCallbacks()
{
	if (!has_set_callbacks)
	{
		if (OpStatus::IsMemoryError(msg_handler->SetCallBack(this, MSG_ES_TIMEOUT, id)))
			return OpStatus::ERR_NO_MEMORY;

		has_set_callbacks = TRUE;
	}

	return OpStatus::OK;
}

void
ES_TimerManager::UnsetCallbacks()
{
	if (has_set_callbacks)
		msg_handler->UnsetCallBacks(this);

	has_set_callbacks = FALSE;
}
