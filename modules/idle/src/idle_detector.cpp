/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/idle/idle_detector.h"

OpActivity::OpActivity(OpActivityType type)
	: m_activity_count(0)
	, m_type(type)
	, m_detector(g_idle_detector)
{
}

OpActivity::OpActivity(OpActivityType type, OpIdleDetector *detector)
	: m_activity_count(0)
	, m_type(type)
	, m_detector(detector)
{
}

/* virtual */
OpActivity::~OpActivity()
{
	Cancel();
}

void
OpActivity::Begin()
{
	if (m_detector && m_activity_count++ == 0)
		m_detector->OnActivityStateChanged(m_type, ACTIVITY_STATE_BUSY);
}

void
OpActivity::End()
{
	if (m_detector && --m_activity_count == 0)
		m_detector->OnActivityStateChanged(m_type, ACTIVITY_STATE_IDLE);

	// Do not leave 'activity_count' in the negative after the function
	// completes.
	m_activity_count = (m_activity_count < 0 ? 0 : m_activity_count);
}

void
OpActivity::Cancel()
{
	if (m_detector && m_activity_count > 0)
	{
		m_detector->OnActivityStateChanged(m_type, ACTIVITY_STATE_IDLE);
		m_activity_count = 0;
	}
}

OpAutoActivity::OpAutoActivity(OpActivityType type)
	: OpActivity(type)
{
	Begin();
}

OpAutoActivity::OpAutoActivity(OpActivityType type, OpIdleDetector *detector)
	: OpActivity(type, detector)
{
	Begin();
}

OpAutoActivity::~OpAutoActivity()
{
	End();
}

OpIdleDetector::OpIdleDetector()
	: m_activity_state(ACTIVITY_STATE_BUSY)
	, m_activity_count(0)
	, m_first(NULL)
	, m_last(NULL)
	, m_delayed_idle(ACTIVITY_DELAYED_IDLE, this)
	, m_disabled(ACTIVITY_DISABLED, this)
	, m_active_listeners(0)
{
	// These locals are present to avoid really long lines.
	MessageHandler *mh = g_main_message_handler;
	OpMessage msg = MSG_IDLE_DELAYED_IDLE_CHECK;
	MH_PARAM_1 id = reinterpret_cast<MH_PARAM_1>(this);

	RAISE_IF_MEMORY_ERROR(mh->SetCallBack(this, msg, id));

	// The detector starts in a 'disabled' state. See
	// OpIdleDetector::OnEnable and OpIdleDetector::OnDisable.
	m_disabled.Begin();
}

/* virtual */
OpIdleDetector::~OpIdleDetector()
{
	m_delayed_idle.Detach();
	m_disabled.Detach();

	g_main_message_handler->UnsetCallBack(this, MSG_IDLE_DELAYED_IDLE_CHECK);

	MH_PARAM_1 id = reinterpret_cast<MH_PARAM_1>(this);
	g_main_message_handler->RemoveDelayedMessages(MSG_IDLE_DELAYED_IDLE_CHECK, id);
}

void
OpIdleDetector::AddListener(OpActivityListener *listener)
{
	BOOL enabled = IsEnabled();

	listener->m_list = this;

	if (!m_first)
		m_first = listener;

	if (m_last)
	{
		m_last->m_suc = listener;
		listener->m_prev = m_last;
	}

	m_last = listener;

	if (listener->m_type != OpActivityListener::PASSIVE)
		++ m_active_listeners;

	if (enabled != IsEnabled())
		OnEnable();
}

void
OpIdleDetector::RemoveListener(OpActivityListener *listener)
{
	if (listener->m_list == NULL || listener->m_list != this)
		return;

	BOOL enabled = IsEnabled();

	listener->m_list = NULL;

	if (listener == m_last)
		m_last = m_last->m_prev;

	OpActivityListener *&target = (listener == m_first) ? m_first : listener->m_prev->m_suc;

	target = listener->m_suc;

	if (listener->m_type != OpActivityListener::PASSIVE)
	{
		OP_ASSERT(m_active_listeners);
		-- m_active_listeners;
	}

	if (enabled != IsEnabled())
		OnDisable();
}

/* virtual */ void
OpIdleDetector::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// We have not set a callback for any other messages.
	OP_ASSERT(msg == MSG_IDLE_DELAYED_IDLE_CHECK);

	m_delayed_idle.End();
}

