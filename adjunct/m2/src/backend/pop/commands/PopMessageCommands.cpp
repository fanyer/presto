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

#include "adjunct/m2/src/backend/pop/commands/PopMessageCommands.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"

#define YY_HEADER_EXPORT_START_CONDITIONS
#define YYSTYPE void
#define YY_NO_UNISTD_H
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"


/////////////////////////////////////////////////////////////////////////////////////
// PopCommands::MessageCommand                                                                            //
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 ** Constructor
 **
 ** PopCommands::MessageCommand::MessageCommand
 ***********************************************************************************/
PopCommands::MessageCommand::MessageCommand()
  : m_current_message(0)
{
}


/***********************************************************************************
 **
 **
 ** PopCommands::MessageCommand::GetPopCommand
 ***********************************************************************************/
OP_STATUS PopCommands::MessageCommand::GetPopCommand(OpString8& command)
{
	if (m_current_message >= m_messages_to_do.GetCount())
		return OpStatus::ERR;

	PopMessage* message = m_messages_to_do.GetByIndex(m_current_message);

	return command.AppendFormat(GetCommand(), message->server_id);
}


/***********************************************************************************
 **
 **
 ** PopCommands::MessageCommand::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::MessageCommand::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	// Bookkeeping
	m_current_message++;
	if (m_current_message >= m_messages_to_do.GetCount())
		return PopCommand::OnSuccessfulComplete(success_msg, connection);

	return connection.SendNextCommand();
}


/***********************************************************************************
 ** Add a message to retrieve to this command
 **
 ** PopCommands::MessageCommand::OnList
 ** @param server_id server ID of message to retrieve
 ** @param uidl UIDL of message to retrieve
 ***********************************************************************************/
OP_STATUS PopCommands::MessageCommand::AddMessage(int server_id, const OpStringC8& uidl)
{
	OpAutoPtr<PopMessage> new_message(OP_NEW(PopMessage, (server_id)));
	if (!new_message.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_message->uidl.Set(uidl));
	RETURN_IF_ERROR(m_messages_to_do.Insert(new_message.get()));
	new_message.release();

	return OpStatus::OK;
}


/////////////////////////////////////////////////////////////////////////////////////
// PopCommands::Retr                                                                            //
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** PopCommands::Retr::RequiresState
 ***********************************************************************************/
int PopCommands::Retr::RequiresState() const
{
	return RETR;
}


/***********************************************************************************
 **
 **
 ** PopCommands::Retr::PrepareToSend
 ***********************************************************************************/
OP_STATUS PopCommands::Retr::PrepareToSend(POP3& connection)
{
	if (m_current_message >= m_messages_to_do.GetCount())
		return OpStatus::ERR;

	// Reserve enough space in the buffer
	connection.GetParseBuffer()->Reset();
	return connection.GetParseBuffer()->Reserve(m_messages_to_do.GetByIndex(m_current_message)->size);
}


/***********************************************************************************
 **
 **
 ** PopCommands::Retr::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Retr::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	// Fetched a message! Create the message in the system
	RETURN_IF_ERROR(CreateMessage(FALSE, connection));

	return MessageCommand::OnSuccessfulComplete(success_msg, connection);
}


/***********************************************************************************
 **
 **
 ** PopCommands::Retr::OnFailedComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Retr::OnFailedComplete(const OpStringC8& error_msg, POP3& connection)
{
	return MessageCommand::OnSuccessfulComplete(error_msg, connection);
}


/***********************************************************************************
 **
 **
 ** PopCommands::Retr::OnList
 ***********************************************************************************/
