/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

#include "adjunct/quick_toolkit/bindings/ReversibleQuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

BOOL OkCancelDialogContext::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			return TRUE;
	}

	return FALSE;
}

OP_STATUS OkCancelDialogContext::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			m_ok = true;
			GetMyBinder()->Commit();
			// fall through

		case OpInputAction::ACTION_CANCEL:
			CallOkCancelHook();
			if (m_dialog && m_dialog->GetDesktopWindow() && !m_dialog->GetDesktopWindow()->IsClosing())
			{
				m_dialog->GetDesktopWindow()->Close();
			}
			return OpStatus::OK;
	}
	return OpStatus::ERR;
}

void OkCancelDialogContext::OnUiClosing()
{
	CallOkCancelHook();
	DialogContext::OnUiClosing();
}

void OkCancelDialogContext::CallOkCancelHook()
{
	if (!m_hook_called)
	{
		m_hook_called = true;

		if (m_ok)
			OnOk();
		else
			OnCancel();
	}
}

QuickBinder* OkCancelDialogContext::CreateBinder()
{
	return OP_NEW(MyBinder, ());
}

OkCancelDialogContext::MyBinder* OkCancelDialogContext::GetMyBinder()
{
	return static_cast<MyBinder*>(GetBinder());
}
