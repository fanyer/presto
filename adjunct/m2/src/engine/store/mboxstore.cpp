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

#include "adjunct/m2/src/engine/store/mboxstore.h"
#include "adjunct/m2/src/engine/store/storemessage.h"

#include "modules/util/opfile/opfile.h"

namespace MboxStoreConstants
{
	const unsigned XOperaStatusVersion = 5; ///< Version of the X-Opera-Status: headers used
};


/***********************************************************************************
 ** Writes out a message to a specified file
 **
 ** MboxStore::WriteRawMessage
 ** @param message Message to write out
 ** @param file Where to write
 **
 ***********************************************************************************/
OP_STATUS MboxStore::WriteRawMessage(StoreMessage& message, OpFile& file)
{
	OP_ASSERT(file.IsOpen());

	// Write status line
	RETURN_IF_ERROR(WriteStatusLine(message, file));

	// Write internet location
	RETURN_IF_ERROR(WriteInternetLocation(message.GetInternetLocation(), file));

	// Write message
	RETURN_IF_ERROR(file.Write(message.GetOriginalRawHeaders(), message.GetOriginalRawHeadersSize()));
	RETURN_IF_ERROR(file.Write("\r\n", 2));
	if (message.GetRawBody())
	{
		RETURN_IF_ERROR(file.Write(message.GetRawBody(), message.GetRawBodySize()));
		RETURN_IF_ERROR(file.Write("\r\n", 2));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Reads a message starting at the current position of the file
 **
 ** MboxStore::ReadRawMessage
 ** @param message Where to save message
 ** @param file Where to read message from
 **
 ***********************************************************************************/
OP_STATUS MboxStore::ReadRawMessage(StoreMessage& message, OpFile& file, BOOL overwrite, BOOL import)
{
	OpString8    linebuf;
	OpString8    internet_location;
	UINT32       mbox_raw_len;
	BOOL         has_internet_location;
	OpFileLength bytes_read;

	OP_ASSERT(file.IsOpen());

	RETURN_IF_ERROR(ReadStatusLine(message, has_internet_location, mbox_raw_len, file, overwrite, import));

	// Read "X-Opera-Location: " line if it's there
	if (has_internet_location)
	{
		RETURN_IF_ERROR(ReadInternetLocation(internet_location, file));
		if (overwrite)
			RETURN_IF_ERROR(message.SetInternetLocation(internet_location));
	}

	// Now read the whole 'raw' message into messagebuf
	char* messagebuf = OP_NEWA(char, mbox_raw_len + 1);
	if (!messagebuf)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = file.Read(messagebuf, mbox_raw_len, &bytes_read);

	if (OpStatus::IsError(ret))
	{
		OP_DELETEA(messagebuf);
		return ret;
	}

	messagebuf[bytes_read] = 0;

	// Add messagebuf to message (takes ownership of messagebuf)
	RETURN_IF_ERROR(message.SetRawMessage(NULL, FALSE, FALSE, messagebuf, bytes_read));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Writes out an mbox-format message to a specified file
 **
 ** MboxStore::WriteRawMessage
 ** @param message Message to write out
 ** @param file Where to write
 **
 ***********************************************************************************/
OP_STATUS MboxStore::WriteMboxMessage(StoreMessage& message, OpFile& file)
{
	// Write 'From ' line
	RETURN_IF_ERROR(WriteFromLine(message, file));

	// Write normal message
	return WriteRawMessage(message, file);
}


/***********************************************************************************
 ** Reads an mbox-format message starting at the current position of the file
 **
 ** MboxStore::ReadMboxMessage
 ** @param message Where to save message
 ** @param file Where to read message from
 **
 ***********************************************************************************/
OP_STATUS MboxStore::ReadMboxMessage(StoreMessage& message, OpFile& file, BOOL override, BOOL import)
{
	// Read 'From ' line
	RETURN_IF_ERROR(ReadFromLine(file));

	// Read rest of message
	return ReadRawMessage(message, file, override, import);
}


/***********************************************************************************
 ** Reads a 'From ' line from an mbox-format file
 **
 ** MboxStore::ReadFromLine
 ** @param file input
 **
 ***********************************************************************************/
OP_STATUS MboxStore::ReadFromLine(OpFile& file)
{
	OpString8 linebuf;

	RETURN_IF_ERROR(file.ReadLine(linebuf));

	if (linebuf.IsEmpty())
		return OpStatus::ERR;

	if (linebuf.Compare("From ", 5) != 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Creates a 'From ' line for an mbox-format file
 **
 ** MboxStore::WriteFromLine
 ** @param message Message for which to create line
 ** @param file Output
 **
 ***********************************************************************************/
OP_STATUS MboxStore::WriteFromLine(StoreMessage& message, OpFile& file)
{
	time_t              recv_time = message.GetRecvTime();
	struct tm*          gmt = NULL;
	Header*             from_header = message.GetHeader(Header::FROM);
	const Header::From* from = from_header ? from_header->GetFirstAddress() : NULL;
	OpString8           imaa_address;
	OpString8           from_line;

	// Get From address
	if (from)
	{
		OP_STATUS ret = from->GetIMAAAddress(imaa_address);

		if (OpStatus::IsError(ret) && ret != OpStatus::ERR_PARSING_FAILED)
			return ret;
	}

	// Get time
	if (recv_time)
		gmt = op_gmtime(&recv_time);

	// Create line
	RETURN_IF_ERROR(from_line.AppendFormat(
					"From %s %.24s %d\r\n",
					imaa_address.HasContent() ? imaa_address.CStr() : "",
					gmt ? asctime(gmt) : "",
					message.GetId()));

	// Write line to file
	return file.Write(from_line.CStr(), from_line.Length());
}


/***********************************************************************************
 ** Reads an 'X-Opera-Status: ' header from a file (Opera-specific header)
 **
 ** MboxStore::ReadStatusLine
 ** @param message Where to save data found in status line (if overwrite)
 ** @param has_internet_location Whether the message as an internet location header
 ** @param mbox_raw_len Length of message in bytes (after Opera-specific headers)
 ** @param file Where to read line from
 ** @param overwrite_ids	Whether to overwrite the message id, parent id and account id found in the status line
 ** @param overwrite_flags	Whether to overwrite the flags found in the status line (useful when importing)
 **
 ***********************************************************************************/
OP_STATUS MboxStore::ReadStatusLine(StoreMessage& message,
									BOOL& has_internet_location,
									UINT32& mbox_raw_len,
									OpFile& file,
									BOOL overwrite_ids,
									BOOL overwrite_flags)
{
	UINT32 version;
	UINT32 message_id;
	UINT32 recv_time;
	UINT32 remote_message_size;
	UINT64 flags;
	UINT32 account_id;
	UINT32 parent_id;
	UINT32 internet_location;
	OpString8 linebuf;
	UINT32 create_type;

	UINT32 flagsLW, flagsHW;

	// Read "X-Opera-Status: " line
	RETURN_IF_ERROR(file.ReadLine(linebuf));

	// Parse "X-Opera-Status: " line
	if (op_sscanf(linebuf.CStr(),
				  "X-Opera-Status: %2x%*8x%8x%8x%8x%8x%8x%8x%2x%*2x%4x%8x%*8x%*8x%8x",
				  &version,
				  &message_id,
				  &recv_time,
				  &remote_message_size,
				  &flagsLW,
				  &mbox_raw_len,
				  &flagsHW,
				  &create_type,
				  &account_id,
				  &parent_id,
				  &internet_location) != 11)
		return OpStatus::ERR;

	flags = (static_cast<UINT64>(flagsHW) << 32) | flagsLW;

	// Check if this was the correct version
	if (version > MboxStoreConstants::XOperaStatusVersion)
		return OpStatus::ERR;

	// Set message properties if requested
	if (overwrite_ids || overwrite_flags)
	{
		message.SetRecvTime(recv_time);
		message.SetMessageSize(remote_message_size);
		if (overwrite_flags)
		{
			message.SetAllFlags(flags);
		}
		if (overwrite_ids)
		{
			message.SetId(message_id);
			message.SetCreateType((MessageTypes::CreateType)create_type);
			if (parent_id)
				message.SetParentId(parent_id);
			RETURN_IF_ERROR(message.SetAccountId(account_id));
		}
	}
	else
	{
		if (message.GetId() != message_id)
			return OpStatus::ERR;
	}

	has_internet_location = internet_location ? TRUE : FALSE;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Creates an 'X-Opera-Status: ' header line (Opera-specific header)
 **
 ** MboxStore::WriteStatusLine
 ** @param message Message for which to create header
 ** @param file Output
 **
 ***********************************************************************************/
OP_STATUS MboxStore::WriteStatusLine(StoreMessage& message, OpFile& file)
{
	OpString8 status_line;

	RETURN_IF_ERROR(status_line.AppendFormat(
			"X-Opera-Status: %.2x%.8x%.8x%.8x%.8x%.8x%.8x%.8x%.2x%.2x%.4x%.8x%.8x%.8x%.8x%.8x%.8x\r\n",
			(UINT8)MboxStoreConstants::XOperaStatusVersion, /* Version of status header */
			0,										/* Unused, previously 'base' id */
			(UINT32)message.GetId(),				/* Message ID */
			(UINT32)message.GetRecvTime(),			/* Time of message received */
			(UINT32)message.GetMessageSize(),		/* Size of message on server */
			(UINT32)message.GetAllFlags(),			/* FlagsLW set on message */
			(UINT32)message.GetLocalMessageSize(),	/* Size of message on disk */
			(UINT32)(message.GetAllFlags() >> 32),	/* FlagsHW set on message */
			(UINT8)message.GetCreateType(),			/* Create Type of outgoing message */
			0,										/* Unused, previously message label */
			(UINT16)message.GetAccountId(),			/* Account ID */
			(UINT32)message.GetParentId(),			/* Parent Message ID */
			0,										/* Unused, previously position of To: header */
			0,										/* Unused, previously position of Subject: header */
			(UINT32)message.GetInternetLocation().Length(), /* Whether this message has an internet location */
			0, 										/* Unused */
			0));									/* Unused */

	return file.Write(status_line.CStr(), 132);
}


/***********************************************************************************
 ** Reads an 'X-Opera-Location: ' header from a file (Opera-specific header)
 **
 ** MboxStore::ReadInternetLocation
 ** @param internet_location Found location
 ** @param file Where to read line from
 **
 ***********************************************************************************/
OP_STATUS MboxStore::ReadInternetLocation(OpString8& internet_location, OpFile& file)
{
	OpString8 linebuf;

	// Read header line
	RETURN_IF_ERROR(file.ReadLine(linebuf));

	if (linebuf.IsEmpty())
		return OpStatus::ERR;

	// Parse header line
	if (linebuf.Compare("X-Opera-Location: ", 18) != 0)
		return OpStatus::ERR;

	return internet_location.Set(linebuf.CStr() + 18);
}


/***********************************************************************************
 ** Writes an 'X-Opera-Location: ' header to a file (Opera-specific header)
 **
 ** MboxStore::WriteInternetLocation
 ** @param internet_location Location to save
 ** @param file Output
 **
 ***********************************************************************************/
OP_STATUS MboxStore::WriteInternetLocation(const OpStringC8& internet_location, OpFile& file)
{
	if (internet_location.HasContent())
	{
		// Write header
		RETURN_IF_ERROR(file.Write("X-Opera-Location: ", 18));
		RETURN_IF_ERROR(file.Write(internet_location.CStr(), internet_location.Length()));
		RETURN_IF_ERROR(file.Write("\r\n", 2));
	}

	return OpStatus::OK;
}

#endif // M2_SUPPORT
