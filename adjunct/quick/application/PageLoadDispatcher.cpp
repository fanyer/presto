/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/PageLoadDispatcher.h"

#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

namespace DispatchableObjectConsts
{
	const int PAGE_LOAD_LIMIT = 2; // default page load limit
	const int NEXT_MESSAGE_TIMEOUT = 200; // ms
	const int SLOW_LOAD_TIMEOUT = 5; // s
}

PageLoadDispatcher::~PageLoadDispatcher()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void DispatchableObject::InitDispatchable()
{
	using namespace DispatchableObjectConsts;

	if (HasHiddenInternals())
		g_main_message_handler->PostDelayedMessage(MSG_QUICK_INIT_HIDDEN_UI, 0, 0, NEXT_MESSAGE_TIMEOUT);
}

OP_STATUS PageLoadDispatcher::Init()
{
	OpMessage messages[] = {
		MSG_QUICK_CONTINUE_SESSION_LOADING,
		MSG_QUICK_INIT_HIDDEN_UI
	};
	RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(this,
				0, messages, ARRAY_SIZE(messages)));

	return OpStatus::OK;
}

OP_STATUS PageLoadDispatcher::SetSession(OpSharedPtr<OpSession> session)
{
	using namespace DispatchableObjectConsts;

	RETURN_VALUE_IF_NULL(session.get(), OpStatus::ERR_NULL_POINTER);
	if (m_session.get())
	{
		// we need to load them or respective session windows
		// will be deleted
		PurgeQueue();
	}
	m_session = session;
	m_queue_closed = false;
	m_load_queue.Empty();
	m_prioritized_windows.Clear();
	m_timestamp = g_timecache->CurrentTime();
	m_page_load_limit = PAGE_LOAD_LIMIT;

	return OpStatus::OK;
}

OP_STATUS PageLoadDispatcher::Enqueue(OpSessionWindow *session, DesktopWindow *window)
{
	if (!session || !window)
		return OpStatus::ERR_NULL_POINTER;

	if (m_queue_closed)
		return OpStatus::ERR;

	window->SetSessionWindow(session);
	int priority = window->GetPriority();

	if (priority < 0)
	{
		RETURN_IF_ERROR(window->InitObject());
	}
	else
	{
		RETURN_IF_ERROR(window->AddListener(this));
		RETURN_IF_ERROR(window->InitPartial());

		if (priority == 0)
		{
			RETURN_IF_ERROR(Process(window));
		}
		else
		{
			unsigned pos = FindPosition(priority);
			RETURN_IF_ERROR(m_load_queue.Insert(pos, window));
		}
	}

	if (window->HasHiddenInternals())
	{
		RETURN_IF_ERROR(m_hidden_ui_queue.Add(window));
	}

	return OpStatus::OK;
}

unsigned PageLoadDispatcher::FindPosition(int priority) const
{
	// edge cases
	
	unsigned size = m_load_queue.GetCount();

	if (priority <= 0 || size == 0)
		return 0;

	if (priority >= 100)
		return size;

	int priority_0 = m_load_queue.Get(0)->GetPriority();
	int priority_n = m_load_queue.Get(size-1)->GetPriority();

	if (priority < priority_0)
		return 0;

	if (priority >= priority_n)
		return size;

	// binary search in table with duplicates
	// insert on vector adds before index, therefore +1
	return FindPosition(priority, 0, size-1) + 1;
}

unsigned PageLoadDispatcher::FindPosition(int priority, unsigned l, unsigned r) const
{
	OP_ASSERT(l <= r);
	OP_ASSERT(r < m_load_queue.GetCount());
	OP_ASSERT(priority > 0 && priority < 100);

	if (l == r) return r;

	unsigned c = (l + r) / 2;
	int priority_c = m_load_queue.Get(c)->GetPriority();

	if (priority < priority_c)
		return FindPosition(priority, l, c);
	if (priority > priority_c)
		return FindPosition(priority, c, r);
	if (l+1 < r)
		return FindPosition(priority, c, r);

	OP_ASSERT(priority_c == priority && l+1 == r);

	if (priority == m_load_queue.Get(r)->GetPriority())
		return r;
	if (priority == m_load_queue.Get(l)->GetPriority())
		return l;

	OP_ASSERT(false);
	return l;
}

OP_STATUS PageLoadDispatcher::Process(DispatchableObject *window)
{
	OP_ASSERT(window);
	RETURN_IF_ERROR(m_load_queue.Insert(0, window)); // push_front
	m_prioritized_windows.Add(window);
	return ProcessQueue(true);
}

