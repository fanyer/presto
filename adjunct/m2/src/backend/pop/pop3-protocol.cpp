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

#ifdef M2_SUPPORT

// Needed to differentiate from the IMAPParser::YYSTYPE, see DSK-221435
namespace POP3Parser {
	union YYSTYPE;
}
using POP3Parser::YYSTYPE;

#define YY_NO_UNISTD_H
#define YY_HEADER_EXPORT_START_CONDITIONS
#define YY_EXTRA_TYPE IncomingParser<YYSTYPE>*

#include "adjunct/m2/src/util/parser.h"
#include "adjunct/m2/src/backend/pop/pop3-protocol.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "adjunct/m2/src/backend/pop/pop3-parse.hpp"
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"
#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"
#include "adjunct/m2/src/backend/pop/commands/PopMiscCommands.h"
#include "adjunct/m2/src/backend/pop/commands/Capa.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/offlinelog.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "modules/locale/oplanguagemanager.h"

extern int POP3parse(yyscan_t yyscanner, POP3* connection);

/***********************************************************************************
 ** Parser helper class
 ***********************************************************************************/
class PopParser : public IncomingParser<YYSTYPE>
{
public:
	/** Constructor
	  */
	PopParser(POP3* connection) : m_connection(connection) {}

	/** Destructor
	  */
	virtual ~PopParser() { if (m_scanner) POP3lex_destroy(m_scanner); }

	/** Initialize external parser
	  */
	virtual OP_STATUS Init()
	{
		// Setup scanner
		if (POP3lex_init(&m_scanner) != 0)
		{
			m_scanner = 0;
			return OpStatus::ERR;
		}

		POP3set_extra(this, m_scanner);

		return OpStatus::OK;
	}

	/** Get next token (by using external tokenizer)
	  */
	virtual int GetNextToken(YYSTYPE* yylval) { return POP3lex(yylval, m_scanner); }

	/** Starts parsing (by using external parser)
	  */
	virtual int Parse() { return POP3parse(m_scanner, m_connection); }

private:
	yyscan_t m_scanner;
	POP3*	 m_connection;
};


/***********************************************************************************
 ** Constructor
 **
 ** POP3::POP3
 ***********************************************************************************/
POP3::POP3(PopBackend& backend)
  : m_state(DISCONNECTED)
  , m_capabilities(0)
  , m_backend(backend)
  , m_parser(0)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** POP3::~POP3
 ***********************************************************************************/
POP3::~POP3()
{
	m_command_queue.Clear();
	OP_DELETE(m_parser);
}


/***********************************************************************************
 ** Initialize the backend. Call this and check result before calling other functions
 **
 ** POP3::Init
 ***********************************************************************************/
OP_STATUS POP3::Init()
{
	// Setup parser
	m_parser = OP_NEW(PopParser, (this));
	if (!m_parser)
		return OpStatus::ERR_NO_MEMORY;

	return m_parser->Init();
}


/***********************************************************************************
 ** Connect to the server and execute pending tasks
 **
 ** POP3::Connect
 ***********************************************************************************/
