/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/imap-processor.h"
#include "adjunct/m2/src/backend/imap/imap-parse.hpp"

typedef YYSTYPE IMAPTokenType;
#define YY_EXTRA_TYPE IncomingParser<IMAPTokenType>*
#define YY_NO_UNISTD_H

#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"
#include "adjunct/m2/src/backend/imap/imap-tokenizer.h"
#include "adjunct/m2/src/backend/imap/imap-parseelements.h"
#include "adjunct/m2/src/backend/imap/imap-flags.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"

#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/store.h"

extern int IMAP4parse(yyscan_t yyscanner, ImapProcessor* processor);

/***********************************************************************************
 ** Parser helper class
 ***********************************************************************************/
class ImapParser : public IncomingParser<IMAPTokenType>
{
public:
	/** Constructor
	  */
	ImapParser(ImapProcessor* processor) : m_processor(processor) {}

	/** Destructor
	  */
	virtual ~ImapParser() { if (m_scanner) IMAP4lex_destroy(m_scanner); }

	/** Initialize external parser
	  */
	virtual OP_STATUS Init()
	{
		// Setup scanner
		if (IMAP4lex_init(&m_scanner) != 0)
		{
			m_scanner = 0;
			return OpStatus::ERR;
		}

		IMAP4set_extra(this, m_scanner);

		return OpStatus::OK;
	}

	/** Get next token (by using external tokenizer)
	  */
	virtual int GetNextToken(IMAPTokenType* yylval) { return IMAP4lex(yylval, m_scanner); }

	/** Starts parsing (by using external parser)
	  */
	virtual int Parse() { return IMAP4parse(m_scanner, m_processor); }

private:
	yyscan_t       m_scanner;
	ImapProcessor* m_processor;
};


/***********************************************************************************
 ** Constructor
 **
 ** ImapProcessor::ImapProcessor
 ** @param parent IMAP4 protocol object this processor reports to
 **
 ***********************************************************************************/
ImapProcessor::ImapProcessor(IMAP4& parent)
  : m_parent(parent)
  , m_parser(0)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** ImapProcessor::~ImapProcessor
 ***********************************************************************************/
ImapProcessor::~ImapProcessor()
{
	OP_DELETE(m_parser);
}


/***********************************************************************************
 ** Initialize the internal scanner and parser, call this and check result before
 ** calling any other methods
 **
 ** ImapProcessor::Init
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::Init()
{
	// Setup parser
	m_parser = OP_NEW(ImapParser, (this));
	if (!m_parser)
		return OpStatus::ERR_NO_MEMORY;

	return m_parser->Init();
}


/***********************************************************************************
 ** Process incoming data
 **
 ** ImapProcessor::Process
 ** @param data Data that came in
 ** @param length Length of data
 ***********************************************************************************/
OP_STATUS ImapProcessor::Process(char* data, unsigned length)
{
	RETURN_IF_ERROR(GetParser()->AddParseItem(data, length));

	return GetParser()->Process();
}


