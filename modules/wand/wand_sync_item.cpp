/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SYNC_HAVE_PASSWORD_MANAGER
#include "modules/wand/wand_sync_item.h"
#include "modules/wand/wandmanager.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/wand/wand_internal.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/sync/sync_util.h"

WandSyncItem::WandSyncItem()
	: wand_item_local_sync_status(SYNC_ADD)
{
}

/* static */ OP_STATUS WandSyncItem::CreateFromOpSyncItem(OpSyncItem *item, WandSyncItem *&wand_sync_item)
{
	wand_sync_item = NULL;
	OpAutoPtr<WandSyncItem> new_wand_sync_item;
	if (item->GetStatus() != OpSyncDataItem::DATAITEM_ACTION_DELETED)
	{
		if (item->GetType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH)
			new_wand_sync_item.reset(OP_NEW(WandPage, ()));
		else if (item->GetType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH)
			new_wand_sync_item.reset(OP_NEW(WandLogin, ()));
		else
			return OpStatus::ERR;
	}

	RETURN_IF_ERROR(new_wand_sync_item->InitFromSyncItem(item));
	wand_sync_item = new_wand_sync_item.release();
	return OpStatus::OK;
}

OP_STATUS WandSyncItem::OpenSyncData(OpFile &file, long version)
{
	if (version >=6)
	{
		long local_sync_status;
		RETURN_IF_ERROR(file.ReadBinLong(local_sync_status));

		switch (local_sync_status)
		{
			case SYNC_ADD:
			case SYNC_MODIFY:
			case SYNCED:
				wand_item_local_sync_status = static_cast<WandSyncItem::SyncStatus>(local_sync_status);
				break;
			default:
				wand_item_local_sync_status = SYNC_ADD;
				break;
		}
		RETURN_IF_ERROR(ReadWandString(file, sync_id, version));
		RETURN_IF_ERROR(ReadWandString(file, sync_data_modified_date, version));
	}
	else
	{
		wand_item_local_sync_status = SYNC_ADD;
		if (g_libcrypto_random_generator)
			RETURN_IF_ERROR(g_libcrypto_random_generator->GetRandomHexString(sync_id, 32));
		RETURN_IF_ERROR(SyncUtil::CreateRFC3339Date(0, sync_data_modified_date));
	}
	return OpStatus::OK;
}

OP_STATUS WandSyncItem::SaveSyncData(OpFile &file)
{
	RETURN_IF_ERROR(file.WriteBinLong(wand_item_local_sync_status));
	RETURN_IF_ERROR(WriteWandString(file, sync_id));
	if (sync_data_modified_date.IsEmpty())
		RETURN_IF_ERROR(SetModifiedDate());
	return WriteWandString(file, sync_data_modified_date);
}

OP_STATUS WandSyncItem::AssignSyncId(BOOL force_reassign)
{
	OP_ASSERT(g_libcrypto_random_generator);
	if (!g_libcrypto_random_generator)
		return OpStatus::OK;

	if (sync_id.IsEmpty() || force_reassign)
		return g_libcrypto_random_generator->GetRandomHexString(sync_id, 32);
	else
		return OpStatus::OK;
}

OP_STATUS WandSyncItem::SetModifiedDate(const uni_char *date_string)
{
	if (!date_string)
	{
		time_t t = static_cast<time_t>(OpDate::GetCurrentUTCTime()/1000.0);
		return SyncUtil::CreateRFC3339Date(t, sync_data_modified_date);
	}
	else
		return sync_data_modified_date.Set(date_string);
}

double WandSyncItem::GetModifiedDateMilliseconds()
{
	double milliseconds = 0;
	if (sync_data_modified_date.HasContent())
		milliseconds = OpDate::ParseRFC3339Date(sync_data_modified_date.CStr());
		if (op_isnan(milliseconds))
			return 0;

	return milliseconds;
}

OP_STATUS WandSyncItem::InitFromSyncItem(OpSyncItem *item)
{
	OP_ASSERT(g_wand_manager->HasSyncPasswordEncryptionKey() && "We should have received the encryption-key by now!");

	if (!item)
		return OpStatus::ERR;

	OP_ASSERT(item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED ||
			  item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_MODIFIED);

	OP_ASSERT(item->GetType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH || item->GetType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH );

	SetLocalSyncStatus(WandSyncItem::SYNCED);

	OpString item_sync_id;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, item_sync_id));
	OP_ASSERT(item_sync_id.HasContent());

	RETURN_IF_ERROR(sync_id.Set(item_sync_id.CStr()));

	OpString item_sync_data_modified_date;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_MODIFIED, item_sync_data_modified_date));
	if (item_sync_data_modified_date.IsEmpty() ||
		op_isnan(OpDate::ParseRFC3339Date(item_sync_data_modified_date.CStr())))
		return OpStatus::ERR;

	RETURN_IF_ERROR(sync_data_modified_date.TakeOver(item_sync_data_modified_date));

	return InitAuthTypeSpecificElements(item);
}

void WandSyncItem::SetLocalSyncStatus(SyncStatus status)
{
	// Cannot modify something on server that hasn't been sent yet.
	if (wand_item_local_sync_status == SYNC_ADD && status == SYNC_MODIFY)
		return;

	wand_item_local_sync_status = status;
}

OpSyncDataItem::DataItemStatus WandSyncItem::GetDataItemStatus()
{
	switch (wand_item_local_sync_status)
	{
		// Only SYNC_ADD and SYNC_MODIFY correspond to OpSyncDataItem::DataItemStatus
		case SYNC_ADD:
			return OpSyncDataItem::DATAITEM_ACTION_ADDED;
		case SYNC_MODIFY:
			return OpSyncDataItem::DATAITEM_ACTION_MODIFIED;
		default:
			return OpSyncDataItem::DATAITEM_ACTION_NONE;
	}
}

