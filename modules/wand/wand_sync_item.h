/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WAND_SYNC_ITEM_H_
#define WAND_SYNC_ITEM_H_
#ifdef SYNC_HAVE_PASSWORD_MANAGER
#include "modules/sync/sync_dataitem.h"

/**
 * Common code of password items.
 */
class WandSyncItem
{
public:
	WandSyncItem();
	virtual ~WandSyncItem(){};

	static OP_STATUS CreateFromOpSyncItem(OpSyncItem *item, WandSyncItem *&wand_sync_item);

	enum SyncStatus
	{
		// Before syncing
		SYNC_ADD = 0,		// the item has been added, but not yet sent to sync queue.
		SYNC_MODIFY,	   	// the item has been modified, but not sent to sync queue.

		// After syncing
		SYNCED,				// The item has been sent to sync queue.
		DELETED				// The item has been deleted and the delete event is sent to sync queue.
						 	// Short term status event, the item should be deleted shortly.
							// Should never end up on disk.
	};

	OP_STATUS 			OpenSyncData(OpFile &file, long version);
	OP_STATUS 			SaveSyncData(OpFile &file);

	/**
	 * Assign random sync ID
	 *
	 * Will do nothing if sync id is alread set and force_reassign is false
	 *
	 * @param force_reassign Give new ID if it already have an assigned ID.
	 * @return OK or ERR_NO_MEMORY
	 *
	 */
	OP_STATUS 			AssignSyncId(BOOL force_reassign = FALSE);

#ifdef SELFTEST
	OP_STATUS			SelftestAssignSyncId(const uni_char *id) { return sync_id.Set(id); }
#endif // SELFTEST

	const uni_char*		GetSyncId(){ return sync_id.CStr(); }

	OP_STATUS			SetModifiedDate(const uni_char *date_string = NULL);
	const uni_char*		GetModifiedDate(){ return sync_data_modified_date.CStr(); }
	double				GetModifiedDateMilliseconds();

	/**
	 * Create item from sync item
	 *
	 *  If decryption fails the incoming item
	 * 	was created with old key. This function deletes the item on the server
	 *	and send the local item up to the server.
	 *
	 * @param item The OpSyncItem containing the data
	 * 			   used to create this WandSyncItem.
	 * @return OK, ERR if missing data, ERR_NO_ACCESS for decryption problems
	 * 		   or ERR_NO_MEMORY for OOM
	 */
	OP_STATUS 			InitFromSyncItem(OpSyncItem *item);

	/**
	 * Get and set the local sync status
	 *
	 * Used to tell if an item has been sync'ed. The statuses
	 * will be regularly checked, and if an item has not
	 * been sync'ed for a reason this will be done.
	 */
	SyncStatus			GetLocalSyncStatus() { return wand_item_local_sync_status; }
	void				SetLocalSyncStatus(SyncStatus status);
	OpSyncDataItem::DataItemStatus	GetDataItemStatus();

	/**
	 * Resolve conflicts that can happen if the the password received
	 * has equal username to a password already stored locally.
	 *
	 * If this item is to be deleted, a delete event for this item is sent to server
	 * If new item is to be deleted, the delete event for the new item is sent to server.
	 *
	 * If the items have same time and same ID, no events are sent and caller
	 * should delete local item quietly.
	 *
	 * @param matching_new_item new item that have matching username
	 * @return TRUE if this item should be deleted, FALSE if new item should be deleted.
	 */
	BOOL 				ResolveConflictReplaceThisItem(WandSyncItem* matching_new_item);

	/**
	 * Sync the item according to local sync status.
	 *
	 * Will only handle SYNC_ADD or SYNC_MODIFY
	 *
	 * @param force_add_status 		If true, the local status will be ignored
	 * 								and the item will be synced
	 * 								with DATAITEM_ACTION_ADDED status.
	 * @param dirty_sync			As specified by OpSyncDataClient::SyncDataFlush()
	 * @param override_sync_block	Send event, even if sync is blocked.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS			SyncItem(BOOL force_add_status = FALSE, BOOL dirty_sync = FALSE, BOOL override_sync_block = FALSE);

	/**
	 * Send delete even for this item to the server.
	 *
	 * The item should be deleted locally shortly after this call.
	 *
	 * @param override_sync_block	send event, even if sync is blocked.
	 */
	OP_STATUS 			SyncDeleteItem(BOOL override_sync_block = FALSE);

	/**
	 * Constructs a OpSyncItem from this WandSyncItem
	 *
	 * @param sync_item [out] The constucted OpSyncItem
	 * @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS	ConstructSyncItem(OpSyncItem *&sync_item);

	/** Check if the password should not be sync'ed for some reason
	 *
	 * @return TRUE if this item should not be synced.
	 **/
	virtual BOOL		PreventSync() = 0;

	virtual OpSyncDataItem::DataItemType GetAuthType() = 0;

protected:
	OP_STATUS			CopyTo(WandSyncItem *copy_item);


	/** Init the type specific elements from the sync item */
	virtual OP_STATUS 	InitAuthTypeSpecificElements(OpSyncItem *item, BOOL modify = FALSE) = 0;
	virtual OP_STATUS	ConstructSyncItemAuthTypeSpecificElements(OpSyncItem *sync_item) = 0;

	OpString sync_id;
	OpString sync_data_modified_date;

	// This sync status is used locally to tell
	// if the item has been sent up to the server.
	// The status is saved locally only.
	SyncStatus wand_item_local_sync_status;
};

#endif // SYNC_HAVE_PASSWORD_MANAGER

#endif /* WAND_SYNC_ITEM_H_ */
