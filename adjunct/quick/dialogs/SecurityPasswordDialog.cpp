/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "SecurityPasswordDialog.h"
#include "ChangeMasterPasswordDialog.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	CONVERT TO WINDOW COMMANDER: AskForSecurityPassword
**
***********************************************************************************/

BOOL AskForSecurityPassword(
							OpWindow *window,
							const uni_char *title, const uni_char *txt,OpString &password)
{
// TODO: rip out when code is no longer called from libssl
	return FALSE;
}

/***********************************************************************************
**
**	SecurityPasswordDialog
**
***********************************************************************************/
SecurityPasswordDialog::SecurityPasswordDialog(OpSSLListener::SSLSecurityPasswordCallback* callback) :
	m_retval(NULL),
	m_ok_enabled(TRUE),
	m_callback(callback)
{
	if (m_callback == NULL)
		return;
	
	OP_ASSERT(m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::JustPassword || m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::NewPassword);

	// Just temporary, until CryptoMasterPasswordHandler gets to provide non-empty strings
	if (m_callback->GetTitle() == Str::NOT_A_STRING)
	{
		g_languageManager->GetString(Str::SI_MSG_SECURE_ASK_PASSWORD, m_title);
		g_languageManager->GetString(Str::SI_MSG_SECURE_ASK_PASSWORD, m_txt);
	}
	else
	{
		g_languageManager->GetString(m_callback->GetTitle(), m_title);
		g_languageManager->GetString(m_callback->GetMessage(), m_txt);
	}
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void SecurityPasswordDialog::OnInit()
{
	OpMultilineEdit* label_just_password = NULL, *label_new_password = NULL, *label = NULL;
	label_just_password = (OpMultilineEdit*)GetWidgetByName("Password_info_label_large");
	label_new_password = (OpMultilineEdit*)GetWidgetByName("Password_info_label");

	if (!label_just_password || !label_new_password)
		return;

	OpEdit* password_edit = (OpEdit*)GetWidgetByName("Password_edit");
	if(password_edit)
	{
		password_edit->SetPasswordMode(TRUE);
	}

	OpEdit* confirm_password_edit = (OpEdit*)GetWidgetByName("Confirm_password_edit");
	if(confirm_password_edit)
	{
		confirm_password_edit->SetPasswordMode(TRUE);
	}

	if (m_callback && m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::NewPassword)
	{
		label_just_password->SetVisibility(FALSE);
		//disable the ok button until both new password and confirm is equal
		m_ok_enabled = FALSE;
		EnableButton(0, m_ok_enabled);
		
		label = label_new_password;
	}
	else
	{
		OpGroup* confirm_group = (OpGroup*)GetWidgetByName("Confirm_new_group");
		if(confirm_group)
		{
			confirm_group->SetVisibility(FALSE);
		}
		
		label_new_password->SetVisibility(FALSE);

		label = label_just_password;
	}

	label->SetLabelMode();
	label->SetText(m_txt.CStr());

	password_edit->SetFocus(FOCUS_REASON_ACTIVATE);

	SetTitle(m_title.CStr());
}


/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 SecurityPasswordDialog::OnOk()
{
	BOOL retval = FALSE;
	if (m_callback)
	{
		// Treat error as cancel
		OpString8 password;
		if (OpStatus::IsSuccess(GetWidgetTextUTF8("Password_edit", password)))
		{
			// In 'JustPassword' mode one shall return the current (entered) password
			// in the 'old_passwd' argument
			retval = TRUE;
			if (m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::JustPassword)
				m_callback->OnSecurityPasswordDone(TRUE, password.CStr(), NULL);
			else if (m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::NewPassword)
				m_callback->OnSecurityPasswordDone(TRUE, NULL, password.CStr());
			else
			{
				m_callback->OnSecurityPasswordDone(FALSE, NULL, NULL);
			}
		}
		else
			m_callback->OnSecurityPasswordDone(FALSE, NULL, NULL);
	}

	if (m_retval)
		*m_retval = retval;

	return 0;
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void SecurityPasswordDialog::OnCancel()
{
	if (m_retval)
		*m_retval = FALSE;
	
	if (m_callback)
	{
		m_callback->OnSecurityPasswordDone(FALSE, NULL, NULL);
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void SecurityPasswordDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{

	// if we add a new password, make sure to inform the user about required quality of password
	if (m_callback && m_callback->GetMode() == OpSSLListener::SSLSecurityPasswordCallback::NewPassword)
	{
		OpString newpassword;
		OpString confirmpassword;

		GetWidgetText("Password_edit", newpassword);
		GetWidgetText("Confirm_password_edit", confirmpassword);

		m_ok_enabled = newpassword.Compare(confirmpassword) == 0 && ChangeMasterPasswordDialog::CheckPasswordPolicy(&confirmpassword);
		EnableButton(0, m_ok_enabled);

		if(ChangeMasterPasswordDialog::CheckPasswordPolicy(&newpassword))
		{
			SetWidgetText("Password_info_label",Str::SI_MSG_SECURE_ASK_NEW_PASSWORD_AGAIN);	
		}
		else
		{
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_UPDATE_PASSWORD);		
		}
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL SecurityPasswordDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OP_ASSERT(child_action != 0);

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_OK:
				{
					child_action->SetEnabled(m_ok_enabled);
					return TRUE;
				}
			default:
				break;
			}

			break;
		}
	}
	return Dialog::OnInputAction(action);
}
