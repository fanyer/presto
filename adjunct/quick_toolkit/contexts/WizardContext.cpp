/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/WizardContext.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"

BOOL WizardContext::CanHandleAction(OpInputAction* action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_BACK:
		case OpInputAction::ACTION_FORWARD:
			return TRUE;
	}

	return OkCancelDialogContext::CanHandleAction(action);
}

BOOL WizardContext::DisablesAction(OpInputAction* action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_BACK: 	return !HasPreviousPage();
		case OpInputAction::ACTION_FORWARD: return !HasNextPage();
		case OpInputAction::ACTION_OK: 		return HasNextPage();
	}
	return OkCancelDialogContext::DisablesAction(action);
}

OP_STATUS WizardContext::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_BACK:
		{
			bool was_last_page = !HasNextPage();
			GoToPreviousPage();
			if (was_last_page)
			{
				RETURN_IF_ERROR(HideWidget("button_OK"));
				RETURN_IF_ERROR(ShowWidget("button_Next"));
			}
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_FORWARD:
		{
			GoToNextPage();
			bool is_last_page = !HasNextPage();
			if (is_last_page)
			{
				RETURN_IF_ERROR(ShowWidget("button_OK"));
				RETURN_IF_ERROR(HideWidget("button_Next"));
			}
			return OpStatus::OK;
		}
	}
	return OkCancelDialogContext::HandleAction(action);
}

OP_STATUS WizardContext::ShowWidget(const OpStringC8& name)
{
	QuickWidget* widget = m_dialog->GetWidgetCollection()->Get<QuickWidget>(name);
	RETURN_VALUE_IF_NULL(widget, OpStatus::ERR);

	widget->Show();
	return OpStatus::OK;
}

OP_STATUS WizardContext::HideWidget(const OpStringC8& name)
{
	QuickWidget* widget = m_dialog->GetWidgetCollection()->Get<QuickWidget>(name);
	RETURN_VALUE_IF_NULL(widget, OpStatus::ERR);

	widget->Hide();
	return OpStatus::OK;
}

bool WizardContext::HasPreviousPage()
{
	return GetPages() != NULL && GetPages()->HasPreviousPage();
}

bool WizardContext::HasNextPage()
{
	return GetPages() != NULL && GetPages()->HasNextPage();
}

void WizardContext::GoToPreviousPage()
{
	if (GetPages() != NULL)
		GetPages()->GoToPreviousPage();
}

void WizardContext::GoToNextPage()
{
	if (GetPages() != NULL)
		GetPages()->GoToNextPage();
}

QuickPagingLayout* WizardContext::GetPages()
{
	if (m_pages == NULL)
		m_pages = m_dialog->GetDialogContent()->GetTypedObject<QuickPagingLayout>();
	return m_pages;
}
