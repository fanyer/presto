/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/m2/src/backend/imap/commands/AuthenticationCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/util/authenticate.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"


///////////////////////////////////////////
//            Authentication
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Authentication::OnFailed
 ***********************************************************************************/
OP_STATUS ImapCommands::Authentication::OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
{
	// Failed password attempt, ask the user for password
	OpString server_msg;
	OpStatus::Ignore(server_msg.Set(failed_msg));

	protocol.GetBackend().Lock();
	protocol.GetBackend().RequestDisconnect();
	protocol.GetBackend().OnAuthenticationRequired(server_msg);

	return OpStatus::OK;
}


///////////////////////////////////////////
//            Authenticate
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Authenticate::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::Authenticate::GetExpandedQueue(IMAP4& protocol)
{
	OpAutoPtr<ImapCommandItem> replacement_command;

	ImapBackend& backend = protocol.GetBackend();

	// Search for an authentication method to use
	AccountTypes::AuthenticationType auth_method = backend.GetAuthenticationMethod();

	if (auth_method == AccountTypes::AUTOSELECT)
	{
		auth_method = backend.GetCurrentAuthMethod();

		if (auth_method == AccountTypes::AUTOSELECT)
		{
			// Autoselect of authentication method, check which methods are supported
			UINT32 supported_authentication = backend.GetAuthenticationSupported();

			if (protocol.GetCapabilities() & ImapFlags::CAP_LOGINDISABLED)
				supported_authentication &= ~(1 << AccountTypes::PLAINTEXT);
			if (!(protocol.GetCapabilities() & ImapFlags::CAP_AUTH_CRAMMD5))
				supported_authentication &= ~(1 << AccountTypes::CRAM_MD5);
			if (!(protocol.GetCapabilities() & ImapFlags::CAP_AUTH_LOGIN))
				supported_authentication &= ~(1 << AccountTypes::LOGIN);
			if (!(protocol.GetCapabilities() & ImapFlags::CAP_AUTH_PLAIN))
				supported_authentication &= ~(1 << AccountTypes::PLAIN);

			// Get the next in the list
			auth_method = backend.GetNextAuthenticationMethod(backend.GetCurrentAuthMethod(), supported_authentication);

			backend.SetCurrentAuthMethod(auth_method);
		}
	}

	// Change to correct command
	switch (auth_method)
	{
		case AccountTypes::CRAM_MD5:
			replacement_command = OP_NEW(ImapCommands::AuthenticateCramMD5, ());
			break;
		case AccountTypes::LOGIN:
			replacement_command = OP_NEW(ImapCommands::AuthenticateLogin, ());
			break;
		case AccountTypes::PLAIN:
			replacement_command = OP_NEW(ImapCommands::AuthenticatePlain, ());
			break;
		case AccountTypes::PLAINTEXT:
		default:
			if (protocol.GetCapabilities() & ImapFlags::CAP_LOGINDISABLED)
				return NULL; // Not allowed to do this
			replacement_command = OP_NEW(ImapCommands::Login, ());
	}

	if (!replacement_command.get())
		return NULL;

	// Create an LSUB command directly following the authentication
	ImapCommandItem* lsub = OP_NEW(ImapCommands::Lsub, ());
	if (!lsub)
		return NULL;

	lsub->DependsOn(replacement_command.get(), protocol);

	return replacement_command.release();
}


///////////////////////////////////////////
//         AuthenticateCramMD5
///////////////////////////////////////////

/***********************************************************************************
 ** Create the MD5 checksum needed in the response
 **
 ** AuthenticateCramMD5::PrepareContinuation
 ***********************************************************************************/
OP_STATUS ImapCommands::AuthenticateCramMD5::PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text)
{
	m_continuation++;

	// Prepare MD5 checksum response
	OpAuthenticate info;

	RETURN_IF_ERROR(protocol.GetBackend().GetLoginInfo(info, response_text));
	return m_md5_response.Set(info.GetResponse());
}


/***********************************************************************************
 **
 **
 ** AuthenticateCramMD5::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::AuthenticateCramMD5::AppendCommand(OpString8& command, IMAP4& protocol)
{
	return command.Append(m_continuation ? m_md5_response.CStr() : "AUTHENTICATE CRAM-MD5");
}


///////////////////////////////////////////
//         AuthenticateLogin
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** AuthenticateLogin::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::AuthenticateLogin::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpAuthenticate info;

	if (m_continuation > 0)
		RETURN_IF_ERROR(protocol.GetBackend().GetLoginInfo(info));

	switch (m_continuation)
	{
		case 0:
			return command.Append("AUTHENTICATE LOGIN");
		case 1:
			return command.Append(info.GetUsername());
		case 2:
			return command.Append(info.GetPassword());
		default:
			return OpStatus::ERR;
	}
}


///////////////////////////////////////////
//         AuthenticatePlain
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** AuthenticatePlain::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::AuthenticatePlain::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpAuthenticate info;

	if (m_continuation > 0)
		RETURN_IF_ERROR(protocol.GetBackend().GetLoginInfo(info));

	return command.Append(m_continuation ? info.GetResponse().CStr() : "AUTHENTICATE PLAIN");
}


///////////////////////////////////////////
//               Login
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Login::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Login::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpAuthenticate info;

	RETURN_IF_ERROR(protocol.GetBackend().GetLoginInfo(info));

	return command.AppendFormat("LOGIN %s %s", info.GetUsername().CStr(), info.GetPassword().CStr());
}