BOOL WandSyncItem::ResolveConflictReplaceThisItem(WandSyncItem* matching_new_item)
{
	double compare_items = 0;
	double compare_id = sync_id.Compare(matching_new_item->GetSyncId());

	double this_time = GetModifiedDateMilliseconds();
	double new_item_time = matching_new_item->GetModifiedDateMilliseconds();

	// We compare sync id's if time is not set on either items,
	// Or if they modify dates are equal.
	if ((this_time == 0 && new_item_time == 0) || this_time == new_item_time)
		compare_items = compare_id;
	else // We compare time otherwise.
		compare_items = this_time - new_item_time;

	BOOL send_back_delete_events = TRUE;
	// If the items have equal IDs we do not
	// send delete events back to link server,
	// since it's the same item
	if (compare_id == 0)
		send_back_delete_events = FALSE;

	if (compare_items > 0)
	{
		 // Local item is "bigger" than new item
		if (send_back_delete_events)
			OpStatus::Ignore(matching_new_item->SyncDeleteItem(TRUE));
		return FALSE;
	}
	else // compare_items <= 0
	{
		// Local item is "smaller" or equal to new item.
		// We let server win and delete local item.
		if (send_back_delete_events)
			OpStatus::Ignore(SyncDeleteItem(TRUE));
		return TRUE;
	}
}

OP_STATUS WandSyncItem::SyncItem(BOOL force_add_status, BOOL dirty_sync, BOOL override_sync_block)
{
	if (
			!g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPasswordManager) ||
			(g_wand_manager->GetBlockOutgoingSyncMessages() && !override_sync_block) ||
			// We allow deleted events without encryption key, since delete does not need encryption.
			// Also, we cannot send delete event later
			!g_wand_manager->HasSyncPasswordEncryptionKey() ||
			(GetLocalSyncStatus() == WandSyncItem::SYNCED && !force_add_status) ||
			PreventSync()
		)
		return OpStatus::OK;

	OpSyncItem *sync_item = NULL;
	if (force_add_status)
	{
		SetLocalSyncStatus(SYNC_ADD);
	}

	RETURN_IF_ERROR(ConstructSyncItem(sync_item));
	OpAutoPtr<OpSyncItem> sync_item_deletor(sync_item);

	RETURN_IF_ERROR(sync_item->CommitItem(dirty_sync, FALSE));

	SetLocalSyncStatus(WandSyncItem::SYNCED);
	return OpStatus::OK;
}

OP_STATUS WandSyncItem::SyncDeleteItem(BOOL override_sync_block)
{
	if (!g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncPasswordManager))
		return OpStatus::OK;

	if (PreventSync())
		return OpStatus::OK;

	if (g_wand_manager->GetBlockOutgoingSyncMessages() && !override_sync_block)
		return OpStatus::OK;

	WandSyncItem::SyncStatus sync_status = GetLocalSyncStatus();

	 // Cannot delete item on server that hasn't been sent to serve yet,
	 // and we cannot delete twice.
	if (sync_status == WandSyncItem::SYNC_ADD || sync_status == WandSyncItem::DELETED )
		return OpStatus::OK;

	OpSyncItem *sync_item;
	RETURN_IF_ERROR(g_sync_coordinator->GetSyncItem(&sync_item,
	                                                GetAuthType(),
													OpSyncItem::SYNC_KEY_ID,
													GetSyncId()));
	OpAutoPtr<OpSyncItem> sync_item_deletor(sync_item);
	RETURN_IF_ERROR(sync_item->SetStatus(OpSyncDataItem::DATAITEM_ACTION_DELETED));
	RETURN_IF_ERROR(sync_item->CommitItem());

	SetLocalSyncStatus(WandSyncItem::DELETED);
	return OpStatus::OK;
}

OP_STATUS WandSyncItem::ConstructSyncItem(OpSyncItem *&sync_item)
{
	sync_item = NULL;

	OP_ASSERT(GetDataItemStatus() != OpSyncDataItem::DATAITEM_ACTION_NONE);
	OP_ASSERT(GetDataItemStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED || sync_id.HasContent());

	RETURN_IF_ERROR(AssignSyncId());
	OpSyncItem *new_sync_item;
	RETURN_IF_ERROR(g_sync_coordinator->GetSyncItem(&new_sync_item,
													GetAuthType(),
													OpSyncItem::SYNC_KEY_ID,
													sync_id.CStr()));
	OpAutoPtr<OpSyncItem> sync_item_deletor(new_sync_item);

	RETURN_IF_ERROR(new_sync_item->SetStatus(GetDataItemStatus()));

	if (sync_data_modified_date.IsEmpty())
		RETURN_IF_ERROR(SetModifiedDate(UNI_L(""))); // Set the 0 date

	RETURN_IF_ERROR(new_sync_item->SetData(OpSyncItem::SYNC_KEY_MODIFIED, sync_data_modified_date));
	RETURN_IF_ERROR(ConstructSyncItemAuthTypeSpecificElements(new_sync_item));

	sync_item = sync_item_deletor.release();
	return OpStatus::OK;
}

OP_STATUS WandSyncItem::CopyTo(WandSyncItem *copy_item)
{
	copy_item->wand_item_local_sync_status = wand_item_local_sync_status;

	RETURN_IF_ERROR(copy_item->sync_id.Set(sync_id.CStr()));
	return copy_item->sync_data_modified_date.Set(sync_data_modified_date.CStr());
}
#endif // SYNC_HAVE_PASSWORD_MANAGER


