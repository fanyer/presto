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

#include "adjunct/m2/src/engine/store/mboxpermail.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "modules/util/opfile/opfile.h"


/***********************************************************************************
 ** Fully update an existing message in the store (overwrites existing message)
 **
 ** MboxPerMail::UpdateMessage
 ** @param message Message to update
 ** @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise.
 **        Might be changed by this function to have new mbx_data.
 ** @return OpStatus::OK if save was successful, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxPerMail::UpdateMessage(StoreMessage& message,
									 INT64& mbx_data)
{
	// Construct file - will be closed on destruction
	OpFile file;

	// Prepare file for writing
	RETURN_IF_ERROR(PrepareFile(file, message, FALSE));

	// Write message to file
	RETURN_IF_ERROR(WriteMboxMessage(message, file));

	// mbx_data is not used for this format
	mbx_data = 0;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Update the status of an existing message in the store
 **
 ** MboxPerMail::UpdateMessageStatus
 ** @param message Message to update
 ** @param mbx_data Existing mbx_data from the store if there was any, 0 otherwise.
 **        Might be changed by this function to have new mbx_data.
 ** @return OpStatus::OK if save was successful, error codes otherwise
 ***********************************************************************************/
 OP_STATUS MboxPerMail::UpdateMessageStatus(StoreMessage& message,
											INT64& mbx_data)
{
	OpFile file;
	OpString8 buf;

	// Prepare file for reading and writing
	RETURN_IF_ERROR(PrepareFile(file, message, FALSE, TRUE));

	// Read 'From ' line, should skip to X-Opera-Status: line
	RETURN_IF_ERROR(ReadFromLine(file));

	// Write X-Opera-Status line
	return WriteStatusLine(message, file);
}


/***********************************************************************************
 ** Get a message from the store
 **
 ** MboxPerMail::GetMessage
 ** @param mbx_data mbx_data that was saved with the message
 ** @param message Where to place the message if found, prefilled with ID, account
 **        ID and sent date
 ** @return OpStatus::OK if message was found and retrieved,
 **         OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxPerMail::GetMessage(INT64 mbx_data,
								  StoreMessage& message,
								  BOOL override)
{
	OpFile file;

	// Prepare file for reading
	RETURN_IF_ERROR(PrepareFile(file, message, TRUE));

	// Read message from file
	return ReadMboxMessage(message, file, override);
}

/***********************************************************************************
 ** Get a draft from the store
 **
 ** MboxPerMail::GetDraftMessage
 ** @param message Where to place the message if found
 ** @param id Message id to retrieve
 ** @return OpStatus::OK if message was found and retrieved,
 **         OpStatus::ERR_FILE_NOT_FOUND if not found, error codes otherwise
 ***********************************************************************************/
OP_STATUS MboxPerMail::GetDraftMessage(StoreMessage& message, message_gid_t id)
{
	OpFile file;
	OpString filename;

	OpString mail_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
	RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cstore%cdrafts%c%d.mbs"),
									 m_store_path.HasContent() ? m_store_path.CStr() : mail_folder.CStr(),
									 PATHSEPCHAR,
									 PATHSEPCHAR,
									 PATHSEPCHAR, id));

	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	return ReadMboxMessage(message, file, TRUE, TRUE);
}

/***********************************************************************************
 ** Remove a message from the store
 **
 ** MboxPerMail::RemoveMessage
 ** @param mbx_data mbx_data that was saved with the message
 ** @param id Message to remove
 ** @param account_id Account of this message
 ** @param sent_date Time when message was sent
 ** @param draft Whether this was a draft
 ** @return OpStatus::OK if delete was successful or if message didn't exist, error
 **         codes otherwise
 ***********************************************************************************/
