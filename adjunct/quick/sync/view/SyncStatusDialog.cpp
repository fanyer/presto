/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/view/SyncStatusDialog.h"

#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpMultiEdit.h"

#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**  SyncStatusDialog::SyncStatusDialog
************************************************************************************/
SyncStatusDialog::SyncStatusDialog(SyncManager & sync_manager, const SyncStatusContext & context) :
   m_sync_manager(sync_manager),
   m_context()
{
	m_context.m_suggested_action = context.m_suggested_action;

	if (OpStatus::IsError(m_context.m_status_text.Set(context.m_status_text)) ||
		OpStatus::IsError(m_context.m_status_icon_name.Set(context.m_status_icon_name)))
		init_status = OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
**  SyncStatusDialog::OnInputAction
************************************************************************************/
/*virtual*/ BOOL
SyncStatusDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT:
			case OpInputAction::ACTION_SYNC_NOW:
				{
					child_action->SetEnabled(m_sync_manager.IsFeatureEnabled());
					return TRUE;
				}
			}
			break;
		}
	case OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT:
		{
			if(m_sync_manager.IsFeatureEnabled())
			{
				// todo: make part of controller
				g_desktop_account_manager->SetLoggedIn(FALSE);
				g_desktop_account_manager->StopServices();

				m_sync_manager.ShowLoginDialogAndEnableLink(this);

				CloseDialog(FALSE);
				return TRUE;
			}
			else
				return FALSE;
			break;
		}
	case OpInputAction::ACTION_SYNC_NOW:
		{
			if(m_sync_manager.IsFeatureEnabled())
			{
				m_sync_manager.SyncNow(SyncManager::Now, TRUE);
				CloseDialog(FALSE);
				return TRUE;
			}
			else
				return FALSE;
		}
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**  SyncStatusDialog::OnInit
************************************************************************************/
/*virtual*/ void
SyncStatusDialog::OnInit()
{
	OpIcon* status_icon = static_cast<OpIcon*>(GetWidgetByName("status_icon"));
	if (status_icon)
	{
		status_icon->SetImage(m_context.m_status_icon_name.CStr());
	}
	OpMultilineEdit* status_edit = static_cast<OpMultilineEdit*>(GetWidgetByName("status_edit"));
	if (status_edit)
	{
		status_edit->SetWrapping(TRUE);
		status_edit->SetFlatMode();
		status_edit->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		status_edit->SetText(m_context.m_status_text.CStr());
	}

	OpLabel* suggestion_label = static_cast<OpLabel*>(GetWidgetByName("suggestion_label"));
	if (suggestion_label)
	{
		suggestion_label->SetWrap(TRUE);
	}

	OpString text;
	switch (m_context.m_suggested_action)
	{
	case SyncStatusContext::ActionChangeUser:
		{
			g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_SUGGESTION_CHANGE_USER, text);
			SetWidgetText("suggestion_label", text.CStr());
			OpButton * button = static_cast<OpButton*>(GetWidgetByName("suggestion_button"));
			if (button)
			{
				button->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT)));

				g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_CHANGE_USER, text);
				button->SetText(text.CStr());
			}
			break;
		}
	case SyncStatusContext::ActionSyncNow:
		{
			g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_SUGGESTION_SYNC_NOW, text);
			SetWidgetText("suggestion_label", text.CStr());
			OpButton * button = static_cast<OpButton*>(GetWidgetByName("suggestion_button"));
			if (button)
			{
				button->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SYNC_NOW)));

				g_languageManager->GetString(Str::S_SYNC_NOW, text);
				button->SetText(text.CStr());
			}
			break;
		}
	case SyncStatusContext::ActionNone:
	default:
		{
			ShowWidget("suggestion_group", FALSE);
		}
	}
}


#endif // SUPPORT_DATA_SYNC
