/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JOINCHAT_H
#define JOINCHAT_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2/src/engine/listeners.h"

/***********************************************************************************
**
**	JoinChatDialog
**
***********************************************************************************/

class JoinChatDialog : public Dialog, public AccountListener
{
	public:

		virtual					~JoinChatDialog();

		virtual void			Init(DesktopWindow* parent_window, INT32 account_id = 0);

		Type					GetType()				{return DIALOG_TYPE_JOIN_CHAT;}
		const char*				GetWindowName()			{return "Join Chat Dialog";}

		virtual void			OnInit();
		virtual void			OnSetFocus();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

		virtual BOOL			OnInputAction(OpInputAction* action);

		// implementing MessageEngine::AccountListener

		virtual void			OnAccountAdded(UINT16 account_id);
		virtual void			OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
		virtual void			OnAccountStatusChanged(UINT16 account_id) {}
		virtual void			OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
		virtual void			OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
		virtual void			OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
		virtual void			OnFolderLoadingCompleted(UINT16 account_id) { }

	private:

		void					NewAccount();
		INT32					AddAccount(UINT16 account_id);

		BOOL					m_creating_account;
		INT32					m_account_id;
};

#endif //JOINCHAT_H
