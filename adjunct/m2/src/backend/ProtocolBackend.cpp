// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/util/authenticate.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2_ui/controllers/AuthenticationFailedController.h"
#include "adjunct/quick/Application.h"
#include "modules/widgets/OpDropDown.h"


ProtocolBackend::ProtocolBackend(MessageDatabase& database)
  : MessageBackend(database)
  , m_account(0)
  , m_retrieved_password(FALSE)
  , m_auth_controller(0)
{
	if (MessageEngine::GetInstance())
		OpStatus::Ignore(m_progress.AddListener(&MessageEngine::GetInstance()->GetMasterProgress()));
}

ProtocolBackend::~ProtocolBackend()
{
	// Any authentication dialogs associated with this backend should now be closed
	OP_DELETE(m_auth_controller);

	// Let progress know that there is no more progress for this backend
	m_progress.EndCurrentAction(TRUE);

	// Ensure that we don't listen to PrivacyManager
	PrivacyManager::GetInstance()->RemoveCallback(this);
}

void ProtocolBackend::OnDialogClosing(DialogContext* context)
{
	if (context == m_auth_controller)
		m_auth_controller = NULL;
}

OP_STATUS ProtocolBackend::OnAuthenticationRequired(const OpStringC& server_message)
{
	// If there's already a dialog up, don't do it again
	if (m_auth_controller)
		return OpStatus::OK;

	// Reset password
	ResetRetrievedPassword();

	// Do the dialog
	m_auth_controller = OP_NEW(AuthenticationFailedController, (*this, server_message));
	RETURN_OOM_IF_NULL(m_auth_controller);

	m_auth_controller->SetListener(this);
	RETURN_IF_ERROR(ShowDialog(m_auth_controller, g_global_ui_context, g_application->GetActiveDesktopWindow()));

	return OpStatus::OK;
}

OP_STATUS ProtocolBackend::GetServername(OpString& servername) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return servername.Set(m_account->GetIncomingServername());
	else if (m_account->IsOutgoingBackend(this))
		return servername.Set(m_account->GetOutgoingServername());
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::GetServername(OpString8& servername) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetIncomingServername(servername);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingServername(servername);
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::GetPort(UINT16& port) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
	{
		port = m_account->GetIncomingPort();
	}
	else if (m_account->IsOutgoingBackend(this))
	{
		port = m_account->GetOutgoingPort();
	}
	else
	{
		return OpStatus::ERR;
	}

	if (port == 0)
	{
		BOOL use_secure_connection;

		RETURN_IF_ERROR(GetUseSecureConnection(use_secure_connection));
		port = GetDefaultPort(use_secure_connection);
	}
	return OpStatus::OK;
}

AccountTypes::AuthenticationType ProtocolBackend::GetAuthenticationMethod() const
{
	if (!m_account)
		return AccountTypes::NONE;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetIncomingAuthenticationMethod();
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingAuthenticationMethod();
	else
		return AccountTypes::NONE;
}


AccountTypes::AuthenticationType ProtocolBackend::GetNextAuthenticationMethod(AccountTypes::AuthenticationType current_method, UINT32 supported_authentication) const
{
	AccountTypes::AuthenticationType next_method = AccountTypes::NONE;

	int i;
	for (i=((int)current_method)-1; i>=0; i--)
	{
		if (supported_authentication & (1<<i))
		{
			next_method = (AccountTypes::AuthenticationType)i;
			break;
		}
	}

	return next_method;
}


AccountTypes::AuthenticationType ProtocolBackend::GetCurrentAuthMethod() const
{
	if (!m_account)
		return AccountTypes::NONE;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetCurrentIncomingAuthMethod();
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetCurrentOutgoingAuthMethod();
	else
		return AccountTypes::NONE;
}

void ProtocolBackend::SetCurrentAuthMethod(AccountTypes::AuthenticationType auth_method) const
{
	if (!m_account)
		return;

	if (m_account->IsIncomingBackend(this))
		m_account->SetCurrentIncomingAuthMethod(auth_method);
	else if (m_account->IsOutgoingBackend(this))
		m_account->SetCurrentOutgoingAuthMethod(auth_method);
}

