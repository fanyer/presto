/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MESSAGE_STORE_H
#define MESSAGE_STORE_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/store/storemessage.h"

class MessageDatabase;
class StoreListener;

/** @brief A 'store' that can be used to save and retrieve the contents of MIME messages
  */
class MessageStore
{
public:
	virtual ~MessageStore() {}

	/**
	 * Adds a new message to the store
	 *
	 * If a message with this id already exists, it will be replaced (including
	 * the 'received' date of that message!)
	 *
	 * @param id Returns the ID of the message if adding was successful
	 * @param message Message to add
	 */
	virtual OP_STATUS AddMessage(message_gid_t& id, StoreMessage& message) = 0;

	/**
	 * Updates the given message metadata on disk with status information from the message
	 * in memory.
	 *
	 * @param message Message to update
	 */
	virtual OP_STATUS UpdateMessage(StoreMessage& message) = 0;

	/**
	 * Replaces local copy of message data
	 *
	 * @param message Message to update
	 */
	virtual OP_STATUS SetRawMessage(StoreMessage& message) = 0;

	/**
	 * Mark a message as removed in the database - the message can't be retrieved after
	 * executing this function. To actually remove, call RemoveMessage().
	 *
	 * @param id ID of message to mark as removed
	 */
	virtual OP_STATUS MarkAsRemoved(message_gid_t id) = 0;

	/**
	 * Get message metadata (important headers and all information that is
	 * not part of the MIME message)
	 *
	 * @param message Where to save retrieved message
	 * @param id ID of message to retrieve
	 */
	virtual OP_STATUS GetMessage(StoreMessage& message, message_gid_t id) = 0;

	/**
	 * Retrieves full message content from the local disk cache if
	 * available.
	 *
	 * @param message Message to retrieve and store output, should be initialized with a call to GetMessage()
	 */
	virtual OP_STATUS GetMessageData(StoreMessage& message) = 0;

	/**
	 * Deletes a message permanently
	 * @param id ID of message to permanently remove
	 */
	virtual OP_STATUS RemoveMessage(message_gid_t id) = 0;

	/** Commit data to disk
	 */
	virtual OP_STATUS CommitData() = 0;

	/** Read (cache) data from the main file
	  * @param startpos Where to start reading in the file (file position)
	  * @param blockcount How many blocks of data to read
	  * @return New position in the file after reading data or 0 when reading is finished
	  */
	virtual OpFileLength ReadData(OpFileLength startpos, int blockcount) = 0;

	/** Whether the database has been completely loaded from disk
	 */
	virtual BOOL HasFinishedLoading() = 0;

	/** Set a flag on a message
	 */
	virtual OP_STATUS SetMessageFlag(message_gid_t id, StoreMessage::Flags flag, BOOL value) = 0;

	/** Get a message flag
	 */
	virtual BOOL	  GetMessageFlag(message_gid_t id, StoreMessage::Flags flag) = 0;
};

#endif // MESSAGE_STORE_H
