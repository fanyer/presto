/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

OP_STATUS CloseDialogContext::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			if (m_dialog && m_dialog->GetDesktopWindow() && !m_dialog->GetDesktopWindow()->IsClosing())
				m_dialog->GetDesktopWindow()->Close();			
			return OpStatus::OK;
	}
	return DialogContext::HandleAction(action);
}

BOOL CloseDialogContext::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
		case OpInputAction::ACTION_CANCEL:
			return TRUE;
	}
	return DialogContext::CanHandleAction(action);
}
