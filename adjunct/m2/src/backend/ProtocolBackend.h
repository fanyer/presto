// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

// ----------------------------------------------------

#ifndef PROTOCOL_BACKEND_H
#define PROTOCOL_BACKEND_H

// ----------------------------------------------------

#include "adjunct/m2/src/engine/backend.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"

class Account;
class Message;
class ChatInfo;
class PrefsFile;
class OfflineLog;
class OpDropDown;
class OpAuthenticate;
class AuthenticationFailedController;

// ----------------------------------------------------

class ProtocolBackend : public MessageBackend, PrivacyManagerCallback, public DialogContextListener
{
	public:
		ProtocolBackend(MessageDatabase& database);
		virtual ~ProtocolBackend();

		virtual OP_STATUS Init(Account* account) = 0;
		virtual AccountTypes::AccountType GetType() const = 0;
		OP_STATUS OnAuthenticationRequired(const OpStringC& server_message = NULL);
		
		// Implementing DialogContextListener
		virtual void OnDialogClosing(DialogContext* context);

        //Account utility functions
		virtual Account*  GetAccountPtr() const {return m_account;}
		//virtual OP_STATUS GetEmail(OpString& email) const;
		//virtual OP_STATUS GetEmail(OpString8& email) const;
		virtual OP_STATUS GetServername(OpString& servername) const;
		virtual OP_STATUS GetServername(OpString8& servername) const;
		virtual OP_STATUS GetPort(UINT16& port) const;
		virtual AccountTypes::AuthenticationType GetAuthenticationMethod() const; //Method specified in accounts.ini.
		virtual AccountTypes::AuthenticationType GetNextAuthenticationMethod(AccountTypes::AuthenticationType current_method, UINT32 supported_authentication) const;
		virtual AccountTypes::AuthenticationType	GetCurrentAuthMethod() const; //Method currently in use. Not saved.
		virtual void	  SetCurrentAuthMethod(AccountTypes::AuthenticationType auth_method) const;
		virtual OP_STATUS GetUsername(OpString& username) const;
		virtual OP_STATUS GetUsername(OpString8& username, BOOL quoted = FALSE) const;

#ifndef WAND_CAP_GANDALF_3_API
		virtual OP_STATUS GetPassword(OpString& password) const;
#endif // !WAND_CAP_GANDALF_3_API

		/** Gets password for this account.
		  * Once the password is retrieved (asynchronously), will call Connect()
		  */
		virtual OP_STATUS RetrievePassword();

		/** @return Whether password for this backend has been retrieved
		  */
#ifdef WAND_CAP_GANDALF_3_API
		virtual BOOL	  HasRetrievedPassword() const { return m_retrieved_password; }
#else
		virtual BOOL	  HasRetrievedPassword() const { return TRUE; }
#endif // WAND_CAP_GANDALF_3_API

		/** Reset data about retrieved passwords (use when password changes)
		  */
		virtual void	  ResetRetrievedPassword();

		virtual BOOL      GetDownloadBodies() const;
		virtual OP_STATUS GetOptionsFile(OpString& options_file) const;
		virtual OP_STATUS GetLogFile(OpString& log_file) const;
		virtual OP_STATUS GetUseSecureConnection(BOOL& use_secure_connection) const;
		virtual BOOL	  GetUseSSL() const { return FALSE; }
		virtual OP_STATUS Log(const OpStringC8& heading, const OpStringC8& text) const;
		virtual OP_STATUS LogFormattedText(const char* textfmt, ...) const;
		virtual void      OnError(const OpStringC& errormessage, EngineTypes::ErrorSeverity severity=EngineTypes::GENERIC_ERROR) const;
		virtual OP_STATUS CreateOptionsFileName();
		BOOL			  HasLoggingEnabled();

		/** Populate a dropdown with all authentication methods for this backend, and the current one selected
		 * @param dropdown Dropdown to populate
		 */
		virtual OP_STATUS PopulateAuthenticationDropdown(OpDropDown* dropdown);

		/** Retry authentication
		  */
		virtual OP_STATUS RetryAuth();

		/** Cancel authentication
		  */
		virtual OP_STATUS CancelAuth() { return Disconnect(); }

		/** Get login info for this backend
		 * @param login_info Login info to fill in
		 * @param password Password for this login
		 * @param challenge Challenge to respond to (if applicable)
		 */
		virtual OP_STATUS GetLoginInfo(OpAuthenticate& login_info, const OpStringC8& challenge = NULL);

		/** Gets progress for this backend
		 */
		virtual ProgressInfo& GetProgress() { return m_progress; }

		// PrivacyManagerCallback
		virtual void OnPasswordRetrieved(const OpStringC& password);
		virtual void OnPasswordFailed();

		virtual const char* GetIcon(BOOL progress_icon);

	protected:
		virtual PrefsFile* GetPrefsFile();

		ProgressInfo m_progress;
		Account*	 m_account;
		OpMisc::SafeString<OpString> m_password;
		BOOL		 m_retrieved_password;

	private:
		AuthenticationFailedController* m_auth_controller;
};

#endif // PROTOCOL_BACKEND_H
