/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HISTORY_DIRECT_HISTORY_H
#define HISTORY_DIRECT_HISTORY_H

#ifdef DIRECT_HISTORY_SUPPORT

#include "modules/util/adt/opvector.h"

#ifdef SYNC_TYPED_HISTORY
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_dataitem.h"
#endif // SYNC_TYPED_HISTORY

#include "modules/history/src/DelayedSave.h"
#include "modules/history/history_locks.h"

class OpFile;

class DirectHistory :
	public DelayedSave
#ifdef SYNC_TYPED_HISTORY
	, public OpSyncDataClient
#endif // SYNC_TYPED_HISTORY
{
	friend class HistoryModule;
	friend class OpTypedHistorySyncLock;
	friend class OpTypedHistorySaveLock;

public:

	// -----------------
	// Insertion type :
	// -----------------
	enum ItemType
	{
		TEXT_TYPE     = 0, ///< Just a string that the user typed
		SEARCH_TYPE   = 1, ///< A string that was a search i.e. "g opera"
		SELECTED_TYPE = 2, ///< A url that was selected in some context
		NICK_TYPE     = 3, ///< A nickname that was typed
		NUM_TYPES, //Do not insert constants after this one
		NO_TYPE
	};

	/**
	 *  Saves direct history to the opera.dir file
	 *
	 * @param force
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    OP_STATUS Save(BOOL force = FALSE) { return RequestSave(force); }

	/**
	 * Adds a new entry
	 *
	 * @param typed_string the string that should be added
	 * @param type the type of entry it was
	 * @param typed the time it was typed
	 * @return OpStatus::OK if typed_string item was added
	 */
	OP_STATUS Add(const OpStringC & typed_string, ItemType type, time_t typed);

	/**
	 * Removes an entry
	 *
	 * @param typed_string the string entry that should be removed
	 * @return TRUE if typed_string was removed
	 * @depricated Use the delete fuction instead
	 */
	DEPRECATED(BOOL Remove(const OpStringC & typed_string));
	BOOL Delete(const OpStringC & typed_string);

	/**
	 * Resets the iteration and returns the first string
	 *
	 * @return the first string stored
	 */
	uni_char * GetFirst();

	/**
	 * Gets the current item and will NOT move iteration to the following item
	 *
	 * @return the current string if there is one - else NULL
	 */
	uni_char * GetCurrent();

	/**
	 * Gets the next item and moves the iteration to the following item
	 *
	 * @return the next string if there is one - else NULL
	 */
	uni_char * GetNext();


	/**
	 * Delete all the strings stored
	 */
	void DeleteAllItems();

	/**
	 * Set the maximum number of strings stored
	 *
	 * @param m the new max
	 */
	void Size(short m);

	/**
	 * Set the most recently typed
	 *
	 * @param text the string typed
	 * @depricated this function does not belong here - this will be stored as a
	 *             matter of course where it should be stored
	 */
	DEPRECATED(void SetMostRecentTyped(const OpStringC& text));

	/**
	 * Get the most recently typed
	 *
	 * @returns the most recently typed
	 * @depricated this function does not belong here - if you want the last typed
	 *      	   string, then use the GetFirst() function
	 */
	DEPRECATED(const OpStringC& GetMostRecentTyped() const);

	// Implementing the OpSyncDataListener interface
#ifdef SYNC_TYPED_HISTORY
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);
#endif // SYNC_TYPED_HISTORY

	struct TypedHistoryItem
	{
		OpString m_string;   ///< The string typed/selected - this field is unique
		time_t m_last_typed; ///< The last time the string was typed/selected
		ItemType m_type;     ///< The type of entry
	};

protected:

	// -----------------
	// Private functions :
	// -----------------
	DirectHistory(short num);

	// -----------------------------------
	// Implementing the DelayedSave interface
	virtual PrefsCollectionFiles::filepref GetFilePref() { return PrefsCollectionFiles::DirectHistoryFile; }
    virtual OP_STATUS Write(OpFile* output_file);
	// -----------------------------------

	OP_STATUS Read();
	OP_STATUS Read(OpFile& input_file);
	~DirectHistory();
	static DirectHistory * CreateL(short num);
	BOOL DeleteItem(UINT32 index);
	void SetHistoryFlag(TypedHistoryItem* item);
	void ClearHistoryFlag(TypedHistoryItem* item);
	OP_STATUS InsertSorted(TypedHistoryItem* item);
	void AdjustSize();
	TypedHistoryItem* RemoveExistingItem(const OpStringC& typed_string, time_t typed, BOOL& newer);
	OP_STATUS Add(const OpStringC& text, const OpStringC& type_string, const OpStringC& last_typed_string);
	const uni_char* GetTypeString(DirectHistory::ItemType type);
	DirectHistory::ItemType GetTypeFromString(const OpStringC& type_string);
	BOOL After(TypedHistoryItem* item_1, TypedHistoryItem* item_2);
	BOOL Delete(const OpStringC & typed_string, BOOL save);
	virtual void DeleteAllItems(BOOL save);
	TypedHistoryItem* Find(const OpStringC& text, int& index);

#ifdef SYNC_TYPED_HISTORY
	virtual OP_STATUS SyncItem(TypedHistoryItem* item, OpSyncDataItem::DataItemStatus sync_status);
	virtual OP_STATUS TypedHistoryItem_to_OpSyncItem(TypedHistoryItem* item, OpSyncItem*& sync_item, OpSyncDataItem::DataItemStatus sync_status);
	virtual OP_STATUS OpSyncItem_to_TypedHistoryItem(OpSyncItem* sync_item, TypedHistoryItem*& item);
	OP_STATUS ProcessSyncItem(OpSyncItem* item);
	OP_STATUS ProcessAddedSyncItem(OpSyncItem* item);
	OP_STATUS ProcessDeletedSyncItem(OpSyncItem* item);
	OP_STATUS BackupFile(OpFile& file);
#endif // SYNC_TYPED_HISTORY

	// -----------------
	// Private fields :
	// -----------------
	short m_max;                          ///< The maximum number allowed in typed history
	unsigned int m_current_index;         ///< The current index in use for GetFirst/GetNext
	OpVector<TypedHistoryItem> m_items;   ///< The actual items present
	OpString m_most_recent_typed;         ///< DEPRICATED the last typed string
	BOOL m_save_blocked;                  ///< Indicates whether saves are currently blocked

#ifdef SYNC_TYPED_HISTORY
	BOOL m_sync_blocked;                  ///< Internal flag to set when you should not emit sync messages
	BOOL m_sync_history_enabled;          ///< Internal flag to set when history sync'ing is enabled 
#endif // SYNC_TYPED_HISTORY
};
#endif // DIRECT_HISTORY_SUPPORT
#endif // HISTORY_DIRECT_HISTORY_H
