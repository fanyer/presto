/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#ifdef M2_MERLIN_COMPATIBILITY

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/store/storeupdater.h"

BOOL3 StoreUpdater::m_needs_update = MAYBE;


/***********************************************************************************
 ** Constructor
 **
 ** StoreUpdater::StoreUpdater
 **
 ***********************************************************************************/
StoreUpdater::StoreUpdater(Store& new_store)
  : m_new_store(new_store)
  , m_errors(FALSE)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** StoreUpdater::~StoreUpdater
 **
 ***********************************************************************************/
StoreUpdater::~StoreUpdater()
{
	g_main_message_handler->UnsetCallBacks(this);
}

/***********************************************************************************
 ** Initialize this class - run before running any other functions (except static)
 **
 ** StoreUpdater::Init
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::Init()
{
	// Initialize messages
	g_main_message_handler->SetCallBack(this, MSG_M2_STORE_UPDATE, (MH_PARAM_1)this);
	RETURN_IF_ERROR(MessageEngine::GetInstance()->InitMessageDatabaseIfNeeded());

	// Initialize old store

	// Construct file
	OpString mail_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder));
	RETURN_IF_ERROR(m_old_store.Init(mail_folder.CStr(), UNI_L("mailbase"), 0, 48));

	// Check format information, we only like format 4
	m_old_store.Seek(0, 0);

	INT32 format;
	RETURN_IF_ERROR(m_old_store.ReadValue(format)); // potentially used by another instance of Opera
	if (format != 4)
		return OpStatus::ERR;

	// Go to the first message
	m_old_store.Seek(1, 0);
	m_current_block = 1;

	// Prepare / read update log
	RETURN_IF_ERROR(PrepareFromLog(m_update_log, m_current_block, FALSE));

	// Check if we're done
	if (m_current_block >= m_old_store.GetBlockCount())
		OnFinished();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Start the update process
 **
 ** StoreUpdater::StartUpdate
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::StartUpdate()
{
	// Start the update!
	m_continue = TRUE;

	g_main_message_handler->PostMessage(MSG_M2_STORE_UPDATE, (MH_PARAM_1)this, 0);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Checks if an update is needed
 **
 ** StoreUpdater::NeedsUpdate
 **
 ***********************************************************************************/
BOOL StoreUpdater::NeedsUpdate()
{
	if (m_needs_update == MAYBE)
	{
		m_needs_update = NO;

		BlockFile old_store;

		OpString mail_folder;
		OP_STATUS get_folder_err = g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mail_folder);
		// Check if old store exists
		if (BlockStorage::FileExists(UNI_L("mailbase.dat"), OPFILE_MAIL_FOLDER) == OpBoolean::IS_TRUE &&
			OpStatus::IsSuccess(get_folder_err) &&
			OpStatus::IsSuccess(old_store.Init(mail_folder.CStr(), UNI_L("mailbase"), 0, 48)))
		{
			if (old_store.GetBlockCount() > 1)
			{
				OpFile   logfile;
				int		 block = 0;

				// Update is needed if new store is empty or if there is an update status available
				if (BlockStorage::FileExists(UNI_L("omailbase.dat"), OPFILE_MAIL_FOLDER) == OpBoolean::IS_FALSE ||
					(OpStatus::IsSuccess(PrepareFromLog(logfile, block, TRUE)) && block))
					m_needs_update = YES;
			}
			else
			{
				// Empty store, remove it
				old_store.RemoveFile();
			}
		}
	}

	return m_needs_update;
}


/***********************************************************************************
 ** Receiving a message from the message loop
 **
 ** StoreUpdater::HandleCallback
 **
 ***********************************************************************************/