OP_STATUS POP3::Connect()
{
	if (!m_backend.HasRetrievedPassword())
		return m_backend.RetrievePassword();

	if (m_state != DISCONNECTED)
		return OpStatus::OK;

	UINT16 port;
	OpString8 servername;

	// Stop existing connection
	if (m_connection)
	{
		OpStatus::Ignore(Cancel());
		OpStatus::Ignore(m_backend.Log("POP OUT: Disconnect", ""));
	}

	// Reset capabilities
	m_capabilities = 0;
	m_timestamp.Empty();
	m_parser->SetRequiredState(GREETING);

	// Get connection properties
	RETURN_IF_ERROR(m_backend.GetServername(servername));
	RETURN_IF_ERROR(m_backend.GetPort(port));

	if (servername.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	// Connect to server
	RETURN_IF_ERROR(StartLoading(servername.CStr(), "pop", port, m_backend.GetUseSSL(), m_backend.GetUseSSL(), IdleTimeout));

	// Update state
	m_state = CONNECTING;
	m_backend.GetProgress().SetCurrentAction(ProgressInfo::CONNECTING);
	m_backend.Log("POP OUT: Connecting...", "");

	return OpStatus::OK;
}


/***********************************************************************************
 ** Cancel any open connections and/or tasks
 **
 ** POP3::Cancel
 ***********************************************************************************/
OP_STATUS POP3::Cancel(BOOL notify)
{
	// Reset properties
	m_state = DISCONNECTED;
	m_command_queue.Clear();

	// Stop connection
	StopLoading();

	// Reset parser
	m_parser->Reset();

	// Update progress
	UpdateProgress();
	if (notify)
		MessageEngine::GetInstance()->GetMasterProgress().NotifyReceived(m_backend.GetAccountPtr());

	return OpStatus::OK;
}


/***********************************************************************************
 ** Fetch a message by UIDL
 **
 ** POP3::FetchMessage
 ** @param uidl UIDL of message to fetch
 ***********************************************************************************/
OP_STATUS POP3::FetchMessage(const OpStringC8& uidl)
{
	OpAutoPtr<OpString8> new_string (OP_NEW(OpString8, ()));
	if (!new_string.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_string->Set(uidl));
	if (OpStatus::IsSuccess(m_uidl_to_fetch.Add(new_string->CStr(), new_string.get())))
		new_string.release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove a message by UIDL
 **
 ** POP3::RemoveMessage
 ** @param uidl UIDL of message to remove
 ***********************************************************************************/
OP_STATUS POP3::RemoveMessage(const OpStringC8& uidl)
{
	if (uidl.IsEmpty())
		return OpStatus::OK;

	OpAutoPtr<OpString8> new_string (OP_NEW(OpString8, ()));
	if (!new_string.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_string->Set(uidl));
	RETURN_IF_ERROR(m_uidl_to_remove.Add(new_string->CStr(), new_string.get()));
	new_string.release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Write pending tasks to offline log
 **
 ** POP3::WriteToOfflineLog
 ** @param offline_log Log to write to
 ***********************************************************************************/
OP_STATUS POP3::WriteToOfflineLog(OfflineLog& offline_log)
{
	for (String8HashIterator<OpString8> it(m_uidl_to_remove); it; it++)
	{
		RETURN_IF_ERROR(offline_log.RemoveMessage(it.GetKey()));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Greeting has been received
 **
 ** POP3::OnGreeting
 ***********************************************************************************/
OP_STATUS POP3::OnGreeting()
{
	// We received the greeting, start sending commands now; we start of with capability
	RETURN_IF_ERROR(AddCommand(OP_NEW(PopCommands::Capa, ())));
	return SendNextCommand();
}


/***********************************************************************************
 ** Current command was successful
 **
 ** POP3::OnSuccess
 ***********************************************************************************/
OP_STATUS POP3::OnSuccess(const OpStringC8& success_msg)
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());
	if (!command)
		return OpStatus::ERR_NULL_POINTER;

	// If this is a multiline command, we wait for OnComplete()
	if (command->HasMultilineResponse())
		return OpStatus::OK;

	return command->OnSuccessfulComplete(success_msg, *this);
}


/***********************************************************************************
 ** Current command was not successful
 **
 ** POP3::OnError
 ***********************************************************************************/
OP_STATUS POP3::OnError(const OpStringC8& error_msg)
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());
	if (!command)
	{
		OpStatus::Ignore(m_backend.Log("POP OUT: Disconnect", ""));
		return Cancel();
	}

	return command->OnFailedComplete(error_msg, *this);
}


/***********************************************************************************
 ** Current multi-line command was completed
 **
 ** POP3::OnMultiLineComplete
 ***********************************************************************************/
OP_STATUS POP3::OnMultiLineComplete()
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());
	if (!command)
		return OpStatus::ERR_NULL_POINTER;

	if (!command->HasMultilineResponse())
		return OpStatus::ERR;

	return command->OnSuccessfulComplete(0, *this);
}