OP_STATUS PageLoadDispatcher::ProcessQueue(bool force)
{
	if (m_load_queue.GetCount() == 0)
		return OpStatus::OK;

	if (!force && m_loading_pages > m_page_load_limit)
		return OpStatus::OK;

	DispatchableObject *load_job = m_load_queue.Remove(0); // pop_front
	OP_ASSERT(load_job);

	DesktopWindow *window = static_cast<DesktopWindow*>(load_job);

	window->RemoveListener(this); // DesktopWindowListener

	switch (window->GetType())
	{
		case OpTypedObject::WINDOW_TYPE_DOCUMENT:
		{
			DocumentDesktopWindow *document = static_cast<DocumentDesktopWindow*>(window);

			RETURN_IF_ERROR(document->AddDocumentWindowListener(this)); // DocumentWindowListener

			return document->InitObject();
		}

		default:
			return OpStatus::ERR;
	}
}

void PageLoadDispatcher::PurgeQueue()
{
	unsigned queue_size = 0;
	while (queue_size = m_load_queue.GetCount())
	{
		ProcessQueue(true);
		OP_ASSERT(m_load_queue.GetCount() < queue_size);
	}
}

void PageLoadDispatcher::OnStartLoading(DocumentDesktopWindow* document_window)
{
	m_loading_pages++;
}

void PageLoadDispatcher::OnLoadingFinished(DocumentDesktopWindow* document_window,
		OpLoadingListener::LoadingFinishStatus,	BOOL was_stopped_by_user)
{
	m_loading_pages--;

	OpStatus::Ignore(m_prioritized_windows.RemoveByItem(document_window));
}

OP_STATUS PageLoadDispatcher::Shove()
{
	using namespace DispatchableObjectConsts;

	if (!m_queue_closed)
		return OpStatus::ERR;

	int prioritized = m_prioritized_windows.GetCount();

	int loading = m_load_queue.GetCount();

	if (g_timecache->CurrentTime() - m_timestamp > SLOW_LOAD_TIMEOUT)
		m_page_load_limit *= 2;

	for (int i = m_page_load_limit - prioritized; i > 0; --i)
	{
		if (OpStatus::IsError(ProcessQueue(false)))
			break;
	}

	if (m_load_queue.GetCount() - loading > 0)
		m_timestamp = g_timecache->CurrentTime();

	return OpStatus::OK;
}

OP_STATUS PageLoadDispatcher::Promote(DesktopWindow *window)
{
	if (OpStatus::IsSuccess(m_load_queue.RemoveByItem(window)))
	{
		return Process(window);
	}

	return OpStatus::OK;
}

void PageLoadDispatcher::OnDesktopWindowActivated(DesktopWindow *desktop_window, BOOL active)
{
	if (active && m_queue_closed)
	{
		// active && m_queue_closed, because we want to 
		// react only if this activation was result of mouse interaction (tricky)
		// otherwise it crashes when pages are being activated during creation
		// of session desktop windows, because window might not have been shown yet
		Promote(desktop_window);
	}
}

void PageLoadDispatcher::OnDesktopWindowClosing(DesktopWindow *desktop_window, BOOL user_initiated)
{
	Remove(desktop_window);
}

OP_STATUS PageLoadDispatcher::Remove(DispatchableObject *object)
{
	OP_STATUS err_load_queue = OpStatus::OK;
	OP_STATUS err_hidden_queue = OpStatus::OK;

	err_load_queue = m_load_queue.RemoveByItem(object);

	if (object->HasHiddenInternals())
		err_hidden_queue = m_hidden_ui_queue.RemoveByItem(object);

	RETURN_IF_ERROR(err_load_queue);
	RETURN_IF_ERROR(err_hidden_queue);
	return OpStatus::OK;
}

void PageLoadDispatcher::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	using namespace DispatchableObjectConsts;

	switch (msg)
	{
		case MSG_QUICK_CONTINUE_SESSION_LOADING:

			m_unlock_init_hidden_ui = true;

			if (m_prioritized_windows.GetCount())
				Shove();
			else
				PurgeQueue();

			if (m_load_queue.GetCount())
			{
				g_main_message_handler->PostDelayedMessage(
						MSG_QUICK_CONTINUE_SESSION_LOADING, 0, 0, NEXT_MESSAGE_TIMEOUT);
			}
			else
			{
				m_session.reset();
				m_queue_closed = false;
			}

			break;

		case MSG_QUICK_INIT_HIDDEN_UI:

			if (m_unlock_init_hidden_ui)
				InitHiddenUI();
			else
				g_main_message_handler->PostDelayedMessage(
						MSG_QUICK_INIT_HIDDEN_UI, 0, 0, NEXT_MESSAGE_TIMEOUT);

			break;
	}
}

OP_STATUS PageLoadDispatcher::InitHiddenUI()
{
	OP_STATUS status = OpStatus::OK;

	for (unsigned i = 0; i < m_hidden_ui_queue.GetCount(); ++i)
	{
		status = m_hidden_ui_queue.Get(i)->InitHiddenInternals();
		if (OpStatus::IsError(status))
			break;
	}

	m_hidden_ui_queue.Clear();

	return status;
}
