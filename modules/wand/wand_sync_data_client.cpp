/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SYNC_HAVE_PASSWORD_MANAGER
#include "modules/wand/wand_sync_item.h"
#include "modules/wand/wand_sync_data_client.h"
#include "modules/wand/wandmanager.h"

OP_STATUS WandSyncDataClient::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	if (type != OpSyncDataItem::DATAITEM_PM_HTTP_AUTH && type != OpSyncDataItem::DATAITEM_PM_FORM_AUTH)
	{
		OP_ASSERT(!"Wrong sync data item type");
		return OpStatus::OK;
	}

	return StoreWandInfo();
}

OP_STATUS WandSyncDataClient::SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error)
{
	if (!new_items || !new_items->First())
	{
		OP_ASSERT(FALSE);
		return OpStatus::OK;
	}

	WandSecurityWrapper security;
	OP_STATUS security_status = security.EnableWithoutWindow();
	RETURN_IF_MEMORY_ERROR(security_status);
	if (security_status == OpStatus::ERR_YIELD)
	{
		data_error =  SYNC_DATAERROR_ASYNC;
		return g_wand_manager->SetSuspendedSyncOperation(SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_AVAILABLE_NO_WINDOW);
	}

	data_error = SYNC_DATAERROR_NONE;

	/* Stop sync messages going back to server when receiving items from server */
	WandManager::SyncBlocker this_blocks_all_sync_messages_to_server;

	for (int round = 0; round < 2; round++)
	{
		for (OpSyncItemIterator iter(new_items->First()); *iter; ++iter)
		{
			OpSyncItem *current = *iter;
			OP_ASSERT(current && (current->GetType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH || current->GetType() == OpSyncDataItem::DATAITEM_PM_FORM_AUTH));

			// We first handle all delete events, then in second round we handle modify/add
			if ((round == 0 && current->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED) ||
				(round == 1 && current->GetStatus() != OpSyncDataItem::DATAITEM_ACTION_DELETED))
				RETURN_IF_MEMORY_ERROR(SyncWandItemAvailable(current, data_error)); // ignore sync errors

		}
	}

	return StoreWandInfo();
}

OP_STATUS WandSyncDataClient::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
{
	if (type != OpSyncDataItem::DATAITEM_PM_HTTP_AUTH && type != OpSyncDataItem::DATAITEM_PM_FORM_AUTH  )
	{
		OP_ASSERT(!"Wrong sync data item type");
		return OpStatus::OK;
	}
	return OpStatus::OK;
}

OP_STATUS WandSyncDataClient::SyncWandItemAvailable(OpSyncItem* item, OpSyncDataError& data_error)
{
	OP_ASSERT(g_wand_manager->HasSyncPasswordEncryptionKey() && "We should have received the encryption-key by now!");
	OP_ASSERT(HasPreparedSSLForCrypto());

	data_error = SYNC_DATAERROR_NONE;
	OpString sync_id;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, sync_id));

	int previous_matching_index;
	WandSyncItem *previous_matching_sync_id_login = FindWandSyncItem(sync_id.CStr(), previous_matching_index);

	switch (item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
	// Fall through. We handle it as a conflict, where the most recent change wins.
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
	{
		OpAutoPtr<WandSyncItem> new_wand_sync_item;

		WandSyncItem *temp_wand_sync_item = NULL;
		OP_STATUS create_status = WandSyncItem::CreateFromOpSyncItem(item, temp_wand_sync_item);
		RETURN_IF_MEMORY_ERROR(create_status);
		if (OpStatus::IsError(create_status)) // Decryption error or item had missing data.
		{
			if (previous_matching_sync_id_login)
			{
				// If we have a local version we send it back to the server, as the
				// incoming item was broken.
				// We change the sync ID to be able to not conflict with the delete
				// event sent below.
				RETURN_IF_ERROR(previous_matching_sync_id_login->AssignSyncId(TRUE));
				RETURN_IF_ERROR(previous_matching_sync_id_login->SyncItem(TRUE, FALSE, TRUE));
			}

			OpSyncItem *sync_item;
			RETURN_IF_ERROR(g_sync_coordinator->GetSyncItem(
								&sync_item, item->GetType(),
								OpSyncItem::SYNC_KEY_ID, sync_id));
			OpAutoPtr<OpSyncItem> sync_item_deletor(sync_item);
			RETURN_IF_ERROR(sync_item->SetStatus(OpSyncDataItem::DATAITEM_ACTION_DELETED));
			return sync_item->CommitItem();
		}
		new_wand_sync_item.reset(temp_wand_sync_item);

		WandSyncItem* previous_login;
		if (previous_matching_sync_id_login)
			previous_login = previous_matching_sync_id_login;
		else
			previous_login = FindWandSyncItemWithSameUser(new_wand_sync_item.get(), previous_matching_index);

		if (previous_login)
		{
			// conflict, delete new or old item
			if (previous_login->ResolveConflictReplaceThisItem(new_wand_sync_item.get()))
			{
				DeleteWandSyncItem(previous_matching_index);
				RETURN_IF_ERROR(StoreWandSyncItem(new_wand_sync_item.get()));
			}
			else
				new_wand_sync_item.reset(NULL);
		}
		else // No conflict. Store new item
			RETURN_IF_ERROR(StoreWandSyncItem(new_wand_sync_item.get()));

		new_wand_sync_item.release();
		break;
	}
	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
	{
		if (previous_matching_sync_id_login)
			DeleteWandSyncItem(previous_matching_index);
		break;
	}
	default:
		OP_ASSERT(!"Unknown status");
		break;
	}
	return OpStatus::OK;
}

#endif // SYNC_HAVE_PASSWORD_MANAGER
