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

#include "adjunct/m2/src/backend/pop/commands/Capa.h"
#include "adjunct/m2/src/backend/pop/commands/PopAuthenticationCommands.h"
#include "adjunct/m2/src/backend/pop/commands/PopMiscCommands.h"
#include "adjunct/m2/src/backend/pop/commands/Uidl.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "modules/locale/oplanguagemanager.h"


#define YY_HEADER_EXPORT_START_CONDITIONS
#define YYSTYPE void
#define YY_NO_UNISTD_H
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"


/***********************************************************************************
 ** State for parser
 **
 ** PopCommands::Capa::RequiresState
 ***********************************************************************************/
int PopCommands::Capa::RequiresState() const
{
	return CAPA;
}


/***********************************************************************************
 ** Action to take when command completes successfully
 **
 ** PopCommands::Capa::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Capa::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	// Create secure commands if necessary
	BOOL secure;
	RETURN_IF_ERROR(connection.GetBackend().GetUseSecureConnection(secure));

	if (secure && connection.GetState() < POP3::SECURE)
	{
		RETURN_IF_ERROR(MakeSecure(connection));
	}
	else if (connection.GetState() < POP3::AUTHENTICATED)
	{
		RETURN_IF_ERROR(CreateAuthenticationCommands(connection));
	}
	else
	{
		RETURN_IF_ERROR(CreateListingCommand(connection));
	}

	return PopCommand::OnSuccessfulComplete(success_msg, connection);
}


/***********************************************************************************
** Action to take when command fails
**
** PopCommands::Capa::OnFailedComplete
***********************************************************************************/
OP_STATUS PopCommands::Capa::OnFailedComplete(const OpStringC8& error_msg, POP3& connection)
{
	// We assume the connection can do UIDL and plaintext logins
	connection.AddCapability(POP3::CAPA_UIDL);
	connection.AddCapability(POP3::CAPA_USER);

	return OnSuccessfulComplete(error_msg, connection);
}


/***********************************************************************************
 ** Create commands needed for authentication
 **
 ** PopCommands::Capa::CreateAuthenticationCommands
 ***********************************************************************************/
OP_STATUS PopCommands::Capa::CreateAuthenticationCommands(POP3& connection)
{
	PopBackend& backend = connection.GetBackend();

	// Search for an authentication method to use
	AccountTypes::AuthenticationType auth_method = backend.GetAuthenticationMethod();

	if (auth_method == AccountTypes::AUTOSELECT)
	{
		auth_method = backend.GetCurrentAuthMethod();

		if (auth_method == AccountTypes::AUTOSELECT)
		{
			// Autoselect of authentication method, check which methods are supported
			UINT32 supported_authentication = backend.GetAuthenticationSupported();

			if (!(connection.GetCapabilities() & POP3::CAPA_CRAMMD5))
				supported_authentication &= ~(1 << AccountTypes::CRAM_MD5);
			if (!(connection.GetCapabilities() & POP3::CAPA_PLAIN))
				supported_authentication &= ~(1 << AccountTypes::PLAIN);
			if (!(connection.GetCapabilities() & POP3::CAPA_USER))
				supported_authentication &= ~(1 << AccountTypes::PLAINTEXT);
			if (connection.GetTimestamp().IsEmpty())
				supported_authentication &= ~(1 << AccountTypes::APOP);

			// Get the next in the list
			auth_method = backend.GetNextAuthenticationMethod(backend.GetCurrentAuthMethod(), supported_authentication);

			backend.SetCurrentAuthMethod(auth_method);
		}
	}

	switch (auth_method)
	{
		case AccountTypes::CRAM_MD5:
			return SetNextCommand(OP_NEW(PopCommands::AuthenticateCramMD5, ()));
		// case AccountTypes::PLAIN:
		//	return SetNextCommand(new PopCommands::AuthenticatePlain);
		case AccountTypes::APOP:
			return SetNextCommand(OP_NEW(PopCommands::AuthenticateApop, ()));
		case AccountTypes::PLAINTEXT:
		default:
			return SetNextCommand(OP_NEW(PopCommands::AuthenticatePlaintext, ()));
	}
}


/***********************************************************************************
 ** Create UIDL or LIST commands depending on capability
 **
 ** PopCommands::Capa::CreateListingCommand
 ***********************************************************************************/
OP_STATUS PopCommands::Capa::CreateListingCommand(POP3& connection)
{
	if (connection.GetCapabilities() & POP3::CAPA_UIDL)
		return SetNextCommand(OP_NEW(PopCommands::Uidl, ()));
	else
		return SetNextCommand(OP_NEW(PopCommands::List, ()));
}


/***********************************************************************************
 ** Create commands for making connection secure
 **
 ** PopCommands::Capa::MakeSecure
 ***********************************************************************************/
OP_STATUS PopCommands::Capa::MakeSecure(POP3& connection)
{
	// Check if it is possible to make this connection secure
	if (!(connection.GetCapabilities() & POP3::CAPA_STARTTLS))
	{
		OpString error_msg;

		// Display an error message about the missing capability
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IDSTR_M2_POP3_ERROR_SERVICE_NOT_AVAILABLE, error_msg));
		connection.GetBackend().OnError(error_msg);
		OpStatus::Ignore(connection.GetBackend().Log("POP OUT: Disconnect", ""));
		return connection.Cancel();
	}

	return SetNextCommand(OP_NEW(PopCommands::Stls, ()));
}

#endif // M2_SUPPORT
