/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/pop/commands/PopAuthenticationCommands.h"
#include "adjunct/m2/src/backend/pop/commands/Capa.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "adjunct/m2/src/engine/account.h"


/***********************************************************************************
 ** What to do when authentication succeeds
 **
 ** PopCommands::AuthenticationCommand::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticationCommand::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	connection.SetState(POP3::AUTHENTICATED);

	Account *account = connection.GetBackend().GetAccountPtr();

	// if we are in SMTP after POP mode, send messages now
	if (account && account->GetSendQueuedAfterChecking())
		account->SendMessages();

	// Fire off a new capability command, capabilities can change when authenticated
	connection.ResetCapabilities();
	RETURN_IF_ERROR(SetNextCommand(OP_NEW(PopCommands::Capa, ())));

	return PopCommand::OnSuccessfulComplete(success_msg, connection);
}


/***********************************************************************************
 ** What to do when authentication fails - displays the password dialog
 **
 ** PopCommands::AuthenticationCommand::OnFailedComplete
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticationCommand::OnFailedComplete(const OpStringC8& error_msg, POP3& connection)
{
	// Failed password attempt, ask the user for password
	OpString server_msg;

	OpStatus::Ignore(server_msg.Set(error_msg));
	OpStatus::Ignore(connection.GetBackend().OnAuthenticationRequired(server_msg));
	OpStatus::Ignore(connection.Cancel());
	OpStatus::Ignore(connection.GetBackend().Log("POP OUT: Disconnect", ""));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticateCramMD5::GetPopCommand
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticateCramMD5::GetPopCommand(OpString8& command)
{
	if (!m_second_stage)
		return command.Append("AUTH CRAM-MD5\r\n");
	else
		return command.AppendFormat("%s\r\n", m_info.GetResponse().CStr());
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticateCramMD5::OnContinueReq
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticateCramMD5::OnContinueReq(const OpStringC8& text, POP3& connection)
{
	m_second_stage = TRUE;

	// Process challenge
	RETURN_IF_ERROR(connection.GetBackend().GetLoginInfo(m_info, text));

	// Send response
	return connection.SendNextCommand();
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticateApop::PrepareToSend
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticateApop::PrepareToSend(POP3& connection)
{
	return connection.GetBackend().GetLoginInfo(m_info, connection.GetTimestamp());
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticateApop::GetPopCommand
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticateApop::GetPopCommand(OpString8& command)
{
	return command.AppendFormat("APOP %s\r\n", m_info.GetResponse().CStr());
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticatePlaintext::PrepareToSend
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticatePlaintext::PrepareToSend(POP3& connection)
{
	return connection.GetBackend().GetLoginInfo(m_info);
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticatePlaintext::GetPopCommand
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticatePlaintext::GetPopCommand(OpString8& command)
{
	if (!m_second_stage)
		return command.AppendFormat("USER %s\r\n", m_info.GetUsername().CStr());
	else
		return command.AppendFormat("PASS %s\r\n", m_info.GetPassword().CStr());
}


/***********************************************************************************
 **
 **
 ** PopCommands::AuthenticatePlaintext::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::AuthenticatePlaintext::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	if (m_second_stage)
		return AuthenticationCommand::OnSuccessfulComplete(success_msg, connection);

	m_second_stage = TRUE;

	return connection.SendNextCommand();
}

#endif // M2_SUPPORT
