/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/sync/view/SyncOldPasswordMissing.h"
#include "adjunct/quick/sync/view/SyncConstants.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"

#include "modules/locale/oplanguagemanager.h"

OP_STATUS SyncOldPasswordMissingDialog::AreYouSureDialog::Init()
{
	OpString message;
	OpString title;

	OpStatus::Ignore(g_languageManager->GetString(Str::D_PASSWORD_MISSING_DOUBLECHECKING_TEXT, message));
	OpStatus::Ignore(g_languageManager->GetString(Str::D_PASSWORD_MISSING_DOUBLECHECKING, title));
	SetOkTextID(Str::DI_IDYES);

	return SimpleDialog::Init(SyncConstant::WINDOW_NAME_AREYOUSURE, title, message, m_parent, TYPE_OK_CANCEL, IMAGE_QUESTION, TRUE);
}

UINT32 SyncOldPasswordMissingDialog::AreYouSureDialog::OnOk()
{
	m_parent->m_sync_manager.AbandonPasswordRecovery();
	m_parent->CloseDialog(FALSE, FALSE, TRUE);
	return 0;
}

SyncOldPasswordMissingDialog::SyncOldPasswordMissingDialog(SyncManager & sync_manager)
	: m_sync_manager(sync_manager)
	, m_in_progress(FALSE)
{
}

SyncOldPasswordMissingDialog::~SyncOldPasswordMissingDialog()
{
	g_main_message_handler->UnsetCallBack(this, MSG_QUICK_PASSWORDS_RECOVERY_KNOWN);
}

void SyncOldPasswordMissingDialog::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_QUICK_PASSWORDS_RECOVERY_KNOWN)
	{
		if (par2) CloseDialog(FALSE, FALSE, TRUE);
		else
		{
			HideProgressInfo();
			ShowError();
		}

		return;
	}
	Dialog::HandleCallback(msg, par1, par2);
}

void SyncOldPasswordMissingDialog::OnInit()
{
	OpIcon* warning_icon = static_cast<OpIcon*>(GetWidgetByName(SyncConstant::WARNING_ICON));
	OP_ASSERT(warning_icon);
	if (warning_icon)
	{
		warning_icon->SetImage("Dialog Warning");
	}

	OpLabel* title_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::HEADER_LABEL));
	if (title_label)
	{
		title_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		title_label->SetRelativeSystemFontSize(SyncConstant::BIGGER);
	}

	OpEdit* password_edit = static_cast<OpEdit*>(GetWidgetByName(SyncConstant::PASSWORD_EDIT));
	if (password_edit) password_edit->SetPasswordMode(TRUE);

	OpLabel* password_star = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::PASSWORD_STAR));
	if (password_star)
	{
		password_star->SetJustify(JUSTIFY_RIGHT, FALSE);
		password_star->SetForegroundColor(SyncConstant::RED); // todo: put color in skin
	}

	OpLabel* incorrect = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::INCORR_LABEL));
	if (incorrect)
	{
		incorrect->SetForegroundColor(SyncConstant::RED); // todo: put color in skin
		incorrect->SetWrap(TRUE);
	}

	SetWidgetValue(SyncConstant::RETRY_RADIO, TRUE);
	OpProgressBar * progress_spinner = static_cast<OpProgressBar *>(GetWidgetByName(SyncConstant::PROGRESS_SPINNER));
	if (progress_spinner)
		progress_spinner->SetType(OpProgressBar::Spinner);

	OpLabel * progress_label = static_cast<OpLabel *>(GetWidgetByName(SyncConstant::PROGRESS_LABEL));
	if (progress_label)
		progress_label->SetWrap(TRUE);

	HideError();
	HideProgressInfo();

	//we have no way to react here...
	OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_QUICK_PASSWORDS_RECOVERY_KNOWN, 0));
}

void SyncOldPasswordMissingDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpRadioButton* retry_radio = static_cast<OpRadioButton*>(GetWidgetByName(SyncConstant::RETRY_RADIO));
	OpRadioButton* replace_radio = static_cast<OpRadioButton*>(GetWidgetByName(SyncConstant::REPLACE_RADIO));

	if ((retry_radio && retry_radio == widget) || (replace_radio && replace_radio == widget))
	{
		BOOL enabled =
			retry_radio ?
				retry_radio->GetValue() :
				replace_radio ? !replace_radio->GetValue() : FALSE;

		SetWidgetEnabled(SyncConstant::PASSWORD_LABEL, enabled);
		SetWidgetEnabled(SyncConstant::PASSWORD_EDIT, enabled);
		SetWidgetEnabled(SyncConstant::PASSWORD_STAR, enabled);
		return;
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

BOOL SyncOldPasswordMissingDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			OkClicked();
			return TRUE;
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OK:
				{
					child_action->SetEnabled(!m_in_progress);
					return TRUE;
				}
			}
			break;
		}
	}
	return Dialog::OnInputAction(action);
}

void SyncOldPasswordMissingDialog::OkClicked()
{
	if (GetWidgetValue(SyncConstant::RETRY_RADIO)) // Retry recovery
	{
		ShowProgressInfo(Str::D_FEATURE_SETUP_LOGGING_IN);

		OpString old_pass;
		GetWidgetText(SyncConstant::PASSWORD_EDIT, old_pass);
		m_sync_manager.RetryPasswordRecovery(old_pass); //this will post the message.
	}
	else // Replace passwords
	{
		AreYouSureDialog* msg = OP_NEW(AreYouSureDialog, (this));
		if (!msg)
			return;
		if (OpStatus::IsError(msg->Init()))
		{
			OP_DELETE(msg);
			//If anything happend during dialog initialization,
			//last thing we are sure of, the user chose to abandon the recovery
			m_sync_manager.AbandonPasswordRecovery();
		}
	}
}

void SyncOldPasswordMissingDialog::ShowError()
{
	ShowWidget(SyncConstant::PASSWORD_STAR, TRUE);
	ShowWidget(SyncConstant::INCORR_LABEL, TRUE);
	CompressGroups();
}

void SyncOldPasswordMissingDialog::HideError()
{
	ShowWidget(SyncConstant::PASSWORD_STAR, FALSE);
	ShowWidget(SyncConstant::INCORR_LABEL, FALSE);
	CompressGroups();
}

void SyncOldPasswordMissingDialog::ShowProgressInfo(Str::LocaleString info_id)
{
	SetInProgress(TRUE);

	HideError();
	ShowWidget(SyncConstant::PROGRESS_SPINNER, TRUE);

	OpString info;
	OpStatus::Ignore(g_languageManager->GetString(info_id, info));

	OpString text;
	GetWidgetText(SyncConstant::PROGRESS_LABEL, text);

	if (text.Compare(info) != 0)
	{
		SetWidgetText(SyncConstant::PROGRESS_LABEL, info.CStr());
		ResetDialog();
	}
}

void SyncOldPasswordMissingDialog::HideProgressInfo()
{
	SetInProgress(FALSE);

	HideError();
	ShowWidget(SyncConstant::PROGRESS_SPINNER, FALSE);

	OpString text;
	GetWidgetText(SyncConstant::PROGRESS_LABEL, text);

	if (text.HasContent())
	{
		SetWidgetText(SyncConstant::PROGRESS_LABEL, "");
		ResetDialog();
	}
}

void SyncOldPasswordMissingDialog::SetInProgress(BOOL in_progress)
{
	if (m_in_progress != in_progress)
	{
		m_in_progress = in_progress;
		g_input_manager->UpdateAllInputStates();
	}
}
