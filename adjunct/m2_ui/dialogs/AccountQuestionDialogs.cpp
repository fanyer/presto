/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "AccountQuestionDialogs.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#ifdef IRC_SUPPORT
#include "adjunct/m2/src/backend/irc/dcc.h"
#endif
# include "modules/util/gen_str.h"

#include "modules/widgets/OpEdit.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/desktop_util/string/stringutils.h" 
#include "modules/locale/oplanguagemanager.h"

// ***************************************************************************
//
//	AskServerCleanupDialog
//
// ***************************************************************************

AskServerCleanupController::AskServerCleanupController(UINT16 account_id):
	SimpleDialogController(TYPE_OK_CANCEL,IMAGE_WARNING,WINDOW_NAME_ASK_SERVER_CLEANUP,Str::S_MAIL_SERVER_CLEANUP_WARNING,Str::S_MAIL_SERVER_CLEANUP),
	m_account_id(account_id)
{
}

void AskServerCleanupController::OnOk()
{
	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	account_manager->GetAccountById(m_account_id, account);

	if (account)
	{
		account->ServerCleanUp();
	}
	SimpleDialogController::OnOk();
}


// ***************************************************************************
//
//	AskKickReasonDialog
//
// ***************************************************************************

AskKickReasonDialog::AskKickReasonDialog(UINT16 account_id,	ChatInfo& chat_info):
	m_account_id(account_id),
	m_chat_info(chat_info)
{
}

OP_STATUS AskKickReasonDialog::Init(DesktopWindow* parent_window,
	const OpStringC& nick)
{
	RETURN_IF_ERROR(m_nick_to_kick.Set(nick));

	Dialog::Init(parent_window);
	return OpStatus::OK;
}


void AskKickReasonDialog::OnInit()
{
	// Format the label of the dialog.
	OpString label_text;
	g_languageManager->GetString(Str::D_KICK_CHAT_USER_DLG_LABEL, label_text);

	if (label_text.HasContent())
	{
		OpString temp_var;
		temp_var.AppendFormat(label_text.CStr(), m_nick_to_kick.CStr());

		OpLabel* label_widget = (OpLabel *)(GetWidgetByName("label_for_Kick_edit"));
		if (label_widget != 0)
			label_widget->SetText(temp_var.CStr());
	}
}


UINT32 AskKickReasonDialog::OnOk()
{
	// Get the kick reason.
	OpEdit* edit_widget = (OpEdit *)(GetWidgetByName("Kick_edit"));
	if (edit_widget != 0)
	{
		OpString kick_reason;
		edit_widget->GetText(kick_reason);

		OpString empty;
		g_m2_engine->SendChatMessage(m_account_id, EngineTypes::KICK_COMMAND,
			kick_reason, m_chat_info, m_nick_to_kick);
	}

	return 0;
}


void AskKickReasonDialog::OnCancel()
{
}

#endif // M2_SUPPORT