void
OpIdleDetector::OnActivityStateChanged(OpActivityType type, OpActivityState state)
{
	// Note: 'type' is currently not used for much. This could be
	// interesting information in the future, but for now we only need
	// to know whether we are globally idle or not. We don't really need
	// to know why.

	// Check whether are a making an idle -> busy transition, or vice-versa.
	switch (state)
	{
	case ACTIVITY_STATE_IDLE:
		// For normal OpActivityTypes (non-ACTIVITY_DELAYED_IDLE), we don't
		// change the state immediately, but instead do a delayed idle check.
		// This will start an ACTIVITY_DELAYED_IDLE OpActivity. When this
		// activity ends, OnActivityStateChanged will be called again, and
		// *then* we change the state to ACTIVITY_STATE_IDLE if no other
		// activities have been started in the meantime.
		//
		// When doing an idle check, @a m_activity_count should be decremented
		// *after* the call to DelayedIdleCheck(). This prevents a state where
		// @a m_activity_count is zero, and the global state is BUSY (which
		// does not make sense). In other cases, when we are truly idle, we
		// decrement *before* (the potential) call to ChangeState(), so
		// @a m_activity_count is in the correct state (zero) when listeners are
		// called. (Not that this information is available to listeners).
		if ((m_activity_count - 1) == 0 && type != ACTIVITY_DELAYED_IDLE)
		{
			DelayedIdleCheck();
			--m_activity_count;
		}
		else if(--m_activity_count == 0)
			ChangeState(ACTIVITY_STATE_IDLE);
		break;
	case ACTIVITY_STATE_BUSY:
		// Here, @a m_activity_count should be incremented *before* the call to
		// ChangeState(), so @m_activity_count is non-zero when listeners are
		// called.
		if (m_activity_count++ == 0)
			ChangeState(ACTIVITY_STATE_BUSY);
		break;
	}

	// Make sure 'activity_count' does not go into the negative.
	//
	// This should not really happen, ever. If it does, it means there is an
	// extra IDLE event which is not matched by a BUSY event.
	//
	// *If* it does happen, this failsafe is better than nothing, because it
	// will leave OpIdleDetector in a sufficiently consistent state to allow
	// subsequent activity to be treated correctly.
	m_activity_count = (m_activity_count < 0 ? 0 : m_activity_count);
}

void
OpIdleDetector::ChangeState(OpActivityState state)
{
	m_activity_state = state;

	OP_STATUS status = BroadcastStateChange(state);

	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
}

OP_STATUS
OpIdleDetector::BroadcastStateChange(OpActivityState state)
{
	BOOL oom = FALSE;

	OpActivityListener *listener = m_first;

	while (listener != NULL)
	{
		OpActivityListener *next = listener->m_suc;

		// Note that it is legal for a listener to remove itself in the
		// 'OnActivityStateChanged' callback. That means that we can not
		// be sure that 'listener' is still valid after this call.
		OP_STATUS err = listener->OnActivityStateChanged(state);

		// On OOM, don't stop the broadcasting, but *do* notify the caller
		// that OOM did indeed happen at some point during execution.

		oom = oom || OpStatus::IsMemoryError(err);

		listener = next;
	}

	// Errors other than OpStatus::ERR_NO_MEMORY are deliberately ignored.

	return oom ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

void
OpIdleDetector::DelayedIdleCheck()
{
	// Inform all listeners that we may soon go idle.
	const OP_STATUS status = BroadcastStateChange(ACTIVITY_STATE_PRE_IDLE);
	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);


	// We are currently idle, but there might be more work lined
	// up in the message queue. To make sure that is not the case,
	// we post MSG_IDLE_DELAYED_IDLE_CHECK with a 1 millisecond delay.
	// If we are still idle when that message comes through, we notify
	// listener that we truly are idle.

	// These locals are present to avoid really long lines.
	MessageHandler *mh = g_main_message_handler;
	OpMessage msg = MSG_IDLE_DELAYED_IDLE_CHECK;
	MH_PARAM_1 id = reinterpret_cast<MH_PARAM_1>(this);

	m_delayed_idle.Begin();

	mh->PostDelayedMessage(msg, id, 0, 1);
}
