/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ACCOUNT_MANAGER_DIALOG_H
#define ACCOUNT_MANAGER_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
#include "adjunct/m2/src/engine/listeners.h"

/***********************************************************************************
**
**	AccountManagerDialog
**
***********************************************************************************/

class AccountManagerDialog : public Dialog, public AccountListener
{
	public:
		virtual					~AccountManagerDialog();

		DialogType				GetDialogType()			{return TYPE_CLOSE;}
		Type					GetType()				{return DIALOG_TYPE_ACCOUNT_MANAGER;}
		const char*				GetWindowName()			{return "Account Manager Dialog";}
		BOOL					GetModality()			{return TRUE;}
		const char*				GetHelpAnchor()			{return "mail.html";}

		virtual void			OnInit();

		virtual BOOL			OnInputAction(OpInputAction* action);

		// implementing MessageEngine::AccountListener

		virtual void			OnAccountAdded(UINT16 account_id);
		virtual void			OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type) {}
		virtual void			OnAccountStatusChanged(UINT16 account_id) {}
		virtual void			OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
		virtual void			OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
		virtual void			OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
		virtual void			OnFolderLoadingCompleted(UINT16 account_id) { }

	private:
		INT32					GetSelectedAccountID();
		void					NewAccount(AccountTypes::AccountType deftype = AccountTypes::UNDEFINED);
		void					DeleteSelectedAcount();
		void					EditSelectedAcount();

		BOOL					m_creating_account;

};

#endif //ACCOUNT_MANAGER_DIALOG_H