void StoreUpdater::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_M2_STORE_UPDATE && par1 == (MH_PARAM_1)this);

	if (msg != MSG_M2_STORE_UPDATE || par1 != (MH_PARAM_1)this)
		return;

	// Check if we should continue
	if (!m_continue)
		return;

	// Update one message
	if (OpStatus::IsError(UpdateMessage()))
		m_errors = TRUE;

	// Increase block
	m_current_block++;

	// Notify listeners that a message has been updated
	OnProgressChanged();

	// Check if we're done
	if (m_current_block >= m_old_store.GetBlockCount())
	{
		OnFinished();
	}
	else
	{
		// Send request for next message
		g_main_message_handler->PostMessage(MSG_M2_STORE_UPDATE, (MH_PARAM_1)this, 0);
	}
}


/***********************************************************************************
 ** Update a single message (current position of m_old_store)
 **
 ** StoreUpdater::UpdateMessage
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::UpdateMessage()
{
	UINT32 message_id = 0;

	// Write to update log
	RETURN_IF_ERROR(WriteLog());

	// Read message ID
	m_old_store.ReadValue((INT32&)message_id);

	// Check if this was an empty entry
	if (message_id == 0 || m_new_store.MessageAvailable(message_id))
	{
		m_old_store.Seek(m_current_block + 1, 0);
	}
	else
	{
		// Read all message details
		INT32      tmp_int;
		INT32      sent_date;
		INT64      mbx_data;
		Message    message;
		MboxStore* store;

		RETURN_IF_ERROR(LogError(message.Init(0), message_id, "Error initializing message"));

		message.SetId(message_id);

		m_old_store.ReadValue(sent_date);
		message.SetDateHeaderValue(Header::DATE, sent_date);
		m_old_store.ReadValue(tmp_int);
		message.SetMessageSize(tmp_int);
		m_old_store.ReadValue(tmp_int);
		message.SetAllFlags(tmp_int);
		m_old_store.ReadValue(tmp_int);
		mbx_data = tmp_int;
		m_old_store.ReadValue((INT32&)ees);

		message.SetAccountId(ees.sixteen, FALSE);

		m_old_store.ReadValue(tmp_int);
		message.SetParentId(tmp_int);

		// Set 'reindexing' flag
		message.SetFlag(Message::IS_WAITING_FOR_INDEXING, TRUE);

		// Unused items
		m_old_store.ReadValue(tmp_int); // prev_from
		m_old_store.ReadValue(tmp_int); // prev_to
		m_old_store.ReadValue(tmp_int); // prev_subject
		m_old_store.ReadValue(tmp_int); // message id hash
		m_old_store.ReadValue(tmp_int); // message location hash

		// Special handling for drafts: make sure that drafts aren't recognized as drafts
		// by the 'raw' store fetcher, since they're in their own directory now but weren't before
		// See also bug 290959
		BOOL draft = message.IsFlagSet(Message::IS_OUTGOING) && !message.IsFlagSet(Message::IS_SENT);

		if (draft)
			message.SetFlag(Message::IS_SENT, TRUE);

		// Try to get message from 'raw' store
		RETURN_IF_ERROR(LogError(m_new_store.m_mbox_store_manager.GetMessage(mbx_data, message, store, TRUE), message_id, "Can't get message from raw storage"));

		if (draft)
			message.SetFlag(Message::IS_SENT, FALSE);

		// Reset Date header, since it should be the same as in the old store. The date specifies
		// in which directory the 'raw' file is.
		message.SetDateHeaderValue(Header::DATE, sent_date);

		// Add message to store
		RETURN_IF_ERROR(LogError(m_new_store.m_maildb->Create(), message_id, "Can't create new record in database"));
		m_new_store.m_current_id = message_id;

		// Set raw store details. Set to 0 if we don't have a body (new store uses this to decide if body is downloaded).
		if (message.GetRawBody() && *(message.GetRawBody()))
		{
			m_new_store.m_maildb->GetField(Store::STORE_MBX_TYPE).SetValue(store->GetType());
			m_new_store.m_maildb->GetField(Store::STORE_MBX_DATA).SetValue(mbx_data);
		}
		else
		{
			m_new_store.m_maildb->GetField(Store::STORE_MBX_TYPE).SetValue(AccountTypes::MBOX_NONE);
			m_new_store.m_maildb->GetField(Store::STORE_MBX_DATA).SetValue(0);
		}

		RETURN_IF_ERROR(LogError(m_new_store.UpdateMessage(message), message_id, "Can't save message to database"));

		if (draft)
			RETURN_IF_ERROR(LogError(m_new_store.SetRawMessage(message), message_id, "Can't copy draft to new drafts directory"));

		// Save changes
		RETURN_IF_ERROR(LogError(m_new_store.m_maildb->Flush(), message_id, "Can't flush data to disk (end)"));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Call when progress has changed
 **
 ** StoreUpdater::OnProgressChanged
 **
 ***********************************************************************************/
