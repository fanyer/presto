/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/mboxmonthly.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "modules/util/opfile/opfile.h"


/***********************************************************************************
 ** Fully update an existing message in the store (overwrites existing message)
 **
 ** MboxMonthly::UpdateMessage
 ** @param message Message to update
 ** @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise.
 **        Might be changed by this function to have new mbx_data.
 ** @return OpStatus::OK if save was successful, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxMonthly::UpdateMessage(StoreMessage& message,
									 INT64& mbx_data)
{
	// Construct file - will be closed on destruction
	OpFile file;

	// Get sent time of message
	time_t date;
	RETURN_IF_ERROR(message.GetDateHeaderValue(Header::DATE, date));

	// Prepare file for writing
	RETURN_IF_ERROR(PrepareFile(file, message.GetId(), message.GetAccountId(), date, FALSE, mbx_data != 0));

	// Go to correct position (if there already was mbx_data), which is past the from header - else write the from header
	if (mbx_data != 0)
		RETURN_IF_ERROR(file.SetFilePos(mbx_data));
	else
		RETURN_IF_ERROR(WriteFromLine(message, file));

	// Set the mbx_data past the from header, start of the raw message
	OpFileLength length;
	RETURN_IF_ERROR(file.GetFilePos(length));
	mbx_data = length;

	// Write message to file
	RETURN_IF_ERROR(WriteRawMessage(message, file));

	// Close file, making sure it's written to disk
	return file.SafeClose();
}


/***********************************************************************************
 ** Update the status of an existing message in the store
 **
 ** MboxMonthly::UpdateMessageStatus
 ** @param message Message to update
 ** @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise.
 **        Might be changed by this function to have new mbx_data.
 ** @return OpStatus::OK if save was successful, error codes otherwise
 ***********************************************************************************/
 OP_STATUS MboxMonthly::UpdateMessageStatus(StoreMessage& message,
											INT64& mbx_data)
{
	OpFile file;
	OpString8 buf;
	time_t sent_date;

	// Get sent time of message
	RETURN_IF_ERROR(message.GetDateHeaderValue(Header::DATE, sent_date));

	// Prepare file for reading and writing
	RETURN_IF_ERROR(PrepareFile(file, message.GetId(), message.GetAccountId(), sent_date, FALSE, mbx_data));

	// Write X-Opera-Status line
	return WriteStatusLine(message, file);
}


/***********************************************************************************
 ** Get a message from the store
 **
 ** MboxMonthly::GetMessage
 ** @param mbx_data mbx_data that was saved with the message
 ** @param message Where to place the message if found, prefilled with ID, account
 **        ID and sent date
 ** @return OpStatus::OK if message was found and retrieved,
 **         OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxMonthly::GetMessage(INT64 mbx_data,
								  StoreMessage& message,
								  BOOL override)
{
	OpFile file;

	// Get time of message
	time_t date;
	RETURN_IF_ERROR(message.GetDateHeaderValue(Header::DATE, date));

	// Prepare file for reading
	RETURN_IF_ERROR(PrepareFile(file, message.GetId(), message.GetAccountId(), date, TRUE, mbx_data));

	// Read message from file
	return ReadRawMessage(message, file, override);
}


/***********************************************************************************
 ** Remove a message from the store
 **
 ** MboxMonthly::RemoveMessage
 ** @param mbx_data mbx_data that was saved with the message
 ** @param id Message to remove
 ** @param account_id Account of this message
 ** @param sent_date Time when message was sent
 ** @param draft Whether this was a draft
 ** @return OpStatus::OK if delete was successful or if message didn't exist, error
 **         codes otherwise
 ***********************************************************************************/
OP_STATUS MboxMonthly::RemoveMessage(INT64 mbx_data,
									 message_gid_t id,
									 UINT16 account_id,
									 time_t sent_date,
									 BOOL draft)
{
	// TODO: implement
	return OpStatus::OK;
}


/***********************************************************************************
 ** Prepare a file for reading or writing
 **
 ** MboxMonthly::PrepareFile
 **
 ***********************************************************************************/
OP_STATUS MboxMonthly::PrepareFile(OpFile& file,
								   message_gid_t id,
								   UINT16 account_id,
								   time_t sent_date,
								   BOOL read_only,
								   INT64 mbx_data)
{
	OpString filename;

	// Get the filename
	RETURN_IF_ERROR(GetFilename(id, account_id, sent_date, filename));

	// Construct file
	RETURN_IF_ERROR(file.Construct(filename.CStr()));

	// If we have mbx_data, file has to exist
	if (mbx_data != 0)
	{
		BOOL exists;
		RETURN_IF_ERROR(file.Exists(exists));
		if (!exists)
			return OpStatus::ERR_FILE_NOT_FOUND;
	}

	// Setup file mode: read for read-only, read-write if we're changing an existing
	// message, append if this is a new message
	int mode = OPFILE_READ;

	if (!read_only)
	{
		if (mbx_data != 0)
			mode |= OPFILE_WRITE;
		else
			mode = OPFILE_APPEND;
	}

	// Open file
	RETURN_IF_ERROR(file.Open(mode));

	// Go to right position in file
	if (mbx_data != 0)
		RETURN_IF_ERROR(file.SetFilePos(mbx_data));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Gets the file path for a specific message
 **
 ** MboxMonthly::GetFilename
 **
 ***********************************************************************************/
OP_STATUS MboxMonthly::GetFilename(message_gid_t id,
								   UINT16        account_id,
								   time_t        sent_date,
								   OpString&     filename)
{
	filename.Empty();

	// Get date details
	struct tm* time = op_gmtime(&sent_date);
	if (!time)
	{
		sent_date = 0; // fallback to 1970, date was probably before 1970.
		time = op_gmtime(&sent_date);

		if (!time)
			return OpStatus::ERR;
	}

	// Make filename
	OpString mail_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
	return filename.AppendFormat(UNI_L("%s%cstore%caccount%d%c%d-%02d.mbs"),
								 m_store_path.HasContent() ? m_store_path.CStr() : mail_folder.CStr(),
								 PATHSEPCHAR,
								 PATHSEPCHAR, account_id,
								 PATHSEPCHAR, time->tm_year + 1900,
								 time->tm_mon + 1);
}


#endif // M2_SUPPORT
