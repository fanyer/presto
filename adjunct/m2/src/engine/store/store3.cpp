/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#include "adjunct/m2/src/engine/store/store3.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/string/hash.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store/asyncstorereader.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/util/blockfile.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/regexp/include/regexp_advanced_api.h"

/***********************************************************************************
 ** Constructor
 **
 ** Store3::Store3
 **
 ***********************************************************************************/
Store3::Store3(MessageDatabase* message_database, CursorDB* cursor_db, CursorDBBackend* backend, BTreeDB<MessageIdKey>* index)
  : m_maildb(cursor_db)
  , m_maildb_backend(backend)
  , m_current_id(0)
  , m_message_database(message_database)
  , m_progress(0)
  , m_message_id_index(index)
  , m_readonly(FALSE)
  , m_next_m2_id_ready(FALSE)
  , m_needs_reindexing(FALSE)
  , m_next_m2_id(1)
  , m_reader_queue(100)
{
		
	if (m_message_database)
	{
		m_message_database->GroupSearchEngineFile(m_maildb_backend->GetGroupMember());
		m_message_database->GroupSearchEngineFile(m_message_id_index->GetGroupMember());
	}
	
	m_cache.SetDuplicateTable(&m_duplicate_table);
}


/***********************************************************************************
 ** Destructor
 **
 ** Store3::~Store3
 **
 ***********************************************************************************/
Store3::~Store3()
{
	DesktopMutexLock lock(m_mutex);

	OpStatus::Ignore(WriteNextM2ID());

	OpStatus::Ignore(m_maildb->Flush());

	m_maildb_backend->Close();
	OpStatus::Ignore(m_message_id_index->Close());

	OP_DELETE(m_maildb);
	OP_DELETE(m_maildb_backend);
	OP_DELETE(m_message_id_index);
}


/***********************************************************************************
 ** Initialize the store - call before calling any other functions and check return
 ** value
 **
 ** Store3::Init
 **
 ***********************************************************************************/
OP_STATUS Store3::Init(OpString& error_message, const OpStringC& store_path)
{
	// Initialize backends
	RETURN_IF_ERROR(InitBackend(error_message, store_path));

	// Initialize mutex
	RETURN_IF_ERROR(m_mutex.Init());

	return OpStatus::OK;
}


/***********************************************************************************
 ** Reinitialize the store (start from scratch)
 **
 ** Store3::ReInit
 **
 ***********************************************************************************/