/***********************************************************************************
 ** Process a tagged response
 **
 ** ImapProcessor::TaggedState
 ** @param tag The tag on the response
 ** @param state State reported for this tag
 ** @param response_code Response code supplied with the state
 ** @param response_text Response text supplied with the state
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::TaggedState(int tag, int state, int response_code, ImapUnion* response_text)
{
	OP_ASSERT(tag);

	// Same actions as with an untagged state reply
	RETURN_IF_ERROR(UntaggedState(state, response_code, response_text));

	// Extra actions: a tagged command has now been completed
	switch(state)
	{
	case IM_STATE_OK:
		return m_parent.OnSuccessfulComplete(tag);
	case IM_STATE_NO:
	case IM_STATE_BAD:
		return m_parent.OnFailedComplete(tag, response_text ? response_text->m_val_string : "");
	default:
		OP_ASSERT(!"State should be OK, NO or BAD");
		return OpStatus::ERR;
	}
}


/***********************************************************************************
 ** Process an untagged state response
 **
 ** ImapProcessor::UntaggedState
 ** @param state Untagged state reported
 ** @param response_code Response code supplied with the state
 ** @param response_text Response text supplied with the state
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::UntaggedState(int state, int response_code, ImapUnion* response_text)
{
	switch(state)
	{
	case IM_STATE_OK:
		break;
	case IM_STATE_NO:
	case IM_STATE_BAD:
		switch(response_code)
		{
		case IM_ALERT: // Message should be reported to the user, see RFC3501
		case IM_PARSE: // Parse error on server, report to user
			m_parent.ReportServerError(response_text ? response_text->m_val_string : "");
			break;
		}
		break;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Process 'flags' or 'permanentflags' response
 **
 ** ImapProcessor::Flags
 ** @param flags The flags applicable to the current mailbox
 ** @param keywords The keywords applicable to the current mailbox
 ** @param permanent_flags whether flags describes only flags that can be set permanently
 ***********************************************************************************/
OP_STATUS ImapProcessor::Flags(int flags, ImapSortedVector* keywords, BOOL permanent_flags)
{
	if (!m_parent.GetCurrentFolder())
		return OpStatus::ERR_NULL_POINTER;

	// Set flags
	m_parent.GetCurrentFolder()->SetFlags(flags, permanent_flags);

	// Keywords ignored for now, we only use extensible keywords (\*)
	return OpStatus::OK;
}


