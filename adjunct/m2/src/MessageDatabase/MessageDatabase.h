/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MESSAGE_DATABASE_H
#define MESSAGE_DATABASE_H

#include "adjunct/m2/src/include/defs.h"

class StoreMessage;
class CommitListener;
class Index;
class SearchGroupable;

/** @brief Allows saving and retrieving of messages and various attributes in a database
  *
  * Unique messages are identified by a message ID. One message can belong to
  * one or more indexes (aka access points). Indexes are identified by an index
  * ID.
  */
class MessageDatabase
{
public:
	virtual ~MessageDatabase() {}

	/** Add a message to the database
	  * @param message Message to add
	  */
	virtual OP_STATUS AddMessage(StoreMessage& message) = 0;

	/** Update metadata of an existing message (important headers and
	  * information that's not part of the MIME message)
	  * @param message Updated details for message
	  */
	virtual OP_STATUS UpdateMessageMetaData(StoreMessage& message) = 0;

	/** Update contents of an existing message (headers and body)
	  * @param message Updated details for message
	  */
	virtual OP_STATUS UpdateMessageData(StoreMessage& message) = 0;

	/** Remove a message from the database
	  * @param message_id Message to remove
	  */
	virtual OP_STATUS RemoveMessage(message_gid_t message_id) = 0;

	/** Get message details from the database
	  * @param message_id Which message to get
	  * @param full Whether to get the full message details and body (slower) or just essential
	  *             headers and metadata (faster)
	  * @param message Where to save message
	  */
	virtual OP_STATUS GetMessage(message_gid_t message_id, BOOL full, StoreMessage& message) = 0;

	/** To be called when a message is sent
	  * Moves message from outbox (changeable) to sent mail (static)
	  */
	virtual OP_STATUS MessageSent(message_gid_t message_id) = 0;

	/** Archive a message
	  * @param message_id Message to archive
	  */
	virtual OP_STATUS ArchiveMessage(message_gid_t message_id) = 0;

	/** Get an index (access point)
	  * @param index_id ID of index to get
	  * @return Index if found, or NULL on not found
	  */
	virtual Index* GetIndex(index_gid_t index_id) = 0;

	/** Remove an index (access point)
	  * @param index_id ID of index to remove
	  */
	virtual OP_STATUS RemoveIndex(index_gid_t index_id) = 0;

	/** Request a commit of all pending data
	  */
	virtual OP_STATUS RequestCommit() = 0;

	/** Commit pending changes to stable storage immediately
	  */
	virtual OP_STATUS Commit() = 0;

	/** Add a commit listener that will receive a message whenever the database
	  * finished committing
	  * @param listener Listener to add
	  */
	virtual OP_STATUS AddCommitListener(CommitListener* listener) = 0;

	/** Remove a commit listener that was previously added with AddCommitListener()
	  * @param listener Listener to remove
	  */
	virtual OP_STATUS RemoveCommitListener(CommitListener* listener) = 0;

	/** Group the various search_engine files (ie. SingleBTree, StringTable) so they are commited and rolled back together
	 */
	virtual void GroupSearchEngineFile(SearchGroupable &group_member) = 0;

	/** Whether the database has been completely loaded from disk
	  */
	virtual BOOL HasFinishedLoading() = 0;

	/** Listener function for when a message is made available in store
	 * @param message_id Message id of the message that was made available
	 * @param read whether it's marked as read or not
	 */
	virtual void OnMessageMadeAvailable(message_gid_t message_id, BOOL read) = 0;

	/** Listener function for when all messages are made available in store
	 */
	virtual void OnAllMessagesAvailable() = 0;
	
	/** Listener function for when a message is modified in store
	 *  @param message_id the changed message
	 */
	virtual void OnMessageChanged(message_gid_t message_id) = 0;
	
	/** Listener function for when a message body is modified in store
	 *  @param message_id the changed message
	 */
	virtual void OnMessageBodyChanged(StoreMessage& message) = 0;
	
	/** Listener function for when a message is marked as sent in store
	 *  @param message_id the changed message
	 */
	virtual void OnMessageMarkedAsSent(message_gid_t message_id) = 0;

	/** Listener function for when a message needs reindexing
	 *  @param message_id the changed message
	 */
	virtual void OnMessageNeedsReindexing(message_gid_t message_id) = 0;

	/** Whether the message database is initialized or not
	  */
	virtual bool IsInitialized() = 0;

	/** Obtain a message change lock, so that changes aren't broadcast until
	  * this lock is destroyed
	  */
	struct MessageChangeLock
	{
		virtual ~MessageChangeLock() {}
	};
	virtual MessageChangeLock* CreateMessageChangeLock() = 0;
};

#endif // MESSAGE_DATABASE_H
