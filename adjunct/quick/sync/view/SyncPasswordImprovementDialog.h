/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SYNC_PASSWORD_IMPROVEMENT_DIALOG_H
#define SYNC_PASSWORD_IMPROVEMENT_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/managers/OperaAccountManager.h"

#include "modules/locale/oplanguagemanager.h"

class SyncPasswordImprovementDialog : 
		public Dialog,
		public OperaAccountController::OAC_Listener
{
	public:

		enum PasswordImprovementError
		{
			ERROR_PASSWORDS_DONT_MATCH = 0,
			ERROR_RETRIEVING_PASSWORDS,
			ERROR_OLD_PASSWORD_TOO_SHORT,
			ERROR_NEW_PASSWORD_TOO_SHORT,
			ERROR_SERVER_PROBLEM,
			ERROR_REQUEST_PROBLEM,
			ERROR_CANT_AUTHENTICATE
		};
		
		SyncPasswordImprovementDialog(): 
			m_was_error(FALSE),
			m_in_progress(FALSE) {}
		virtual ~SyncPasswordImprovementDialog();
		OP_STATUS Init(DesktopWindow* parent);

		OP_STATUS GetPasswordFromEdit(OpString& passwd, const char edit_name[]);
		void ShowError(PasswordImprovementError error);
		void HideError();
		void ShowProgress(BOOL show);

		/**
		 * @see OperaAccountController::OAC_Listener
		 */
		virtual void OnOperaAccountPasswordChanged(OperaAuthError err);
		virtual void OnOperaAccountAuth(OperaAuthError error, const OpStringC &shared_secret);

		/**
		 * @see OpInputContext
		 */
		virtual Type GetType() { return DIALOG_TYPE_SYNC_PASSWORD_IMPROVEMENT; }
		virtual BOOL OnInputAction(OpInputAction* action);

		/**
		 * @see Dialog
		 */
		virtual void OnInit();
		virtual DialogType GetDialogType() { return TYPE_OK_CANCEL; }
		virtual BOOL GetIsBlocking() { return TRUE; }
		virtual Str::LocaleString GetOkTextID() 
				{ return Str::S_TITLE_LITERAL_CHANGE_PASSWORD; }
		virtual Str::LocaleString GetCancelTextID() 
				{ return Str::S_TITLE_LITERAL_KEEP_OLD_PASSWORD; }

		/**
		 * @see DesktopWindow
		 */
		virtual const char*	GetWindowName() { return "Sync Password Improvement Dialog"; }

	private:

		OpString m_username;
		OpString m_new_passwd;
		BOOL m_was_error;
		BOOL m_in_progress;

		void BindEditWithPasswordStrength(const char edit_name[], const char passwd_name[]);
};

#endif // SYNC_PASSWORD_IMPROVEMENT_DIALOG_H