/***********************************************************************************
 ** Process an 'appenduid' response code (UIDPLUS only)
 **
 ** ImapProcessor::AppendUid
 ** @param uid_validity UID validity of the mailbox a message was appended to
 ** @param uid UID of the appended message
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::AppendUid(unsigned uid_validity, unsigned uid)
{
	if (!m_parent.GetSentCommand())
		return OpStatus::ERR_NULL_POINTER;

	return m_parent.GetSentCommand()->OnAppendUid(m_parent, uid_validity, uid);
}


/***********************************************************************************
 ** Process a 'copyuid' response code (UIDPLUS only)
 **
 ** ImapProcessor::CopyUid
 ** @param uid_validity UID validity of the mailbox messages were copied to
 ** @param source_set message sequence set (UIDs) of the original messages
 ** @param dest_set message sequence set (UIDs) of the new messages
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::CopyUid(unsigned uid_validity, ImapNumberVector* source_set, ImapNumberVector* dest_set)
{
	if (!source_set || !dest_set || !m_parent.GetSentCommand())
		return OpStatus::ERR_NULL_POINTER;

	return m_parent.GetSentCommand()->OnCopyUid(m_parent, uid_validity, *source_set, *dest_set);
}


/***********************************************************************************
 ** Process received namespace data
 **
 ** ImapProcessor::Namespace
 ** @param personal Personal namespace data
 ** @param other Other namespace data
 ** @param shared Shared namespace data
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::Namespace(ImapVector* personal, ImapVector* other, ImapVector* shared)
{
	OP_ASSERT(personal && other && shared);

	RETURN_IF_ERROR(m_parent.GetBackend().ResetNamespaces());
	RETURN_IF_ERROR(AddNamespaces(personal, ImapNamespace::PERSONAL));
	RETURN_IF_ERROR(AddNamespaces(other, ImapNamespace::OTHER));
	RETURN_IF_ERROR(AddNamespaces(shared, ImapNamespace::SHARED));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Process a 'search' response
 **
 ** ImapProcessor::Search
 ** @param search_results Messages found that conform to the previously sent search criteria
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::Search(ImapNumberVector* search_results)
{
	// Check if there was a sent command
	if (!m_parent.GetSentCommand() || !search_results)
		return OpStatus::ERR_NULL_POINTER;

	// Propagate result to sent command
	return m_parent.GetSentCommand()->OnSearchResults(*search_results);
}


/***********************************************************************************
 ** Process a 'list' or 'lsub' response
 **
 ** ImapProcessor::List
 ** @param flags List flags for this mailbox
 ** @param delimiter Delimiter character used in this mailbox
 ** @param mailbox Name of the mailbox
 ** @param is_lsub Whether the mailbox is subscribed (lsub response)
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::List(int flags, char delimiter, ImapUnion* mailbox, BOOL is_lsub)
{
	OP_ASSERT(mailbox);

	// Workaround for INBOXes that are \NoSelect, e.g. Courier-IMAP
	if (op_stricmp(mailbox->m_val_string, "INBOX") == 0)
		flags &= ~ImapFlags::LFLAG_NOSELECT;

	// Update mailbox information on parent
	RETURN_IF_ERROR(m_parent.GetBackend().UpdateFolder(mailbox->m_val_string, delimiter, flags, is_lsub));

	// Insert into subscribed folder list for protocol, or 'list' list for module
	if (is_lsub)
		RETURN_IF_ERROR(m_parent.AddToSubscribedFolderList(mailbox->m_val_string));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Process a part of a status response
 **
 ** ImapProcessor::Status
 ** @param status_type Type of status message received
 ** @param status_value The value for this type
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::Status(int status_type, UINT64 status_value)
{
	ImapCommandItem* sent_command = m_parent.GetSentCommand();
	ImapFolder* status_mailbox    = sent_command ? sent_command->GetMailbox() : 0;

	if (!status_mailbox)
		return OpStatus::ERR_NULL_POINTER;

	switch(status_type)
	{
	case IM_MESSAGES:
		return status_mailbox->SetExists(status_value);
	case IM_UIDNEXT:
		return status_mailbox->SetUidNext(status_value);
	case IM_UIDVALIDITY:
		return status_mailbox->SetUidValidity(status_value);
	case IM_UNSEEN:
		return status_mailbox->SetUnseen(status_value);
	case IM_RECENT:
		return status_mailbox->SetRecent(status_value);
	case IM_HIGHESTMODSEQ:
		return status_mailbox->UpdateModSeq(status_value);
	default:
		OP_ASSERT(!"Invalid status message received");
		return OpStatus::ERR;
	}
}


/***********************************************************************************
 ** Process a 'fetch' response. Assumes that m_message has been set with values
 ** for this message.
 **
 ** ImapProcessor::Fetch
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::Fetch()
{
	if (!m_parent.GetCurrentFolder())
	{
		OP_ASSERT(!"No folder. Wrong command sent?");
		return OpStatus::ERR;
	}

	OpString8 internet_location;
	message_gid_t message_id;

	// Update progress
	if (m_parent.GetSentCommand() && m_parent.GetSentCommand()->GetFetchFlags())
		m_parent.GetBackend().UpdateProgressGroup(m_parent.GetSentCommand()->GetProgressAction(), 1, FALSE);

	if (m_message.m_uid > 0)
	{
		// Update UID information
		RETURN_IF_ERROR(m_parent.GetCurrentFolder()->SetMessageUID(m_message.m_server_id, m_message.m_uid));
		// Get message by UID
		message_id = m_parent.GetCurrentFolder()->GetMessageByUID(m_message.m_uid);
	}
	else
	{
		// Get message by server sequence id
		message_id = m_parent.GetCurrentFolder()->GetMessageByServerID(m_message.m_server_id);
		if (message_id == 0)
		{
			// Maybe we can get this via a QRESYNC?
			if ((m_parent.GetCapabilities() & ImapFlags::CAP_QRESYNC) && m_parent.GetCurrentFolder()->LastKnownModSeq() < m_message.m_mod_seq)
				return m_parent.InsertCommand(OP_NEW(ImapCommands::FullSync, (m_parent.GetCurrentFolder())));
			else
				return OpStatus::OK; // Can't do anything with this message; Hope it comes in via an exists
		}
	}

	if (message_id == 0)
	{
		// Message not yet in store, this is a new message
		if (!m_message.m_raw_text_complete && !m_message.m_raw_headers)
		{
			ImapCommandItem* sent_command = m_parent.GetSentCommand();

			// We need a header or the whole mail, fetch more information about this message
			if (!sent_command || !(sent_command->GetFetchFlags() & (ImapCommands::Fetch::HEADERS | ImapCommands::Fetch::ALL_HEADERS | ImapCommands::Fetch::COMPLETE)))
				return m_parent.FetchMessageByUID(m_parent.GetCurrentFolder(), m_message.m_uid);
			else if (sent_command && (sent_command->GetFetchFlags() & ImapCommands::Fetch::BODYSTRUCTURE))
				return m_parent.FetchMessageByUID(m_parent.GetCurrentFolder(), m_message.m_uid, FALSE, FALSE, FALSE, TRUE);
		}
		else
		{
			return NewMessage();
		}
	}
	else
	{
		// Existing message, update information
		return UpdateMessage(message_id);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Do all necessary actions to start processing a new command
 **
 ** ImapProcessor::NextCommand
 **
 ***********************************************************************************/