OP_STATUS MboxPerMail::RemoveMessage(INT64 mbx_data,
									 message_gid_t id,
									 UINT16 account_id,
									 time_t sent_date,
									 BOOL draft)
{
	OpFile	 file;
	OpString filename;
	int		 pos;

	// Get the filename
	RETURN_IF_ERROR(GetFilename(id, account_id, sent_date, draft, filename));

	// Construct file
	RETURN_IF_ERROR(file.Construct(filename.CStr()));

	// Delete file
	RETURN_IF_ERROR(file.Delete());

	// Check for empty directories
	while ((pos = filename.FindLastOf(PATHSEPCHAR)) != KNotFound)
	{
		OpFile directory;
		BOOL   found_files = FALSE;

		// Get directory name
		filename.Delete(pos);

		// Get a folder lister
		OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*"), filename.CStr());
		if (!lister)
			break;

		// Check if there are files in this folder
		while (lister->Next())
		{
			if (!uni_str_eq(".", lister->GetFileName()) && !uni_str_eq("..", lister->GetFileName()))
			{
				found_files = TRUE;
				break;
			}
		}
		OP_DELETE(lister);

		// If we found any files, break out of the loop
		if (found_files)
			break;

		// There were no files, we will now remove this directory
		RETURN_IF_ERROR(directory.Construct(filename.CStr()));
		if (OpStatus::IsError((directory.Delete())))
			break;
	}

	if (draft)
		return RemoveDraftAttachments(id);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** MboxPerMail::RemoveDraftAttachments
 **
 ***********************************************************************************/
OP_STATUS MboxPerMail::RemoveDraftAttachments(message_gid_t id)
{
	OpString folder_name;

	if (m_store_path.HasContent())
		RETURN_IF_ERROR(folder_name.Set(m_store_path.CStr()));
	else
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, folder_name));
	RETURN_IF_ERROR(folder_name.AppendFormat(UNI_L("%cstore%cdrafts%c%d"), PATHSEPCHAR, PATHSEPCHAR, PATHSEPCHAR, id));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(folder_name.CStr()));
	
	return file.Delete();
}


/***********************************************************************************
 ** Prepare a file for reading or writing
 **
 ** MboxPerMail::PrepareFile
 **
 ***********************************************************************************/
OP_STATUS MboxPerMail::PrepareFile(OpFile& file,
								   StoreMessage& message,
								   BOOL read_only,
								   BOOL check_existence)
{
	OpString filename;

	// Check if this message is a draft
	BOOL draft = message.IsFlagSet(StoreMessage::IS_OUTGOING) && !message.IsFlagSet(StoreMessage::IS_SENT);

	// Get time of message
	time_t sent_date;
	RETURN_IF_ERROR(message.GetDateHeaderValue(Header::DATE, sent_date));

	// Get the filename
	RETURN_IF_ERROR(GetFilename(message.GetId(), message.GetAccountId(), sent_date, draft, filename));

	// Construct file
	RETURN_IF_ERROR(file.Construct(filename.CStr()));

	// If read-only, or check_existence, file has to exist
	if (read_only || check_existence)
	{
		BOOL exists;
		RETURN_IF_ERROR(file.Exists(exists));
		if (!exists)
			return OpStatus::ERR_FILE_NOT_FOUND;
	}

	// Open file
	return file.Open(read_only ? OPFILE_READ : OPFILE_WRITE | OPFILE_READ);
}


/***********************************************************************************
 ** Gets the file path for a specific message
 **
 ** MboxPerMail::GetFilename
 **
 ***********************************************************************************/
OP_STATUS MboxPerMail::GetFilename(message_gid_t id,
								   UINT16		 account_id,
								   time_t		 sent_date,
								   BOOL			 draft,
								   OpString&	 filename)
{
	filename.Empty();

	// Special case for drafts
	if (draft)
	{
		OpString mail_folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
		return filename.AppendFormat(UNI_L("%s%cstore%cdrafts%c%d.mbs"),
									 m_store_path.HasContent() ? m_store_path.CStr() : mail_folder.CStr(),
									 PATHSEPCHAR,
									 PATHSEPCHAR,
									 PATHSEPCHAR, id);
	}
	else
	{
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
		return filename.AppendFormat(UNI_L("%s%cstore%caccount%d%c%d%c%02d%c%02d%c%d.mbs"),
									m_store_path.HasContent() ? m_store_path.CStr() : mail_folder.CStr(),
									PATHSEPCHAR,
									PATHSEPCHAR, account_id,
									PATHSEPCHAR, time->tm_year + 1900,
									PATHSEPCHAR, time->tm_mon + 1,
									PATHSEPCHAR, time->tm_mday,
									PATHSEPCHAR, id);
	}
}

#endif // M2_SUPPORT