OP_STATUS Store3::ReInit()
{
	DesktopMutexLock lock(m_mutex);

	// Clear backends
	RETURN_IF_ERROR(m_maildb_backend->Clear());
	RETURN_IF_ERROR(m_message_id_index->Clear());
	m_cache.Clear();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Adds a message to the Store
 **
 ** Store3::AddMessage
 **
 ***********************************************************************************/
OP_STATUS Store3::AddMessage(message_gid_t& id, StoreMessage& message, BOOL draft, BOOL headers_only)
{
	// Update threads with this new message
	RETURN_IF_ERROR(UpdateThreads(message));

	// Save message meta-info to the mailbase
	RETURN_IF_ERROR(UpdateMessage(message));
	id = message.GetId();

	// Save the whole message to disk
	if (!headers_only)
		RETURN_IF_ERROR(SetRawMessage(message));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Returns highest unique ID given to a message in Store
 **
 ** Store3::GetLastId
 **
 ***********************************************************************************/
message_gid_t Store3::GetLastId()
{
	DesktopMutexLock lock(m_mutex);

	return m_next_m2_id - 1;
}

/***********************************************************************************
 ** Returns the vector of message ids of all the replies to a specified message (all the direct children)
 **
 ** Store3::GetChildrenIds
 ** @param message_id Message to find the children for
 ** @param children_ids All messages that are children of the given message
 **
 ***********************************************************************************/
OP_STATUS Store3::GetChildrenIds(message_gid_t message_id, OpINTSet& children_ids)
{
	DesktopMutexLock lock(m_mutex);
	return m_cache.GetChildrenIds(message_id, children_ids);
}

/***********************************************************************************
 ** Returns the vector of all messages in the thread that a specified message is
 ** part of
 **
 ** Store3::GetTheadIds
 ** @param message_id Message to find a thread for
 ** @param thread_ids All messages in the thread that contains message_id
 **
 ***********************************************************************************/
OP_STATUS Store3::GetThreadIds(message_gid_t message_id, OpINTSet& thread_ids)
{
	DesktopMutexLock lock(m_mutex);
	return m_cache.GetThreadIds(message_id, thread_ids);
}


/***********************************************************************************
 ** Compares two messages according to sort_by, typically for display purposes
 **
 ** Store3::CompareMessages
 **
 ***********************************************************************************/
int Store3::CompareMessages(message_gid_t one, message_gid_t two, SortType sort_by)
{
	DesktopMutexLock lock(m_mutex);
	StoreField compare;

	// Find out which fields to compare
	switch (sort_by)
	{
		case SORT_BY_THREADED_SENT_DATE:
		case SORT_BY_THREADED_SENT_DATE_DESCENDING:
		case SORT_BY_SENT_DATE:
		case SORT_BY_SIZE:
		case SORT_BY_FLAGS:
		case SORT_BY_STATUS:
		case SORT_BY_ATTACHMENT:
		case SORT_BY_LABEL:
		{
			int result = CompareMessagesWithSortCache(one, two, sort_by);
			return result ? result : one - two;
		}
		case SORT_BY_ID:
			return one - two;
		case SORT_BY_MESSAGE_ID_HASH:
			compare = STORE_MESSAGE_ID;
			break;
		case SORT_BY_ACCOUNT_ID:
			compare = STORE_ACCOUNT_ID;
			break;
		case SORT_BY_PARENT:
			compare = STORE_PARENT;
			break;
		case SORT_BY_PREV_FROM:
			compare = STORE_FROM;
			break;
		case SORT_BY_PREV_TO:
			compare = STORE_TO;
			break;
		case SORT_BY_PREV_SUBJECT:
			compare = STORE_SUBJECT;
			break;
		case SORT_BY_MBOX_POS:
		default:
			// Not handled
			return 0;
	}

	// String or integer?
	if (m_maildb->GetField(compare).IsVariableLength())
	{
		OpString field1, field2;

		// Get field values
		RETURN_IF_ERROR(GotoMessage(one));
		RETURN_IF_ERROR(m_maildb->GetField(compare).GetStringValue(field1));

		RETURN_IF_ERROR(GotoMessage(two));
		RETURN_IF_ERROR(m_maildb->GetField(compare).GetStringValue(field2));

		// Compare
		bool is_feed = GetMessageFlag(one, StoreMessage::IS_NEWSFEED_MESSAGE) || GetMessageFlag(two, StoreMessage::IS_NEWSFEED_MESSAGE);

		int result = CompareStrings(field1, field2, compare, !is_feed);
		return result ? result : one - two;
	}
	else
	{
		INT64 field1, field2;

		// Get field values (size will be done automatically if field size is smaller than 64-bit)
		RETURN_IF_ERROR(GotoMessage(one));
		m_maildb->GetField(compare).GetValue(&field1);

		RETURN_IF_ERROR(GotoMessage(two));
		m_maildb->GetField(compare).GetValue(&field2);

		// Compare
		int result = field1 - field2;
		return result ? result : one - two;
	}
}


/***********************************************************************************
 ** Updates the message meta-info on disk with status information from the message
 **
 ** Store3::UpdateMessage
 ** @param draft Used for ougoing messages, going from drafts to outbox to sent and needing frequent updates
 **
 ***********************************************************************************/
OP_STATUS Store3::UpdateMessage(StoreMessage& message, BOOL draft)
{
	DesktopMutexLock lock(m_mutex);

	// Check if this is an existing message, otherwise create a new row
	if (message.GetId())
		RETURN_IF_ERROR(GotoMessage(message.GetId()));
	else if (!HasFinishedLoading())
		return OpStatus::ERR;
	else
	{
		RETURN_IF_ERROR(m_maildb->Create());
		m_current_id = 0;
	}

	// Get values we need to save
	Header::HeaderValue to, from, subject;
	time_t sent_date;
	OpString8 message_id;

	OpStatus::Ignore(message.GetHeaderValue(Header::TO, to));
	OpStatus::Ignore(message.GetHeaderValue(Header::FROM, from));
	OpStatus::Ignore(message.GetDateHeaderValue(Header::DATE, sent_date));
	OpStatus::Ignore(message.GetMessageId(message_id));
	OpStatus::Ignore(message.GetHeaderValue(Header::SUBJECT, subject));

	// If there's no received time yet, use the current time
	if (!message.GetRecvTime())
		message.SetRecvTime(g_timecache->CurrentTime());

	// Every message needs a sent date. If there's no sent date, use current
	if (!sent_date)
	{
		sent_date = g_timecache->CurrentTime();
		RETURN_IF_ERROR(message.SetDateHeaderValue(Header::DATE, sent_date));
	}

	// Get M2 ID, set current ID once we have it
	if (!message.GetId())
	{
		RETURN_IF_ERROR(CreateM2ID(message));
		m_current_id = message.GetId();

		// If this is a new message, no storage type yet
		RETURN_IF_ERROR(m_maildb->GetField(STORE_MBX_TYPE).SetValue(AccountTypes::MBOX_NONE));
		RETURN_IF_ERROR(m_maildb->GetField(STORE_MBX_DATA).SetValue(0));
	}

	// Save data content in backend
	RETURN_IF_ERROR(m_maildb->GetField(STORE_RECV_DATE) .SetValue(message.GetRecvTime()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_SENT_DATE) .SetValue(sent_date));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_SIZE)      .SetValue(message.GetMessageSize()));
	RETURN_IF_ERROR(WriteFlags(message.GetAllFlags()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_ACCOUNT_ID).SetValue(message.GetAccountId()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_PARENT)    .SetValue(message.GetParentId()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_M2_ID)     .SetValue(message.GetId()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_FROM)      .SetStringValue(from.CStr()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_TO)        .SetStringValue(to.CStr()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_SUBJECT)   .SetStringValue(subject.CStr()));
	if (message_id.HasContent())
		RETURN_IF_ERROR(m_maildb->GetField(STORE_MESSAGE_ID).SetStringValue(message_id.CStr()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_INTERNET_LOCATION).SetStringValue(message.GetInternetLocation().CStr()));

	// Flush data to make sure it is saved, and to get an ID
	RETURN_IF_ERROR(m_maildb->Flush());

	// Save message ID to message ID index
	if (message_id.HasContent())
	{
		MessageIdKey id_key(Hash::String(message_id.CStr()) & 0x7FFFFFFF, message.GetId());

		RETURN_IF_ERROR(m_message_id_index->Insert(id_key, TRUE));
	}

	// Make sure cache is updated
	RETURN_IF_ERROR(UpdateCache(message.GetId()));


	// Alert listeners
	AlertMessageChanged(message.GetId());

	// Make sure to save data
	return RequestCommit();
}


/***********************************************************************************
 ** Called when message has been confirmed sent.
 **
 ** Store3::MessageSent
 **
 ***********************************************************************************/
