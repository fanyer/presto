/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/sync/view/SyncPasswordImprovementDialog.h"
#include "adjunct/quick/sync/view/SyncConstants.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpPasswordStrength.h"

#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

SyncPasswordImprovementDialog::~SyncPasswordImprovementDialog()
{
	if (OpStatus::IsError(g_desktop_account_manager->RemoveListener(this)))
	{
		OP_ASSERT(!"Removing SyncPasswordImprovementDialog listener failed.");
	}
}

OP_STATUS SyncPasswordImprovementDialog::Init(DesktopWindow* parent)
{
	RETURN_IF_ERROR(g_desktop_account_manager->AddListener(this));
	RETURN_IF_ERROR(g_desktop_account_manager->GetUsername(m_username));

	return Dialog::Init(parent);
}

BOOL SyncPasswordImprovementDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OP_ASSERT(child_action != 0);

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_CANCEL:
				case OpInputAction::ACTION_OK:
					child_action->SetEnabled(!m_in_progress);
					break;
			}

			return TRUE;
		}
		case OpInputAction::ACTION_OK:
		{
			m_was_error = FALSE;
			ShowProgress(FALSE);
			HideError();

			OpString old_passwd, new_passwd, confirm_new_passwd;
			if (OpStatus::IsError(GetPasswordFromEdit(old_passwd, SyncConstant::OLD_PASSWD_EDIT)) ||
				OpStatus::IsError(GetPasswordFromEdit(new_passwd, SyncConstant::NEW_PASSWD_EDIT)) ||
				OpStatus::IsError(GetPasswordFromEdit(confirm_new_passwd,
					SyncConstant::CONFIRM_NEW_PASSWD_EDIT)) ||
				old_passwd.IsEmpty() || new_passwd.IsEmpty() ||
				confirm_new_passwd.IsEmpty())
			{
				ShowError(ERROR_RETRIEVING_PASSWORDS);
			}
			else if (old_passwd.Length() < OperaAccountContext::GetPasswordMinLength())
			{
				ShowError(ERROR_OLD_PASSWORD_TOO_SHORT);
			}
			else if (new_passwd.Length() < OperaAccountContext::GetPasswordMinLength())
			{
				ShowError(ERROR_NEW_PASSWORD_TOO_SHORT);
			}
			else if (new_passwd.Compare(confirm_new_passwd) != 0)
			{
				ShowError(ERROR_PASSWORDS_DONT_MATCH);
			}
			else
			{
				RETURN_VALUE_IF_ERROR(m_new_passwd.Set(new_passwd), TRUE);
				if (OpStatus::IsError(
						g_desktop_account_manager->ChangePassword(m_username, old_passwd,
							new_passwd)))
				{
					ShowError(ERROR_REQUEST_PROBLEM);
				}
				else
					ShowProgress(TRUE);
			}

			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}

void SyncPasswordImprovementDialog::OnInit()
{
	ShowProgress(FALSE);

	OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName(SyncConstant::OLD_PASSWD_EDIT));
	OP_ASSERT(edit);
	if (edit)
		edit->SetPasswordMode(TRUE);

	BindEditWithPasswordStrength(SyncConstant::NEW_PASSWD_EDIT, SyncConstant::NEW_PASSWD_STRENGTH);

	OpIcon* warning_icon = static_cast<OpIcon*>(GetWidgetByName(SyncConstant::WARNING_ICON));
	OP_ASSERT(warning_icon);
	if (warning_icon)
	{
		warning_icon->SetImage("Dialog Warning");
	}

	OpLabel* header_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::HEADER_LABEL));
	OP_ASSERT(header_label);
	if (header_label)
	{
		header_label->SetRelativeSystemFontSize(SyncConstant::HEADER_LABEL_REL_SIZE);
		SetLabelInBold(SyncConstant::HEADER_LABEL);
	}

	OpLabel* error_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::ERROR_LABEL));
	OP_ASSERT(error_label);
	if (error_label)
	{
		error_label->SetForegroundColor(OP_RGB(255, 0, 0));
	}

	OpEdit* confirm_passwd_edit =
			static_cast<OpEdit*>(GetWidgetByName(SyncConstant::CONFIRM_NEW_PASSWD_EDIT));
	OP_ASSERT(confirm_passwd_edit);
	if (confirm_passwd_edit)
		confirm_passwd_edit->SetPasswordMode(TRUE);
}