void ImapProcessor::NextCommand()
{
	// we don't expect more data, make sure we don't time out
	m_parent.StopConnectionTimer();
}


/***********************************************************************************
 ** Report a parse error
 **
 ** ImapProcessor::ReportParseError
 ** @param error_message Error message that can be displayed to the user
 **
 ***********************************************************************************/
void ImapProcessor::OnParseError(const char* error_message)
{
	ImapCommandItem* sent_command = m_parent.GetSentCommand();

	if (sent_command && sent_command->GetFetchFlags())
	{
		m_parent.GetBackend().LogFormattedText("IMAP#%u/%u: Parse error in message %d: %s",
							m_parent.GetId(), m_parent.GetBackend().GetConnectionCount(), m_message.m_server_id, error_message);
	}
	else
	{
		m_parent.GetBackend().LogFormattedText("IMAP#%u/%u: Parse error: %s",
							m_parent.GetId(), m_parent.GetBackend().GetConnectionCount(), error_message);
	}
}


/***********************************************************************************
 ** Remove parse stack items that are already processed
 **
 ** ImapProcessor::Reset
 **
 ***********************************************************************************/
void ImapProcessor::Reset()
{
	m_parser->Reset();
}


/***********************************************************************************
 ** Set message properties
 **
 ** ImapProcessor::SetMessageRawText
 **
 ***********************************************************************************/
void ImapProcessor::SetMessageRawText(int type, ImapUnion* raw_text)
{
	switch (type)
	{
		case ImapSection::HEADERS:
			m_message.m_raw_headers = raw_text;
			break;
		case ImapSection::MIME:
			m_message.m_raw_mime_headers = raw_text;
			break;
		case ImapSection::PART:
			m_message.m_raw_text_part = raw_text;
			break;
		case ImapSection::COMPLETE:
		default:
			m_message.m_raw_text_complete = raw_text;
	}
}