/***********************************************************************************
 ** Received a continuation request
 **
 ** POP3::OnContinueReq
 ***********************************************************************************/
OP_STATUS POP3::OnContinueReq(const OpStringC8& text)
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());
	if (!command)
		return OpStatus::ERR_NULL_POINTER;

	return command->OnContinueReq(text, *this);
}


/***********************************************************************************
 ** Received a UIDL
 **
 ** POP3::OnUIDL
 ***********************************************************************************/
OP_STATUS POP3::OnUIDL(int server_id, const OpStringC8& uidl)
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());
	if (!command)
		return OpStatus::ERR_NULL_POINTER;

	return command->OnUIDL(server_id, uidl, *this);
}


/***********************************************************************************
 ** Received a LIST response
 **
 ** POP3::OnList
 ***********************************************************************************/
OP_STATUS POP3::OnList(int server_id, int size)
{
	PopCommand* command = static_cast<PopCommand*>(m_command_queue.First());

	while (command)
	{
		RETURN_IF_ERROR(command->OnList(server_id, size, *this));
		command = static_cast<PopCommand*>(command->Suc());
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Got a parse error
 **
 ** POP3::OnParseError
 ***********************************************************************************/
void POP3::OnParseError(const OpStringC8& error)
{
	m_backend.LogFormattedText("Parse error: %s", error.CStr() ? error.CStr() : "?");
}


/***********************************************************************************
 ** Upgrade connection to TLS
 **
 ** POP3::UpgradeToTLS
 ***********************************************************************************/
OP_STATUS POP3::UpgradeToTLS()
{
	if (!StartTLSConnection())
	{
		OpStatus::Ignore(m_backend.Log("POP OUT: Disconnect", ""));
		Cancel();

		OpString errormsg;
		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_MAIL_TLS_NEGOTIATION_FAILED, errormsg)))
			m_backend.OnError(errormsg);

		return OpStatus::ERR;
	}
	else
	{
		m_state = SECURE;
		return OpStatus::OK;
	}
}


/***********************************************************************************
 ** Remove the current command and send the next command
 **
 ** POP3::ProceedToNextCommand
 ***********************************************************************************/
OP_STATUS POP3::ProceedToNextCommand()
{
	Link* item = m_command_queue.First();
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	// Remove first item from queue
	item->Out();
	OP_DELETE(item);

	// Send the next command
	return SendNextCommand();
}


/***********************************************************************************
 ** Add a command to the end of the command queue
 **
 ** POP3::AddCommand
 ** @param command Command to add
 ***********************************************************************************/
