/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/ChangeMasterPasswordDialog.h"
#include "modules/libcrypto/libcrypto_module.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/libssl/ssldlg.h"

/***********************************************************************************
**
**	ChangeMasterPasswordDialog
**
***********************************************************************************/

ChangeMasterPasswordDialog::ChangeMasterPasswordDialog()
{
	m_ok_enabled = FALSE;
	m_listener   = 0;
}

/***********************************************************************************
**
**	~ChangeMasterPasswordDialog
**
***********************************************************************************/

ChangeMasterPasswordDialog::~ChangeMasterPasswordDialog()
{
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void ChangeMasterPasswordDialog::OnInit()
{
	OpEdit* passwordedit = (OpEdit*)GetWidgetByName("Current_password_edit");
	if(passwordedit)
	{
		passwordedit->SetPasswordMode(TRUE);
	}
	passwordedit = (OpEdit*)GetWidgetByName("Password_edit");
	if(passwordedit)
	{
		passwordedit->SetPasswordMode(TRUE);
	}
	passwordedit = (OpEdit*)GetWidgetByName("Confirm_password_edit");
	if(passwordedit)
	{
		passwordedit->SetPasswordMode(TRUE);
	}

	OpMultilineEdit* passwordlabel = (OpMultilineEdit*)GetWidgetByName("Password_info_label");
	if(passwordlabel)
	{
		passwordlabel->SetLabelMode();
		if(g_libcrypto_master_password_handler->HasMasterPassword())
		{
			SetWidgetEnabled("Password_edit", FALSE);
			SetWidgetEnabled("Confirm_password_edit", FALSE);
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_OLD_PASSWORD);
		}
		else	// no old password
		{
			SetWidgetEnabled("Current_password_edit", FALSE);
			SetWidgetEnabled("Password_edit", TRUE);
			SetWidgetEnabled("Confirm_password_edit", TRUE);
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_FIRST_PASSWORD);
		}
	}

	//disable the ok button until both new password and confirm is equal
	EnableButton(0, m_ok_enabled);
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 ChangeMasterPasswordDialog::OnOk()
{
	OP_STATUS status = OpStatus::OK;

	OpString8 currentpassword;
	OpString8 newpassword;

	OpEdit* passwordedit = (OpEdit*)GetWidgetByName("Current_password_edit");

	if(passwordedit)
	{
		OpString text;
		passwordedit->GetText(text);
		currentpassword.SetUTF8FromUTF16(text);
	}

	passwordedit = (OpEdit*)GetWidgetByName("Password_edit");

	if(passwordedit)
	{
		OpString text;
		passwordedit->GetText(text);
		newpassword.SetUTF8FromUTF16(text);
	}

	if(g_libcrypto_master_password_handler->HasMasterPassword())
	{
		status = g_libcrypto_master_password_handler->ChangeMasterPassword(currentpassword.CStr(), newpassword.CStr());
	}
	else
	{
		status = g_libcrypto_master_password_handler->SetMasterPassword(newpassword.CStr());
	}

	if(OpStatus::IsSuccess(status))
	{
		BroadcastMasterPasswordChanged();

		return 1;
	}
	else
	{
		return 0;
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void ChangeMasterPasswordDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Current_password_edit"))
	{
		OpString currentpass;
		widget->GetText(currentpass);

		BOOL current_passed = CheckPassword(&currentpass);
		SetWidgetEnabled("Password_edit", current_passed);
		SetWidgetEnabled("Confirm_password_edit", current_passed);

		if(current_passed)
		{
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_NEW_PASSWORD);
		}
		else
		{
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_OLD_PASSWORD);
		}
	}
	else if(widget->IsNamed("Password_edit"))
	{
		OpString newpassword;
		widget->GetText(newpassword);
		if(CheckPasswordPolicy(&newpassword))
		{
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_NEW_PASSWORD_AGAIN);
		}
		else
		{
			SetWidgetText("Password_info_label", Str::SI_MSG_SECURE_ASK_UPDATE_PASSWORD);
		}
	}

	if (widget->IsNamed("Confirm_password_edit") ||
		widget->IsNamed("Password_edit"))
	{
		OpString newpassword, confirmpassword;

		GetWidgetText("Password_edit", newpassword);
		GetWidgetText("Confirm_password_edit", confirmpassword);

		if(GetWidgetByName("Confirm_password_edit")->IsVisible())
		{
			m_ok_enabled = newpassword.Compare(confirmpassword) == 0 && CheckPasswordPolicy(&confirmpassword);
		}
		else
		{
			m_ok_enabled = CheckPasswordPolicy(&newpassword);
		}

		EnableButton(0, m_ok_enabled);
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

/***********************************************************************************
**
**	CheckPassword
**
***********************************************************************************/

BOOL ChangeMasterPasswordDialog::CheckPassword(OpString* password)
{
	OP_ASSERT(password);

	OpString8 passwordInUtf8;

	RETURN_VALUE_IF_ERROR(passwordInUtf8.SetUTF8FromUTF16(*password), FALSE);

	return OpStatus::IsSuccess(g_libcrypto_master_password_handler->CheckLibsslPasswordCheckcode(passwordInUtf8.CStr()));
}

/***********************************************************************************
**
**	CheckPasswordPolicy
**
***********************************************************************************/

BOOL ChangeMasterPasswordDialog::CheckPasswordPolicy(OpString* password)
{
	OP_ASSERT(password);

	OpString8 passwordInUtf8;

	RETURN_VALUE_IF_ERROR(passwordInUtf8.SetUTF8FromUTF16(*password), FALSE);

	return OpStatus::IsSuccess(g_libcrypto_master_password_handler->CheckPasswordPolicy(passwordInUtf8.CStr()));
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ChangeMasterPasswordDialog::OnInputAction(OpInputAction* action)
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
