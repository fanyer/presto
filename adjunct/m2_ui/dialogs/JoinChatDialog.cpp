/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)
//# include "modules/prefs/storage/pfmap.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "JoinChatDialog.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/Application.h"

/***********************************************************************************
**
**	JoinChatDialog
**
***********************************************************************************/

void JoinChatDialog::Init(DesktopWindow* parent_window, INT32 account_id)
{
	m_account_id = account_id;
	m_creating_account = FALSE;

	Dialog::Init(parent_window);
}

void JoinChatDialog::OnInit()
{
	SetWidgetEnabled("Accounts_dropdown",		FALSE);
	SetWidgetEnabled("Room_edit",				FALSE);
	SetWidgetEnabled("List_button",			FALSE);

	OpDropDown* accounts = (OpDropDown*) GetWidgetByName("Accounts_dropdown");

	if (accounts)
	{
		AccountManager* account_manager = g_m2_engine->GetAccountManager();

		int i;

		for (i = 0; i < account_manager->GetAccountCount(); i++)
		{
			AddAccount(account_manager->GetAccountByIndex(i)->GetAccountId());
		}
	}

	g_m2_engine->AddAccountListener(this);
}

void JoinChatDialog::OnSetFocus()
{
	if (IsWidgetEnabled("Accounts_dropdown"))
	{
		SetWidgetFocus("Room_edit", TRUE);
	}
}

JoinChatDialog::~JoinChatDialog()
{
	g_m2_engine->RemoveAccountListener(this);
}

/***********************************************************************************
**
**	NewAccount
**
***********************************************************************************/

void JoinChatDialog::NewAccount()
{
	g_application->CreateAccount(AccountTypes::IRC, NULL, this);
	m_creating_account = TRUE;
}

/***********************************************************************************
**
**	AddAccount
**
***********************************************************************************/

INT32 JoinChatDialog::AddAccount(UINT16 account_id)
{
	INT32 got_index = -1;

	OpDropDown* accounts = (OpDropDown*) GetWidgetByName("Accounts_dropdown");

	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	account_manager->GetAccountById(account_id, account);

	if (account && account->GetIncomingProtocol() == AccountTypes::IRC)
	{
		accounts->AddItem(account->GetAccountName().CStr(), -1, &got_index, account->GetAccountId());

		SetWidgetEnabled("Accounts_dropdown",		TRUE);
		SetWidgetEnabled("Room_edit",				TRUE);

		if (m_account_id == account->GetAccountId())
		{
			accounts->SelectItem(got_index, TRUE);
		}
	}

	return got_index;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL JoinChatDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEW_ACCOUNT:
		{
			NewAccount();
			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 JoinChatDialog::OnOk()
{
	OpDropDown* servers = (OpDropDown*) GetWidgetByName("Accounts_dropdown");

	if (servers)
	{
		OpString room;
		GetWidgetText("Room_edit", room);

		// Temporary fix for people typing # before the room name, something
		// they should not do here.
		if (room.HasContent() && room[0] == '#')
			room.Delete(0, 1);

		UINT32 account_id = (UINTPTR)servers->GetItemUserData(servers->GetSelectedItem());
		ChatInfo chat_info(room,OpStringC());
		g_application->GoToChat(account_id, chat_info, TRUE);
	}

	return 0;
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void JoinChatDialog::OnCancel()
{

	//abort

}

/***********************************************************************************
**
**	OnAccountAdded
**
***********************************************************************************/

void JoinChatDialog::OnAccountAdded(UINT16 account_id)
{
	OpDropDown* accounts = (OpDropDown*) GetWidgetByName("Accounts_dropdown");

	if (m_creating_account || accounts->GetSelectedItem() == -1)
	{
		INT32 got_index = AddAccount(account_id);

		if (got_index != -1)
		{
			accounts->SelectItem(got_index, TRUE);

			SetWidgetFocus("Room_edit", TRUE);
		}
	}

	m_creating_account = FALSE;
}

/***********************************************************************************
**
**	OnAccountRemoved
**
***********************************************************************************/

void JoinChatDialog::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{

}

#endif //M2_SUPPORT && IRC_SUPPORT
