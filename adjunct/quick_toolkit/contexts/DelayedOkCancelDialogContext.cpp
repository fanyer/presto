/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/DelayedOkCancelDialogContext.h"

#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


DelayedOkCancelDialogContext::DelayedOkCancelDialogContext()
	: m_attached_to_window(false)
	, m_disable_timer(NULL)
{
}

DelayedOkCancelDialogContext::~DelayedOkCancelDialogContext()
{
	OP_DELETE(m_disable_timer);

	if (m_dialog != NULL && m_dialog->GetDesktopWindow() != NULL)
		m_dialog->GetDesktopWindow()->RemoveListener(this);
}

BOOL DelayedOkCancelDialogContext::DisablesAction(OpInputAction* action)
{
	if(!AttachToWindowIfNeeded())
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			return m_disable_timer != NULL;

		default:
			return OkCancelDialogContext::DisablesAction(action);
	}
}

void DelayedOkCancelDialogContext::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (active)
	{
		if (m_disable_timer == NULL)
		{
			m_disable_timer = OP_NEW(OpTimer, ());
			g_input_manager->UpdateAllInputStates();
		}

		if (m_disable_timer == NULL)
			return;

		m_disable_timer->SetTimerListener(this);
		m_disable_timer->Start(ENABLE_AFTER);
	}
}

void DelayedOkCancelDialogContext::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == m_disable_timer);

	OP_DELETE(m_disable_timer);
	m_disable_timer = NULL;
	g_input_manager->UpdateAllInputStates();
}

bool DelayedOkCancelDialogContext::AttachToWindowIfNeeded()
{
	if (!m_attached_to_window)
	{
		OP_ASSERT(m_dialog != NULL && m_dialog->GetDesktopWindow() != NULL);
		DesktopWindow* window = m_dialog->GetDesktopWindow();
		RETURN_VALUE_IF_ERROR(window->AddListener(this), false);
		m_attached_to_window = true;

		OnDesktopWindowActivated(window, window->IsActive());
	}
	return true;
}