OP_STATUS ProtocolBackend::GetUsername(OpString& username) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return username.Set(m_account->GetIncomingUsername());
	else if (m_account->IsOutgoingBackend(this))
		return username.Set(m_account->GetOutgoingUsername());
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::GetUsername(OpString8& username, BOOL quoted) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		RETURN_IF_ERROR(m_account->GetIncomingUsername(username));
	else if (m_account->IsOutgoingBackend(this))
		RETURN_IF_ERROR(m_account->GetOutgoingUsername(username));
	else
		return OpStatus::ERR;

	if (quoted)
	{
		OpString8 unquoted_string;

		RETURN_IF_ERROR(unquoted_string.TakeOver(username));
		RETURN_IF_ERROR(OpMisc::QuoteString(unquoted_string, username));
	}

	return OpStatus::OK;
}


#ifndef WAND_CAP_GANDALF_3_API
OP_STATUS ProtocolBackend::GetPassword ( OpString& password ) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend (this))
		return m_account->GetIncomingPassword(password);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingPassword(password);
	else
		return OpStatus::ERR;
}
#endif // !WAND_CAP_GANDALF_3_API


/***********************************************************************************
 ** Gets password for this account.
 **
 ** ProtocolBackend::RetrievePassword
 **
 ***********************************************************************************/
OP_STATUS ProtocolBackend::RetrievePassword()
{
#ifdef WAND_CAP_GANDALF_3_API
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetIncomingPassword(this);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingPassword(this);
	else
		return OpStatus::ERR;
#else
	return OpStatus::OK;
#endif // WAND_CAP_GANDALF_3_API
}


/***********************************************************************************
 ** Reset data about retrieved passwords (use when password changes)
 **
 ** ProtocolBackend::ResetRetrievedPassword
 **
 ***********************************************************************************/
void ProtocolBackend::ResetRetrievedPassword()
{
	m_retrieved_password = FALSE;
	m_password.Empty();
}


BOOL ProtocolBackend::GetDownloadBodies() const
{
	return m_account ? m_account->GetDownloadBodies() : FALSE;
}

OP_STATUS ProtocolBackend::GetOptionsFile(OpString& options_file) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetIncomingOptionsFile(options_file);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingOptionsFile(options_file);
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::GetLogFile(OpString& log_file) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->GetIncomingLogFile(log_file);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->GetOutgoingLogFile(log_file);
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::Log(const OpStringC8& heading, const OpStringC8& text) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->LogIncoming(heading, text);
	else if (m_account->IsOutgoingBackend(this))
		return m_account->LogOutgoing(heading, text);
	else
		return OpStatus::ERR;
}

OP_STATUS ProtocolBackend::LogFormattedText(const char* textfmt, ...) const
{
	// Don't do the work if it's not necessary
	if (!m_account || !m_account->BackendHasLogging(this))
		return OpStatus::OK;

	OpString8 textstring;
	OP_STATUS ret = OpStatus::OK;
	va_list args;

	va_start(args, textfmt);
	ret = textstring.AppendVFormat(textfmt, args);
	va_end(args);
	if(ret != OpStatus::OK)
		return ret;

	return Log(textstring, "");
}

OP_STATUS ProtocolBackend::CreateOptionsFileName()
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	if (m_account->IsIncomingBackend(this))
		return m_account->CreateIncomingOptionsFileName();
	else if (m_account->IsOutgoingBackend(this))
		return m_account->CreateOutgoingOptionsFileName();
	else
		return OpStatus::ERR;
}

BOOL ProtocolBackend::HasLoggingEnabled()
{
	return m_account->BackendHasLogging(this);
}