OP_STATUS POP3::AddCommand(PopCommand* command)
{
	if (!command)
		return OpStatus::ERR_NO_MEMORY;

	command->Into(&m_command_queue);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get the buffer used by the parser
 **
 ** POP3::GetParseBuffer
 ***********************************************************************************/
StreamBuffer<char>* POP3::GetParseBuffer()
{
	if (m_parser)
		return &m_parser->GetTempBuffer();

	return 0;
}


/***********************************************************************************
 ** UIDL has been handled (message fetched or removed)
 **
 ** POP3::OnUIDLHandled
 ** @param uidl UIDL that has been handled
 ***********************************************************************************/
OP_STATUS POP3::OnUIDLHandled(const OpStringC8& uidl)
{
	if (uidl.IsEmpty())
		return OpStatus::OK;

	// Remove uidl from our todo-lists
	OpString8* removed_uidl;

	if (OpStatus::IsSuccess(m_uidl_to_remove.Remove(uidl.CStr(), &removed_uidl)))
		OP_DELETE(removed_uidl);
	if (OpStatus::IsSuccess(m_uidl_to_fetch.Remove(uidl.CStr(), &removed_uidl)))
		OP_DELETE(removed_uidl);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Called when data is received over the connection
 **
 ** POP3::ProcessReceivedData
 ***********************************************************************************/
OP_STATUS POP3::ProcessReceivedData()
{
	OP_STATUS ret = OpStatus::OK;
	char* buf;
	unsigned buf_length;

	// We got data, therefore we are connected
	m_backend.GetProgress().SetConnected(TRUE);
	m_state = max(m_state, m_backend.GetUseSSL() ? SECURE : CONNECTED);

	// Read incoming data
	ret = ReadDataIntoBuffer(buf, buf_length);

	// Log the string
	m_backend.LogFormattedText("POP IN: %s", buf);

	// Add the string to the parser
	if (OpStatus::IsSuccess(ret))
		ret = m_parser->AddParseItem(buf, buf_length);

	// Parse as much as possible
	if (OpStatus::IsSuccess(ret))
		ret = m_parser->Process();

	// Update progress
	UpdateProgress();

	return ret;
}


/***********************************************************************************
 ** Takes care of the messages MSG_COMM_LOADING_FINISHED and
 ** MSG_COMM_LOADING_FAILED. Is called from the comm system and
 ** also from a function in this class. MSG_COMM_LOADING_FINISHED rc == OpStatus::OK
 ** MSG_COMM_LOADING_FAILED rc == OpStatus::ERR;
 **
 ** POP3::OnClose
 ***********************************************************************************/
void POP3::OnClose(OP_STATUS rc)
{
	if (OpStatus::IsError(rc))
	{
		// don't notify, we'll throw an error message here:
		OpStatus::Ignore(m_backend.Log("POP OUT: Disconnect", ""));
		Cancel(FALSE);
		OpString errormsg;
		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_MAIL_POP_CONNECTION_FAILED, errormsg)))
			m_backend.OnError(errormsg);
	}
	else
	{
		OpStatus::Ignore(m_backend.Log("POP IN: Disconnect", ""));
		Cancel();
	}
}


/***********************************************************************************
 ** Connection requests restart
 **
 ** POP3::OnRestartRequested
 ***********************************************************************************/
void POP3::OnRestartRequested()
{
	OpStatus::Ignore(m_backend.Log("POP OUT: Disconnect", ""));
	Cancel(FALSE);
	Connect();
}


/***********************************************************************************
 ** Send the next command in the queue
 **
 ** POP3::SendNextCommand
 ***********************************************************************************/
OP_STATUS POP3::SendNextCommand()
{
	// Send a QUIT command if we're finished
	if (!m_command_queue.First())
		RETURN_IF_ERROR(AddCommand(OP_NEW(PopCommands::Quit, ())));

	// Get the command to execute
	PopCommand* next_command = static_cast<PopCommand*>(m_command_queue.First());
	if (!next_command)
		return OpStatus::ERR;

	// Prepare to send
	RETURN_IF_ERROR(next_command->PrepareToSend(*this));
	if (next_command->RequiresState())
		m_parser->SetRequiredState(next_command->RequiresState());

	// Prepare the command string
	OpString8 command_string;
	RETURN_IF_ERROR(next_command->GetPopCommand(command_string));

	// Log and send command string
	m_backend.LogFormattedText("POP OUT: %s", next_command->UsesPassword() ? "[command contains password]" : command_string.CStr());
	SendData(command_string);

	// Wipe string for safety if necessary
	if (next_command->UsesPassword())
		command_string.Wipe();

	// Update progress
	UpdateProgress();

	return OpStatus::OK;
}


/***********************************************************************************
** Update the progress
**
** POP3::UpdateProgress
***********************************************************************************/
OP_STATUS POP3::UpdateProgress()
{
	PopCommand* current_command = static_cast<PopCommand*>(m_command_queue.First());

	// If there are no commands, we're done
	if (!current_command)
		return m_backend.GetProgress().EndCurrentAction(TRUE);

	// Set progress information
	RETURN_IF_ERROR(m_backend.GetProgress().SetCurrentAction(current_command->GetProgressAction(), current_command->GetProgressTotalCount()));
	m_backend.GetProgress().SetCurrentActionProgress(current_command->GetProgressCount(), current_command->GetProgressTotalCount());
	m_backend.GetProgress().SetSubProgress(GetParseBuffer()->GetFilled(), GetParseBuffer()->GetCapacity());

	return OpStatus::OK;
}


#endif // M2_SUPPORT
