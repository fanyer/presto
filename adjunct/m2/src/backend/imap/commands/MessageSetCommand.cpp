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

#include "adjunct/m2/src/backend/imap/commands/MessageSetCommand.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/offlinelog.h"


///////////////////////////////////////////
//           MessageSetCommand
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** MessageSetCommand::CanExtendWith
 ***********************************************************************************/
BOOL ImapCommands::MessageSetCommand::CanExtendWith(const MessageSetCommand* command) const
{
	return command->GetExtendableType() == GetExtendableType() &&
		   m_mailbox					== command->m_mailbox &&
		   m_flags						== command->m_flags;
}


/***********************************************************************************
 **
 **
 ** MessageSetCommand::ExtendWith
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSetCommand::ExtendWith(const MessageSetCommand* command, IMAP4& protocol)
{
	unsigned count_before_merge = m_message_set.Count();

	RETURN_IF_ERROR(m_message_set.Merge(command->m_message_set));

	// Update progress, now that count has changed
	protocol.GetBackend().IncreaseProgressTotal(GetProgressAction(), m_message_set.Count() - count_before_merge);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MessageSetCommand::PrepareSetForOffline
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageSetCommand::PrepareSetForOffline(OpINT32Vector& message_ids)
{
	if (m_message_set.IsInvalid() ||
		!(m_flags & USE_UID))
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_message_set.ToVector(message_ids));

	return m_mailbox->ConvertUIDToM2Id(message_ids);
}


///////////////////////////////////////////
//               Copy
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Copy::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS ImapCommands::Copy::OnSuccessfulComplete(IMAP4& protocol)
{
	if (m_to_mailbox->GetIndex())
		RETURN_IF_ERROR(m_to_mailbox->GetIndex()->DecreaseNewMessageCount(m_message_set.Count()));
	return protocol.GetBackend().SyncFolder(m_to_mailbox);
}


/***********************************************************************************
 **
 **
 ** Copy::OnCopyUid
 ***********************************************************************************/
OP_STATUS ImapCommands::Copy::OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set)
{
	// Source set should be just as big as dest set, else we're not going to do this
	if (source_set.GetCount() != dest_set.GetCount())
		return OpStatus::OK;

	// We try to copy the original messages and save their UIDs
	for (unsigned i = 0; i < source_set.GetCount(); i++)
	{
		message_gid_t message_id = m_mailbox->GetMessageByUID(source_set.Get(i));
		int			  dest_uid   = dest_set.Get(i);

		protocol.GetBackend().OnMessageCopied(message_id, m_to_mailbox, dest_uid, FALSE);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** Copy::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::Copy::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));

	return offline_log.CopyMessages(message_ids, m_mailbox->GetIndexId(), m_to_mailbox->GetIndexId());
}


/***********************************************************************************
 **
 **
 ** Copy::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Copy::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpINT32Vector uids;
	RETURN_IF_ERROR(m_message_set.ToVector(uids));
	
	for (UINT32 i = 0; i < uids.GetCount(); i++)
	{
		message_gid_t* message_id = OP_NEW(message_gid_t,(m_mailbox->GetMessageByUID(uids.Get(i))));
		RETURN_OOM_IF_NULL(message_id);
		RETURN_IF_ERROR(m_uid_m2id_map.Add(uids.Get(i), message_id ));
	}

	OpString8 message_set_string;
	RETURN_IF_ERROR(m_message_set.GetString(message_set_string));

	return command.AppendFormat("%sCOPY %s %s",
								m_flags & USE_UID ? "UID " : "",
								message_set_string.CStr(),
								m_to_mailbox->GetQuotedName().CStr());
}


///////////////////////////////////////////
//               Move
///////////////////////////////////////////

/***********************************************************************************
 ** Move: first copy, then delete
 **
 ** Move::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::Move::GetExpandedQueue(IMAP4& protocol)
{
	// Create COPY command
	OpAutoPtr<ImapCommands::MoveCopy> expanded_queue (OP_NEW(ImapCommands::MoveCopy, (m_mailbox,
																	  m_to_mailbox,
																	  0, 0)));

	if (!expanded_queue.get() || OpStatus::IsError(expanded_queue->GetMessageSet().Copy(m_message_set)))
		return NULL;

	// Create message delete command
	ImapCommands::MessageDelete* delete_command = OP_NEW(ImapCommands::MessageDelete, (m_mailbox, 0, 0));

	if (!delete_command || OpStatus::IsError(delete_command->GetMessageSet().Copy(m_message_set)))
		return NULL;

	delete_command->DependsOn(expanded_queue.get(), protocol);

	return expanded_queue.release();
}


/***********************************************************************************
 **
 **
 ** Move::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::Move::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));

	return offline_log.MoveMessages(message_ids, m_mailbox->GetIndexId(), m_to_mailbox->GetIndexId());
}

/***********************************************************************************
 **
 **
 ** MoveCopy::OnCopyUid
 ***********************************************************************************/