OP_STATUS ProtocolBackend::PopulateAuthenticationDropdown(OpDropDown* dropdown)
{
	OP_ASSERT(dropdown);
	if (!dropdown)
		return OpStatus::ERR_NULL_POINTER;

	OpString auth_type;

	for (INTPTR i = 31; i >= 0; i--)
	{
		if ((GetAuthenticationSupported() & (1 << i)) &&
				   OpStatus::IsSuccess(m_account->GetAuthenticationString(
									   (AccountTypes::AuthenticationType)i,
										auth_type)))
		{
			// Add currently selected type to string if autoselect
			if (i == AccountTypes::AUTOSELECT)
			{
				AccountTypes::AuthenticationType current_type = GetCurrentAuthMethod();
				OpString current_type_string;

				if (current_type != AccountTypes::AUTOSELECT &&
								OpStatus::IsSuccess(m_account->GetAuthenticationString(current_type,
						current_type_string)))
				{
					RETURN_IF_ERROR(auth_type.AppendFormat(UNI_L(" (%s)"), current_type_string.CStr()));
				}
			}

			// Add generated string to dropdown
			int dropdown_index;
			RETURN_IF_ERROR(dropdown->AddItem(auth_type.CStr(), -1, &dropdown_index, i));

			// Select if necessary
			if (GetAuthenticationMethod() == i)
				dropdown->SelectItem(dropdown_index, TRUE);
		}
	}

	return OpStatus::OK;
}

OP_STATUS ProtocolBackend::GetLoginInfo(OpAuthenticate& login_info, const OpStringC8& challenge)
{
	OpString username;

	RETURN_IF_ERROR(GetUsername(username));

#ifdef WAND_CAP_GANDALF_3_API
	return login_info.Init(GetCurrentAuthMethod(), GetType(), username, m_password, challenge);
#else
	OpString password;
	RETURN_IF_ERROR(GetPassword(password));
	return login_info.Init(GetCurrentAuthMethod(), GetType(), username, password, challenge);
#endif // WAND_CAP_GANDALF_3_API
}

void ProtocolBackend::OnPasswordRetrieved(const OpStringC& password)
{
	m_password.Set(password);
	m_retrieved_password = TRUE;

	if (m_account->IsIncomingBackend(this))
		OpStatus::Ignore(Connect());
	else
		OpStatus::Ignore(GetAccountPtr()->SendMessages());
}

void ProtocolBackend::OnPasswordFailed()
{
	m_password.Empty();
	m_retrieved_password = FALSE;
}

OP_STATUS ProtocolBackend::GetUseSecureConnection(BOOL& use_secure_connection) const
{
	if (!m_account)
		return OpStatus::ERR_NULL_POINTER;

	use_secure_connection = TRUE;
	if (m_account->IsIncomingBackend(this))
		use_secure_connection = m_account->GetUseSecureConnectionIn();
	else if (m_account->IsOutgoingBackend(this))
		use_secure_connection = m_account->GetUseSecureConnectionOut();
	else
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS ProtocolBackend::RetryAuth()
{
	if (m_account->IsIncomingBackend(this))
		return FetchMessages(TRUE);
	else
		return GetAccountPtr()->SendMessages();
}

void ProtocolBackend::OnError(const OpStringC& errormessage, EngineTypes::ErrorSeverity severity) const
{
	m_account->OnError(errormessage, severity);
}

PrefsFile* ProtocolBackend::GetPrefsFile()
{
	OpString prefs_file_path;

	if (!m_account)
		return NULL;

	if (m_account->GetIncomingBackend() == this)
	{
		//Find incoming options file
		m_account->GetIncomingOptionsFile(prefs_file_path);
	}
	else if (m_account->GetOutgoingBackend() == this)
	{
		// Find outgoing options file
		m_account->GetOutgoingOptionsFile(prefs_file_path);
	}
	else
	{
		return NULL;
	}

	if (prefs_file_path.IsEmpty())
		return NULL;

	//Create prefsfile
	return MessageEngine::GetInstance()->GetGlueFactory()->CreatePrefsFile(prefs_file_path);
}

const char* ProtocolBackend::GetIcon(BOOL progress_icon)
{
	if (!GetProgress().IsConnected() && GetType() != AccountTypes::POP)
		return m_account->GetLowBandwidthMode() ? "Mail Accounts Offline LBM" : "Mail Accounts Offline";
	else if (m_account->GetLowBandwidthMode())
			return "Mail Accounts LBM";

	return progress_icon ? NULL : "Mail Accounts";
}

#endif //M2_SUPPORT
