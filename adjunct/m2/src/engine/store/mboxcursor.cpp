/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/mboxcursor.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/util/compress.h"
#include "adjunct/desktop_util/datastructures/StreamBuffer.h"


/***********************************************************************************
 ** Destructor
 **
 ** MboxCursor::~MboxCursor
 ***********************************************************************************/
MboxCursor::~MboxCursor()
{
	OpStatus::Ignore(m_mboxdb.Flush());
	m_mboxdb_backend.Close();
}


/***********************************************************************************
 ** Initializes this message store. No other functions are called before this is called.
 **
 ** MboxCursor::Init
 **
 ***********************************************************************************/
OP_STATUS MboxCursor::Init(const OpStringC& store_path)
{
	OpString filename;

	OpString mail_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
	// Initialize the cursor
	RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cstore%ccontent.dat"),
										  store_path.HasContent() ? store_path.CStr() : mail_folder.CStr(),
												PATHSEPCHAR,
												PATHSEPCHAR));
	RETURN_IF_ERROR(m_mboxdb_backend.Open(filename.CStr(), BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE, 8));

	m_mboxdb.SetStorage(&m_mboxdb_backend);

	// Initialize the fields - don't change this if you don't want to break compatibility
	RETURN_IF_ERROR(m_mboxdb.AddField("size",       8));
	RETURN_IF_ERROR(m_mboxdb.AddField("compressed", 0));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Fully update an existing message in the store (overwrites existing message)
 **
 ** MboxCursor::UpdateMessage
 ** @param message Message to update
 ** @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise.
 **                 Might be changed by this function to have new mbx_data.
 ** @return OpStatus::OK if save was successful, OpStatus::ERR_NOT_SUPPORTED if
 **                      not supported for this store, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxCursor::UpdateMessage(StoreMessage& message, INT64& mbx_data)
{
	// Get to correct row
	if (mbx_data)
	{
		if (m_mboxdb.GetID() != (BSCursor::RowID)mbx_data)
			RETURN_IF_ERROR(m_mboxdb.Goto(mbx_data));
	}
	else
	{
		RETURN_IF_ERROR(m_mboxdb.Create());
	}

	// Create data
	StreamBuffer<char> uncompressed;
	StreamBuffer<char> compressed;

	// Create uncompressed data
	size_t length = message.GetOriginalRawHeadersSize() + 2 + message.GetRawBodySize();

	RETURN_IF_ERROR(uncompressed.Reserve(length));
	RETURN_IF_ERROR(uncompressed.Append(message.GetOriginalRawHeaders(), message.GetOriginalRawHeadersSize()));
	RETURN_IF_ERROR(uncompressed.Append("\r\n", 2));
	RETURN_IF_ERROR(uncompressed.Append(message.GetRawBody(), message.GetRawBodySize()));

	// Create compressed data (compress terminator as well)
	RETURN_IF_ERROR(Compressor::Compress(uncompressed.GetData(), length + 1, compressed));

	// Put data into database
	RETURN_IF_ERROR(m_mboxdb[MBOX_SIZE]				 .SetValue(length));
 	RETURN_IF_ERROR(m_mboxdb[MBOX_COMPRESSED_CONTENT].SetValue(compressed.GetData(), compressed.GetFilled()));

	// Flush, to get an ID
	RETURN_IF_ERROR(m_mboxdb.Flush());

	mbx_data = m_mboxdb.GetID();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get a message from the store
 **
 ** MboxCursor::GetMessage
 ***********************************************************************************/
OP_STATUS MboxCursor::GetMessage(INT64 mbx_data, StoreMessage& message, BOOL override)
{
	// Get to correct row
	if (m_mboxdb.GetID() != (BSCursor::RowID)mbx_data)
		RETURN_IF_ERROR(m_mboxdb.Goto(mbx_data));

	// Get data
	size_t	  length;
	size_t	  compressed_size = m_mboxdb[MBOX_COMPRESSED_CONTENT].GetSize();
	OpString8 compressed_holder;
	char*	  compressed = compressed_holder.Reserve(compressed_size);
	if (!compressed)
		return OpStatus::ERR_NO_MEMORY;

	m_mboxdb[MBOX_SIZE]              .GetValue(&length);
	m_mboxdb[MBOX_COMPRESSED_CONTENT].GetValue(compressed, compressed_size);

	// Decompress data
	StreamBuffer<char> uncompressed;

	RETURN_IF_ERROR(uncompressed.Reserve(length));
	RETURN_IF_ERROR(Compressor::Decompress(compressed, compressed_size, uncompressed));

	// Put data into message
	return message.SetRawMessage(NULL, FALSE, FALSE, uncompressed.Release(), length);
}


/***********************************************************************************
 ** Remove a message from the store
 **
 ** MboxCursor::RemoveMessage
 ***********************************************************************************/
OP_STATUS MboxCursor::RemoveMessage(INT64 mbx_data, message_gid_t id, UINT16 account_id, time_t sent_date, BOOL draft)
{
	// Get to correct row
	if (m_mboxdb.GetID() != (BSCursor::RowID)mbx_data)
		RETURN_IF_ERROR(m_mboxdb.Goto(mbx_data));

	return m_mboxdb.Delete();
}


/***********************************************************************************
 **
 ** MboxCursor::CommitData
 ***********************************************************************************/
OP_STATUS MboxCursor::CommitData()
{
	OpStatus::Ignore(m_mboxdb.Flush());

	if (m_mboxdb_backend.InTransaction())
		return m_mboxdb_backend.Commit();

	return OpStatus::OK;
}

#endif // M2_SUPPORT
