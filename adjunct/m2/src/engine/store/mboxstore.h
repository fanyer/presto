// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef MBOXSTORE_H
#define MBOXSTORE_H

#include "adjunct/m2/src/include/defs.h"


class StoreMessage;
class OpFile;

/** @brief Abstract class for 'raw' mail storage
  * @author Arjan van Leeuwen
  *
  * The store is able to handle different formats for 'raw' mail storage.
  * Every raw mail storage format should be implemented by inheriting from this class.
  * If you create your own mail storage class, remember to make an entry for it in
  * AccountTypes::MboxType.
  */
class MboxStore
{
public:
	enum StoreFeatures
	{
		UPDATE_FULL_MESSAGES = 1 << 0 ///< it's possible to update one existing message in this store using UpdateMessage()
	};

	virtual ~MboxStore() {}

	/** Initializes this message store. No other functions are called before this is called.
	  * @param store_path explisitly defines the mail root directory, if empty default to MAIL_ROOT_DIR
	  */
	virtual OP_STATUS Init(const OpStringC& store_path) = 0;

	/** Gets type of store
	  * @return Type of store
	  */
	virtual AccountTypes::MboxType GetType() = 0;

	/** Gets features supported by this store
	  * @return Bitwise OR of StoreFeatures values
	  */
	virtual int GetFeatures() = 0;

	/** Add a new message to the store
	  * @param message Message to add
	  * @param mbx_data Should be set to new mbx_data if save was successful
	  * @return OpStatus::OK if save was successful, error codes otherwise
	  */
	virtual OP_STATUS AddMessage(StoreMessage& message, INT64& mbx_data) = 0;

	/** Fully update an existing message in the store (overwrites existing message)
	  * @param message Message to update
	  * @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise. Might be changed by this function to have new mbx_data.
	  * @return OpStatus::OK if save was successful, OpStatus::ERR_NOT_SUPPORTED if not supported for this store, error codes otherwise
	  */
	virtual OP_STATUS UpdateMessage(StoreMessage& message, INT64& mbx_data) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** Update the status of an existing message in the store
	  * @param message Message to update
	  * @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise. Might be changed by this function to have new mbx_data.
	  * @return OpStatus::OK if save was successful, error codes otherwise
	  */
	virtual OP_STATUS UpdateMessageStatus(StoreMessage& message, INT64& mbx_data) = 0;

	/** Get a message from the store
	  * @param mbx_data mbx_data that was saved with the message
	  * @param message Where to place the message if found, prefilled with ID, account ID and sent date
	  * @param override Whether to override data already found in message with data from raw file
	  * @return OpStatus::OK if message was found and retrieved, OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
	  */
	virtual OP_STATUS GetMessage(INT64 mbx_data, StoreMessage& message, BOOL override = FALSE) = 0;

	/** Get a draft from the store, only implemented by MboxPerMail
	  * @param message Where to place the message if found
	  * @param id Message id to retrieve
	  * @return OpStatus::OK if message was found and retrieved, OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
	  */
	virtual OP_STATUS GetDraftMessage(StoreMessage& message, message_gid_t id) { return OpStatus::ERR; }

	/** Whether a message is cached in this store
	  * @param id Message to check for
	  * @return Whether the message is cached, or TRUE if the store doesn't have a cache
	  */
	virtual BOOL IsCached(message_gid_t id) = 0;

	/** Remove a message from the store
	  * @param mbx_data mbx_data that was saved with the message
	  * @param id Message to remove
	  * @param account_id Account of this message
	  * @param sent_date Time when message was sent
	  * @param draft Whether this was a draft
	  * @return OpStatus::OK if delete was successful or if message didn't exist, error codes otherwise
	  */
	virtual OP_STATUS RemoveMessage(INT64 mbx_data, message_gid_t id, UINT16 account_id, time_t sent_date, BOOL draft = FALSE) = 0;

	/** Commit pending data to the store (if necessary)
	  */
	virtual OP_STATUS CommitData() = 0;

	/** Reads an mbox-format message starting at the current position of the file
	  * @param message Where to save message
	  * @param file Where to read message from
	  * @param override Whether to override data already found in message with data from raw file
	  * @param import ignore message id and account id
	  */
	static OP_STATUS ReadMboxMessage(StoreMessage& message, OpFile& file, BOOL override, BOOL import = FALSE);

protected:

	/** Writes out a message to a specified file, not including mbox-format 'From ' line
	  * @param message Message to write out
	  * @param file Where to write
	  */
	static OP_STATUS WriteRawMessage(StoreMessage& message, OpFile& file);

	/** Reads a message starting at the current position of the file, not including mbox-format 'From ' line
	  * @param message Where to save message
	  * @param file Where to read message from
	  * @param override Whether to override data already found in message with data from raw file
	  * @param import ignore mesasge id and account id
	  */
	static OP_STATUS ReadRawMessage(StoreMessage& message, OpFile& file, BOOL override, BOOL import = FALSE);

	/** Writes out an mbox-format message to a specified file
	  * @param message Message to write out
	  * @param file Where to write
	  */
	static OP_STATUS WriteMboxMessage(StoreMessage& message, OpFile& file);

	/** Reads a 'From ' line from an mbox-format file, gets time received
	  * @param file Where to read line from
	  */
	static OP_STATUS ReadFromLine(OpFile& file);

	/** Creates a 'From ' line for an mbox-format file
	  * @param message Message for which to create line
	  * @param file Output
	  */
	static OP_STATUS WriteFromLine(StoreMessage& message, OpFile& file);

	/** Reads an 'X-Opera-Status: ' header from a file (Opera-specific header)
	  * @param message Where to save data found in status line (if overwrite)
	  * @param has_internet_location Whether the message as an internet location header
	  * @param mbox_raw_len Length of message in bytes (after Opera-specific headers)
	  * @param file Where to read line from
	  * @param overwrite_ids	Whether to overwrite the message id, parent id and account id found in the status line
	  * @param overwrite_flags	Whether to overwrite the flags found in the status line (useful when importing)
	  */
	static OP_STATUS ReadStatusLine(StoreMessage& message, BOOL& has_internet_location, UINT32& mbox_raw_len, OpFile& file, BOOL overwrite_ids = FALSE, BOOL ovewrite_flags = FALSE);

	/** Creates an 'X-Opera-Status: ' header line (Opera-specific header)
	  * @param message Message for which to create header
	  * @param recv_time Time when message was received
	  * @param file Output
	  */
	static OP_STATUS WriteStatusLine(StoreMessage& message, OpFile& file);

	/** Reads an 'X-Opera-Location: ' header from a file (Opera-specific header)
	  * @param internet_location Found location
	  * @param file Where to read line from
	  */
	static OP_STATUS ReadInternetLocation(OpString8& internet_location, OpFile& file);

	/** Writes an 'X-Opera-Location: ' header to a file (Opera-specific header)
	  * @param internet_location Location to save
	  * @param file Output
	  */
	static OP_STATUS WriteInternetLocation(const OpStringC8& internet_location, OpFile& file);
};

#endif // MBOXSTORE_H