OP_STATUS PopCommands::Retr::OnList(int server_id, int size, POP3& connection)
{
	// See if we have this message in our list
	PopMessage key(server_id);

	PopMessage* message = m_messages_to_do.FindSimilar(&key);
	if (message)
		message->size = size;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Creates a message from the message currently in buffer
 **
 ** PopCommands::Retr::CreateMessage
 ** @param can_be_partial Whether this might have been a partial fetch
 ***********************************************************************************/
OP_STATUS PopCommands::Retr::CreateMessage(BOOL can_be_partial, POP3& connection)
{
	PopMessage* pop_message = m_messages_to_do.GetByIndex(m_current_message);
	size_t      msg_length  = connection.GetParseBuffer()->GetFilled();
	Message     message;

	// Check if we already had this message
	message_gid_t m2_id = connection.GetBackend().GetUIDLManager().GetUIDL(pop_message->uidl);

	// Change content of message
	if (m2_id)
		RETURN_IF_ERROR(connection.GetBackend().GetMessageDatabase().GetMessage(m2_id, FALSE, message));
	else
		RETURN_IF_ERROR(message.Init(connection.GetBackend().GetAccountPtr()->GetAccountId()));

	message.SetMessageSize(pop_message->size);

	if (OpStatus::IsSuccess(message.SetRawMessage(NULL, FALSE, FALSE, connection.GetParseBuffer()->Release(), msg_length)))
	{
		BOOL partially_fetched = can_be_partial && msg_length < (size_t)pop_message->size;
		message.SetFlag(Message::PARTIALLY_FETCHED,partially_fetched);

		if (m2_id)
		{
			connection.GetBackend().GetMessageDatabase().UpdateMessageMetaData(message);
			connection.GetBackend().GetMessageDatabase().UpdateMessageData(message);
		}
		else
		{
			RETURN_IF_ERROR(message.SetInternetLocation(pop_message->uidl));
			RETURN_IF_ERROR(connection.GetBackend().GetAccountPtr()->Fetched(message, HasHeadersOnly(), TRUE));
			RETURN_IF_ERROR(connection.GetBackend().GetUIDLManager().AddUIDL(pop_message->uidl, message.GetId()));
		}

		RETURN_IF_ERROR(connection.GetBackend().GetUIDLManager().SetUIDLFlag(pop_message->uidl,UidlManager::PARTIALLY_FETCHED,partially_fetched));
	}

	// Make sure that the connection knows this message is handled now
	return connection.OnUIDLHandled(m_messages_to_do.GetByIndex(m_current_message)->uidl);
}


/////////////////////////////////////////////////////////////////////////////////////
// PopCommands::Top                                                                //
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** PopCommands::Top::GetPopCommand
 ***********************************************************************************/
OP_STATUS PopCommands::Top::GetPopCommand(OpString8& command)
{
	if (m_current_message >= m_messages_to_do.GetCount())
		return OpStatus::ERR;

	PopMessage* message = m_messages_to_do.GetByIndex(m_current_message);

	return command.AppendFormat("TOP %d %d\r\n", message->server_id, m_lines);
}


/***********************************************************************************
 **
 **
 ** PopCommands::Top::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Top::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	// Fetched a message! Create the message in the system
	RETURN_IF_ERROR(CreateMessage(TRUE, connection));

	return MessageCommand::OnSuccessfulComplete(success_msg, connection);
}


/////////////////////////////////////////////////////////////////////////////////////
// PopCommands::Dele                                                               //
/////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** PopCommands::Dele::PrepareToSend
 ***********************************************************************************/
OP_STATUS PopCommands::Dele::PrepareToSend(POP3& connection)
{
	if (!m_flushed)
	{
		RETURN_IF_ERROR(connection.GetBackend().GetMessageDatabase().Commit());
		m_flushed = TRUE;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** PopCommands::Dele::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Dele::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	if (m_current_message >= m_messages_to_do.GetCount())
		return OpStatus::ERR;

	PopMessage* message = m_messages_to_do.GetByIndex(m_current_message);

	RETURN_IF_ERROR(connection.GetBackend().GetUIDLManager().RemoveUIDL(message->uidl));

	return MessageCommand::OnSuccessfulComplete(success_msg, connection);
}


#endif // M2_SUPPORT
