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

#include "adjunct/m2/src/backend/pop/commands/Uidl.h"
#include "adjunct/m2/src/backend/pop/commands/PopMessageCommands.h"
#include "adjunct/m2/src/backend/pop/commands/PopMiscCommands.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"

#define YY_HEADER_EXPORT_START_CONDITIONS
#define YYSTYPE void
#define YY_NO_UNISTD_H
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"


/***********************************************************************************
 ** Constructor
 **
 ** PopCommands::Uidl::Uidl
 ***********************************************************************************/
PopCommands::Uidl::Uidl()
  : m_retr_command(0)
  , m_top_command(0)
  , m_dele_command(0)
  , m_list_command(0)
{
}


/***********************************************************************************
 ** Required state
 **
 ** PopCommands::Uidl::RequiresState
 ***********************************************************************************/
int PopCommands::Uidl::RequiresState() const
{
	return UIDL;
}

/***********************************************************************************
 ** Required state
 **
 ** PopCommands::Uidl::MessageExpired
 ***********************************************************************************/
BOOL PopCommands::Uidl::MessageExpired( const OpStringC8& uidl, POP3& connection )
{
	
	if (connection.GetBackend().GetAccountPtr()->IsDelayedRemoveFromServerEnabled())
	{
		time_t now = g_timecache->CurrentTime();
		time_t then = connection.GetBackend().GetUIDLManager().GetReceivedTime(uidl);
		if (then > 0 && now - then > (time_t)(connection.GetBackend().GetAccountPtr()->GetRemoveFromServerDelay()))
		{
			if (connection.GetBackend().GetAccountPtr()->IsRemoveFromServerOnlyIfCompleteMessage())
			{
				// if we don't have the complete message, don't remove from server 
				if (connection.GetBackend().GetUIDLManager().GetUIDLFlag(uidl,UidlManager::PARTIALLY_FETCHED))
					return FALSE;
			}
			if (connection.GetBackend().GetAccountPtr()->IsRemoveFromServerOnlyIfMarkedAsRead())
			{
				// if the message is not marked as read, don't remove from server
				if (!connection.GetBackend().GetUIDLManager().GetUIDLFlag(uidl,UidlManager::IS_READ))
					return FALSE;
			}

			return TRUE;
		}
	}
	return FALSE;
}

/***********************************************************************************
 ** React to text input from the server in reply to this command
 **
 ** PopCommands::Uidl::OnInput
 ***********************************************************************************/
OP_STATUS PopCommands::Uidl::OnUIDL(int server_id, const OpStringC8& uidl, POP3& connection)
{
	message_gid_t existing        = connection.GetBackend().GetUIDLManager().GetUIDL(uidl);
	BOOL          complete        = FALSE;
	BOOL          leave_on_server = connection.GetBackend().GetAccountPtr()->GetLeaveOnServer();
	BOOL          force_fetch     = connection.ShouldFetchUIDL(uidl);

	Message message;
	if (OpStatus::IsSuccess(connection.GetBackend().GetMessageDatabase().GetMessage(existing, FALSE, message)))
		complete = !message.IsFlagSet(Message::PARTIALLY_FETCHED);

	if ((complete && !leave_on_server) ||
		MessageExpired(uidl,connection)|| /* if date is older than X days */
		connection.ShouldRemoveUIDL(uidl))
	{
		// This message should be removed from the server
		RETURN_IF_ERROR(AddMessageToCommand(m_dele_command, server_id, uidl));
	}
	else if (!existing || force_fetch)
	{
		// Message should be fetched. Check if we want to fetch the full message
		BOOL fetch_full = force_fetch || !connection.GetBackend().GetAccountPtr()->GetLowBandwidthMode();

		// Remove message from server if we don't need it after this
		if (!leave_on_server && fetch_full)
			RETURN_IF_ERROR(AddMessageToCommand(m_dele_command, server_id, uidl));

		// Fetch the message
		if (fetch_full)
		{
			RETURN_IF_ERROR(AddMessageToCommand(m_retr_command, server_id, uidl));
		}
		else
		{
			RETURN_IF_ERROR(AddMessageToCommand(m_top_command, server_id, uidl));
			m_top_command->SetLinesToFetch(connection.GetBackend().GetAccountPtr()->GetFetchMaxLines());
		}

		// Run a LIST command to get the size
		if (!m_list_command)
		{
			m_list_command = OP_NEW(PopCommands::List, ());
			RETURN_IF_ERROR(SetNextCommand(m_list_command));
		}
	}

	return OpStatus::OK;
}

#endif // M2_SUPPORT