OP_STATUS ImapCommands::MoveCopy::OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set)
{
	// This Copy is actually a Move
	// We need to move the message locally as well, and remove the old internet location
	if (source_set.GetCount() != dest_set.GetCount())
		return OpStatus::OK;

	// We try to move the original messages and save their UIDs
	for (unsigned i = 0; i < source_set.GetCount(); i++)
	{
		message_gid_t *message_id;
		RETURN_IF_ERROR(m_uid_m2id_map.GetData(source_set.Get(i), &message_id));
		int			  dest_uid   = dest_set.Get(i);
		
		// remove from source folder
		RETURN_IF_ERROR(protocol.GetBackend().GetUIDManager().RemoveUID(m_mailbox->GetIndexId(), source_set.Get(i))); 
		RETURN_IF_ERROR(m_mailbox->GetIndex()->RemoveMessage(*message_id));

		RETURN_IF_ERROR(protocol.GetBackend().OnMessageCopied(*message_id, m_to_mailbox, dest_uid, TRUE));
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MoveCopy::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::MoveCopy::AppendCommand(OpString8& command, IMAP4& protocol)
{	
	RETURN_IF_ERROR(Copy::AppendCommand(command, protocol));

	if (protocol.GetCapabilities() & ImapFlags::CAP_UIDPLUS)
	{
		for (INT32HashIterator<message_gid_t> it(m_uid_m2id_map); it; it++)
		{
			RETURN_IF_ERROR(protocol.GetBackend().GetUIDManager().RemoveUID(m_mailbox->GetIndexId(), it.GetKey()));
		}
	}
	return OpStatus::OK;
}

///////////////////////////////////////////
//               Fetch
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Fetch::PrepareToSend
 ***********************************************************************************/
OP_STATUS ImapCommands::Fetch::PrepareToSend(IMAP4& protocol)
{
	// Special: check if fetch contains too many messages to do in one go, split in
	// that case
	if (m_flags & (BODY | HEADERS | ALL_HEADERS | COMPLETE))
	{
		unsigned max_msg = (m_flags & (BODY | COMPLETE)) ? 20 : 50;

		if (m_message_set.ContainsMoreThan(max_msg))
		{
			// Split off a new command
			ImapCommands::Fetch* split = CreateNewCopy();
			if (!split)
				return OpStatus::ERR_NO_MEMORY;

			split->Follow(this, protocol);

			// Do the split
			return m_message_set.SplitOff(max_msg, split->m_message_set);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Fetch::GetProgressAction
 ***********************************************************************************/
ProgressInfo::ProgressAction ImapCommands::Fetch::GetProgressAction() const
{
	if (m_flags & (HEADERS | ALL_HEADERS))
		return ProgressInfo::FETCHING_HEADERS;
	else if (m_flags & (BODY | TEXTPART | COMPLETE))
		return ProgressInfo::FETCHING_MESSAGES;
	else
		return ProgressInfo::SYNCHRONIZING;
}


/***********************************************************************************
 **
 **
 ** Fetch::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Fetch::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpString8 commands;

	RETURN_IF_ERROR(GetFetchCommands(commands));

	OpString8 message_set_string;
	RETURN_IF_ERROR(m_message_set.GetString(message_set_string));

	RETURN_IF_ERROR(command.AppendFormat("%sFETCH %s (UID %s%sFLAGS)",
								m_flags & USE_UID   	? "UID "						: "",								
								message_set_string.CStr(),
								protocol.GetCapabilities() & ImapFlags::CAP_QRESYNC ? "MODSEQ " : "",
								commands.HasContent()	? commands.CStr()				: ""
										));

	if (m_flags & CHANGED)
	{
		RETURN_IF_ERROR(command.AppendFormat(" (CHANGEDSINCE %lld VANISHED)", m_mailbox->LastKnownModSeq()));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets the string for commands (e.g. "RFC822.HEADER BODY.PEEK[]"
 ** Empty string allowed
 ** If content exists, should end in a space
 **
 ** Fetch::GetFetchCommands
 ***********************************************************************************/
OP_STATUS ImapCommands::Fetch::GetFetchCommands(OpString8& commands) const
{
	if (m_flags & BODY)
		RETURN_IF_ERROR(commands.Append("BODY.PEEK[TEXT] "));

	if (m_flags & COMPLETE)
		RETURN_IF_ERROR(commands.Append("BODY.PEEK[] "));

	if (m_flags & HEADERS)
	{
		OpString8 needed_headers;
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->GetNeededHeaders(needed_headers));
		RETURN_IF_ERROR(commands.AppendFormat("RFC822.SIZE BODY.PEEK[HEADER.FIELDS (%s%s%s)] ",
											  GetDefaultHeaders(),
											  needed_headers.HasContent() ? " " : "",
											  needed_headers.HasContent() ? needed_headers.CStr() : ""));
	}

	if (m_flags & ALL_HEADERS)
		RETURN_IF_ERROR(commands.Append("RFC822.SIZE BODY.PEEK[HEADER] "));

	if (m_flags & BODYSTRUCTURE)
		RETURN_IF_ERROR(commands.Append("BODYSTRUCTURE "));
	
	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets default headers in space-separated format, e.g. "SUBJECT FROM"
 **
 ** Fetch::GetDefaultHeaders
 ***********************************************************************************/
const char* ImapCommands::Fetch::GetDefaultHeaders() const
{
	return "SUBJECT FROM TO CC REPLY-TO DATE MESSAGE-ID REFERENCES IN-REPLY-TO LIST-ID LIST-POST X-MAILING-LIST DELIVERED-TO";
}


/***********************************************************************************
 ** Gets the string for commands (e.g. "RFC822.HEADER BODY.PEEK[]"
 ** Empty string allowed
 ** If content exists, should end in a space
 **
 ** Fetch::CreateNewCopy
 ***********************************************************************************/
ImapCommands::Fetch* ImapCommands::Fetch::CreateNewCopy() const
{
	return OP_NEW(ImapCommands::Fetch, (m_mailbox, 0, 0, m_flags));
}


///////////////////////////////////////////
//               TextPartFetch
///////////////////////////////////////////


/***********************************************************************************
 ** Received the text part depth
 **
 ** TextPartFetch::OnTextPartDepth
 ***********************************************************************************/
OP_STATUS ImapCommands::TextPartFetch::OnTextPartDepth(unsigned uid, unsigned textpart_depth, IMAP4& protocol)
{
	TextPartBodyFetch* body_fetch = OP_NEW(TextPartBodyFetch, (m_mailbox, uid, textpart_depth));
	if (!body_fetch)
		return OpStatus::ERR_NO_MEMORY;

	body_fetch->DependsOn(this, protocol);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets default headers in space-separated format, e.g. "SUBJECT FROM"
 **
 ** TextPartFetch::GetDefaultHeaders
 ***********************************************************************************/
const char* ImapCommands::TextPartFetch::GetDefaultHeaders() const
{
	return "SUBJECT FROM TO CC REPLY-TO DATE MESSAGE-ID REFERENCES IN-REPLY-TO LIST-ID LIST-POST X-MAILING-LIST DELIVERED-TO CONTENT-TYPE CONTENT-TRANSFER-ENCODING";
}


/***********************************************************************************
 **
 **
 ** TextPartFetch::CreateNewCopy
 ***********************************************************************************/
ImapCommands::Fetch* ImapCommands::TextPartFetch::CreateNewCopy() const
{
	return OP_NEW(TextPartFetch, (m_mailbox, 0, 0, m_flags & USE_UID));
}


/***********************************************************************************
 ** Gets the string for commands (e.g. "RFC822.HEADER BODY.PEEK[]"
 ** Empty string allowed
 ** If content exists, should end in a space
 **
 ** TextPartFetch::TextPartBodyFetch::GetFetchCommands
 ***********************************************************************************/
OP_STATUS ImapCommands::TextPartFetch::TextPartBodyFetch::GetFetchCommands(OpString8& commands) const
{
	OpString8 part;

	RETURN_IF_ERROR(part.Set("1"));
		
	// For depth deeper than 1, we add .1 ("1" works for both single part and the first part of a multipart)
	for (unsigned i = 1; i < m_textpart_depth; i++)
		RETURN_IF_ERROR(part.Append(".1"));

	return commands.AppendFormat("BODY.PEEK[%s.MIME] BODY.PEEK[%s] ", part.CStr(), part.CStr());
}


///////////////////////////////////////////
//               Search
///////////////////////////////////////////


/***********************************************************************************
 **
 **
 ** Search::OnSearchResults
 ***********************************************************************************/
OP_STATUS ImapCommands::Search::OnSearchResults(OpINT32Vector& search_results)
{
	// propagate to children, they might use the result
	for (ImapCommandItem* child = FirstChild(); child; child = child->FirstChild())
		RETURN_IF_ERROR(child->OnSearchResults(search_results));

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Search::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Search::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpString8 message_set_string;
	RETURN_IF_ERROR(m_message_set.GetString(message_set_string));

	return command.AppendFormat("%sSEARCH%s%s %s%s",
								m_flags & FOR_UID ? "UID "	   : "",
								m_flags & NOT	  ? " NOT"	   : "",
								m_flags & BY_UID  ? " UID"	   : "",
								message_set_string.CStr(),
								m_flags & DELETED ? " DELETED" : "");
}


///////////////////////////////////////////
//               Store
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** Store::IsUnnecessary
 ***********************************************************************************/
BOOL ImapCommands::Store::IsUnnecessary(const IMAP4& protocol) const
{
	return ((m_flags & KEYWORDS) && !(m_mailbox->GetPermanentFlags() & ImapFlags::FLAG_STAR)) ||
		   MessageSetCommand::IsUnnecessary(protocol);
}


/***********************************************************************************
 **
 **
 ** Store::CanExtendWith
 ***********************************************************************************/
BOOL ImapCommands::Store::CanExtendWith(const MessageSetCommand* command) const
{
	// Store can only be extended if keyword value is the same
	return MessageSetCommand::CanExtendWith(command) &&
		   !static_cast<const ImapCommands::Store*>(command)->m_keyword.Compare(m_keyword);
}


/***********************************************************************************
 **
 **
 ** Store::PrepareToSend
 ***********************************************************************************/
OP_STATUS ImapCommands::Store::PrepareToSend(IMAP4& protocol)
{
	// Cut up sequences to small pieces, to prevent command lines that are too long (bug #289140)
	if (m_message_set.RangeCount() > 300)
	{
		ImapCommands::Store* split = OP_NEW(Store, (m_mailbox, 0, 0, m_flags, m_keyword));
		if (!split)
			return OpStatus::ERR_NO_MEMORY;

		split->DependsOn(this, protocol);
		return m_message_set.SplitOffRanges(300, split->m_message_set);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Store::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::Store::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));

	if (m_flags & FLAG_DELETED)
	{
		if (m_flags & ADD_FLAGS)
			RETURN_IF_ERROR(offline_log.RemoveMessages(message_ids, FALSE));
		else
			RETURN_IF_ERROR(offline_log.MessagesMovedFromTrash(message_ids));
	}

	if (m_flags & FLAG_SEEN)
	{
		RETURN_IF_ERROR(offline_log.ReadMessages(message_ids, m_flags & ADD_FLAGS));
	}

	if (m_flags & FLAG_ANSWERED)
	{
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
			RETURN_IF_ERROR(offline_log.ReplyToMessage(message_ids.Get(i)));
	}

	if (m_flags & KEYWORDS)
	{
		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			if (m_flags & ADD_FLAGS)
				RETURN_IF_ERROR(offline_log.KeywordAdded(message_ids.Get(i), m_keyword));
			else
				RETURN_IF_ERROR(offline_log.KeywordRemoved(message_ids.Get(i), m_keyword));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Store::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::Store::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpString8 tmp_string;
	OpString8 message_set_string;
	RETURN_IF_ERROR(m_message_set.GetString(message_set_string));

	return command.AppendFormat("%sSTORE %s %s%sFLAGS%s (%s)",
								m_flags & USE_UID		? "UID "	: "",
								message_set_string.CStr(),
								m_flags & ADD_FLAGS		? "+"		: "",
								m_flags & REMOVE_FLAGS	? "-"		: "",
								m_flags & SILENT		? ".SILENT" : "",
								GetStoreFlags(tmp_string));
}


/***********************************************************************************
 **
 **
 ** Store::GetStoreFlags
 ***********************************************************************************/
const char* ImapCommands::Store::GetStoreFlags(OpString8& tmp_string)
{
	if (m_flags & FLAG_ANSWERED)
		OpStatus::Ignore(tmp_string.Append("\\Answered"));
	if (m_flags & FLAG_FLAGGED)
		OpStatus::Ignore(tmp_string.AppendFormat("%s\\Flagged", tmp_string.HasContent() ? " " : ""));
	if (m_flags & FLAG_SEEN)
		OpStatus::Ignore(tmp_string.AppendFormat("%s\\Seen", tmp_string.HasContent() ? " " : ""));
	if (m_flags & FLAG_DELETED)
		OpStatus::Ignore(tmp_string.AppendFormat("%s\\Deleted", tmp_string.HasContent() ? " " : ""));
	if (m_flags & KEYWORDS)
		OpStatus::Ignore(tmp_string.AppendFormat("%s%s", tmp_string.HasContent() ? " " : "", m_keyword.CStr()));

	return tmp_string.CStr();
}


///////////////////////////////////////////
//            MessageDelete
///////////////////////////////////////////

/***********************************************************************************
 ** Permanently deleting messages
 ** - Mark message deleted
 ** - If UIDPLUS available
 **		- If IMAP Trash folder
 **			 1) copy to trash (with SpecialExpungeCopy)
 **			 2) flag as \deleted in source folder and UID expunge from source folder
 **			 3) flag as \deleted in trash folder and UID expunge from trash folder (done in SpecialExpungeCopy::OnCopyUID)
 **		- Else
 **			1) flag as \deleted
 **			2) do UID EXPUNGE
 ** - Else
 **     0) Search for messages that are deleted and have a UID that's not in the set we're deleting
 **        i.e. the messages in trash that we don't want to delete
 **     1) Remove the \Deleted flag from the messages found in 0)
 **     2) Do an EXPUNGE
 **     3) Add the \Deleted flag back to the messages found in 0)
 **
 ** MessageDelete::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::MessageDelete::GetExpandedQueue(IMAP4& protocol)
{
	// Flag messages as \deleted 
	OpAutoPtr<ImapCommands::Store> flag_as_deleted (OP_NEW(ImapCommands::Store, (m_mailbox, 0, 0,
											USE_UID | Store::ADD_FLAGS | Store::SILENT | Store::FLAG_DELETED)));

	if (!flag_as_deleted.get() || OpStatus::IsError(flag_as_deleted->GetMessageSet().Copy(m_message_set)))
		return NULL;

	// Check if UIDPLUS is available
	if (protocol.GetCapabilities() & ImapFlags::CAP_UIDPLUS)
	{
		// do UID EXPUNGE to remove the message from the source folder
		ImapCommands::UidExpunge * uid_expunge = OP_NEW(ImapCommands::UidExpunge, (m_mailbox, 0, 0));

		if (!uid_expunge || OpStatus::IsError(uid_expunge->GetMessageSet().Copy(m_message_set)))
		{
			OP_DELETE(uid_expunge);
			return NULL;
		}

		uid_expunge->DependsOn(flag_as_deleted.get(), protocol);

		if (m_trash_folder && m_mailbox != m_trash_folder)
		{
			// copy to trash
			ImapCommands::SpecialExpungeCopy *special_expunge_copy  = OP_NEW(ImapCommands::SpecialExpungeCopy, (m_mailbox, m_trash_folder, 0, 0));

			if (!special_expunge_copy || OpStatus::IsError(special_expunge_copy->GetMessageSet().Copy(m_message_set)))
			{
				OP_DELETE(special_expunge_copy);
				return NULL;
			}

			flag_as_deleted->DependsOn(special_expunge_copy, protocol);
			flag_as_deleted.release();
			return special_expunge_copy;
		}

	}
	else
	{
		// Search for messages that are deleted and have a UID that's not in the set we're deleting
		ImapCommands::Search* search = OP_NEW(ImapCommands::Search, (m_mailbox, 0, 0,
				Search::FOR_UID | Search::BY_UID | Search::NOT | Search::DELETED));

		if (!search || OpStatus::IsError(search->GetMessageSet().Copy(m_message_set)))
			return NULL;

		search->DependsOn(flag_as_deleted.get(), protocol);

		// Remove the \Deleted flag from the messages found in 1)
		ImapCommandItem* undelete = OP_NEW(ImapCommands::StoreResults, (m_mailbox,
				USE_UID | Store::REMOVE_FLAGS | Store::SILENT | Store::FLAG_DELETED));
		if (!undelete)
			return NULL;

		undelete->DependsOn(search, protocol);

		// Do an EXPUNGE
		ImapCommandItem* expunge = OP_NEW(ImapCommands::Expunge, (m_mailbox));
		if (!expunge)
			return NULL;

		expunge->DependsOn(undelete, protocol);

		// Add the \Deleted flag back to the messages found in 1)
		ImapCommandItem* add_delete = OP_NEW(ImapCommands::StoreResults, (m_mailbox,
				USE_UID | Store::ADD_FLAGS | Store::SILENT | Store::FLAG_DELETED));
		if (!add_delete)
			return NULL;

		add_delete->DependsOn(expunge, protocol);
	}

	return flag_as_deleted.release();
}


/***********************************************************************************
 **
 **
 ** MessageDelete::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageDelete::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));

	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		OpString8 internet_location;
		if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetStore()->GetMessageInternetLocation(message_ids.Get(i), internet_location)))
			RETURN_IF_ERROR(offline_log.RemoveMessage(internet_location));
	}

	return OpStatus::OK;
}

///////////////////////////////////////////
//            UidExpunge
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** UidExpunge::AppendCommand
 ***********************************************************************************/
OP_STATUS ImapCommands::UidExpunge::AppendCommand(OpString8& command, IMAP4& protocol)
{
	OpString8 message_set_string;
	RETURN_IF_ERROR(m_message_set.GetString(message_set_string));

	return command.AppendFormat("UID EXPUNGE %s", message_set_string.CStr());
}

///////////////////////////////////////////
//            SpecialDeleteCopy
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** SpecialDeleteCopy::OnCopyUid
 ***********************************************************************************/
OP_STATUS ImapCommands::SpecialExpungeCopy::OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set)
{
	OpStatus::Ignore(MoveCopy::OnCopyUid(protocol, uid_validity, source_set, dest_set));

	ImapCommands::MessageSet destination_set;

	for (unsigned i = 0; i < dest_set.GetCount(); i++)
	{
		destination_set.InsertRange(dest_set.Get(i), dest_set.Get(i));
	}

	// permanently delete the messages from trash
	OpAutoPtr<ImapCommands::MessageDelete> delete_messages (OP_NEW(ImapCommands::MessageDelete, (m_to_mailbox, 0, 0)));

	if (!delete_messages.get() || OpStatus::IsError(delete_messages->GetMessageSet().Copy(destination_set)))
		return OpStatus::ERR;

	return protocol.GetBackend().GetConnection(m_to_mailbox).InsertCommand(delete_messages.release());
}

///////////////////////////////////////////
//            MessageMarkAsDeleted
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** MessageMarkAsDeleted::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::MessageMarkAsDeleted::GetExpandedQueue(IMAP4& protocol)
{
	if (m_trash_folder)
	{
		OpAutoPtr<ImapCommands::SpecialMarkAsDeleteCopy> copy_to_trash (OP_NEW(ImapCommands::SpecialMarkAsDeleteCopy, (m_mailbox, m_trash_folder, 0, 0)));
		
		if (!copy_to_trash.get() || OpStatus::IsError(copy_to_trash->GetMessageSet().Copy(m_message_set)))
			return NULL;

		ImapCommands::MessageDelete* delete_command = OP_NEW(ImapCommands::MessageDelete, (m_mailbox, 0, 0));

		if (!delete_command || OpStatus::IsError(delete_command->GetMessageSet().Copy(m_message_set)))
		{
			OP_DELETE(delete_command);
			return NULL;
		}

		delete_command->DependsOn(copy_to_trash.get(), protocol);
					
		return copy_to_trash.release();
	}
	else
	{
		ImapCommands::Store* queue = OP_NEW(ImapCommands::Store, (m_mailbox, 0, 0,
			ImapCommands::USE_UID | ImapCommands::Store::ADD_FLAGS | Store::SILENT | ImapCommands::Store::FLAG_DELETED | ImapCommands::Store::FLAG_SEEN));

		if (!queue || OpStatus::IsError(queue->GetMessageSet().Copy(m_message_set)))
		{
			OP_DELETE(queue);
			return NULL;
		}
		return queue;
	}
}

/***********************************************************************************
 **
 **
 ** MessageMarkAsDeleted::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageMarkAsDeleted::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));

	return offline_log.RemoveMessages(message_ids, FALSE);
}

///////////////////////////////////////////
//            SpecialMarkAsDeleteCopy
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** SpecialMarkAsDeleteCopy::OnCopyUid
 ***********************************************************************************/
OP_STATUS ImapCommands::SpecialMarkAsDeleteCopy::OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set)
{
	OpStatus::Ignore(MoveCopy::OnCopyUid(protocol, uid_validity, source_set, dest_set));
	
	// mark the messages as \seen in the Trash folder
	OpAutoPtr<ImapCommands::Store> mark_seen (OP_NEW(ImapCommands::Store, (m_to_mailbox, 0, 0,
			ImapCommands::USE_UID | ImapCommands::Store::ADD_FLAGS | Store::SILENT | ImapCommands::Store::FLAG_SEEN)));
	
	RETURN_OOM_IF_NULL(mark_seen.get());

	for (unsigned i = 0; i < dest_set.GetCount(); i++)
	{
		RETURN_IF_ERROR(mark_seen->GetMessageSet().InsertRange(dest_set.Get(i), dest_set.Get(i)));
	}

	return protocol.GetBackend().GetConnection(m_to_mailbox).InsertCommand(mark_seen.release());
}

///////////////////////////////////////////
//            MessageMarkAsSpam
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** MessageMarkAsSpam::GetExpandedQueue
 ***********************************************************************************/

ImapCommandItem* ImapCommands::MessageMarkAsSpam::GetExpandedQueue(IMAP4& protocol)
{
	const char* keyword_spam = "$Junk";
	const char* keyword_not_spam = "$NotJunk";

	OpAutoPtr<ImapCommands::Store> remove_keyword  (OP_NEW(ImapCommands::Store, (m_mailbox, 0, 0,
					ImapCommands::USE_UID | ImapCommands::Store::REMOVE_FLAGS | ImapCommands::Store::SILENT | ImapCommands::Store::KEYWORDS, m_mark_as_spam ? keyword_not_spam : keyword_spam)));

	OpAutoPtr<ImapCommands::Store> add_keyword (OP_NEW(ImapCommands::Store, (m_mailbox, 0, 0,
		ImapCommands::USE_UID | ImapCommands::Store::ADD_FLAGS | ImapCommands::Store::SILENT | ImapCommands::Store::KEYWORDS, m_mark_as_spam ? keyword_spam : keyword_not_spam)));

			
	if (!add_keyword.get() || !remove_keyword.get() || 
		OpStatus::IsError(remove_keyword->GetMessageSet().Copy(m_message_set)) || 
		OpStatus::IsError(add_keyword->GetMessageSet().Copy(m_message_set)))
		return NULL;

	add_keyword->DependsOn(remove_keyword.get(), protocol);

	if (m_to_folder)
	{
		ImapCommands::Move * move_to_folder = OP_NEW(ImapCommands::Move, (m_mailbox, m_to_folder, 0, 0));
		if (!move_to_folder || OpStatus::IsError(move_to_folder->GetMessageSet().Copy(m_message_set)))
		{
			OP_DELETE(move_to_folder);
			return NULL;
		}
		move_to_folder->DependsOn(add_keyword.get(), protocol);
	}

	add_keyword.release();
	return remove_keyword.release();
}

/***********************************************************************************
 **
 **
 ** MessageMarkAsSpam::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageMarkAsSpam::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));
	return offline_log.MarkMessagesAsSpam(message_ids, m_mark_as_spam);
}

///////////////////////////////////////////
//            MessageUndelete
///////////////////////////////////////////

/***********************************************************************************
 **
 **
 ** MessageUndelete::GetExpandedQueue
 ***********************************************************************************/
ImapCommandItem* ImapCommands::MessageUndelete::GetExpandedQueue(IMAP4& protocol)
{
	OpAutoPtr<ImapCommands::Store> remove_deleted_flag (OP_NEW(ImapCommands::Store, (m_mailbox, 0, 0,
						ImapCommands::USE_UID | ImapCommands::Store::REMOVE_FLAGS | Store::SILENT | ImapCommands::Store::FLAG_DELETED)));
	
	RETURN_VALUE_IF_NULL(remove_deleted_flag.get(), NULL);
	RETURN_VALUE_IF_ERROR(remove_deleted_flag->GetMessageSet().Copy(m_message_set), NULL);

	if (m_to_folder)
	{
		OpAutoPtr<ImapCommands::Move> move_from_trash_to_inbox (OP_NEW(ImapCommands::Move, (m_mailbox, m_to_folder, 0, 0)));

		RETURN_VALUE_IF_NULL(move_from_trash_to_inbox.get(), NULL);
		RETURN_VALUE_IF_ERROR(move_from_trash_to_inbox->GetMessageSet().Copy(m_message_set), NULL);
		move_from_trash_to_inbox->DependsOn(remove_deleted_flag.get(), protocol);
		move_from_trash_to_inbox.release();
	}

	return remove_deleted_flag.release();
}


/***********************************************************************************
 **
 **
 ** MessageUndelete::WriteToOfflineLog
 ***********************************************************************************/
OP_STATUS ImapCommands::MessageUndelete::WriteToOfflineLog(OfflineLog& offline_log)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(PrepareSetForOffline(message_ids));
	return offline_log.MessagesMovedFromTrash(message_ids);
}