void SyncPasswordImprovementDialog::BindEditWithPasswordStrength(const char edit_name[],
		const char passwd_name[])
{
	OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName(edit_name));
	OP_ASSERT(edit);
	if (edit)
		edit->SetPasswordMode(TRUE);

	OpPasswordStrength* passwd =
			static_cast<OpPasswordStrength*>(GetWidgetByName(passwd_name));
	OP_ASSERT(passwd);
	if (passwd && edit)
		passwd->AttachToEdit(edit);
}

OP_STATUS SyncPasswordImprovementDialog::GetPasswordFromEdit(OpString& passwd,
		const char edit_name[])
{
	OpEdit* edit = static_cast<OpEdit*>(GetWidgetByName(edit_name));
	OP_ASSERT(edit);
	if (!edit)
		return OpStatus::ERR;

	return edit->GetText(passwd);
}

void SyncPasswordImprovementDialog::HideError()
{
	OpLabel* error_msg_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::ERROR_LABEL));
	OP_ASSERT(error_msg_label);
	if (!error_msg_label)
		return;

	error_msg_label->SetVisibility(FALSE);
}

void SyncPasswordImprovementDialog::ShowError(PasswordImprovementError error)
{
	OpLabel* error_msg_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::ERROR_LABEL));
	OP_ASSERT(error_msg_label);
	if (!error_msg_label)
		return;

	error_msg_label->SetVisibility(TRUE);

	Str::LocaleString error_msg_id(Str::S_NOT_A_STRING);
	switch (error)
	{
		case ERROR_PASSWORDS_DONT_MATCH:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_PASSWORDS_DONT_MATCH;
			break;
		}
		case ERROR_RETRIEVING_PASSWORDS:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_RETRIEVING_PASSWORDS;
			break;
		}
		case ERROR_OLD_PASSWORD_TOO_SHORT:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_OLD_PASSWORD_TOO_SHORT;
			break;
		}
		case ERROR_NEW_PASSWORD_TOO_SHORT:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_PASSWORD_TOO_SHORT;
			break;
		}
		case ERROR_SERVER_PROBLEM:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_SERVER_PROBLEM;
			break;
		}
		case ERROR_CANT_AUTHENTICATE:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_CANT_AUTHENTICATE;
			break;
		}
		case ERROR_REQUEST_PROBLEM:
		{
			error_msg_id = Str::D_PASSWORD_IMPROVEMENT_ERROR_REQUEST_PROBLEM;
			break;
		}
		default:
		{
			OP_ASSERT(!"You forgot to handle new error type.");
			break;
		}
	}

	OpString error_msg;
	OpStatus::Ignore(g_languageManager->GetString(error_msg_id, error_msg));
	OpStatus::Ignore(error_msg_label->SetText(error_msg));
	m_was_error = TRUE;
}

void SyncPasswordImprovementDialog::ShowProgress(BOOL show)
{
	m_in_progress = show;
	g_input_manager->UpdateAllInputStates();

	OpProgressBar* progress_spinner =
			static_cast<OpProgressBar *>(GetWidgetByName(SyncConstant::PROGRESS_SPINNER));
	OP_ASSERT(progress_spinner);
	if (!progress_spinner)
		return;

	progress_spinner->SetType(OpProgressBar::Spinner);

	OpLabel* progress_label = static_cast<OpLabel*>(GetWidgetByName(SyncConstant::PROGRESS_LABEL));
	OP_ASSERT(progress_label);
	if (!progress_label)
		return;

	progress_spinner->SetVisibility(show);
	progress_label->SetVisibility(show);

	OpString changing_password_str;
	OpStatus::Ignore(g_languageManager->GetString(
			Str::D_PASSWORD_IMPROVEMENT_CHANGING_PASSWORD, changing_password_str));
	OpStatus::Ignore(progress_label->SetText(changing_password_str));

	ResetDialog();
}

void SyncPasswordImprovementDialog::OnOperaAccountPasswordChanged(OperaAuthError err)
{
	switch (err)
	{
		case AUTH_OK:
			break;
		case AUTH_ACCOUNT_AUTH_FAILURE:
			ShowProgress(FALSE);
			ShowError(ERROR_CANT_AUTHENTICATE);
			break;
		default:
			ShowProgress(FALSE);
			ShowError(ERROR_SERVER_PROBLEM);
			break;
	}
}

void SyncPasswordImprovementDialog::OnOperaAccountAuth(OperaAuthError err,
		const OpStringC& shared_secret)
{
	ShowProgress(FALSE);

	switch (err)
	{
		case AUTH_OK:
			// It can be an AUTH_OK for old password login.
			if (!m_was_error)
				CloseDialog(FALSE);
			break;
		default:
			ShowError(ERROR_SERVER_PROBLEM);
			break;
	}
}