void StoreUpdater::OnProgressChanged()
{
	// Notify all our listeners that the progress has changed
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		// We distract one from the numbers, since the messages start at block 1
		m_listeners.GetNext(iterator)->OnStoreUpdateProgressChanged(m_current_block - 1, m_old_store.GetBlockCount() - 1);
	}
}


/***********************************************************************************
 ** Call when finished
 **
 ** StoreUpdater::OnFinished
 **
 ***********************************************************************************/
void StoreUpdater::OnFinished()
{
	m_continue = FALSE;
	m_current_block = 0;

	OpStatus::Ignore(WriteLog());
	OpStatus::Ignore(m_update_log.Close());

	if (!m_errors)
	{
		// If there were no errors, we remove the old store and the update log, no more use for it
		OpStatus::Ignore(m_old_store.RemoveFile());
		OpStatus::Ignore(m_update_log.Delete());
	}

	// Update finished
	// TODO Enable when it's safe to restart M2 while Opera is running
	// m_needs_update = NO;
}


/***********************************************************************************
 ** Write the current status of the updater to the log
 **
 ** StoreUpdater::WriteLog
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::WriteLog()
{
	OpString8 to_write;

	RETURN_IF_ERROR(to_write.AppendFormat("Current block %010d\r\n", m_current_block));
	RETURN_IF_ERROR(m_update_log.SetFilePos(0));
	RETURN_IF_ERROR(m_update_log.Write(to_write.CStr(), 26));

	return m_update_log.Flush();
}


/***********************************************************************************
 ** Prepare the updater from the update log.
 **
 ** StoreUpdater::PrepareFromLog
 ** @param file Where to open file
 ** @param block Which block was read from the file
 ** @param read_only Whether to just open the log read-only (for checking status)
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::PrepareFromLog(OpFile& file, int& block, BOOL read_only)
{
	BOOL exists;

	// Construct file
	RETURN_IF_ERROR(file.Construct(UNI_L("storeupdate.log"), OPFILE_MAIL_FOLDER));

	// If file exists, read data from it
	RETURN_IF_ERROR(file.Exists(exists));

	if (exists)
	{
		OpString8 line;

		RETURN_IF_ERROR(file.Open(read_only ? OPFILE_READ : OPFILE_READ|OPFILE_WRITE));
		RETURN_IF_ERROR(file.ReadLine(line));
		if (line.HasContent())
			op_sscanf(line.CStr(), "Current block %d", &block);
	}
	else if (!read_only)
	{
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** If errorval is an error, write a message to the log
 **
 ** StoreUpdater::LogError
 ** @param errorval Error value to evaluate
 ** @param message_id ID of message that was being converted when error occurred
 ** @param errormsg Error message to write to the log in case errorval is an error
 ** @return errorval
 **
 ***********************************************************************************/
OP_STATUS StoreUpdater::LogError(OP_STATUS errorval, unsigned message_id, const char* errormsg)
{
	if (OpStatus::IsError(errorval))
	{
		OpString8 to_write;

		if (OpStatus::IsSuccess(to_write.AppendFormat("Message %u: error %d - %s\r\n", message_id, errorval, errormsg)) &&
			OpStatus::IsSuccess(m_update_log.SetFilePos(0, SEEK_FROM_END)))
		{
			OpStatus::Ignore(m_update_log.Write(to_write.CStr(), to_write.Length()));
		}
	}

	return errorval;
}

#endif // M2_MERLIN_COMPATIBILITY

#endif // M2_SUPPORT