/***********************************************************************************
 ** Create a new message locally with the properties in m_message
 **
 ** ImapProcessor::NewMessage
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::NewMessage()
{
	OpString8 internet_location;

	OP_ASSERT(m_parent.GetCurrentFolder());

	// Initialize new message
	Message message;

	RETURN_IF_ERROR(message.Init(m_parent.GetBackend().GetAccountPtr()->GetAccountId()));

	// Calculate and set internet location
	RETURN_IF_ERROR(m_parent.GetCurrentFolder()->GetInternetLocation(m_message.m_uid, internet_location));
	RETURN_IF_ERROR(message.SetInternetLocation(internet_location));

	// Update properties of message
	RETURN_IF_ERROR(UpdateMessage(message, TRUE));

	// Add message to store and indexes
	ImapCommandItem* sent_command = m_parent.GetSentCommand();
	BOOL			 only_headers = !sent_command || !(sent_command->GetFetchFlags() & (ImapCommands::Fetch::BODY | ImapCommands::Fetch::TEXTPART | ImapCommands::Fetch::COMPLETE));

	RETURN_IF_ERROR(m_parent.GetBackend().GetAccountPtr()->Fetched(message, only_headers));

	// Add UID to UID manager
	RETURN_IF_ERROR(m_parent.GetCurrentFolder()->AddUID(m_message.m_uid, message.GetId()));

	// Put in IMAP folder, mark as deleted or as junk if necessary (Not done in UpdateMessage)
	if (m_parent.GetCurrentFolder()->GetIndex())
		RETURN_IF_ERROR(m_parent.GetCurrentFolder()->GetIndex()->NewMessage(message.GetId()));
	if (m_message.m_flags & ImapFlags::FLAG_DELETED || m_parent.GetCurrentFolder() == m_parent.GetBackend().GetFolderByType(AccountTypes::FOLDER_TRASH))
		RETURN_IF_ERROR(MessageEngine::GetInstance()->OnDeleted(message.GetId(), FALSE));

	// Handle keywords
	RETURN_IF_ERROR(UpdateKeywords(message.GetId(), m_message.m_keywords, true));

	if ((m_message.m_flags & ImapFlags::FLAG_FLAGGED) !=0)
	{
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->SetMessageFlag(message.GetId(), Message::IS_FLAGGED, (m_message.m_flags & ImapFlags::FLAG_FLAGGED) != 0));
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->AddToPinBoard(message.GetId(), (m_message.m_flags & ImapFlags::FLAG_FLAGGED) != 0));
	}

	// Check if a mod seq was given
	if (m_message.m_mod_seq)
		m_parent.GetCurrentFolder()->UpdateModSeq(m_message.m_mod_seq, true);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Update a locally-stored message in the current mailbox with the properties in
 ** m_message
 **
 ** ImapProcessor::UpdateMessage
 ** @param message_id ID in the local store of the message to update
 ** @param safety_check Whether to check and stop execution if this operation is unsafe
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::UpdateMessage(message_gid_t message_id, BOOL safety_check)
{
	Store2* store = MessageEngine::GetInstance()->GetStore();

	OP_ASSERT(message_id != 0 && m_parent.GetCurrentFolder() && store);

	// Some properties can only be set if the message is first retrieved from store.
	// Check if this is necessary. We try to avoid this, because it causes expensive disk access.
	if (m_message.m_size || m_message.m_raw_text_complete || m_message.m_raw_text_part || m_message.m_raw_headers || m_message.m_raw_mime_headers)
	{
		Message message;

		// Retrieve message from store
		RETURN_IF_ERROR(store->GetMessage(message, message_id));

		// Call update function on retrieved message, will not set flags
		RETURN_IF_ERROR(UpdateMessage(message, FALSE));

		// Save changes to store
		RETURN_IF_ERROR(MessageEngine::GetInstance()->UpdateMessage(message));

		ImapCommandItem* sent_command = m_parent.GetSentCommand();
		if (sent_command && (sent_command->GetFetchFlags() & (ImapCommands::Fetch::BODY | ImapCommands::Fetch::COMPLETE | ImapCommands::Fetch::TEXTPART)))
			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->SetRawMessage(message));
	}

	// Safety check: abort this update if something else in the queue is changing the message
	if (safety_check && m_parent.IsOperationInQueue(m_parent.GetCurrentFolder(), m_message.m_uid))
		return OpStatus::OK;

	if ((m_message.m_flags & ImapFlags::FLAGS_INCLUDED_IN_FETCH) != 0)
	{
		// Set flags
		UINT64 existing_flags = store->GetMessageFlags(message_id);

		if (((existing_flags & 1 << Message::IS_READ) != 0) != ((m_message.m_flags & ImapFlags::FLAG_SEEN) != 0))
		{
			// The indexer needs to know about changes in the 'read' flag (to remove messages from unread view)
			// This has to be done _before_ setting the actual flag, so that any listening indexmodels (they get notified when
			// setting the flag) see that the message is gone
			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->MessageRead(message_id, (m_message.m_flags & ImapFlags::FLAG_SEEN) != 0));
			RETURN_IF_ERROR(store->SetMessageFlag(message_id, Message::IS_READ, (m_message.m_flags & ImapFlags::FLAG_SEEN) != 0));
		}

		// Other flags
		if (((existing_flags & 1 << Message::IS_REPLIED) != 0) != ((m_message.m_flags & ImapFlags::FLAG_ANSWERED) != 0))
			RETURN_IF_ERROR(store->SetMessageFlag(message_id, Message::IS_REPLIED, (m_message.m_flags & ImapFlags::FLAG_ANSWERED) != 0));
		if (((existing_flags & 1 << Message::IS_DELETED) != 0) != ((m_message.m_flags & ImapFlags::FLAG_DELETED) != 0))
			RETURN_IF_ERROR(store->SetMessageFlag(message_id, Message::IS_DELETED, (m_message.m_flags & ImapFlags::FLAG_DELETED) != 0));
		if (((existing_flags & (UINT64)1 << Message::IS_FLAGGED) != 0) != ((m_message.m_flags & ImapFlags::FLAG_FLAGGED) !=0))
		{
			RETURN_IF_ERROR(store->SetMessageFlag(message_id, Message::IS_FLAGGED, (m_message.m_flags & ImapFlags::FLAG_FLAGGED) != 0));
			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->AddToPinBoard(message_id, (m_message.m_flags & ImapFlags::FLAG_FLAGGED) != 0));
		}

		// Keywords
		RETURN_IF_ERROR(UpdateKeywords(message_id, m_message.m_keywords, false));

		// Handle 'deleted' flag, move message to trash if necessary
		if (m_message.m_flags & ImapFlags::FLAG_DELETED || m_parent.GetCurrentFolder() == m_parent.GetBackend().GetFolderByType(AccountTypes::FOLDER_TRASH))
		{
			Index* index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::TRASH);
			if (index && !index->Contains(message_id))
			{
				RETURN_IF_ERROR(MessageEngine::GetInstance()->OnDeleted(message_id, FALSE));
			}
		}
		else if (existing_flags & 1 << Message::IS_DELETED)
		{
			// TODO correct?
			Index* index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::TRASH);
			if (index)
				RETURN_IF_ERROR(index->RemoveMessage(message_id));
		}
	
		if (m_message.m_flags & ImapFlags::FLAG_NOT_SPAM)
		{
			Index* spam = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::SPAM);
			if (spam && spam->Contains(message_id))
			{
				RETURN_IF_ERROR(spam->RemoveMessage(message_id));
			}
		}
		else if (m_message.m_flags & ImapFlags::FLAG_SPAM)
		{
			Index* spam = MessageEngine::GetInstance()->GetIndexer()->GetIndexById(IndexTypes::SPAM);
			if (spam && !spam->Contains(message_id))
			{
				RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->SilentMarkMessageAsSpam(message_id));
			}
		}
	}

	// Check if a mod seq was given
	if (m_message.m_mod_seq)
		RETURN_IF_ERROR(m_parent.GetCurrentFolder()->UpdateModSeq(m_message.m_mod_seq, true));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Update a message with the properties in m_message - doesn't change store or
 ** index
 **
 ** ImapProcessor::UpdateMessage
 ** @param message Message to update
 ** @param set_flags Whether to set flags and keywords on this message
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::UpdateMessage(Message& message, BOOL set_flags)
{
	// Set flags and keywords if requested
	if (set_flags)
	{
		// Set generic flags
		if (m_message.m_flags)
		{
			message.SetFlag(Message::IS_SEEN, (m_message.m_flags & ImapFlags::FLAG_SEEN) != 0);
			message.SetFlag(Message::IS_READ, (m_message.m_flags & ImapFlags::FLAG_SEEN) != 0);
			message.SetFlag(Message::IS_REPLIED, (m_message.m_flags & ImapFlags::FLAG_ANSWERED) != 0);
			message.SetFlag(Message::IS_DELETED, (m_message.m_flags & ImapFlags::FLAG_DELETED) != 0);
			
			// we should ignore both if these flags are both set: http://www.ietf.org/mail-archive/web/morg/current/msg00441.html
			if (!(m_message.m_flags & ImapFlags::FLAG_NOT_SPAM && m_message.m_flags & ImapFlags::FLAG_SPAM))
			{
				if (m_message.m_flags & ImapFlags::FLAG_NOT_SPAM)
				{
					message.SetConfirmedNotSpam(TRUE);
				}
				else if (m_message.m_flags & ImapFlags::FLAG_SPAM)
				{
					message.SetConfirmedSpam(TRUE);
				}
			}
		}

		// set properties based on the IMAP folder the message is in
		if (m_parent.GetCurrentFolder() == m_parent.GetBackend().GetFolderByType(AccountTypes::FOLDER_SPAM) && !message.IsConfirmedNotSpam())
		{
			message.SetConfirmedSpam(TRUE);
		}
		
		if (m_parent.GetCurrentFolder() == m_parent.GetBackend().GetFolderByType(AccountTypes::FOLDER_SENT))
		{
			message.SetFlag(Message::IS_OUTGOING, TRUE);
			message.SetFlag(Message::IS_SENT, TRUE);
		}

		if (m_parent.GetBackend().GetAccountPtr()->GetServerHasSpamFilter() && !message.IsConfirmedSpam())
		{
			message.SetConfirmedNotSpam(TRUE);
		}
	}
	else if (m_parent.GetBackend().GetAccountPtr()->GetServerHasSpamFilter())
	{
		// this message can be spam filtered if we don't set the confirmed not spam flag
		if (m_parent.GetCurrentFolder() == m_parent.GetBackend().GetFolderByType(AccountTypes::FOLDER_SPAM))
			message.SetConfirmedSpam(true);
		else
			message.SetConfirmedNotSpam(true);
	}

	// Set size
	if (m_message.m_size)
		message.SetMessageSize((UINT32)m_message.m_size);

	// Set attachment flags
	BOOL first = TRUE;
	UpdateBodyStructure(message, m_message.m_body_structure, first);

	// Set message contents
	if (m_message.m_raw_text_complete)
	{
		RETURN_IF_ERROR(message.SetRawMessage(NULL, FALSE, FALSE, m_message.m_raw_text_complete->m_val_string, m_message.m_raw_text_complete->m_length));
		m_message.m_raw_text_complete->Release();
		message.SetFlag(Message::PARTIALLY_FETCHED, FALSE);
		m_parent.GetBackend().MessageReceived(message.GetId());
	}
	else
	{
		if (m_message.m_raw_headers)
		{
			RETURN_IF_ERROR(message.SetRawMessage(NULL, FALSE, FALSE, m_message.m_raw_headers->m_val_string, m_message.m_raw_headers->m_length));
			m_message.m_raw_headers->Release(); // Message has taken ownership of string, so should be released
		}

		if (m_message.m_raw_mime_headers || m_message.m_raw_text_part)
		{
			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageData(message));
			
			if (m_message.m_raw_mime_headers)
				RETURN_IF_ERROR(message.SetHeadersFromMimeHeaders(m_message.m_raw_mime_headers->m_val_string));

			if (m_message.m_raw_text_part)
			{
				RETURN_IF_ERROR(message.SetRawBody(NULL, FALSE, m_message.m_raw_text_part->m_val_string));
				m_message.m_raw_text_part->Release(); // Message has taken ownership of string, so should be released

				message.SetFlag(Message::PARTIALLY_FETCHED, !message.IsFlagSet(Message::IS_ONEPART_MESSAGE));
				if (message.IsFlagSet(Message::IS_ONEPART_MESSAGE))
					m_parent.GetBackend().MessageReceived(message.GetId());
			}
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Update attachment flags in a message based on body structure
 **
 ** ImapProcessor::UpdateBodyStructure
 ** @param message Message to update
 ** @param body_structure Body structure to analyze
 **
 ***********************************************************************************/