OP_STATUS Store3::MessageSent(message_gid_t id)
{
	// TODO Check order of UpdateMessage: earlier or later?
	Message message;

	// Get all data
	RETURN_IF_ERROR(GetMessage(message, id));
	if (!message.GetRawBody())
		OpStatus::Ignore(GetMessageData(message));

	// Remove existing raw message
	OpStatus::Ignore(RemoveRawMessage(id)); // Ignore because even if it fails, it's important to set the flag correctly and continue the operation

	// Set sent flag
	message.SetFlag(StoreMessage::IS_SENT, TRUE);
	RETURN_IF_ERROR(UpdateMessage(message));

	// Set raw message
	RETURN_IF_ERROR(SetRawMessage(message));

	// Notify Message Database
	m_message_database->OnMessageMarkedAsSent(id);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Reads in a Message from disk or cache, but only fills in
 ** essential headers. For full body, call GetRawMessage() later.
 **
 ** Store3::MessageSent
 **
 ***********************************************************************************/
OP_STATUS Store3::GetMessage(StoreMessage& message, message_gid_t id, BOOL import, BOOL minimal_headers)
{
	DesktopMutexLock lock(m_mutex);
	INT64 tmp_int;

	RETURN_IF_ERROR(GotoMessage(id));

	// Set message details: account
	if (!import)
	{
		RETURN_IF_ERROR(message.Init((UINT16)m_maildb->GetField(STORE_ACCOUNT_ID).GetValue(&tmp_int)));
	}

	// ID
	message.SetId(id);

	// Integer values
	message.SetMessageSize(m_maildb->GetField(STORE_SIZE)     .GetValue(&tmp_int));
	message.SetAllFlags(   ReadFlags()                                           );
	message.SetParentId(   m_maildb->GetField(STORE_PARENT)   .GetValue(&tmp_int));
	message.SetRecvTime(   m_maildb->GetField(STORE_RECV_DATE).GetValue(&tmp_int));

	// Date headers
	OpStatus::Ignore(message.SetDateHeaderValue(Header::DATE, (time_t)m_maildb->GetField(STORE_SENT_DATE).GetValue(&tmp_int)));

	// String headers
	OpString tmp_string;
	if (!minimal_headers || !message.IsFlagSet(StoreMessage::IS_OUTGOING))
	{
		RETURN_IF_ERROR(m_maildb->GetField(STORE_FROM).GetStringValue(tmp_string));
		if (tmp_string.HasContent())
		{
				OpStatus::Ignore(message.SetHeaderValue(Header::FROM, tmp_string));
		}
	}
	
	if (!minimal_headers || message.IsFlagSet(StoreMessage::IS_OUTGOING))
	{
		RETURN_IF_ERROR(m_maildb->GetField(STORE_TO).GetStringValue(tmp_string));
		if (tmp_string.HasContent())
		{
			OpStatus::Ignore(message.SetHeaderValue(Header::TO, tmp_string));
		}
	}

	RETURN_IF_ERROR(m_maildb->GetField(STORE_SUBJECT).GetStringValue(tmp_string));
	if (tmp_string.HasContent())
	{
		OpStatus::Ignore(message.SetHeaderValue(Header::SUBJECT, tmp_string));
	}

	if (!minimal_headers)
	{
		OpString8 tmp_string8;
		RETURN_IF_ERROR(m_maildb->GetField(STORE_INTERNET_LOCATION).GetStringValue(tmp_string8));
		if (tmp_string8.HasContent())
		{
			OpStatus::Ignore(message.SetInternetLocation(tmp_string8));
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Setting of a message flag
 **
 ** Store3::SetMessageFlag
 **
 ***********************************************************************************/
OP_STATUS Store3::SetMessageFlag(message_gid_t id, StoreMessage::Flags flag, BOOL value)
{
	// Fetch existing flags and set new flags
	UINT64 oldflags = GetMessageFlags(id);
	UINT64 newflags = value ? oldflags | (static_cast<UINT64>(1) << flag) : oldflags & ~(static_cast<UINT64>(1) << flag);

	return SetMessageFlags(id, newflags);
}


/***********************************************************************************
 ** Sets all message flags
 **
 ** Store3::SetMessageFlags
 **
 ***********************************************************************************/
OP_STATUS Store3::SetMessageFlags(message_gid_t id, INT64 flags)
{
	StoreItem cache_item = GetFromCache(id);

	if (cache_item.flags != flags)
	{
		DesktopMutexLock lock(m_mutex);

		// Set new flags in mailbase and cache
		RETURN_IF_ERROR(GotoMessage(id));
		RETURN_IF_ERROR(WriteFlags(flags));
		cache_item.flags = flags;
		m_cache.UpdateItem(cache_item);

		// call messagechanged here because the lowlevel storeitem UpdateMessage() does not call it
		AlertMessageChanged(id);

		return RequestCommit();
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Retrieves the account id of a message
 **
 ** Store3::GetMessageAccountId
 **
 ***********************************************************************************/
UINT16 Store3::GetMessageAccountId(message_gid_t id)
{
	DesktopMutexLock lock(m_mutex);
	UINT32			 account_id;

	if (OpStatus::IsSuccess(GotoMessage(id)))
		return m_maildb->GetField(STORE_ACCOUNT_ID).GetValue(&account_id);
	else
		return 0;
}


/***********************************************************************************
 ** Gets the internet location of a specific message
 **
 ** Store3::GetMessageInternetLocation
 **
 ***********************************************************************************/
OP_STATUS Store3::GetMessageInternetLocation(message_gid_t id, OpString8& internet_location)
{
	DesktopMutexLock lock(m_mutex);

	RETURN_IF_ERROR(GotoMessage(id));

	return m_maildb->GetField(STORE_INTERNET_LOCATION).GetStringValue(internet_location);
}


/***********************************************************************************
 ** Gets the message ID of a specific message
 **
 ** Store3::GetMessageMessageId
 **
 ***********************************************************************************/
OP_STATUS Store3::GetMessageMessageId(message_gid_t id, OpString8& message_id)
{
	DesktopMutexLock lock(m_mutex);

	RETURN_IF_ERROR(GotoMessage(id));

	return m_maildb->GetField(STORE_MESSAGE_ID).GetStringValue(message_id);
}


/***********************************************************************************
 ** Gets a M2 message ID by looking at message id headers
 **
 ** Store3::GetMessageByMessageId
 **
 ***********************************************************************************/
message_gid_t Store3::GetMessageByMessageId(const OpStringC8& message_id)
{
	if (message_id.IsEmpty())
		return 0;

	DesktopMutexLock lock(m_mutex);
	message_gid_t m2_id = 0;

	// Search for message id
	MessageIdKey search_key(Hash::String(message_id.CStr()) & 0x7FFFFFFF, 0);
	OpAutoPtr< SearchIterator< MessageIdKey > > iterator (m_message_id_index->Search(search_key, operatorGT));

	// Check if there were any results
	if (iterator.get() && !iterator->Empty() && iterator->Get().GetMessageIdHash() == search_key.GetMessageIdHash())
	{
		do
		{
			OpString8 found_message_id;

			RETURN_IF_ERROR(GotoMessage(iterator->Get().GetM2Id()));
			RETURN_IF_ERROR(m_maildb->GetField(STORE_MESSAGE_ID).GetStringValue(found_message_id));

			if (!message_id.Compare(found_message_id))
				m2_id = iterator->Get().GetM2Id();

		} while (iterator->Next() && iterator->Get().GetMessageIdHash() == search_key.GetMessageIdHash());
	}

	if (GetFromCache(m2_id).master_id != 0)
		return GetFromCache(m2_id).master_id;

	return m2_id;
}


/***********************************************************************************
 ** Schedules a message for deletion from the message base, to be called by selexicon
 **
 ** Store3::RemoveMessage
 **
 ***********************************************************************************/
OP_STATUS Store3::RemoveMessage(message_gid_t id)
{
	// Put into list of deleted messages
	DeletedMessage* message = OP_NEW(DeletedMessage, (id));
	if (!message)
		return OpStatus::ERR_NO_MEMORY;

	message->Into(&m_deleted_messages);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Deletes all messages marked for removal from cache, store and disk
 **
 ** Store3::CleanUpDeletedMessages
 **
 ***********************************************************************************/

OP_STATUS Store3::CleanUpDeletedMessages()
{
	// Write out 'deleted' message to store
    for (DeletedMessage* msg = static_cast<DeletedMessage*>(m_deleted_messages.First()); msg; msg = static_cast<DeletedMessage*>(msg->Suc()))
    {
		if (OpStatus::IsError(GotoMessage(msg->m2_id)))
			continue; // Don't remove if it can't be found

		// Remove message from sort cache
		RemoveFromCache(msg->m2_id);

		// Remove message from 'raw' storage
		OpStatus::Ignore(RemoveRawMessage(msg->m2_id));

		// Remove message from message ID index
		OpString8  message_id;
		RETURN_IF_ERROR(m_maildb->GetField(STORE_MESSAGE_ID).GetStringValue(message_id)); //only OOM
		if (message_id.HasContent())
		{
			MessageIdKey key(Hash::String(message_id.CStr()) & 0x7FFFFFFF, msg->m2_id);

			OpStatus::Ignore(m_message_id_index->Delete(key));
		}

		// Remove message from mail db
		OpStatus::Ignore(m_maildb->Delete());
		m_current_id = 0;
    }
	
	// Request a new commit if we actually deleted items
	if (m_deleted_messages.First() != NULL)
		OpStatus::Ignore(RequestCommit());

    m_deleted_messages.Clear();

	return OpStatus::OK;
}


/***********************************************************************************
 ** Deletes multiple messages permanently from the message base
 **
 ** Store3::RemoveMessages
 **
 ***********************************************************************************/
OP_STATUS Store3::RemoveMessages(const OpINT32Vector& id_vector)
{
	int		  vector_count = id_vector.GetCount();
	OP_STATUS ret 		   = OpStatus::OK;

	if (vector_count == 0)
		return OpStatus::OK;

	for (int j = 0; OpStatus::IsSuccess(ret) && j < vector_count; j++)
		ret = RemoveMessage(id_vector.Get(j));

	AlertMessageChanged(UINT_MAX);

	return ret;
}


/***********************************************************************************
 **
 **
 ** Store3::ReadData
 **
 ***********************************************************************************/
OpFileLength Store3::ReadData(OpFileLength startpos, int blockcount)
{
	DesktopMutexLock lock(m_mutex);
	int          block_size = m_maildb_backend->GetBlockSize();
	OpFileLength file_size  = m_maildb_backend->GetFileSize();
	OpFileLength pos = startpos;
	OpFileLength endpos = startpos + blockcount * block_size;

	OpAutoPtr<MessageDatabase::MessageChangeLock> changelock(m_message_database ? m_message_database->CreateMessageChangeLock() : 0);
	message_gid_t reading_current_id;
	for (; pos < file_size && pos < endpos; pos += block_size)
	{
		if (m_maildb_backend->IsStartBlock(pos) &&
			OpStatus::IsSuccess(m_maildb->Goto(pos / block_size)))
		{
			// Build cache
			m_maildb->GetField(STORE_M2_ID).GetValue(&m_current_id);
			if (m_current_id == 0 && m_next_m2_id_ready)
			{
				m_current_id = m_next_m2_id++;
				if (OpStatus::IsError(m_maildb->GetField(STORE_M2_ID).SetValue(m_current_id)))
					m_current_id = 0;
			}
			reading_current_id = m_current_id;
			
			if (OpStatus::IsSuccess(UpdateCache(m_current_id)) && reading_current_id != 0)
				MakeMessageAvailable(reading_current_id);
			
			m_next_m2_id = max(m_next_m2_id, reading_current_id + 1);
		}
	}


	if (pos >= file_size)
	{
		m_next_m2_id_ready = TRUE;
		m_needs_reindexing = FALSE;

		if (m_message_database)
			m_message_database->OnAllMessagesAvailable();

		if (m_progress)
			m_progress->EndCurrentAction(FALSE);

		return 0;
	}

	if (m_progress)
		m_progress->SetCurrentActionProgress(pos, file_size);

	return pos;
}

void Store3::MakeMessageAvailable(message_gid_t message_id)
{
	// If there is an omailbase.dat with messages left but no accounts, m_message_database will not be initialized, avoid crashes
	if (!m_message_database || !m_message_database->IsInitialized())
		return;

	const StoreItem& item = GetFromCache(message_id);

	// Permanently removed messages need to be re-removed
	if (item.flags & (1 << StoreMessage::PERMANENTLY_REMOVED))
	{
		m_message_database->RemoveMessage(item.m2_id);
	}
	else
	{
		// Let others know message has become available'
		m_message_database->OnMessageMadeAvailable(item.m2_id, item.flags & (1 << StoreMessage::IS_READ));

		// Message might need indexing
		if (item.flags & (1 << StoreMessage::IS_WAITING_FOR_INDEXING) || m_needs_reindexing)
		{
			if (m_needs_reindexing)
				SetMessageFlag(item.m2_id, StoreMessage::IS_WAITING_FOR_INDEXING, TRUE);

			m_message_database->OnMessageNeedsReindexing(item.m2_id);
		}
		
	}
}

/***********************************************************************************
 ** Retrieves full message content from the local disk cache if available.
 **
 ** Store3::GetMessageData
 **
 ***********************************************************************************/
OP_STATUS Store3::GetMessageData(StoreMessage& message)
{
	DesktopMutexLock lock(m_mutex);
	INT32	   mbx_type;
	INT64	   mbx_data;
	MboxStore* mbox_store;

	RETURN_IF_ERROR(GotoMessage(message.GetId()));

	// Get message data
	m_maildb->GetField(STORE_MBX_TYPE).GetValue(&mbx_type);
	m_maildb->GetField(STORE_MBX_DATA).GetValue(&mbx_data);

	// Get store for this message
	RETURN_IF_ERROR(m_mbox_store_manager.GetStore(mbx_type, 0, mbox_store));

	// Get message
	OP_STATUS ret = mbox_store->GetMessage(mbx_data, message);

	// If this message wasn't in permanent storage, we're finished now - doesn't matter if it returned an error
	if (mbx_type == AccountTypes::MBOX_NONE)
		return OpStatus::OK;

	// If message can't be found, reset mbox status on this message
	// unless the store is readonly (like when importing)
	if (ret == OpStatus::ERR_FILE_NOT_FOUND && !m_readonly)
	{
		m_maildb->GetField(STORE_MBX_TYPE).SetValue(AccountTypes::MBOX_NONE);
		m_maildb->GetField(STORE_MBX_DATA).SetValue(0);
		UpdateCache(message.GetId());
		RequestCommit();

		return OpStatus::OK;
	}

	return ret;
}

/***********************************************************************************
 **  Retrieves full message of a draft from disk
 **
 ** Store3::GetDraftData
 **
 ***********************************************************************************/
OP_STATUS Store3::GetDraftData(StoreMessage& message, message_gid_t id)
{
	MboxStore* mbox_store;

	// Get store for this message
	RETURN_IF_ERROR(m_mbox_store_manager.GetStore(AccountTypes::MBOX_PER_MAIL, 0, mbox_store));

	// Get draft
	return mbox_store->GetDraftMessage(message, id);
}

/***********************************************************************************
 ** Checks if a message has a body
 **
 ** Store3::HasMessageDownloadedBody
 **
 ***********************************************************************************/
OP_STATUS Store3::HasMessageDownloadedBody(message_gid_t id, BOOL& has_downloaded_body)
{
	has_downloaded_body = FALSE;

	if (!id)
		return OpStatus::OK;

	INT32 mbx_type = GetFromCache(id).mbx_type;

	// Get store for this message and check
	MboxStore* store;

	RETURN_IF_ERROR(m_mbox_store_manager.GetStore(mbx_type, 0, store));
	has_downloaded_body = store->IsCached(id);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Stores local copy of message data after retrieval of body from server.
 **
 ** Store3::SetRawMessage
 **
 ***********************************************************************************/
OP_STATUS Store3::SetRawMessage(StoreMessage& message)
{
	DesktopMutexLock lock(m_mutex);
	MboxStore* mbox_store;
	INT32	   mbx_type;
	INT64	   mbx_data = 0;
	BOOL	   outgoing = message.GetAllFlags() & (1 << StoreMessage::IS_OUTGOING);

	// Make sure raw data is current
	OpStatus::Ignore(message.PrepareRawMessageContent());

	RETURN_IF_ERROR(GotoMessage(message.GetId()));

	// Get type and store
	mbx_type = message.GetPreferredStorage();
	RETURN_IF_ERROR(m_mbox_store_manager.GetStore(mbx_type, outgoing ? MboxStore::UPDATE_FULL_MESSAGES : 0, mbox_store));

	// Add message
	RETURN_IF_ERROR(mbox_store->AddMessage(message, mbx_data));

	// Set properties in database
	RETURN_IF_ERROR(m_maildb->GetField(STORE_SIZE)    .SetValue(message.GetMessageSize()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_MBX_TYPE).SetValue(mbox_store->GetType()));
	RETURN_IF_ERROR(m_maildb->GetField(STORE_MBX_DATA).SetValue(mbx_data));

	// Save database
	RETURN_IF_ERROR(RequestCommit());

	UpdateCache(message.GetId());

	// Notify message database that message body has changed
	if (m_message_database)
		m_message_database->OnMessageBodyChanged(message);

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** Store3::MessageAvailable
 **
 ***********************************************************************************/
BOOL Store3::MessageAvailable(message_gid_t id)
{
	const StoreItem& item = GetFromCache(id);

	return item.m2_id != 0 && !(item.flags & (1 << StoreMessage::PERMANENTLY_REMOVED));
}


/***********************************************************************************
 ** Change the storage type for all messages of a certain account
 **
 ** Store3::ChangeStorageType
 **
 ***********************************************************************************/
OP_STATUS Store3::ChangeStorageType(UINT16 account_id, int to_type)
{
	DesktopMutexLock lock(m_mutex);

	// Walk through all messages in the store
	int          block_size = m_maildb_backend->GetBlockSize();
	OpFileLength file_size  = m_maildb_backend->GetFileSize();

	for (OpFileLength pos = 0; pos < file_size; pos += block_size)
	{
		if (m_maildb_backend->IsStartBlock(pos) &&
			OpStatus::IsSuccess(m_maildb->Goto(pos / block_size)))
		{
			// Save id
			m_maildb->GetField(STORE_M2_ID).GetValue(&m_current_id);

			UINT32 message_account_id;
			int    mbx_type, flags;

			// Check if we need to change this one
			if (m_maildb->GetField(STORE_ACCOUNT_ID).GetValue(&message_account_id) == account_id &&
				m_maildb->GetField(STORE_MBX_TYPE)  .GetValue(&mbx_type) != to_type &&
				mbx_type != AccountTypes::MBOX_NONE &&
				!(m_maildb->GetField(STORE_FLAGS)   .GetValue(&flags) & 1 << StoreMessage::IS_OUTGOING))
			{
				Message message;

				// Get message
				RETURN_IF_ERROR(GetMessage(message, m_current_id));

				// See if we can get a body for the message
				if (OpStatus::IsSuccess(GetMessageData(message)))
				{
					// Remove body from current storage
					RETURN_IF_ERROR(RemoveRawMessage(message.GetId()));

					// Add body to new storage
					RETURN_IF_ERROR(SetRawMessage(message));

					// Notify listeners that message has changed
					AlertMessageChanged(message.GetId());
				}
			}
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Request a commit of indexes
 **
 ** Store3::RequestCommit
 **
 ***********************************************************************************/
OP_STATUS Store3::RequestCommit()
{
	if (!g_op_system_info->IsInMainThread())
		return OpStatus::OK;

	OP_ASSERT(!m_readonly);

	return m_message_database ? m_message_database->RequestCommit() : OpStatus::OK;
}


/***********************************************************************************
 ** Commit data to disk
 **
 ** Store3::CommitData
 **
 ***********************************************************************************/
OP_STATUS Store3::CommitData()
{
	DesktopMutexLock lock(m_mutex);

	OpStatus::Ignore(WriteNextM2ID());

	// Commit own indexes
	if (OpStatus::IsError(m_maildb->Flush()) ||	// Flush current row before committing
		(m_maildb_backend->InTransaction() && OpStatus::IsError(m_maildb_backend->Commit())) ||
		OpStatus::IsError(m_message_id_index->Commit()) ||
		OpStatus::IsError(CleanUpDeletedMessages()))
	{
		OpStatus::Ignore(RequestCommit());
		return OpStatus::ERR;
	}

	// Commit raw message stores
	return m_mbox_store_manager.CommitData();
}


/***********************************************************************************
 ** Removes a message from raw storage
 **
 ** Store3::RemoveRawMessage
 ** @param id M2 ID of the message to remove
 **
 ***********************************************************************************/
OP_STATUS Store3::RemoveRawMessage(message_gid_t id)
{
	DesktopMutexLock lock(m_mutex);
	INT32	   mbx_type;
	INT64	   mbx_data;
	UINT16	   account_id;
	time_t	   sent_date;
	MboxStore* mbox_store;
	int		   flags;

	RETURN_IF_ERROR(GotoMessage(id));

	// Get data for this message
	m_maildb->GetField(STORE_MBX_TYPE).GetValue(&mbx_type);

	// Remove message from 'raw' storage
	m_maildb->GetField(STORE_MBX_DATA)  .GetValue(&mbx_data);
	m_maildb->GetField(STORE_ACCOUNT_ID).GetValue(&account_id);
	m_maildb->GetField(STORE_SENT_DATE) .GetValue(&sent_date);
	m_maildb->GetField(STORE_FLAGS)     .GetValue(&flags);

	BOOL draft = (flags & (1 << StoreMessage::IS_OUTGOING)) && !(flags & (1 << StoreMessage::IS_SENT));

	RETURN_IF_ERROR(m_mbox_store_manager.GetStore(mbx_type, 0, mbox_store));
	RETURN_IF_ERROR(mbox_store->RemoveMessage(mbx_data, id, account_id, sent_date, draft));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Updates thread information for a specific message, call when added
 **
 ** Store3::UpdateThreads
 **
 ***********************************************************************************/
OP_STATUS Store3::UpdateThreads(StoreMessage& message)
{
	// Look for references
	Header*		  references	 = message.GetHeader(Header::REFERENCES);
	int			  index_from_end = 0;
	message_gid_t parent		 = 0;
	OpString8	  reference_id;

	if (!references)
	{
		// Take in-reply-to instead
		references = message.GetHeader(Header::INREPLYTO);
		if (!references) // No references found at all
			return OpStatus::OK;
	}

	// Get the message ids for the references, start from the end, stop as soon as we found a message we know
	while (!parent && OpStatus::IsSuccess(references->GetMessageId(reference_id, index_from_end)) && reference_id.HasContent())
	{
		parent = GetMessageByMessageId(reference_id);
		index_from_end++;
	}

	// Save information on parent
	if (parent)
	{
		if (g_pcm2->GetIntegerPref(PrefsCollectionM2::SplitUpThreadWhenSubjectChanged))
		{
			Header::HeaderValue subject1;
			OpString subject2;
			RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject1));
			RETURN_IF_ERROR(GotoMessage(parent));
			RETURN_IF_ERROR(m_maildb->GetField(STORE_SUBJECT).GetStringValue(subject2));
			if (CompareStrings(subject1, subject2, STORE_SUBJECT, true) == 0)
				message.SetParentId(parent);
		}
		else
		{
			message.SetParentId(parent);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Compare two database fields
 **
 ** Store3::CompareStrings
 **
 ***********************************************************************************/
int Store3::CompareStrings(const OpStringC& field1, const OpStringC& field2, StoreField compare, bool strip_subject)
{
	switch (compare)
	{
		case STORE_FROM:
		case STORE_TO:
			return StripAddress(field1).CompareI(StripAddress(field2));
		case STORE_SUBJECT:
			if (strip_subject)
				return StripSubject(field1).CompareI(StripSubject(field2));
			else
				return field1.CompareI(field2);
		default:
			return field1.CompareI(field2);
	}
}


/***********************************************************************************
 ** Strip an address field for comparisons
 **
 ** Store3::StripAddress
 **
 ***********************************************************************************/
OpStringC Store3::StripAddress(const OpStringC& address)
{
	if (address.IsEmpty())
		return address;

	const uni_char* scanner = address.CStr();

	while (*scanner == '"' || *scanner == '<')
		scanner++;

	return scanner;
}

/***********************************************************************************
 ** Strip a subject field for comparisons
 **
 ** Store3::StripSubject
 **
 ***********************************************************************************/
OpStringC Store3::StripSubject(const OpStringC& subject)
{
	if (subject.IsEmpty())
		return subject;

	int last_square_bracket = subject.FindLastOf(']');
	int last_colon = subject.FindLastOf(':');
	const uni_char* scanner = subject.CStr() + max(last_square_bracket, last_colon)+1;
	while (uni_isspace(*scanner))
		scanner ++;

	return scanner;
}

/***********************************************************************************
 ** Sets the cursor to a specific message
 **
 ** Store3::GotoMessage
 **
 ***********************************************************************************/
OP_STATUS Store3::GotoMessage(message_gid_t id)
{
	if (id == 0)
		return OpStatus::ERR;

	DesktopMutexLock lock(m_mutex);

	if (m_current_id == id)
		return OpStatus::OK;

	// Check if requested message exists and get row id
	const StoreItem& item = GetFromCache(id);

	if (!item.m2_id || OpStatus::IsError(m_maildb->Goto(item.row_id)))
	{
		m_current_id = 0;
		return OpStatus::ERR;
	}

	m_current_id = id;
	return OpStatus::OK;
}


/***********************************************************************************
 ** Get an M2 ID for a message and adds it to M2 ID index
 **
 ** Store3::CreateM2ID
 **
 ***********************************************************************************/
OP_STATUS Store3::CreateM2ID(StoreMessage& message)
{
	// Get new M2 id
	if (!m_next_m2_id_ready)
		return OpStatus::ERR;
	message.SetId(m_next_m2_id++);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Compare the sorting of two messages using the sort cache
 **
 ** Store3::CompareMessagesWithSortCache
 **
 ***********************************************************************************/
int Store3::CompareMessagesWithSortCache(message_gid_t one, message_gid_t two, SortType sort_by)
{
	const StoreItem& message_one = GetFromCache(one);
	const StoreItem& message_two = GetFromCache(two);

	switch(sort_by)
	{
		case SORT_BY_SENT_DATE:
			return message_one.sent_date - message_two.sent_date;
		case SORT_BY_THREADED_SENT_DATE:
			return message_one.child_sent_date - message_two.child_sent_date;
		case SORT_BY_THREADED_SENT_DATE_DESCENDING:
		{
			if (message_one.thread_root_id == message_two.thread_root_id)
				return message_two.sent_date - message_one.sent_date;
			else
				return message_one.child_sent_date - message_two.child_sent_date;
		}
		case SORT_BY_SIZE:
			return message_one.size - message_two.size;
		case SORT_BY_FLAGS:
			return message_one.flags - message_two.flags;
		case SORT_BY_STATUS:
		{
			if (message_one.flags >> StoreMessage::IS_FLAGGED & 1)
				return 1;
			if (message_two.flags >> StoreMessage::IS_FLAGGED & 1)
				return -1;

			int flags = 0;
			for (int i = StoreMessage::IS_REPLIED; i < StoreMessage::STATUS_FLAG_COUNT; i++)
				flags |= (1 << i);

			int res = (message_one.flags & flags) - (message_two.flags & flags);
			if (res != 0)
				return res;

			return !(message_one.flags >> StoreMessage::IS_READ & 1) - !(message_two.flags >> StoreMessage::IS_READ & 1);
		}
		case SORT_BY_ATTACHMENT:
			return CompareFlagRange(message_one.flags, message_two.flags, StoreMessage::HAS_ATTACHMENT, StoreMessage::ATTACHMENT_FLAG_COUNT);
	}

	return 0;
}


/***********************************************************************************
 ** Compare two bitsets of flags, but only compare a specific range of flags
 **
 ** flags1 will be considered larger than flags2 if and only if
 ** (Exists x : range_start <= x < range_end : flags1 & x &&
 **       (ForAll y : range_start <= y < range_end : flags2 & y => x > y) )
 **
 ** Store3::CompareFlagRange
 **
 ***********************************************************************************/
int Store3::CompareFlagRange(int flags1, int flags2, int range_start, int range_end)
{
	int flags = 0;

	for (int i = range_start; i < range_end; i++)
		flags |= (1 << i);

	return (flags1 & flags) - (flags2 & flags);
}


/***********************************************************************************
 ** Add a message/update a message in the cache
 **
 ** Store3::UpdateCache
 **
 ***********************************************************************************/
OP_STATUS Store3::UpdateCache(message_gid_t id)
{
	DesktopMutexLock lock(m_mutex);

	RETURN_IF_ERROR(GotoMessage(id));

	StoreItem item(id, m_maildb->GetID());

	// Update item
	m_maildb->GetField(STORE_PARENT)   .GetValue(&item.parent_id);
	m_maildb->GetField(STORE_SENT_DATE).GetValue(&item.sent_date);
	m_maildb->GetField(STORE_SENT_DATE).GetValue(&item.child_sent_date);
	m_maildb->GetField(STORE_SIZE)     .GetValue(&item.size);
	item.flags = ReadFlags();
	m_maildb->GetField(STORE_MBX_TYPE) .GetValue(&item.mbx_type);
	item.master_id = GetFromCache(id).master_id;
	item.thread_root_id = GetFromCache(id).thread_root_id;

	if (item.master_id == 0)
	{
		RETURN_IF_ERROR(AddToDuplicateTable(item));
	}

	return m_cache.UpdateItem(item);
}

OP_STATUS Store3::AddToDuplicateTable(StoreItem &item)
{
	if (m_current_id != item.m2_id)
		return OpStatus::ERR;

	// find out if there are other messages with the same Message-Id and cache that
	OpString8 message_id;
	RETURN_IF_ERROR(m_maildb->GetField(STORE_MESSAGE_ID).GetStringValue(message_id));

	if (message_id.HasContent())
	{
		MessageIdKey search_key(Hash::String(message_id.CStr()) & 0x7FFFFFFF, 0);
		OpAutoPtr< SearchIterator< MessageIdKey > > iterator (m_message_id_index->Search(search_key, operatorGT));

		// Check if there were any results
		if ( iterator.get() && !iterator->Empty() && iterator->Get().GetMessageIdHash() == search_key.GetMessageIdHash() )
		{
			do
			{
				if (iterator->Get().GetM2Id() == item.m2_id)
					continue;

				OpString8 found_message_id;
				StoreItem dupe = GetFromCache(iterator->Get().GetM2Id());

				// it might not be in cache yet, look for another dupe
				if (dupe.m2_id == 0)
					continue;

				RETURN_IF_ERROR(GotoMessage(iterator->Get().GetM2Id()));
				RETURN_IF_ERROR(m_maildb->GetField(STORE_MESSAGE_ID).GetStringValue(found_message_id));

				if (!message_id.Compare(found_message_id))
				{
					if (dupe.master_id != 0)
					{
						item.master_id = dupe.master_id;
						
					}
					else if (dupe.m2_id != 0)
					{
						dupe.master_id = dupe.m2_id;
						item.master_id = dupe.m2_id;
						RETURN_IF_ERROR(m_cache.UpdateItem(dupe));
					}
					return m_duplicate_table.AddDuplicate(item.master_id, item.m2_id);
				}

			} while (iterator->Next() && iterator->Get().GetMessageIdHash() == search_key.GetMessageIdHash());
		}
	}
	return OpStatus::OK;
}

void Store3::RemoveFromDuplicateTable(StoreItem& cache_item)
{
	// remember the old set of duplicates before removing one of them in case we remove the only duplicate
	OpINT32Vector dupes;
	const DuplicateMessage* dupe = m_duplicate_table.GetDuplicates(cache_item.master_id);
	while (dupe)
	{
		OpStatus::Ignore(dupes.Add(dupe->m2_id));
		dupe = dupe->next;
	}

	message_gid_t new_master_id, old_master = cache_item.master_id;
	OpStatus::Ignore(m_duplicate_table.RemoveDuplicate(cache_item.master_id, cache_item.m2_id, new_master_id));
	if (new_master_id == 0)
	{
		for (UINT32 i = 0; i < dupes.GetCount(); i++)
		{
			cache_item = GetFromCache(dupes.Get(i));
			cache_item.master_id = 0;
			OpStatus::Ignore(m_cache.UpdateItem(cache_item));
		}
	}
	else if (new_master_id != old_master)
	{
		const DuplicateMessage* dupe = m_duplicate_table.GetDuplicates(new_master_id);
		while (dupe)
		{
			cache_item = GetFromCache(dupe->m2_id); 
			cache_item.master_id = new_master_id;
			OpStatus::Ignore(m_cache.UpdateItem(cache_item));
			dupe = dupe->next;
		}
	}
}


/***********************************************************************************
 ** Remove a message from the cache
 **
 ** Store3::RemoveFromSortCache
 **
 ***********************************************************************************/
void Store3::RemoveFromCache(message_gid_t id)
{
	DesktopMutexLock lock(m_mutex);

	StoreItem cache_item = GetFromCache(id);
	
	if (cache_item.master_id != 0)
	{
		RemoveFromDuplicateTable(cache_item);
	}

	m_cache.RemoveItem(id);
}


/***********************************************************************************
 ** Get a message from the cache
 **
 ** Store3::GetFromCache
 ** @param id ID of message to get
 ** @return The actual item in cache if found, or an item preset with all 0 if not
 **
 ***********************************************************************************/
const StoreItem& Store3::GetFromCache(message_gid_t id)
{
	DesktopMutexLock lock(m_mutex);
	return m_cache.GetFromCache(id);
}


/***********************************************************************************
 ** Alert listeners that a message changed
 **
 ** Store3::AlertMessageChanged
 **
 ***********************************************************************************/
void Store3::AlertMessageChanged(message_gid_t id)
{
	if (!g_op_system_info->IsInMainThread())
		return;

	if (m_message_database)
		m_message_database->OnMessageChanged(id);
}


/***********************************************************************************
 ** Initializes backends
 **
 ** Store3::InitBackend
 **
 ***********************************************************************************/
OP_STATUS Store3::InitBackend(OpString& error_message, const OpStringC& storage_path)
{
	RETURN_IF_ERROR(InitMailbase(error_message, storage_path));
	RETURN_IF_ERROR(InitMessageIdIndex(error_message, storage_path));
	RETURN_IF_ERROR(InitCaches());

	return OpStatus::OK;
}


/***********************************************************************************
 ** Initializes mailbase
 **
 ** Store3::InitMailbase
 **
 ***********************************************************************************/
OP_STATUS Store3::InitMailbase(OpString& error_message, const OpStringC& storage_path)
{
	OP_PROFILE_METHOD("Init Mail Base");
	if (!m_maildb || !m_maildb_backend)
		return OpStatus::ERR_NO_MEMORY;

	OpString filename;

	// Open storage backend
	if (storage_path.HasContent())
	{
		RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%c%s"), storage_path.CStr(), PATHSEPCHAR, MAILBASE_FILENAME));
		RETURN_IF_ERROR(m_mbox_store_manager.SetStorePath(storage_path));
	}
	else
	{
		RETURN_IF_ERROR(MailFiles::GetMailBaseFilename(filename));
		OpString mailfolder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, mailfolder));
		RETURN_IF_ERROR(m_mbox_store_manager.SetStorePath(mailfolder.CStr()));
	}

	OP_STATUS ret = m_maildb_backend->Open(filename.CStr(), m_readonly ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE, 8);

	if (OpStatus::IsError(ret))
	{
		OpString failure_message;
		if (ret == OpStatus::ERR_NO_ACCESS)
			OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_FILE_NO_ACCESS, failure_message));
		else
			OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_CORRUPTED_FILE, failure_message));
		OpStatus::Ignore(error_message.AppendFormat(failure_message.CStr(), filename.CStr()));
		return ret;
	}

	m_maildb->SetStorage(m_maildb_backend);

	// Initialize the fields - don't change this if you don't want to break compatibility
	RETURN_IF_ERROR(m_maildb->AddField("recvdate", 8));
	RETURN_IF_ERROR(m_maildb->AddField("sentdate", 8));
	RETURN_IF_ERROR(m_maildb->AddField("size"    , 4));
	RETURN_IF_ERROR(m_maildb->AddField("flags"   , 4));
	RETURN_IF_ERROR(m_maildb->AddField("label"   , 1));
	RETURN_IF_ERROR(m_maildb->AddField("acctid"  , 2));
	RETURN_IF_ERROR(m_maildb->AddField("parent"  , 4));
	RETURN_IF_ERROR(m_maildb->AddField("m2_id"   , 4));
	RETURN_IF_ERROR(m_maildb->AddField("mbx_type", 4));
	RETURN_IF_ERROR(m_maildb->AddField("mbx_data", 8));
	RETURN_IF_ERROR(m_maildb->AddField("reserv1" , 4));
	RETURN_IF_ERROR(m_maildb->AddField("reserv2" , 4));
	RETURN_IF_ERROR(m_maildb->AddField("from"    , 0));
	RETURN_IF_ERROR(m_maildb->AddField("to"      , 0));
	RETURN_IF_ERROR(m_maildb->AddField("subject" , 0));
	RETURN_IF_ERROR(m_maildb->AddField("msg_id"  , 0));
	RETURN_IF_ERROR(m_maildb->AddField("int_loc" , 0));

	// this is backwards compatible since user headers are always 0 until it's used
	// if it failed to read from the user headers, make sure to use the highest value
	if (!m_maildb_backend->ReadUserHeader(0, &m_next_m2_id, sizeof(message_gid_t)) || m_next_m2_id == 0)
		m_next_m2_id = 1;
	else
	{
		m_next_m2_id_ready = TRUE;
		m_duplicate_table.SetTotalMessageCount(m_next_m2_id);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Initializes message-id index
 **
 ** Store3::InitMessageIdIndex
 **
 ***********************************************************************************/
OP_STATUS Store3::InitMessageIdIndex(OpString& error_message, const OpStringC& storage_path)
{
	OP_PROFILE_METHOD("Init Message Id Index");
	if (!m_message_id_index)
		return OpStatus::ERR_NO_MEMORY;

	OpString filename;

	if (storage_path.IsEmpty())
	{
		// Construct path of btree
		RETURN_IF_ERROR(MailFiles::GetMessageIDFilename(filename));
	}
	else
	{
		RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cindexer%c%s"), storage_path.CStr(), PATHSEPCHAR, PATHSEPCHAR, MESSAGE_ID_FILENAME));
	}

	// Open BTree
	OP_STATUS ret = m_message_id_index->Open(filename.CStr(), m_readonly ? BlockStorage::OpenRead : BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE);
	if (OpStatus::IsError(ret))
	{
		OpString failure_message;
		if (ret == OpStatus::ERR_NO_ACCESS)
			OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_FILE_NO_ACCESS, failure_message));
		else
			OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_CORRUPTED_FILE, failure_message));
		OpStatus::Ignore(error_message.AppendFormat(failure_message.CStr(), filename.CStr()));
		return ret;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Initializes in-memory caches
 **
 ** Store3::InitCaches
 **
 ***********************************************************************************/
OP_STATUS Store3::InitCaches()
{
	if (m_progress)
		RETURN_IF_ERROR(m_progress->SetCurrentAction(ProgressInfo::LOADING_DATABASE, m_maildb_backend->GetFileSize()));

	int blockcount = 1000;
	if (!g_pcm2->GetIntegerPref(PrefsCollectionM2::LoadMailDatabasesAsynchronously))
		blockcount = m_maildb_backend->GetFileSize() / m_maildb_backend->GetBlockSize();

	AsyncStoreReader* reader = OP_NEW(AsyncStoreReader, (*this, 0, blockcount));
	return m_reader_queue.AddCommand(reader);
}

/***********************************************************************************
 ** Writes the highest m2 id to the user header
 **
 ** Store3::WriteNextM2ID
 **
 ***********************************************************************************/

OP_STATUS Store3::WriteNextM2ID()
{
	if (!m_maildb_backend->WriteUserHeader(0, &m_next_m2_id, sizeof(message_gid_t)))
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

/***********************************************************************************
 ** Reads the messge flags
 **
 ** Store3::ReadFlags
 **
 ***********************************************************************************/

UINT64 Store3::ReadFlags()
{
	UINT64 tmp_int;
	m_maildb->GetField(STORE_FLAGS2).GetValue(&tmp_int);
	UINT64 flags = tmp_int << 32;
	m_maildb->GetField(STORE_FLAGS).GetValue(&tmp_int);
	flags |= tmp_int;
	return flags;
}

/***********************************************************************************
 ** Writes the message flags
 **
 ** Store3::WriteFlags
 **
 ***********************************************************************************/

OP_STATUS Store3::WriteFlags(UINT64 flags)
{
	INT32 part_of_flags = flags;
	RETURN_IF_ERROR(m_maildb->GetField(STORE_FLAGS).SetValue(part_of_flags));
	part_of_flags = flags >> 32;
	RETURN_IF_ERROR(m_maildb->GetField(STORE_FLAGS2).SetValue(part_of_flags));
	return OpStatus::OK;
}

#endif //M2_SUPPORT
