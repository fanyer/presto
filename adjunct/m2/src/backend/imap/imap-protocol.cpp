/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/backend/imap/imap-processor.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"

#include "adjunct/m2/src/backend/imap/commands/AuthenticationCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MiscCommands.h"

#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/offlinelog.h"
#include "adjunct/m2/src/engine/store.h"

#include "adjunct/m2/src/util/authenticate.h"

#include "adjunct/desktop_util/datastructures/ZlibStream.h"
#include "modules/locale/oplanguagemanager.h"

namespace ImapConstants
{
	const time_t   ConnectionTimeout = 120;
	const unsigned IdleTimeout       = 25 * 60 * 1000;
};
using namespace ImapConstants;


/***********************************************************************************
 ** Constructor
 **
 ** IMAP4::IMAP4
 ** @param backend ImapBackend object this object has to report to
 ** @param id Necessary? ID of this connection
 **
 ***********************************************************************************/
IMAP4::IMAP4(ImapBackend* backend, INT8 id)
  : m_current_folder(NULL)
  , m_backend(backend)
  , m_id(id)
  , m_current_folder_selected(false)
  , m_processor(NULL)
  , m_state(0)
  , m_capabilities(0)
  , m_loop(NULL)
  , m_compressor(NULL)
  , m_decompressor(NULL)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** IMAP4::~IMAP4
 **
 ***********************************************************************************/
IMAP4::~IMAP4()
{
	OP_DELETE(m_processor);
	OP_DELETE(m_compressor);
	OP_DELETE(m_decompressor);

	MessageEngine::GetInstance()->GetGlueFactory()->DeleteMessageLoop(m_loop);
}


/***********************************************************************************
 ** Initialize member variables
 **
 ** IMAP4::Init
 **
 ***********************************************************************************/
OP_STATUS IMAP4::Init()
{
	// Create and initialize message loop
	m_loop = MessageEngine::GetInstance()->GetGlueFactory()->CreateMessageLoop();
	if (!m_loop)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_loop->SetTarget(this));

	// Create and initialize processor
	RETURN_IF_ERROR(InitProcessor());

	// Set listener for idle timer
	m_idle_timer.SetTimerListener(this);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Connect to the server
 **
 ** IMAP4::Connect
 **
 ***********************************************************************************/