void ImapProcessor::UpdateBodyStructure(Message& message, ImapBody* body_structure, BOOL& first, unsigned depth)
{
	if (!body_structure)
		return;

	// Recursively parse body structures if this is a multipart
	if (body_structure->m_multipart)
	{
		if (body_structure->m_body_list)
		{
			for (unsigned i = 0; i < body_structure->m_body_list->GetCount(); i++)
			{
				UpdateBodyStructure(message, (ImapBody*)body_structure->m_body_list->Get(i), first, depth + 1);

				// We only parse one alternative
				if (body_structure->m_media_subtype == IM_ALTERNATIVE)
					break;
			}
		}

		return;
	}
	else if (depth == 0)
	{
		message.SetFlag(Message::IS_ONEPART_MESSAGE, TRUE);
	}

	// This is not a multipart. Convert types to types that Message understands
	Message::MediaType media_type = Message::TYPE_UNKNOWN;
	Message::MediaSubtype media_subtype = Message::SUBTYPE_UNKNOWN;

	switch (body_structure->m_media_type)
	{
		case IM_APPLICATION:
			media_type = Message::TYPE_APPLICATION;
			break;
		case IM_AUDIO:
			media_type = Message::TYPE_AUDIO;
			break;
		case IM_IMAGE:
			media_type = Message::TYPE_IMAGE;
			break;
		case IM_VIDEO:
			media_type = Message::TYPE_VIDEO;
			break;
	}

	switch (body_structure->m_media_subtype)
	{
		case IM_ARCHIVE:
			media_subtype = Message::SUBTYPE_ARCHIVE;
			break;
		case IM_OGG:
			media_subtype = Message::SUBTYPE_OGG;
			break;
	}

	// Update message attachment flags
	if (!first || media_type != Message::TYPE_UNKNOWN)
		message.SetAttachmentFlags(media_type, media_subtype);

	// Reset flag for first text body if found (first body doesn't count as attachment)
	if (body_structure->m_media_type == IM_TEXT && first)
	{
		first = FALSE;
		if (m_parent.GetSentCommand())
			m_parent.GetSentCommand()->OnTextPartDepth(m_message.m_uid, depth, m_parent);
	}

	UpdateBodyStructure(message, body_structure->m_body, first, depth + 1);
}


/***********************************************************************************
 ** Add namespaces of a specific type to the parent connection
 **
 ** ImapProcessor::AddNamespaces
 ** @param namespaces Namespaces to add
 ** @param namespace_type Type of the namespaces to add
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::AddNamespaces(ImapVector* namespaces, ImapNamespace::NamespaceType namespace_type)
{
	OP_ASSERT(namespaces);

	for(UINT32 i = 0; i < namespaces->GetCount(); i++)
	{
		ImapNamespaceDesc* name_space = (ImapNamespaceDesc*)namespaces->Get(i);
		OP_ASSERT(name_space && name_space->m_prefix && name_space->m_delimiter);
		RETURN_IF_ERROR(m_parent.GetBackend().AddNamespace(namespace_type, name_space->m_prefix->m_val_string, name_space->m_delimiter));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Process keywords on a message
 **
 ** ImapProcessor::UpdateKeywords
 **
 ***********************************************************************************/
OP_STATUS ImapProcessor::UpdateKeywords(message_gid_t message_id, ImapSortedVector* keywords, bool new_message)
{
	return MessageEngine::GetInstance()->GetIndexer()->SetKeywordsOnMessage(message_id, keywords, new_message);
}

#endif // M2_SUPPORT
