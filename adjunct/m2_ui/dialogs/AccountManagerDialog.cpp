/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef M2_SUPPORT
//#include "modules/prefs/storage/pfmap.h"


#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "AccountManagerDialog.h"
#include "AccountPropertiesDialog.h"
#include "adjunct/m2/src/engine/engine.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void AccountManagerDialog::OnInit()
{
	m_creating_account = FALSE;

	OpTreeView* accounts = (OpTreeView*) GetWidgetByName("Accounts_treeview");

	if (accounts)
	{
		OpTreeModel* accounts_model;

		if (g_m2_engine->GetAccountsModel(&accounts_model) == OpStatus::OK)
		{
			accounts->SetMatchType(MATCH_STANDARD);
			accounts->SetTreeModel(accounts_model, 1);
			accounts->SetColumnVisibility(2, FALSE);
			accounts->SetColumnWeight(0, 50);
			accounts->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_EDIT_PROPERTIES)));
		}
	}

	g_m2_engine->AddAccountListener(this);
}

/***********************************************************************************
**
**	~AccountManagerDialog
**
***********************************************************************************/

AccountManagerDialog::~AccountManagerDialog()
{
	g_m2_engine->RemoveAccountListener(this);
}

/***********************************************************************************
**
**	GetSelectedAccountID
**
***********************************************************************************/

INT32 AccountManagerDialog::GetSelectedAccountID()
{
	OpTreeView* accounts = (OpTreeView*) GetWidgetByName("Accounts_treeview");

	if (!accounts)
	{
		return 0;
	}

	OpTreeModelItem* item;

	item = accounts->GetSelectedItem();

	if (item == 0)
	{
		return FALSE;
	}

	return item->GetID();
}

/***********************************************************************************
**
**	NewAccount
**
***********************************************************************************/

void AccountManagerDialog::NewAccount(AccountTypes::AccountType deftype)
{
	m_creating_account = TRUE;
	g_application->CreateAccount(deftype, NULL, this);
}

/***********************************************************************************
**
**	DeleteSelectedAcount
**
***********************************************************************************/

void AccountManagerDialog::DeleteSelectedAcount()
{
	g_application->DeleteAccount(GetSelectedAccountID(), this);
}

/***********************************************************************************
**
**	EditSelectedAcount
**
***********************************************************************************/

void AccountManagerDialog::EditSelectedAcount()
{
	g_application->EditAccount(GetSelectedAccountID(), this, GetModality());
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL AccountManagerDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEW_ACCOUNT:
			NewAccount();
			return TRUE;

		case OpInputAction::ACTION_DELETE:
			DeleteSelectedAcount();
			return TRUE;

		case OpInputAction::ACTION_EDIT_PROPERTIES:
			EditSelectedAcount();
			return TRUE;
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**	OnAccountAdded
**
***********************************************************************************/

void AccountManagerDialog::OnAccountAdded(UINT16 account_id)
{
	OpTreeView* accounts = (OpTreeView*) GetWidgetByName("Accounts_treeview");

	if (!accounts || !m_creating_account)
	{
		return;
	}

	accounts->SetSelectedItemByID(account_id);
	m_creating_account = FALSE;
}
#endif //M2_SUPPORT
