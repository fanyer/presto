/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DIRECT_HISTORY_SUPPORT

#ifdef HISTORY_SUPPORT
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#endif // HISTORY_SUPPORT

#include "modules/history/direct_history.h"
#include "modules/url/url2.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/str.h"
#include "modules/util/opfile/opfile.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/util/adt/bytebuffer.h"

#ifdef SYNC_TYPED_HISTORY
#include "modules/sync/sync_util.h"
#include "modules/stdlib/util/opdate.h"
#endif // SYNC_TYPED_HISTORY

template<typename T> class Resetter
{
public:
	Resetter(T& reset, T reset_to) : m_reset(reset), m_reset_to(reset_to) {}
	~Resetter() { m_reset = m_reset_to; }
private:
	T& m_reset;
	T  m_reset_to;
};

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory::DirectHistory(short m) :
	m_max(m > 0 ? m : 0)
	, m_save_blocked(FALSE)
#ifdef SYNC_TYPED_HISTORY
	, m_sync_blocked(FALSE)
	, m_sync_history_enabled(FALSE)
#endif // SYNC_TYPED_HISTORY
{

}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory::~DirectHistory()
{
	Save(TRUE);

	m_items.DeleteAll();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory * DirectHistory::CreateL(short num)
{
	DirectHistory* dh = OP_NEW(DirectHistory, (num));

	if (!dh)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	return dh;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::DeleteAllItems()
{
	DeleteAllItems(TRUE);
}

void DirectHistory::DeleteAllItems(BOOL save)
{
	m_most_recent_typed.Empty();

	while(m_items.GetCount())
	{
		DeleteItem(m_items.GetCount()-1);
	}

	if(save)
		Save(TRUE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::Add(const OpStringC & typed_string, ItemType type, time_t typed)
{
	// -----------------
	// Sanity checking
	// -----------------

	if(m_max <= 0)
		return OpStatus::OK;

	if(typed_string.IsEmpty())
		return OpStatus::ERR;

	// -----------------
	// Initial assuption about sync status
	// -----------------

#ifdef SYNC_TYPED_HISTORY
	OpSyncDataItem::DataItemStatus sync_status = OpSyncDataItem::DATAITEM_ACTION_ADDED;
#endif // SYNC_TYPED_HISTORY

	// -----------------
	// Remove any existing item that is older then this, if it exists this is a modification
	// -----------------

	BOOL newer = TRUE;
	TypedHistoryItem* found_item = RemoveExistingItem(typed_string, typed, newer);

	if(!newer)
		return OpStatus::OK;

#ifdef SYNC_TYPED_HISTORY
	if(found_item)
		sync_status = OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
#endif // SYNC_TYPED_HISTORY

	// -----------------
	// Create new item or reuse existing item
	// -----------------

	OpAutoPtr<TypedHistoryItem> item(found_item ? found_item : OP_NEW(TypedHistoryItem, ()));
	
	if(!item.get())
		return OpStatus::ERR_NO_MEMORY;
	
	// -----------------
	// Update the item
	// -----------------

	RETURN_IF_ERROR(item->m_string.Set(typed_string));
	
	item->m_type = type;
	item->m_last_typed = typed;
	
	// -----------------
	// Insert, sync, update state and save
	// -----------------

	RETURN_IF_ERROR(InsertSorted(item.get()));
	
#ifdef SYNC_TYPED_HISTORY
	RETURN_IF_ERROR(SyncItem(item.get(), sync_status));
#endif // SYNC_TYPED_HISTORY
	
	// Notify history
	SetHistoryFlag(item.get());
	
	// Item was successfully inserted so release it
	item.release();
	
	// Remove last if vector is full
	AdjustSize();

	// Don't save from add if save lock is locked
	if(OpTypedHistorySaveLock::IsLocked())
		return OpStatus::OK;

	return Save();
}

/***********************************************************************************
 ** The list is sorted with the most recent on top, the most recent will have the 
 ** highest number in m_last_typed, meaning that the list will have the lowest
 ** m_last_typed at the bottom.
 **
 **
 ***********************************************************************************/
BOOL DirectHistory::After(TypedHistoryItem* item_1, TypedHistoryItem* item_2)
{
	return item_2->m_last_typed > item_1->m_last_typed;
}

OP_STATUS DirectHistory::InsertSorted(TypedHistoryItem* item)
{
	int start = 0;
	int end   = m_items.GetCount();

	int n2;

	while (end > start)
	{
		n2 = (end - start) / 2;

		if(After(item, m_items.Get(start + n2)))
			start = start + n2 + 1;
		else
			end = start + n2;
	}

	return m_items.Insert(start, item);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::SetHistoryFlag(TypedHistoryItem* item)
{
#ifdef HISTORY_SUPPORT
	if(!g_globalHistory)
		return;

	HistoryPage * history_item = 0;
	RETURN_VOID_IF_ERROR(g_globalHistory->GetItem(item->m_string, history_item));

	if(!history_item->IsInHistory())
		return;

	switch(item->m_type)
	{
	case DirectHistory::TEXT_TYPE:
		history_item->SetIsTyped();
		break;
	case DirectHistory::SELECTED_TYPE:
		history_item->SetIsSelected();
		break;
	}
#endif // HISTORY_SUPPORT
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::ClearHistoryFlag(TypedHistoryItem* item)
{
#ifdef HISTORY_SUPPORT
	if(!g_globalHistory)
		return;

	HistoryPage * history_item = 0;
	RETURN_VOID_IF_ERROR(g_globalHistory->GetItem(item->m_string, history_item));

	if(!history_item->IsInHistory())
		return;

	switch(item->m_type)
	{
	case DirectHistory::TEXT_TYPE:
		history_item->ClearIsTyped();
		break;
	case DirectHistory::SELECTED_TYPE:
		history_item->ClearIsSelected();
		break;
	}
#endif // HISTORY_SUPPORT
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DirectHistory::Remove(const OpStringC & typed_string)
{
	return Delete(typed_string);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DirectHistory::Delete(const OpStringC & typed_string)
{ 
	return Delete(typed_string, TRUE); 
}

BOOL DirectHistory::Delete(const OpStringC & typed_string, BOOL save)
{
	int index = -1;
	TypedHistoryItem* item = Find(typed_string, index);

	if(item)
		return DeleteItem(index);

	if(save)
		Save();

    return FALSE;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory::TypedHistoryItem* DirectHistory::RemoveExistingItem(const OpStringC& typed_string,
																   time_t typed,
																   BOOL& newer)
{
	int index = -1;
	TypedHistoryItem* item = Find(typed_string, index);

	if(!item)
		return 0;

	TypedHistoryItem* found_item = 0;

	// If the typed is newer than remove the item,
	// it needs to be moved. If not that igore the remove
	// request and notify the caller that this new item
	// is older than the one we already have.
	newer = (item->m_last_typed < typed);
	
	if(newer) // Duplicate needs to be removed
	{
		m_items.Remove(index);
		found_item = item;
		
		// Notify history
		ClearHistoryFlag(item);
	}

	return found_item;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory::TypedHistoryItem* DirectHistory::Find(const OpStringC& text, int& index)
{
    for (unsigned int i = 0; i < m_items.GetCount(); i++)
    {
		TypedHistoryItem* item = m_items.Get(i);

        if (text == item->m_string)
        {
			index = i;
            return item;
        }
    }

	return NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DirectHistory::DeleteItem(UINT32 index)
{
	TypedHistoryItem* item = m_items.Get(index);

	if(!item)
		return FALSE;

	// Notify history
	ClearHistoryFlag(item);

#ifdef SYNC_TYPED_HISTORY
	// TODO : Is it right to ignore errors here ?
	OP_STATUS status = SyncItem(item, OpSyncDataItem::DATAITEM_ACTION_DELETED);
	OP_ASSERT(OpStatus::IsSuccess(status));
	OpStatus::Ignore(status);
#endif // SYNC_TYPED_HISTORY

	m_items.Delete(index);

	return TRUE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
uni_char * DirectHistory::GetFirst()
{
	m_current_index = 0;

	if(m_current_index < m_items.GetCount())
		return m_items.Get(m_current_index)->m_string.CStr();

	return NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
uni_char * DirectHistory::GetCurrent()
{
	if(m_current_index < m_items.GetCount())
		return m_items.Get(m_current_index)->m_string.CStr();

	return NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
uni_char * DirectHistory::GetNext()
{
	m_current_index++;

	if(m_current_index < m_items.GetCount())
		return m_items.Get(m_current_index)->m_string.CStr();

	return NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::Write(OpFile* output_file)
{
	// -----------------
	// Make the xml
	// -----------------

	XMLFragment fragment;

	RETURN_IF_ERROR(fragment.OpenElement(UNI_L("typed_history")));

	for (unsigned int i = 0; i < m_items.GetCount(); ++i)
	{
		RETURN_IF_ERROR(fragment.OpenElement(UNI_L("typed_history_item")));

		OpString date;

#ifdef SYNC_TYPED_HISTORY
		// Get the last typed as a RFC3339 date string
		RETURN_IF_ERROR(SyncUtil::CreateRFC3339Date(m_items.Get(i)->m_last_typed, date));
#else
		RETURN_IF_ERROR(date.AppendFormat(UNI_L("%ld"), m_items.Get(i)->m_last_typed));
#endif // SYNC_TYPED_HISTORY

		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("content"), m_items.Get(i)->m_string.CStr()));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("type"), GetTypeString(m_items.Get(i)->m_type)));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("last_typed"), date.CStr()));

		fragment.CloseElement();
	}

	fragment.CloseElement();

	// -----------------
	// Get the xml into a ByteBuffer
	// -----------------

	ByteBuffer buffer;
	RETURN_IF_ERROR(fragment.GetEncodedXML(buffer, TRUE, "utf-8", TRUE));

	// -----------------
	// Write the ByteBuffer to the file, one chunk at a time
	// -----------------

	for(UINT32 chunk = 0; chunk < buffer.GetChunkCount(); chunk++)
	{
		unsigned int bytes = 0;
		char* chunk_ptr = buffer.GetChunk(chunk, &bytes);

		if(chunk_ptr)
			RETURN_IF_ERROR(output_file->Write(chunk_ptr, bytes));
	}


	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::Read()
{
	// Will block saving from adds resulting from this read
	OpTypedHistorySaveLock lock;

	OpFile file;
	OP_STATUS status;

	TRAP(status, g_pcfiles->GetFileL(GetFilePref(), file));
	
	RETURN_IF_ERROR(status);
	
	if(!file.GetFullPath())
		return OpStatus::ERR;
	
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	
	status = Read(file);
	
	file.Close();
	
	return status;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::Read(OpFile& input_file)
{
#ifdef SYNC_TYPED_HISTORY
	OpTypedHistorySyncLock lock;
#endif // SYNC_TYPED_HISTORY
	
	// Parse XML file
	XMLFragment fragment;
	RETURN_IF_ERROR(fragment.Parse(&input_file));

	OpString item_text;
	OpString item_type;
	OpString item_last_typed;

	if(!fragment.EnterElement(UNI_L("typed_history")))
		return OpStatus::ERR;
	
	while(fragment.EnterElement(UNI_L("typed_history_item")))
	{
		RETURN_IF_ERROR(item_text.Set(fragment.GetAttribute(UNI_L("content"))));
		RETURN_IF_ERROR(item_type.Set(fragment.GetAttribute(UNI_L("type"))));
		RETURN_IF_ERROR(item_last_typed.Set(fragment.GetAttribute(UNI_L("last_typed"))));
		
		RETURN_IF_ERROR(Add(item_text, item_type, item_last_typed));
		
		fragment.LeaveElement();
	}
		  
	fragment.LeaveElement();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::Size(short m)
{
	m_max = (m > 0) ? m : 0;
	AdjustSize();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::AdjustSize()
{
	while(m_items.GetCount() > (unsigned int) m_max)
	{
		DeleteItem(m_items.GetCount()-1);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DirectHistory::SetMostRecentTyped(const OpStringC &text)
{
	m_most_recent_typed.Set(text.CStr());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC & DirectHistory::GetMostRecentTyped() const
{
	return m_most_recent_typed;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char* DirectHistory::GetTypeString(DirectHistory::ItemType type)
{
	switch(type)
	{
	case TEXT_TYPE     : return UNI_L("text");
	case SEARCH_TYPE   : return UNI_L("search");
	case SELECTED_TYPE : return UNI_L("selected");
	case NICK_TYPE     : return UNI_L("nickname");
	}

	return 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
DirectHistory::ItemType DirectHistory::GetTypeFromString(const OpStringC& type_string)
{
	if(type_string.Compare("text") == 0)
		return TEXT_TYPE;

	if(type_string.Compare("search") == 0)
		return SEARCH_TYPE;

	if(type_string.Compare("selected") == 0)
		return SELECTED_TYPE;

	if(type_string.Compare("nickname") == 0)
		return NICK_TYPE;

	return NO_TYPE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::Add(const OpStringC& text,
							 const OpStringC& type_string,
							 const OpStringC& last_typed_string)
{
	ItemType type = GetTypeFromString(type_string);
	if(type == NO_TYPE)
		return OpStatus::ERR;

	time_t last_typed;

#ifdef SYNC_TYPED_HISTORY
	RETURN_IF_ERROR(SyncUtil::ParseRFC3339Date(last_typed, last_typed_string));
#else
	last_typed = uni_atoi(last_typed_string.CStr());
#endif // SYNC_TYPED_HISTORY

	return Add(text, type, last_typed);
}

#ifdef SYNC_TYPED_HISTORY

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::TypedHistoryItem_to_OpSyncItem(TypedHistoryItem* item,
														OpSyncItem*& sync_item,
														OpSyncDataItem::DataItemStatus sync_status)
{
	// Get the type as a string:
	const uni_char* item_type = GetTypeString(item->m_type);
	if(!item_type)
		return OpStatus::ERR;

	// Get the last typed as a RFC3339 date string
	OpString date;
	RETURN_IF_ERROR(SyncUtil::CreateRFC3339Date(item->m_last_typed, date));

	RETURN_IF_ERROR(g_sync_coordinator->GetSyncItem(&sync_item,
													OpSyncDataItem::DATAITEM_TYPED_HISTORY,
													OpSyncItem::SYNC_KEY_CONTENT,
													item->m_string.CStr()));

	// Set the action / status
	sync_item->SetStatus(sync_status);

	// Set the other attributes:
	SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TYPE, item_type), sync_item);
	SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_LAST_TYPED, date), sync_item);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::OpSyncItem_to_TypedHistoryItem(OpSyncItem* sync_item, TypedHistoryItem*& item)
{
	RETURN_IF_ERROR(ProcessSyncItem(sync_item));

	OpString item_string;
	RETURN_IF_ERROR(sync_item->GetData(OpSyncItem::SYNC_KEY_CONTENT, item_string));

	int index = -1;
	item = Find(item_string, index);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::SyncItem(TypedHistoryItem* item, 
								  OpSyncDataItem::DataItemStatus sync_status)
{
	// History sync'ing is currently not enabled
	if(!m_sync_history_enabled)
		return OpStatus::OK;

	// We are blocking sync'ing now
	if(OpTypedHistorySyncLock::IsLocked())
		return OpStatus::OK;

	if(!item)
		return OpStatus::ERR;

	// Create the item with the string as primary key:
	OpSyncItem *sync_item = NULL;
	RETURN_IF_ERROR(TypedHistoryItem_to_OpSyncItem(item, sync_item, sync_status));

	// Commit the item:
	SYNC_RETURN_IF_ERROR(sync_item->CommitItem(), sync_item);

	g_sync_coordinator->ReleaseSyncItem(sync_item);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error)
{
	// We should never get an empty list
	if(!new_items || !new_items->First())
	{
		OP_ASSERT(FALSE);
		return OpStatus::OK;
	}

	// Block calls to sync changes coming from the server
	OpTypedHistorySyncLock lock;

	for (OpSyncItemIterator current(new_items->First()); *current; ++current)
	{
		if(current->GetType() != OpSyncDataItem::DATAITEM_TYPED_HISTORY)
			continue;

		RETURN_IF_ERROR(ProcessSyncItem(*current));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::ProcessSyncItem(OpSyncItem* item)
{
	switch(item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
		return ProcessAddedSyncItem(item);
	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
		return ProcessDeletedSyncItem(item);
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::ProcessAddedSyncItem(OpSyncItem* item)
{
	OpString item_string;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_CONTENT, item_string));

	OpString item_type;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_TYPE, item_type));
	if (item_type.IsEmpty())
		return OpStatus::OK;

	OpString item_last_typed;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_LAST_TYPED, item_last_typed));
	if (item_last_typed.IsEmpty())
	{
		time_t t = static_cast<time_t>(OpDate::GetCurrentUTCTime()/1000.0);
		RETURN_IF_ERROR(SyncUtil::CreateRFC3339Date(t, item_last_typed));
	}

	return Add(item_string, item_type, item_last_typed);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::ProcessDeletedSyncItem(OpSyncItem* item)
{
	OpString item_string;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_CONTENT, item_string));

	Delete(item_string);
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	if(type != OpSyncDataItem::DATAITEM_TYPED_HISTORY)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}

	for(unsigned int i = 0; i < m_items.GetCount(); i++)
	{
		RETURN_IF_ERROR(SyncItem(m_items.Get(i), OpSyncDataItem::DATAITEM_ACTION_ADDED));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
{
	if(type != OpSyncDataItem::DATAITEM_TYPED_HISTORY)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}

	m_sync_history_enabled = has_support;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	if(type != OpSyncDataItem::DATAITEM_TYPED_HISTORY)
	{
		OP_ASSERT(FALSE); // We should not get any other notifications
		return OpStatus::OK;
	}

	// Force a save
	RETURN_IF_ERROR(Save(TRUE));

	// Get the typed history file
	OpFile file;
	OP_STATUS status;
	TRAP(status, g_pcfiles->GetFileL(GetFilePref(), file));

	RETURN_IF_ERROR(status);
	RETURN_IF_ERROR(BackupFile(file));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DirectHistory::BackupFile(OpFile& file)
{
	// Check if the file to backup actually exists
	BOOL exists = FALSE;
	OP_STATUS status = file.Exists(exists);
	if (OpStatus::IsError(status) || !exists)
		return OpStatus::ERR;

	// Create the backupfiles filename
	OpString backupFilename;
	RETURN_IF_ERROR(backupFilename.Set(file.GetFullPath()));
	RETURN_IF_ERROR(backupFilename.Append(UNI_L(".pre_sync")));

	exists = FALSE;
	OpFile backupFile;

	RETURN_IF_ERROR(backupFile.Construct( backupFilename.CStr()));
	RETURN_IF_ERROR(backupFile.Exists( exists ));

	if(exists)
		return OpStatus::OK; // File already exists

	return backupFile.CopyContents(&file, TRUE );
}


#endif // SYNC_TYPED_HISTORY

#endif // DIRECT_HISTORY_SUPPORT
