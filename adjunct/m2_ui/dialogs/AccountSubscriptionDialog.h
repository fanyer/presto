/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ACCOUNTSUBSCRIPTIONDIALOG_H
#define ACCOUNTSUBSCRIPTIONDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2_ui/models/groupsmodel.h"

class DeleteAccountFolderController;

/***********************************************************************************
**
**	AccountSubscriptionDialog
**
***********************************************************************************/

class AccountSubscriptionDialog : public Dialog, public AccountListener, public DialogContextListener
{
	public:
		                        AccountSubscriptionDialog();
		virtual					~AccountSubscriptionDialog();

		virtual void			Init(AccountTypes::AccountType account_type, DesktopWindow* window);
		virtual void			Init(UINT16 account_id, DesktopWindow* window);

		DialogType				GetDialogType()			{return m_read_only ? TYPE_CLOSE : TYPE_OK_CANCEL;}
		Type					GetType()				{return DIALOG_TYPE_ACCOUNT_SUBSCRIPTION;}
		const char*				GetWindowName()			{return "Account Subscription Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor();

		virtual void			OnInitVisibility();
		virtual void			OnInit();
		virtual UINT32			OnOk() {m_commit = TRUE; return 0;}

		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnItemChanged(OpWidget *widget, INT32 pos);

		// implementing DialogContextListener
		virtual void			OnDialogClosing(DialogContext* dialog);

		// Implementing the DesktopWindowListener interface
		virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual void			OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);

		virtual BOOL			GetShowProgressIndicator() const;

		// implementing MessageEngine::AccountListener

		virtual void			OnAccountAdded(UINT16 account_id);
		virtual void			OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
		virtual void			OnAccountStatusChanged(UINT16 account_id) {}
		virtual void			OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable);
		virtual void			OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
		virtual void			OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
		virtual void			OnFolderLoadingCompleted(UINT16 account_id);

	private:
		// Methods.
		void UpdateProgressInformation(GroupsModel *groups_model = 0);

		// Members.
		OpTreeView*							m_accounts_treeview;
		OpTreeView*							m_groups_treeview;

		OpINT32HashTable<GroupsModel>		m_group_models_hashtable;
		OpVector<GroupsModel>				m_group_models_list;

		BOOL								m_creating_account;
		BOOL								m_commit;
		BOOL								m_read_only;

		AccountTypes::AccountType			m_account_type;
        UINT16								m_account_id;
		DeleteAccountFolderController*		m_delete_account_folder_controller;
		DesktopWindow*						m_mail_index_properties_dialog;
};

#endif //ACCOUNTSUBSCRIPTIONDIALOG_H
