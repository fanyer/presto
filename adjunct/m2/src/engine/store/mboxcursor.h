// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef MBOXCURSOR_H
#define MBOXCURSOR_H

#include "adjunct/m2/src/engine/store/mboxstore.h"
#include "modules/search_engine/Cursor.h"


/** @brief A store that saves mails into a binary compressed file
  * @author Arjan van Leeuwen
  *
  * This store saves compressed messages to a BSCursor.
  * Messages are saved in store/content.dat
  */
class MboxCursor : public MboxStore
{
public:
	virtual ~MboxCursor();

	/** Initializes this message store. No other functions are called before this is called.
	  */
	OP_STATUS Init(const OpStringC& store_path);

	/** Gets type of store
	  * @return Type of store
	  */
	AccountTypes::MboxType GetType() { return AccountTypes::MBOX_CURSOR; }

	/** Gets features supported by this store
	  * @return Bitwise OR of StoreFeatures values
	  */
	int GetFeatures() { return UPDATE_FULL_MESSAGES; }

	/** Add a new message to the store
	  * @param message Message to add
	  * @param mbx_data Should be set to new mbx_data if save was successful
	  * @return OpStatus::OK if save was successful, error codes otherwise
	  */
	OP_STATUS AddMessage(StoreMessage& message, INT64& mbx_data) { return UpdateMessage(message, mbx_data); }

	/** Fully update an existing message in the store (overwrites existing message)
	  * @param message Message to update
	  * @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise. Might be changed by this function to have new mbx_data.
	  * @return OpStatus::OK if save was successful, OpStatus::ERR_NOT_SUPPORTED if not supported for this store, error codes otherwise
	  */
	OP_STATUS UpdateMessage(StoreMessage& message, INT64& mbx_data);

	/** Update the status of an existing message in the store
	  * @param message Message to update
	  * @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise. Might be changed by this function to have new mbx_data.
	  * @return OpStatus::OK if save was successful, error codes otherwise
	  */
	OP_STATUS UpdateMessageStatus(StoreMessage& message, INT64& mbx_data) { return OpStatus::OK; }

	/** Get a message from the store
	  * @param mbx_data mbx_data that was saved with the message
	  * @param message Where to place the message if found, prefilled with ID, account ID and sent date
	  * @param override Whether to override data already found in message with data from raw file
	  * @return OpStatus::OK if message was found and retrieved, OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
	  */
	OP_STATUS GetMessage(INT64 mbx_data, StoreMessage& message, BOOL override);

	/** Remove a message from the store
	  * @param mbx_data mbx_data that was saved with the message
	  * @param id Message to remove
	  * @param account_id Account of this message
	  * @param sent_date Time when message was sent
	  * @param draft Whether this was a draft
	  * @return OpStatus::OK if delete was successful or if message didn't exist, error codes otherwise
	  */
	OP_STATUS RemoveMessage(INT64 mbx_data, message_gid_t id, UINT16 account_id, time_t sent_date, BOOL draft = FALSE);

	BOOL IsCached(message_gid_t id) { return TRUE; }

	OP_STATUS CommitData();

private:
	enum MboxCursorField
	{
		MBOX_SIZE,
		MBOX_COMPRESSED_CONTENT
	};

	BSCursor				m_mboxdb;
	BlockStorage			m_mboxdb_backend;
};

#endif // MBOXCURSOR_H
