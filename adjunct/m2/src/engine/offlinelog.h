//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef OFFLINE_LOG_H
#define OFFLINE_LOG_H

#include "adjunct/m2/src/include/defs.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opfile/opfile.h"

class Account;

class OfflineLog
{
public:
	/** Constructor
	  * @param account Which account this log belongs to
	  */
	OfflineLog(Account& account);

	/** Destructor
	  */
	~OfflineLog();

	/** Replay the log that is on disk, re-executing all actions
	  */
	OP_STATUS ReplayLog();

	/** Check if the offline log is busy
	  */
	BOOL	  IsBusy() const { return m_log_file && m_log_file->IsOpen(); }

	//----- Write actions

	/** Insert a message into a folder
	  * @param message_id Message to insert
	  * @param destination_index Destination folder index
	  */
	OP_STATUS InsertMessage(message_gid_t message_id, index_gid_t destination_index);

	/** Remove messages
	  * @param message_ids Messages to remove
	  * @param permanently Whether messages are permanently removed or not (move to trash)
	  */
	OP_STATUS RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently);

	/** Remove a message permanently by internet location
	  * @param internet_location Internet location of message to remove
	  */
	OP_STATUS RemoveMessage(const OpStringC8& internet_location);

	/** Move messages
	  * @param message_ids Messages to move
	  * @param source_index_id Source
	  * @param destination_index_id Destination
	  */
	OP_STATUS MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);

	/** Copy messages
	  * @param message_ids Messages to copy
	  * @param source_index_id Source
	  * @param destination_index_id Destination
	  */
	OP_STATUS CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);

	/** Mark messages as read or unread
	  * @param message_ids Messages to mark
	  * @param read Whether to mark as read or unread
	  */
	OP_STATUS ReadMessages(const OpINT32Vector& message_ids, BOOL read);

	/** Set flag Flagged for message
	  * @param message_ids Messages to set
	  * @param read Whether to set as flagged or unflagged
	  */
	OP_STATUS FlaggedMessage(message_gid_t message_id, BOOL flagged);

	/** Signal that a message has been replied to
	  * @param message_id Message that has been replied to
	  */
	OP_STATUS ReplyToMessage(message_gid_t message_id);

	/** Empty the trash (Expunge all messages in trash)
	  * @param done_removing_messages Whether this was called before or after removing the messages from local storage
	  */
	OP_STATUS EmptyTrash(BOOL done_removing_messages);

	/** Undelete messages
	  * @param message_ids Messages to undelete
	  */
	OP_STATUS MessagesMovedFromTrash(const OpINT32Vector& message_ids);

	/** Mark messages as spam or not
	  * @param message_ids Messages to mark
	  * @param whether they are spam or not
	  */
	OP_STATUS MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam);

	/** A keyword has been added to a message
	  * @param message_id Message to which a keyword has been added
	  * @param keyword Keyword that was added to the message
	  */
	OP_STATUS KeywordAdded(message_gid_t message_id, const OpStringC8& keyword);

	/** A keyword has been removed from a message
	  * @param message_id Message from which a keyword has been removed
	  * @param keyword Keyword that was removed from the message
	  */
	OP_STATUS KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword);

private:
	enum OfflineCommand
	{
		OFFLINE_INSERT = 1,
		OFFLINE_MOVE,
		OFFLINE_EXPUNGE,
		OFFLINE_MARK_READ,
		OFFLINE_MARK_UNREAD,
		OFFLINE_MARK_REPLIED,
		OFFLINE_TAG,
		OFFLINE_DELETE,
		OFFLINE_UNDELETE,
		OFFLINE_ADD_KEYWORD,
		OFFLINE_REMOVE_KEYWORD,
		OFFLINE_COPY,
		OFFLINE_MARK_SPAM,
		OFFLINE_MARK_NOT_SPAM,
		OFFLINE_MARK_FLAGGED,
		OFFLINE_MARK_UNFLAGGED,
	};

	OP_STATUS ReadCommand(INT32& command, UINT32& index1, UINT32& index2, OpINT32Vector& message_ids, OpString8& internet_location);
	OP_STATUS WriteCommand(OfflineCommand command, UINT32 index1, UINT32 index2, INT32 message_id, const OpStringC8& internet_location);
	OP_STATUS WriteCommand(OfflineCommand command, UINT32 index1, UINT32 index2, const OpINT32Vector& message_ids, const OpStringC8& internet_location);
	OP_STATUS PrepareForWrite();
	OP_STATUS WriteFinished();
	OP_STATUS PrepareForRead(BOOL& exists);
	OP_STATUS ReadFinished();
	OP_STATUS ConstructFile();
	OP_STATUS RemoveIfIncorrectVersion(BOOL& exists);
	OP_STATUS Read(void* data, OpFileLength data_len);

	OpFile*   m_log_file;
	Account&  m_account;
};

#endif // OFFLINE_LOG_H
