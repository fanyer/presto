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

#define SYNC_LOG_VERSION 1

#include "adjunct/m2/src/engine/offlinelog.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/store.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"


/***********************************************************************************
 ** Constructor
 **
 ** OfflineLog::OfflineLog
 **
 ***********************************************************************************/
OfflineLog::OfflineLog(Account& account)
  : m_log_file(NULL)
  , m_account(account)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** OfflineLog::~OfflineLog
 **
 ***********************************************************************************/
OfflineLog::~OfflineLog()
{
	OP_DELETE(m_log_file);
}


/***********************************************************************************
 ** Replay the log that is on disk, re-executing all actions
 **
 ** OfflineLog::ReplayLog
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ReplayLog()
{
	BOOL           exists;
	INT32          command;
	UINT32         index1;
	UINT32         index2;
	OpINT32Vector  message_ids;
	OpString8      internet_location;

	// Don't replay log when we're still in offline mode
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return OpStatus::OK;

	// Open file
	RETURN_IF_ERROR(PrepareForRead(exists));
	if (!exists)
		return OpStatus::OK;

	// Read all commands from file that can be read. We ignore the result, we want to continue
	while (OpStatus::IsSuccess(ReadCommand(command, index1, index2, message_ids, internet_location)))
	{
		switch(command)
		{
			case OFFLINE_INSERT:
				if (message_ids.GetCount() > 0)
					OpStatus::Ignore(m_account.InsertMessage(message_ids.Get(0), index1));
				break;
			case OFFLINE_MOVE:
				OpStatus::Ignore(m_account.MoveMessages(message_ids, index1, index2));
				break;
			case OFFLINE_COPY:
				OpStatus::Ignore(m_account.CopyMessages(message_ids, index1, index2));
				break;
			case OFFLINE_EXPUNGE:
				OpStatus::Ignore(m_account.RemoveMessage(internet_location));
				break;
			case OFFLINE_MARK_READ:
				OpStatus::Ignore(m_account.ReadMessages(message_ids, TRUE));
				break;
			case OFFLINE_MARK_UNREAD:
				OpStatus::Ignore(m_account.ReadMessages(message_ids, FALSE));
				break;
			case OFFLINE_MARK_FLAGGED:
				OpStatus::Ignore(m_account.FlaggedMessage(message_ids.Get(0), TRUE));
				break;
			case OFFLINE_MARK_UNFLAGGED:
				OpStatus::Ignore(m_account.FlaggedMessage(message_ids.Get(0), FALSE));
				break;
			case OFFLINE_MARK_REPLIED:
				if (message_ids.GetCount() > 0)
					OpStatus::Ignore(m_account.ReplyToMessage(message_ids.Get(0)));
				break;
			case OFFLINE_TAG:
				// TODO
				break;
			case OFFLINE_DELETE:
				OpStatus::Ignore(m_account.RemoveMessages(message_ids, FALSE));
				break;
			case OFFLINE_UNDELETE:
				OpStatus::Ignore(m_account.MessagesMovedFromTrash(message_ids));
				break;
			case OFFLINE_REMOVE_KEYWORD:
				for (unsigned i = 0; i < message_ids.GetCount(); i++)
					OpStatus::Ignore(m_account.KeywordRemoved(message_ids.Get(i), internet_location));
				break;
			case OFFLINE_ADD_KEYWORD:
				for (unsigned i = 0; i < message_ids.GetCount(); i++)
					OpStatus::Ignore(m_account.KeywordAdded(message_ids.Get(i), internet_location));
				break;
			case OFFLINE_MARK_NOT_SPAM:
			case OFFLINE_MARK_SPAM:
				OpStatus::Ignore(m_account.MarkMessagesAsSpam(message_ids, command == OFFLINE_MARK_SPAM, TRUE));
				break;
			default:
				OP_ASSERT(!"Unknown command in synchronization file");
		}
	}

	// We're done, either it was end of file or it couldn't be read for some reason. Remove
	// the processed log file.
	return ReadFinished();
}


/***********************************************************************************
 ** Insert a message into a folder
 **
 ** OfflineLog::InsertMessage
 ** @param message_id Message to insert
 ** @param destination_index Destination folder index
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::InsertMessage(message_gid_t message_id, index_gid_t destination_index)
{
	return WriteCommand(OFFLINE_INSERT, destination_index, 0, message_id, "");
}


/***********************************************************************************
 ** Remove messages
 **
 ** OfflineLog::RemoveMessages
 ** @param message_ids Messages to remove
 ** @param permanently Whether messages are permanently removed or not (move to trash)
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently)
{
	if (permanently)
	{
		// When messages are permanently deleted, they are no longer in the local store. Save all the internet locations.

		Store* store = MessageEngine::GetInstance()->GetStore();

		for (unsigned i = 0; i < message_ids.GetCount(); i++)
		{
			OpString8 internet_location;

			if (OpStatus::IsSuccess(store->GetMessageInternetLocation(message_ids.Get(i), internet_location)))
				RETURN_IF_ERROR(RemoveMessage(internet_location));
		}
	}
	else
	{
		return WriteCommand(OFFLINE_DELETE, 0, 0, message_ids, "");
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Remove a message permanently by internet location
 **
 ** OfflineLog::RemoveMessage
 ** @param internet_location Internet location of message to remove
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::RemoveMessage(const OpStringC8& internet_location)
{
	return WriteCommand(OFFLINE_EXPUNGE, 0, 0, 0, internet_location);
}


/***********************************************************************************
 ** Move messages
 **
 ** OfflineLog::MoveMessages
 ** @param message_ids Messages to move
 ** @param source_index_id Source
 ** @param destination_index_id Destination
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	return WriteCommand(OFFLINE_MOVE, source_index_id, destination_index_id, message_ids, "");
}


/***********************************************************************************
 ** Copy messages
 **
 ** OfflineLog::MoveMessages
 ** @param message_ids Messages to copy
 ** @param source_index_id Source
 ** @param destination_index_id Destination
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id)
{
	return WriteCommand(OFFLINE_COPY, source_index_id, destination_index_id, message_ids, "");
}


/***********************************************************************************
 ** Mark messages as read or unread
 **
 ** OfflineLog::ReadMessages
 ** @param message_ids Messages to mark
 ** @param read Whether to mark as read or unread
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ReadMessages(const OpINT32Vector& message_ids, BOOL read)
{
	return WriteCommand(read ? OFFLINE_MARK_READ : OFFLINE_MARK_UNREAD, 0, 0, message_ids, "");
}


/***********************************************************************************
 ** Set flag Flagged for message
 **
 ** OfflineLog::FlaggedMessage
 ** @param message_ids Messages to set
 ** @param read Whether to set as flagged or unflagged
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::FlaggedMessage(message_gid_t message_id, BOOL flagged)
{
	return WriteCommand(flagged ? OFFLINE_MARK_FLAGGED : OFFLINE_MARK_UNFLAGGED, 0, 0, message_id, "");
}


/***********************************************************************************
 ** Signal that a message has been replied to
 **
 ** OfflineLog::ReplyToMessage
 ** @param message_id Message that has been replied to
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ReplyToMessage(message_gid_t message_id)
{
	return WriteCommand(OFFLINE_MARK_REPLIED, 0, 0, message_id, "");
}


/***********************************************************************************
 ** Empty the trash (Expunge all messages in trash)
 **
 ** OfflineLog::EmptyTrash
 ** @param done_removing_messages Whether this was called before or after removing the messages from local storage
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::EmptyTrash(BOOL done_removing_messages)
{
	if (done_removing_messages)
		return OpStatus::OK;

	// Get messages in trash for this account and remove them
	Index* trash = MessageEngine::GetInstance()->GetIndexById(IndexTypes::TRASH);
	if (!trash)
		return OpStatus::ERR_NULL_POINTER;

	for (INT32SetIterator it(trash->GetIterator()); it; it++)
	{
		message_gid_t message_id = it.GetData();
		Message message;

		if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetStore()->GetMessage(message, message_id)) &&
			message.GetAccountId() == m_account.GetAccountId())
		{
			RETURN_IF_ERROR(WriteCommand(OFFLINE_EXPUNGE, 0, 0, 0, message.GetInternetLocation()));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Undelete messages
 **
 ** OfflineLog::MessagesMovedFromTrash
 ** @param message_ids Messages to undelete
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::MessagesMovedFromTrash(const OpINT32Vector& message_ids)
{
	return WriteCommand(OFFLINE_UNDELETE, 0, 0, message_ids, "");
}


OP_STATUS OfflineLog::MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam)
{
	return WriteCommand(is_spam ? OFFLINE_MARK_SPAM : OFFLINE_MARK_NOT_SPAM, 0, 0, message_ids, "");
}

/***********************************************************************************
 ** A keyword has been added to a message
 **
 ** OfflineLog::KeywordAdded
 ** @param message_id Message to which a keyword has been added
 ** @param keyword Keyword that was added to the message
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::KeywordAdded(message_gid_t message_id, const OpStringC8& keyword)
{
	return WriteCommand(OFFLINE_ADD_KEYWORD, 0, 0, message_id, keyword);
}


/***********************************************************************************
 ** A keyword has been removed from a message
 **
 ** OfflineLog::KeywordRemoved
 ** @param message_id Message from which a keyword has been removed
 ** @param keyword Keyword that was removed from the message
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword)
{
	return WriteCommand(OFFLINE_REMOVE_KEYWORD, 0, 0, message_id, keyword);
}


/***********************************************************************************
 ** Read the next offline command from disk, run PrepareForRead() before using this
 **
 ** OfflineLog::WriteCommand
 ** @param command Command read
 ** @param index1 First index
 ** @param index2 Second index
 ** @param message_id Message ID
 ** @param internet_location Internet location of message
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ReadCommand(INT32&		  command,
								  UINT32&		  index1,
								  UINT32&		  index2,
								  OpINT32Vector&  message_ids,
								  OpString8&	  internet_location)
{
	UINT32       message_count;
	UINT32		 loc_length;

	// Reset variables
	message_ids.Clear();
	internet_location.Empty();

	// Read command, index1, index2, message count
	RETURN_IF_ERROR(Read(&command,		 4));
	RETURN_IF_ERROR(Read(&index1,		 4));
	RETURN_IF_ERROR(Read(&index2,		 4));
	RETURN_IF_ERROR(Read(&message_count, 4));

	// Read message ids
	for (unsigned i = 0; i < message_count; i++)
	{
		INT32 message_id;

		RETURN_IF_ERROR(Read(&message_id, 4));
		RETURN_IF_ERROR(message_ids.Add(message_id));
	}

	// Read internet location string
	RETURN_IF_ERROR(Read(&loc_length,	 4));
	if (loc_length > 0)
	{
		// Reserve space for string
		char* buf = internet_location.Reserve(loc_length - 1); // int_buf includes terminating NULL character
		if (!buf)
			return OpStatus::ERR_NO_MEMORY;

		// Read string from file
		RETURN_IF_ERROR(Read(buf, loc_length));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Write an offline command to disk, DON'T run PrepareForWrite() before using this
 **
 ** OfflineLog::WriteCommand
 ** @param command Command to write
 ** @param index1 First index (if needed)
 ** @param index2 Second index (if needed)
 ** @param message_id Message ID (if needed)
 ** @param internet_location Internet location of message (if needed)
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::WriteCommand(OfflineCommand command, UINT32 index1, UINT32 index2, INT32 message_id, const OpStringC8& internet_location)
{
	OpINT32Vector message_ids;

	RETURN_IF_ERROR(message_ids.Add(message_id));

	return WriteCommand(command, index1, index2, message_ids, internet_location);
}


/***********************************************************************************
 ** Write an offline command to disk, DON'T run PrepareForWrite() before using this
 **
 ** OfflineLog::WriteCommand
 ** @param command Command to write
 ** @param index1 First index (if needed)
 ** @param index2 Second index (if needed)
 ** @param message_ids Message IDs (if needed)
 ** @param internet_location Internet location of message (if needed)
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::WriteCommand(OfflineCommand		command,
								   UINT32				index1,
								   UINT32				index2,
								   const OpINT32Vector& message_ids,
								   const OpStringC8&	internet_location)
{
	// Open file
	RETURN_IF_ERROR(PrepareForWrite());

	// Format of a 'record':
	//   command             INT32   One of OfflineCommand
	//   index1              UINT32   Command argument, id of index
	//   index2              UINT32   Command argument, id of index
	//   message_count       UINT32  Number of message ids to follow
	//   message_id          INT32 * message_count Message Ids
	//   internet_loc_length UINT32  Length of internet_location including terminating NULL-character
	//   internet_location   NULL-terminated string

	INT32  int_command         = command;
	UINT32 message_count       = message_ids.GetCount();
	UINT32 internet_loc_length = internet_location.HasContent() ? internet_location.Length() + 1 : 0; // includes terminating NULL-character

	// Write integer values
	RETURN_IF_ERROR(m_log_file->SetFilePos(0,	SEEK_FROM_END));
	RETURN_IF_ERROR(m_log_file->Write(&int_command,			4));
	RETURN_IF_ERROR(m_log_file->Write(&index1,				4));
	RETURN_IF_ERROR(m_log_file->Write(&index2,				4));
	RETURN_IF_ERROR(m_log_file->Write(&message_count,		4));

	// Write message ids
	for (unsigned i = 0; i < message_count; i++)
	{
		INT32 message_id = message_ids.Get(i);

		RETURN_IF_ERROR(m_log_file->Write(&message_id,		4));
	}

	// Write internet location
	RETURN_IF_ERROR(m_log_file->Write(&internet_loc_length,	4));

	if (internet_loc_length > 0)
	{
		RETURN_IF_ERROR(m_log_file->Write(internet_location.CStr(), internet_loc_length));
	}

	// Close file
	return WriteFinished();
}


/***********************************************************************************
 ** Prepare log file for writing - caller can write if this doesn't return error
 **
 ** OfflineLog::PrepareForWrite
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::PrepareForWrite()
{
	BOOL exists;

	if (!m_log_file)
		RETURN_IF_ERROR(ConstructFile());

	// Check if file already exists
	RETURN_IF_ERROR(m_log_file->Exists(exists));

	// Open file for reading/writing
	OP_ASSERT(!m_log_file->IsOpen());
	RETURN_IF_ERROR(m_log_file->Open(OPFILE_APPEND|OPFILE_READ));

	if (!exists)
	{
		// Write version number if this is a new file
		INT32 version = SYNC_LOG_VERSION;
		RETURN_IF_ERROR(m_log_file->Write(&version, 4));
	}
	else
	{
		// Check version number if this is an existing file
		RETURN_IF_ERROR(RemoveIfIncorrectVersion(exists));
		if (!exists)
			return PrepareForWrite();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Call this after writing a record to the log
 **
 ** OfflineLog::WriteFinished
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::WriteFinished()
{
	return m_log_file->Close();
}


/***********************************************************************************
 ** Prepare log file for reading - caller can read if this doesn't return error
 **
 ** OfflineLog::PrepareForRead
 ** @param exists Whether the log exists
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::PrepareForRead(BOOL& exists)
{
	if (!m_log_file)
		RETURN_IF_ERROR(ConstructFile());

	OP_ASSERT(!m_log_file->IsOpen());

	// Check if file exists
	RETURN_IF_ERROR(m_log_file->Exists(exists));

	if (exists)
	{
		// Open, but delete if this is incorrect version
		RETURN_IF_ERROR(m_log_file->Open(OPFILE_READ));
		return RemoveIfIncorrectVersion(exists);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Call this after finishing reading from the log, will remove the log
 **
 ** OfflineLog::ReadFinished
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ReadFinished()
{
	RETURN_IF_ERROR(m_log_file->Close());
	return m_log_file->Delete();
}


/***********************************************************************************
 ** Construct log file, call before doing anything else on log file
 **
 ** OfflineLog::ConstructFile
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::ConstructFile()
{
	// Get path
	OpString path;

	OpString mail_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
	RETURN_IF_ERROR(path.AppendFormat(UNI_L("%s%csync%coffline_account%d.log"),
									  mail_folder.CStr(),
									  PATHSEPCHAR,
									  PATHSEPCHAR,
									  m_account.GetAccountId()));

	// Construct file
	m_log_file = OP_NEW(OpFile, ());
	if (!m_log_file)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = m_log_file->Construct(path.CStr());
	if (OpStatus::IsError(ret))
	{
		OP_DELETE(m_log_file);
		m_log_file = NULL;
	}

	return ret;
}


/***********************************************************************************
 ** Checks the log file and removes it if the version number doesn't match the
 ** current one
 **
 ** OfflineLog::RemoveIfIncorrectVersion
 ** @param exists Whether the file still exists after executing this function
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::RemoveIfIncorrectVersion(BOOL& exists)
{
	INT32		 version_number;
	OpFileLength bytes_read;

	// Go to start of file
	RETURN_IF_ERROR(m_log_file->SetFilePos(0));

	// Read version number
	RETURN_IF_ERROR(m_log_file->Read(&version_number, 4, &bytes_read));

	// Check version number
	if (bytes_read != 4 || version_number != SYNC_LOG_VERSION)
	{
		// Version number was incorrect, remove file
		RETURN_IF_ERROR(m_log_file->Close());
		RETURN_IF_ERROR(m_log_file->Delete());
		exists = FALSE;
	}
	else
	{
		exists = TRUE;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Read data from log file, return error if read failed or if EOF
 **
 ** OfflineLog::Read
 **
 ***********************************************************************************/
OP_STATUS OfflineLog::Read(void* data, OpFileLength data_len)
{
	OpFileLength bytes_read;

	RETURN_IF_ERROR(m_log_file->Read(data, data_len, &bytes_read));
	if (bytes_read != data_len)
		return OpStatus::ERR;

	return OpStatus::OK;
}


#endif // M2_SUPPORT