OP_STATUS IMAP4::Connect()
{
	/* TODO: This function should really, really be in crcomm.h!!! */

	// Don't connect if the connection is locked, or if we're already connected
	if ((m_backend->GetOptions() & ImapFlags::OPTION_LOCKED) || (m_state & (ImapFlags::STATE_CONNECTING | ImapFlags::STATE_CONNECTED)))
		return OpStatus::OK;

	if (!m_backend->HasRetrievedPassword())
		return m_backend->RetrievePassword();

	UINT16 port;
	OpString8 servername;

	// Stop existing connection
	if (m_connection)
		OpStatus::Ignore(Disconnect("Reconnecting..."));

	// Reset compression
	OP_DELETE(m_compressor);
	m_compressor = NULL;
	OP_DELETE(m_decompressor);
	m_decompressor = NULL;

	// Get connection properties
	RETURN_IF_ERROR(m_backend->GetServername(servername));
	RETURN_IF_ERROR(m_backend->GetPort(port));

	if (servername.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	// Connect to server
	m_backend->LogFormattedText("IMAP#%u/%u: Connecting...", m_id, m_backend->GetConnectionCount());
	RETURN_IF_ERROR(StartLoading(servername.CStr(), "imap", port, m_backend->GetUseSSL(), m_backend->GetUseSSL(), ConnectionTimeout));

	// Update state
	m_state |= ImapFlags::STATE_CONNECTING;
	m_backend->GetProgress().SetCurrentAction(ProgressInfo::CONNECTING);

	// When using SSL, the state is secure from the start
	if (m_backend->GetUseSSL())
		m_state |= ImapFlags::STATE_SECURE;

	// Make sure we load at least the INBOX
	RETURN_IF_ERROR(m_backend->UpdateFolder("INBOX", '/', 0, TRUE));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Disconnect the connection to the server (forcefully - use Logout()
 ** for the nice way)
 **
 ** IMAP4::Disconnect
 ** @param why Reason for disconnect
 **
 ***********************************************************************************/
OP_STATUS IMAP4::Disconnect(const OpStringC8& why)
{
	if (m_state != 0)
	{
		if (m_backend)
			m_backend->LogFormattedText("IMAP#%u/%u: Disconnected: %s", m_id, m_backend->GetConnectionCount(), why.CStr());
	}

	// Reset properties
	m_current_folder = NULL;
	m_state			 = 0;
	m_capabilities	 = 0;
	m_idle_timer.Stop();
	m_backend->ResetProgress();

	// Handle commands
	CreateSaveableQueue();

	// Stop connection
	RETURN_IF_ERROR(StopLoading());

	// Reset parser
	if (m_processor)
		m_processor->Reset();

	// Reset folder sync state
	for (unsigned i = 0; i < m_own_folders.GetCount(); i++)
		m_own_folders.GetByIndex(i)->RequestFullSync();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Called when data is received, handles all processing of incoming data
 **
 ** IMAP4::ProcessReceivedData
 **
 ***********************************************************************************/
OP_STATUS IMAP4::ProcessReceivedData()
{
	OP_STATUS ret = OpStatus::OK;
	char* buf;
	unsigned buf_length;
	ProgressInfo::ProgressAction progress_group = ProgressInfo::NONE;

	// Get progress data
	if (GetSentCommand())
		progress_group = GetSentCommand()->GetProgressAction();

	// We got data, therefore we are connected
	m_state |= ImapFlags::STATE_CONNECTED;
	m_backend->GetProgress().SetConnected(TRUE);

	// Read incoming data
	ret = ReadDataIntoBuffer(buf, buf_length);

	// Add the string to the processor
	if (OpStatus::IsSuccess(ret))
	{
		RETURN_IF_ERROR(DecodeReceivedData(buf, buf_length));

		// Log the string
		m_backend->LogFormattedText("IMAP#%u/%u IN {%s}: %s",
								m_id,
								m_backend->GetConnectionCount(),
								m_current_folder ? m_current_folder->GetName().CStr() : "",
								buf);
		ret = m_processor->Process(buf, buf_length);
	}

	// Update progress
	if (progress_group != ProgressInfo::NONE)
		m_backend->UpdateProgressGroup(progress_group, 0, TRUE);

	// Send the next command
	OpStatus::Ignore(SendNextCommand());

	return ret;
}

/***********************************************************************************
 ** Does additional decoding needed on incoming data if necessary
 **
 ** IMAP4::DecodeReceivedData
 **
 ***********************************************************************************/
OP_STATUS IMAP4::DecodeReceivedData(char*& buf, unsigned& buf_length)
{
	if (!(m_state & ImapFlags::STATE_COMPRESSED))
		return OpStatus::OK;

	if (!m_decompressor)
	{
		m_decompressor = OP_NEW(DecompressZlibStream, ());
		if (!m_decompressor)
			return OpStatus::ERR_NO_MEMORY;
	}

	StreamBuffer<char> decompressed;
	RETURN_IF_ERROR(m_decompressor->AddData(buf, buf_length));
	RETURN_IF_ERROR(m_decompressor->Flush(decompressed));

	OP_DELETEA(buf);
	buf_length = decompressed.GetFilled();
	buf = decompressed.Release();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Inserts 'command' into the send queue and sends the next command if possible
 **
 ** IMAP4::InsertCommand
 ** @param command The command to insert
 ** @param at_start Whether the command should be inserted at the start of the queue
 **
 ***********************************************************************************/
OP_STATUS IMAP4::InsertCommand(ImapCommandItem* command, BOOL at_start)
{
	if (!command)
		return OpStatus::ERR_NULL_POINTER;

	if (at_start)
	{
		// If at_start is set, add (as close) to the start of the queue (as possible)
		if (m_send_queue.FirstChild() && (m_send_queue.FirstChild()->IsSent() || m_send_queue.FirstChild()->IsContinuation()))
			command->Follow(m_send_queue.FirstChild(), *this);
		else
			command->IntoStartOfQueue(&m_send_queue, *this);
	}
	else
	{
		command->IntoQueue(&m_send_queue, *this);
	}

	return DelayedSendNextCommand();
}


/***********************************************************************************
 ** Inserts 'command' into the send queue and sends the next command if possible
 **
 ** IMAP4::InsertCommand
 ** @param command The command to insert
 ** @param at_start Whether the command should be inserted at the start of the queue
 **
 ***********************************************************************************/
OP_STATUS IMAP4::InsertCommand(ImapCommands::MessageSetCommand* command, BOOL at_start)
{
	RETURN_OOM_IF_NULL(command);
	OpAutoPtr<ImapCommandItem> command_holder(command);

	// Check if we can extend an existing command
	ImapCommandItem* potential = FirstUnsent();

	while (potential && !potential->CanExtendWith(command))
		potential = potential->Suc();

	if (potential)
		return potential->ExtendWith(command, *this);

	return InsertCommand(command_holder.release(), at_start);
}


/***********************************************************************************
 ** Timer event
 **
 ** IMAP4::OnTimeOut
 **
 ***********************************************************************************/
void IMAP4::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_idle_timer)
	{
		OpStatus::Ignore(GenerateIdleCommands());
	}
}


/***********************************************************************************
 ** Signal that a message has been received
 **
 ** IMAP4::Receive(OpMessage message)
 **
 ***********************************************************************************/
OP_STATUS IMAP4::Receive(OpMessage message)
{
	switch(message)
	{
		case MSG_M2_SEND_NEXT_IMAP_COMMAND:
			return SendNextCommand();
		case MSG_M2_DISCONNECT:
			return Disconnect("Received disconnect order");
		default:
			OP_ASSERT(!"Unknown message received");
			return OpStatus::ERR;
	}
}

/***********************************************************************************
 ** Stop the connection timer to avoid time outs when it's just IDLEing
 **
 ** IMAP4::StopConnectionTimer()
 **
 ***********************************************************************************/
void IMAP4::StopConnectionTimer()
{
	if (m_connection)
		m_connection->StopConnectionTimer();
}


/***********************************************************************************
 ** Run this function when the sent command has been successfully completed
 ** Removes the command from the queue and reinserts its children
 ** Sends the next command in the queue
 **
 ** IMAP4::OnSuccessfulComplete
 ** @param tag Tag of the completed command
 **
 ***********************************************************************************/
OP_STATUS IMAP4::OnSuccessfulComplete(int tag)
{
	OP_STATUS ret;
	ImapCommandItem *sent_command = m_send_queue.FirstChild();

	// Assert that this is the command we sent - if we are getting successful
	// completes for commands that aren't sent, something really weird is going on
	if (!sent_command || !sent_command->IsSent() || tag != sent_command->GetTag())
		return OpStatus::ERR;

	// Let the sent command do its completion routines
	ret = sent_command->OnSuccessfulComplete(*this);

	// Update progress
	m_backend->UpdateProgressGroup(sent_command->GetProgressAction(), sent_command->GetProgressTotalCount(), FALSE);

	// Remove the command from the queue, but reinsert children
	RemoveCommand(sent_command, TRUE);

	// Send next command
	OpStatus::Ignore(SendNextCommand());

	return ret;
}


/***********************************************************************************
 ** Run this function when the sent command has been completed, but with errors
 ** Removes the command and its children from the queue
 ** Sends the next command in the queue
 **
 ** IMAP4::OnFailedComplete
 ** @param tag Tag of the completed command
 ** @param response_text Message from the server for failed command
 **
 ***********************************************************************************/
OP_STATUS IMAP4::OnFailedComplete(int tag, const OpStringC8& response_text)
{
	OP_STATUS ret;
	ImapCommandItem *sent_command = m_send_queue.FirstChild();

	// Assert that this is the command we sent - if we are getting successful
	// completes for commands that aren't sent, something really weird is going on
	if (!sent_command || !sent_command->IsSent() || tag != sent_command->GetTag())
		return OpStatus::ERR;

	// Let the sent command do its completion routines
	ret = sent_command->OnFailed(*this, response_text);

	// Cleanup, don't reinsert children
	RemoveCommand(sent_command, FALSE);

	// Send next command
	RETURN_IF_ERROR(SendNextCommand());

	return ret;
}


/***********************************************************************************
 ** Send a continuation of the sent command
 **
 ** IMAP4::OnContinuationRequest
 ** @param response_text Any text that was sent with the continuation request
 **
 ***********************************************************************************/
OP_STATUS IMAP4::OnContinuationRequest(const OpStringC8& response_text)
{
	// Find the appropriate continuation command
	ImapCommandItem* cont_command = m_send_queue.FirstChild();

	// Continuation command can react on response_text
	RETURN_IF_ERROR(cont_command->PrepareContinuation(*this, response_text));

	// Reset sent property for continuation
	cont_command->SetSent(FALSE);

	// If we're not waiting for another command to appear, send now
	if (!cont_command->WaitForNext() || cont_command->Suc())
		RETURN_IF_ERROR(SendCommand(cont_command));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Signal that the server is going to close the connection
 **
 ** IMAP4::OnBye
 **
 ***********************************************************************************/
OP_STATUS IMAP4::OnBye()
{
	if (m_send_queue.FirstChild() &&
		m_send_queue.FirstChild()->IsSent() &&
		m_send_queue.FirstChild()->HandleBye(*this))
	{
		RemoveCommand(m_send_queue.FirstChild(), FALSE);
		return OpStatus::OK;
	}
	else
		return OnUnexpectedDisconnect();
}


/***********************************************************************************
 ** Act on expunged message
 **
 ** IMAP4::OnExpunge
 ** @param expunged_id Message sequence number of expunged message
 **
 ***********************************************************************************/
OP_STATUS IMAP4::OnExpunge(unsigned expunged_id)
{
	// Tell all commands currently in the queue that a message has been expunged
	ImapCommandItem* command = m_send_queue.Next();

	while (command)
	{
		if (!command->IsSent())
			RETURN_IF_ERROR(command->OnExpunge(expunged_id));
		command = command->Next();
	}

	// Tell current folder about expunged message
	if (!m_current_folder)
		return OpStatus::ERR_NULL_POINTER;

	return m_current_folder->Expunge(expunged_id);
}


/***********************************************************************************
 ** Get last sent command (if available)
 **
 ** IMAP4::GetSentCommand
 ** @return Last sent command, or NULL if not available
 **
 ***********************************************************************************/
ImapCommandItem* IMAP4::GetSentCommand() const
{
	if (m_send_queue.FirstChild() && m_send_queue.FirstChild()->IsSent())
		return m_send_queue.FirstChild();

	return NULL;
}

/***********************************************************************************
 ** Set the capabilities for this connection
 ** @param capability_list List of capabilities, see ImapFlags::Capability
 ** IMAP4::SetCapabilities
 **
 ***********************************************************************************/
void IMAP4::SetCapabilities(int capability_list) 
{ 
	m_capabilities = capability_list; 
	m_state |= ImapFlags::STATE_RECEIVED_CAPS;

	if (m_backend->GetOptions() & ImapFlags::OPTION_DISABLE_QRESYNC)
		m_capabilities &= ~ImapFlags::CAP_QRESYNC;

	if (m_backend->GetOptions() & ImapFlags::OPTION_DISABLE_UIDPLUS)
		m_capabilities &= ~ImapFlags::CAP_UIDPLUS;
	
	if (m_backend->GetOptions() & ImapFlags::OPTION_DISABLE_COMPRESS)
		m_capabilities &= ~ImapFlags::CAP_COMPRESS_DEFLATE;
	
	if (m_backend->GetOptions() & ImapFlags::OPTION_DISABLE_SPECIAL_USE)
	{
		m_capabilities &= ~ImapFlags::CAP_SPECIAL_USE;
		m_capabilities &= ~ImapFlags::CAP_XLIST;
	}

	if ((m_capabilities & ImapFlags::CAP_ID) && GetBackend().HasLoggingEnabled())
		InsertCommand(OP_NEW(ImapCommands::ID, ()), TRUE);
}


/***********************************************************************************
 ** Check whether this connection is responsible for a specified mailbox
 **
 ** IMAP4::HasMailbox
 ** @param mailbox The mailbox to check
 ** @return Whether this connection is responsible for mailbox
 **
 ***********************************************************************************/
BOOL IMAP4::HasMailbox(ImapFolder* mailbox)
{
	return (m_own_folders.Find(mailbox) >= 0);
}


/***********************************************************************************
 ** Report an error to the user
 **
 ** IMAP4::ReportError
 ** @param error_message Message to show the user
 ** @param from_server Whether this was an error on the server (as opposed to an error in Opera)
 **
 ***********************************************************************************/
void IMAP4::ReportServerError(const OpStringC8& error_message) const
{
	OpString full_error_msg;

	OpStatus::Ignore(g_languageManager->GetString(Str::D_MAIL_IMAP_SERVER_REPORTED_ERROR, full_error_msg));
	OpStatus::Ignore(full_error_msg.Append("\n\n"));
	OpStatus::Ignore(full_error_msg.Append(error_message));

	m_backend->OnError(full_error_msg);
}


/***********************************************************************************
 ** Check for new messages in all folders that this connection maintains
 **
 ** IMAP4::FetchNewMessages
 **
 ***********************************************************************************/
OP_STATUS IMAP4::FetchNewMessages(BOOL force)
{
	// First, do a check in the current folder
	if (m_current_folder)
		RETURN_IF_ERROR(SyncFolder(m_current_folder));

	// Check all our other folders as well
	for (unsigned i = 0; i < m_own_folders.GetCount(); i++)
	{
		ImapFolder* folder = m_own_folders.GetByIndex(i);

		if (folder && folder != m_current_folder)
		{
			RETURN_IF_ERROR(SyncFolder(folder));
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 ** Force a full sync of all folders
 **
 ** IMAP4::RefreshAll
 **
 ***********************************************************************************/
OP_STATUS IMAP4::RefreshAll()
{
	for (unsigned i = 0; i < m_own_folders.GetCount(); i++)
	{
		ImapFolder* folder = m_own_folders.GetByIndex(i);

		if (folder && folder->IsSubscribed())
		{
			RETURN_IF_ERROR(InsertCommand(OP_NEW(ImapCommands::ForcedFullSync, (folder)), TRUE));
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 ** Synchronize a specific folder
 **
 ** IMAP4::SyncFolder
 ** @param folder Which folder to synchronize
 **
 ***********************************************************************************/
OP_STATUS IMAP4::SyncFolder(ImapFolder* folder)
{
	if (!folder || !folder->IsSubscribed())
		return OpStatus::OK;

	return InsertCommand(OP_NEW(ImapCommands::Sync, (folder)), folder->NeedsFullSync());
}


/***********************************************************************************
 ** Fetch a message by server UID
 **
 ** IMAP4::FetchMessageByUID
 ** @param folder Folder on server where message is located
 ** @param uid Server UID of the message to fetch
 ** @param force_full_message If set, always fetch the complete message, otherwise, get headers only or body according to preferences
 **
 ***********************************************************************************/
OP_STATUS IMAP4::FetchMessageByUID(ImapFolder* folder, unsigned uid, BOOL force_fetch_body, BOOL force_fetch_full_body, BOOL high_priority, BOOL force_no_bodystructure)
{
	BOOL fetch_body = force_fetch_body || m_backend->GetDownloadBodies();
	BOOL fetch_full_body = force_fetch_full_body || !m_backend->GetAccountPtr()->GetFetchOnlyText();

	if (fetch_body)
	{
		if (fetch_full_body)
			return InsertCommand(OP_NEW(ImapCommands::CompleteFetch, (folder, uid, uid, TRUE)), high_priority);
		else
			return InsertCommand(OP_NEW(ImapCommands::TextPartFetch, (folder, uid, uid, TRUE)), high_priority);
	}
	else
	{
		return InsertCommand(OP_NEW(ImapCommands::HeaderFetch, (folder, uid, uid, TRUE, !force_no_bodystructure)), high_priority);
	}
}


/***********************************************************************************
 ** Fetch messages by sequence set ID
 **
 ** IMAP4::FetchMessagesByID
 ** @param folder Folder on server where message is located
 ** @param id_from Start of the set to fetch (inclusive)
 ** @param id_to End of the set to fetch (inclusive)
 ** @param force_full_message If set, always fetch the complete message, otherwise, get headers only or body according to preferences
 **
 ***********************************************************************************/
OP_STATUS IMAP4::FetchMessagesByID(ImapFolder* folder, unsigned id_from, unsigned id_to)
{
	if (m_backend->GetDownloadBodies())
	{
		if (!m_backend->GetAccountPtr()->GetFetchOnlyText())
			return InsertCommand(OP_NEW(ImapCommands::CompleteFetch, (folder, id_from, id_to, FALSE)));
		else
			return InsertCommand(OP_NEW(ImapCommands::TextPartFetch, (folder, id_from, id_to, FALSE)));
	}
	else
	{
		return InsertCommand(OP_NEW(ImapCommands::HeaderFetch, (folder, id_from, id_to, FALSE, TRUE)));
	}
}


/***********************************************************************************
 ** Check if there is an operation in the queue that affects a certain message
 **
 ** IMAP4::IsOperationInQueue
 ** @param folder Folder on server where message is located
 ** @param uid Server UID of the message to check
 ** @return Whether there is an operation waiting in the queue that will change this message
 **
 ***********************************************************************************/
BOOL IMAP4::IsOperationInQueue(ImapFolder* folder, unsigned uid) const
{
	ImapCommandItem* item = m_send_queue.FirstChild();

	while (item && (item->IsSent() || !item->Changes(folder, uid)))
	{
		item = item->Suc();
	}

	return item != 0;
}


/***********************************************************************************
 ** Make this connection responsible for a folder
 **
 ** IMAP4::AddFolder
 ** @param folder Folder that this connection will have to maintain
 **
 ***********************************************************************************/
OP_STATUS IMAP4::AddFolder(ImapFolder* folder)
{
	if (HasMailbox(folder))
		return OpStatus::OK;

	RETURN_IF_ERROR(m_own_folders.Insert(folder));

	// This folder was already scheduled for synchronization, make sure it happens on
	// this connection too
	if (folder->IsScheduledForSync())
		return SyncFolder(folder);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove a folder from this connection's list
 **
 ** IMAP4::RemoveFolder
 ** @param folder Folder that this connection should stop maintaining
 **
 ***********************************************************************************/
void IMAP4::RemoveFolder(ImapFolder* folder)
{
	// Check the queue for first-level commands that use this folder
	ImapCommandItem* command = m_send_queue.FirstChild();

	while (command)
	{
		ImapCommandItem* next = command->Suc();

		if (!command->IsSent() && !command->IsContinuation() && command->GetMailbox() == folder)
			RemoveCommand(command, FALSE);

		command = next;
	}

	// Remove the folder from our list
	m_own_folders.Remove(folder);
}


/***********************************************************************************
 ** Take over the connection and all details from another connection
 **
 ** IMAP4::TakeOverCompletely
 **  @param other_connection The connection to take over
 **
 ***********************************************************************************/
OP_STATUS IMAP4::TakeOverCompletely(IMAP4& other_connection)
{
	// Takeover folders
	for (unsigned i = 0; i < other_connection.m_own_folders.GetCount(); i++)
	{
		RETURN_IF_ERROR(m_own_folders.Insert(other_connection.m_own_folders.GetByIndex(i)));
	}
	other_connection.m_own_folders.Clear();

	// Takeover commands
	while (ImapCommandItem* item = other_connection.m_send_queue.FirstChild())
	{
		item->Out(other_connection);

		// Only takeover non-simple commands
		if (item->IsSaveable())
			item->IntoQueue(&m_send_queue, *this);
		else
			OP_DELETE(item);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Write commands in this connection's queue to the offline log
 **
 ** IMAP4::WriteToOfflineLog
 ** @param offline_log Offline log to write to
 **
 ***********************************************************************************/
OP_STATUS IMAP4::WriteToOfflineLog(OfflineLog& offline_log)
{
	// Write all first-level commands
	ImapCommandItem* command = m_send_queue.FirstChild();

	while (command)
	{
		RETURN_IF_ERROR(command->WriteToOfflineLog(offline_log));
		command = command->Next();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Generate a new tag for a command sent over this connection
 **
 ** IMAP4::GenerateNewTag
 ** @param new_tag Where to save the tag
 **
 ***********************************************************************************/
OP_STATUS IMAP4::GenerateNewTag(OpString8& new_tag)
{
	return m_backend->GenerateNewTag(new_tag);
}


/***********************************************************************************
 ** Start a secure TLS connection with the current server
 **
 ** IMAP4::UpgradeToTLS
 **
 ***********************************************************************************/
OP_STATUS IMAP4::UpgradeToTLS()
{
	if (!StartTLSConnection())
	{
		OpString errormsg;

		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_MAIL_TLS_NEGOTIATION_FAILED, errormsg)))
			m_backend->OnError(errormsg);

		return OpStatus::ERR;
	}
	else
	{
		// We're now secured, force a retry for capabilities
		RemoveState(ImapFlags::STATE_RECEIVED_CAPS);
		AddState(ImapFlags::STATE_SECURE);
		return OpStatus::OK;
	}
}


/***********************************************************************************
 ** Start a timer that will regenerate an IDLE event when it times out
 **
 ** IMAP4::StartIdleTimer
 **
 ***********************************************************************************/
OP_STATUS IMAP4::StartIdleTimer()
{
	m_idle_timer.Start(IdleTimeout);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Stop the idle timer
 **
 ** IMAP4::StopIdleTimer
 **
 ***********************************************************************************/
OP_STATUS IMAP4::StopIdleTimer()
{
	m_idle_timer.Stop();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add a folder to the subscribed folder list
 **
 ** IMAP4::AddToSubscribedFolderList
 **
 ***********************************************************************************/
OP_STATUS IMAP4::AddToSubscribedFolderList(const OpStringC8& folder_name)
{
	ImapFolder* folder;

	RETURN_IF_ERROR(m_backend->GetFolderByName(folder_name, folder));
	if (folder->GetIndex())
		return m_subscribed_folder_list.Insert(folder->GetIndex()->GetId());
	else
		return OpStatus::OK;
}


/***********************************************************************************
 ** Sync the actual folder list with m_subscribed_folder_list, call after successful
 ** LSUB
 **
 ** IMAP4::ProcessSubscribedFolderList
 **
 ***********************************************************************************/
OP_STATUS IMAP4::ProcessSubscribedFolderList()
{
	// Walk through indexes, try to find indexes for this account
	Indexer*      indexer = MessageEngine::GetInstance()->GetIndexer();
	OpINT32Vector indexes;

	RETURN_IF_ERROR(indexer->GetChildren(IndexTypes::FIRST_ACCOUNT + m_backend->GetAccountPtr()->GetAccountId(), indexes));

	for (unsigned i = 0; i < indexes.GetCount(); i++)
	{
		// Check if this index is in the list of subscribed folders
		if (m_subscribed_folder_list.Contains(indexes.Get(i)))
			continue;

		// Try to find an ImapFolder for this index
		ImapFolder*			  folder = NULL;
		OpAutoPtr<ImapFolder> folder_ptr;
		Index*				  current		= indexer->GetIndexById(indexes.Get(i));

		if (!current)
			continue;

		IndexSearch* search = current->GetSearch();
		if (!search ||
			OpStatus::IsError(m_backend->GetFolderByName(search->GetSearchText(), folder)))
		{
			// ImapFolder not found, create a dummy folder
			folder = OP_NEW(ImapFolder, (*m_backend, 0, 0, 0, 0, current->GetId()));
			if (!folder)
				return OpStatus::ERR_NO_MEMORY;

			folder_ptr = folder;
		}

		// Don't remove INBOX, which doesn't have to be in the list of subscribed folders
		if (!search->GetSearchText().CompareI("INBOX"))
			continue;

		// Remove the folder and its contents
		if (m_current_folder == folder)
			m_current_folder = NULL;
		RETURN_IF_ERROR(folder->SetSubscribed(FALSE));
		RemoveFolder(folder);
	}

	ResetSubscribedFolderList();

	// If there are no more folders for this connection, we can now disconnect
	if (!GetFirstSelectableFolder())
		return RequestDisconnect();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Handles a connection closed by the server
 **
 ** IMAP4::OnClose
 ** @param rc More information about the disconnect
 **
 ***********************************************************************************/
void IMAP4::OnClose(OP_STATUS rc)
{
	if (m_state != 0)
	{
		// This was an unexpected disconnect
		OpStatus::Ignore(OnUnexpectedDisconnect());
	}
}


/***********************************************************************************
 ** Connection requests to be restarted
 **
 ** IMAP4::OnRestartRequested
 **
 ***********************************************************************************/
void IMAP4::OnRestartRequested()
{
	OpStatus::Ignore(Disconnect("Restarting connection..."));
	OpStatus::Ignore(Connect());
}


/***********************************************************************************
 ** Removes 'command' from the send queue
 **
 ** IMAP4::RemoveCommand
 ** @param command The command to remove
 ** @param reinsert_children Whether to reinsert the children of this command into the queue
 **
 ***********************************************************************************/
void IMAP4::RemoveCommand(ImapCommandItem* command, BOOL reinsert_children)
{
	// Remove the command from the queue
	command->Out(*this);

	if (reinsert_children)
	{
		// Find last child
		ImapCommandItem *child = command->LastChild();

		// Reinsert children
		while(child)
		{
			ImapCommandItem* next_child = child->Prev();
			child->Out(*this);
			InsertCommand(child, TRUE);
			child = next_child;
		}
	}

	// Cleanup command and remaining children
	OP_DELETE(command);
}



/***********************************************************************************
 ** If it exists, sends the next available unsent command in the queue
 **
 ** IMAP4::DelayedSendNextCommand
 **
 ***********************************************************************************/
OP_STATUS IMAP4::DelayedSendNextCommand()
{
	if (m_loop)
		return m_loop->Post(MSG_M2_SEND_NEXT_IMAP_COMMAND);
	else
		return OpStatus::ERR_NULL_POINTER;
}


/***********************************************************************************
 ** If it exists, sends the next available unsent command in the queue
 **
 ** IMAP4::SendNextCommand
 **
 ***********************************************************************************/
OP_STATUS IMAP4::SendNextCommand()
{
	// Expand command if necessary
	while (m_send_queue.FirstChild() && m_send_queue.FirstChild()->IsMetaCommand(*this))
		RETURN_IF_ERROR(ReplaceFirstCommand(m_send_queue.FirstChild()->GetExpandedQueue(*this)));

	// command_to_send is the command we have to send now - but we don't need to send if there's no real_next_command
	ImapCommandItem* command_to_send   = m_send_queue.FirstChild();
	ImapCommandItem* real_next_command = command_to_send && command_to_send->WaitForNext() ? command_to_send->Suc() : command_to_send;

	// Check if we have anything to send
	if (!real_next_command)
	{
		if (!command_to_send)
			RETURN_IF_ERROR(GenerateIdleCommands());
		m_backend->ResetProgress();
		return OpStatus::OK;
	}

	// Bail out if the message is already sent
	if (command_to_send->IsSent())
		return OpStatus::OK;

	// Remove unnecessary commands
	if (real_next_command->IsUnnecessary(*this))
	{
		RemoveCommand(real_next_command, TRUE);
		return SendNextCommand();
	}

	// Setup state, only continue if we are in the right state
	BOOL correct_state;
	RETURN_IF_ERROR(SetupStateForCommand(command_to_send, correct_state));
	if (!correct_state)
		return OpStatus::OK;

	// Send command
	return SendCommand(command_to_send);
}


/***********************************************************************************
 ** Send a command
 **
 ** IMAP4::SendCommand
 ** @param command_to_send The command to send
 **
 ***********************************************************************************/
OP_STATUS IMAP4::SendCommand(ImapCommandItem* command_to_send)
{
	// Don't do anything if we're in a locked state
	if (m_backend->GetOptions() & ImapFlags::OPTION_LOCKED)
		return OpStatus::OK;

	// Allow command to prepare itself
	RETURN_IF_ERROR(command_to_send->PrepareToSend(*this));

	// Get command string to send
	OpString8 command_string;
	RETURN_IF_ERROR(command_to_send->GetImapCommand(command_string, *this));

	// Log and send command string
	m_backend->LogFormattedText("IMAP#%u/%u OUT {%s}: %s",
								m_id,
								m_backend->GetConnectionCount(),
								m_current_folder ? m_current_folder->GetName().CStr() : "",
								command_to_send->UsesPassword() ? "[command contains password]" : command_string.CStr());

	RETURN_IF_ERROR(EncodeAndSendData(command_string));

	command_to_send->SetSent();

	// Wipe string for safety if necessary
	if (command_to_send->UsesPassword())
		command_string.Wipe();

	// Update progress
	m_backend->UpdateProgressGroup(command_to_send->GetProgressAction(), 0, TRUE);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Encode data in string if necessary and send it to the server
 **
 ** IMAP4::EncodeAndSendData
 ***********************************************************************************/
OP_STATUS IMAP4::EncodeAndSendData(OpString8& command_string)
{
	if (!(m_state & ImapFlags::STATE_COMPRESSED))
		return SendData(command_string);

	if (!m_compressor)
	{
		m_compressor = OP_NEW(CompressZlibStream, ());
		if (!m_compressor)
			return OpStatus::ERR_NO_MEMORY;
	}

	StreamBuffer<char> compressed;
	RETURN_IF_ERROR(m_compressor->AddData(command_string.CStr(), command_string.Length()));
	RETURN_IF_ERROR(m_compressor->Flush(compressed));

	size_t length = compressed.GetFilled();

	return SendData(compressed.Release(), length);
}


/***********************************************************************************
 ** Replace the first command in the queue
 **
 ** IMAP4::ReplaceFirstCommand
 ** @param replace_with Queue or command to replace the first command with
 ***********************************************************************************/
OP_STATUS IMAP4::ReplaceFirstCommand(ImapCommandItem* replace_with)
{
	ImapCommandItem* first_command = m_send_queue.FirstChild();

	if (!first_command)
		return OpStatus::ERR;

	// Make sure replacement will happen by placing replacement as a child of first_command
	if (replace_with)
		replace_with->DependsOn(first_command, *this);

	// Take out the first command, reinsert children
	RemoveCommand(first_command, TRUE);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Generates commands that should be executed when there is nothing to do
 **
 ** IMAP4::GenerateIdleCommands
 **
 ***********************************************************************************/
OP_STATUS IMAP4::GenerateIdleCommands()
{
	// If we're not currently in the selected folder, do it now
	if (!m_backend->GetSelectedFolder() && !m_current_folder && GetFirstSelectableFolder())
		return InsertCommand(OP_NEW(ImapCommands::Select, (GetFirstSelectableFolder())));
	else if (HasMailbox(m_backend->GetSelectedFolder()) && m_current_folder != m_backend->GetSelectedFolder() && m_backend->GetSelectedFolder()->IsSelectable())
		return InsertCommand(OP_NEW(ImapCommands::Select, (m_backend->GetSelectedFolder())));

	// Go into IDLE-mode if possible
	if (m_current_folder && (m_capabilities & ImapFlags::CAP_IDLE) && !(m_backend->GetOptions() & ImapFlags::OPTION_DISABLE_IDLE))
		return InsertCommand(OP_NEW(ImapCommands::Idle, ()));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Removes items from the queue that are not saveable
 **
 ** IMAP4::CreateSaveableQueue
 **
 ***********************************************************************************/
void IMAP4::CreateSaveableQueue()
{
	ImapCommandItem* item = m_send_queue.FirstChild();

	// Loop through all first-level items
	while (item)
	{
		ImapCommandItem* next_item = item->Suc();

		if (!item->IsSaveable())
		{
			// This item is not saveable, remove from queue
			item->Out(*this);
			OP_DELETE(item);
		}
		else
		{
			// This item is saveable - make it and its children saveable
			while (item)
			{
				item->MakeSaveable();
				item = item->Next();
			}
		}

		item = next_item;
	}
}


OP_STATUS IMAP4::OnUnexpectedDisconnect()
{
	// Check if sent command should be removed
	if (m_backend->GetConnectRetries() == m_backend->GetMaxConnectRetries() &&
		m_send_queue.FirstChild() &&
		m_send_queue.FirstChild()->IsSent() &&
		m_send_queue.FirstChild()->RemoveIfCausesDisconnect(*this))
	{
		// Cleanup, don't reinsert children
		RemoveCommand(m_send_queue.FirstChild(), FALSE);
	}

	OpStatus::Ignore(Disconnect("Connection closed by server, unexpected disconnect"));
	return m_backend->OnUnexpectedDisconnect(*this);
}


/***********************************************************************************
 ** Sets up a state for this connection so that command can be sent
 **
 ** IMAP4::SetupStateForCommand
 ** @param command The command for which to setup a state
 ** @param succeeded Set to TRUE if and only if we are now in a correct state to execute this command
 **
 ***********************************************************************************/
OP_STATUS IMAP4::SetupStateForCommand(ImapCommandItem* command, BOOL& succeeded)
{
	OP_ASSERT(command);

	BOOL secure;

	RETURN_IF_ERROR(m_backend->GetUseSecureConnection(secure));

	int needed_state = command->NeedsState(secure, *this);
	int todo_state = (needed_state ^ m_state) & needed_state;

	// Check states that we haven't reached
	if (todo_state)
	{
		// This is an if/else-structure because the states can only be reached one at a time, in this order
		// e.g. you can only reach STATE_AUTHENTICATED if you're already in STATE_CONNECTED.
		if (todo_state & ImapFlags::STATE_CONNECTED)
			RETURN_IF_ERROR(Connect());
		else if (todo_state & ImapFlags::STATE_RECEIVED_CAPS)
			RETURN_IF_ERROR(InsertCommand(OP_NEW(ImapCommands::Capability, ()), TRUE));
		else if (todo_state & ImapFlags::STATE_SECURE)
			RETURN_IF_ERROR(MakeSecure());
		else if (todo_state & ImapFlags::STATE_AUTHENTICATED)
			RETURN_IF_ERROR(Authenticate());
		else if (todo_state & ImapFlags::STATE_ENABLED_QRESYNC)
			RETURN_IF_ERROR(InsertCommand(OP_NEW(ImapCommands::EnableQResync, ()), TRUE));
		else if (todo_state & ImapFlags::STATE_COMPRESSED)
			RETURN_IF_ERROR(InsertCommand(OP_NEW(ImapCommands::Compress, ()), TRUE));
	}

	succeeded = (m_state & needed_state) == needed_state;
	if (!succeeded)
		return OpStatus::OK;

	// Check if this command needs certain selected or unselected mailboxes
	if (command->NeedsSelectedMailbox() && command->NeedsSelectedMailbox() != m_current_folder)
	{
		succeeded = FALSE;
		ImapCommandItem * select = OP_NEW(ImapCommands::Select, (command->NeedsSelectedMailbox()));
		if (!select)
			return OpStatus::ERR_NO_MEMORY;
		command->DependsOn(select, *this);

		return InsertCommand(select, TRUE);
	}
	else if (command->NeedsUnselectedMailbox() && command->NeedsUnselectedMailbox() == m_current_folder)
	{
		succeeded = FALSE;
		return InsertCommand(OP_NEW(ImapCommands::Unselect, (command->NeedsUnselectedMailbox())), TRUE);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Try to make the connection secure
 **
 ** IMAP4::MakeSecure()
 **
 ***********************************************************************************/
OP_STATUS IMAP4::MakeSecure()
{
	// Check if we have the capability to do STARTTLS
	if (!(m_capabilities & ImapFlags::CAP_STARTTLS))
	{
		OpString error_msg;

		// Display an error message about the missing capability
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IDSTR_M2_POP3_ERROR_SERVICE_NOT_AVAILABLE, error_msg));
		m_backend->OnError(error_msg);

		m_backend->Lock();
		return m_backend->Disconnect("Can't get to secure state");
	}

	return InsertCommand(OP_NEW(ImapCommands::Starttls, ()), TRUE);
}


/***********************************************************************************
 ** Get first selectable folder for this connection
 **
 ** IMAP4::GetFirstSelectableFolder
 **
 ***********************************************************************************/
ImapFolder* IMAP4::GetFirstSelectableFolder() const
{
	for (unsigned i = 0; i < m_own_folders.GetCount(); i++)
	{
		ImapFolder* folder = m_own_folders.GetByIndex(i);
		if (folder->IsSelectable() && folder->IsSubscribed())
			return folder;
	}

	return 0;
}


/***********************************************************************************
 ** Create and initialize processor
 **
 ** IMAP4::InitProcessor
 **
 ***********************************************************************************/
OP_STATUS IMAP4::InitProcessor()
{
	// Create and initialize processor
	ImapProcessor* processor = OP_NEW(ImapProcessor, (*this));
	if (!processor)
		return OpStatus::ERR_NO_MEMORY;

	m_processor = processor;

	return processor->Init();
}


/***********************************************************************************
 ** Start authentication
 **
 ** IMAP4::Authenticate
 **
 ***********************************************************************************/
OP_STATUS IMAP4::Authenticate()
{
	return InsertCommand(OP_NEW(ImapCommands::Authenticate, ()), TRUE);
}


#endif // M2_SUPPORT
