/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 *
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/sync/SyncSpeedDialEntries.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "modules/img/imagedump.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/sync/sync_coordinator.h"

#ifdef SUPPORT_SYNC_SPEED_DIAL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SyncSpeedDialEntries::SyncSpeedDialEntries() :
	m_lock_entries(FALSE)
{ 
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SyncSpeedDialEntries::~SyncSpeedDialEntries() 
{  
	for (UINT32 i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
	{
		const DesktopSpeedDial& entry = *g_speeddial_manager->GetSpeedDial(i);
		entry.RemoveListener(*this);
	}
}

// SpeedDialConfigurationListener
void SyncSpeedDialEntries::OnSpeedDialLayoutChanged()
{
	EnableSpeedDialListening(FALSE, FALSE);
	EnableSpeedDialListening(TRUE, FALSE);
}

void SyncSpeedDialEntries::OnSpeedDialDataChanged(const DesktopSpeedDial &sd)
{
	if (m_lock_entries)
	{
		return;
	}
	SpeedDialChangedNoLock(sd, OpSyncDataItem::DATAITEM_ACTION_MODIFIED,FALSE);
}

void SyncSpeedDialEntries::EnableSpeedDialListening(BOOL enable, BOOL config_listener)
{
	if (enable)
	{
		if(config_listener)
		{
			g_speeddial_manager->AddConfigurationListener(this);
		}

		g_speeddial_manager->AddListener(*this);
	}
	else
	{
		if(config_listener)
		{
			g_speeddial_manager->RemoveConfigurationListener(this);
		}
		g_speeddial_manager->RemoveListener(*this);
	}
}

OP_STATUS SyncSpeedDialEntries::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	if (type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2)
	{
		OpFile sd_file;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::SpeedDialFile, sd_file));
		g_sync_manager->BackupFile(sd_file);
		return OpStatus::OK;
	}
	else if(type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS)
	{
		// nothing to do
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

// Process incoming items
OP_STATUS SyncSpeedDialEntries::SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error)
{
	OP_STATUS status = OpStatus::OK;
	BOOL do_dirty_sync = FALSE;

	// Note: with the current "design": The same item
	// might be in both the new_items and the synced_items list
	if(new_items && new_items->First())
	{
		OpSyncItemIterator iter(new_items->First());
			
		do
		{
			OpSyncItem *current = iter.GetDataItem();
					
			OP_ASSERT(current);
			if(current)
			{
				if(current->GetType() == OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS)
				{
					status = ProcessBlacklistSyncItem(current, do_dirty_sync);
					OP_ASSERT(OpStatus::IsSuccess(status));
				}
				else
				{
					status = ProcessSyncItem(current, do_dirty_sync);
					OP_ASSERT(OpStatus::IsSuccess(status));
				}
				if (OpStatus::IsError(status) || do_dirty_sync)
				{
					if (do_dirty_sync)
					{
						data_error =  SYNC_DATAERROR_INCONSISTENCY;
					}
					break; // Don't process more items
				}
			}
		} while (iter.Next());
	}
	if(OpStatus::IsError(status))
	{
		// reset the sync state to get fresh data from the server
		g_sync_coordinator->ResetSupportsState(SYNC_SUPPORTS_SPEEDDIAL_2, FALSE);

		// cancel any pending save now
		g_speeddial_manager->CancelUpdate();
	}
	return status;
}

OP_STATUS SyncSpeedDialEntries::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	if (type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2)
	{
		OP_ASSERT(g_sync_coordinator->GetSupports(SYNC_SUPPORTS_SPEEDDIAL_2));
		if (!g_sync_coordinator->GetSupports(SYNC_SUPPORTS_SPEEDDIAL_2))
		{
			return OpStatus::ERR;
		}
		if (is_dirty)
		{
			SendAllItems(TRUE);
		}
		else if (first_sync)
		{
			SendAllItems(FALSE);
		}
		// send off our list of deleted items
		for(UINT32 i = 0; i < g_speeddial_manager->GetDeletedSpeedDialIDCount(); i++)
		{
			SpeedDialPartnerIDChanged(g_speeddial_manager->GetDeletedSpeedDialID(i), OpSyncDataItem::DATAITEM_ACTION_ADDED, is_dirty);
		}
	}
	else if(type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS)
	{
		// we do it above
	}
	else
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


OP_STATUS SyncSpeedDialEntries::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
{
	if (type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2)
	{
		EnableSpeedDialListening(has_support, TRUE);
	}
	else if(type == OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS)
	{
		// nothing to do
	}
	else
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

const DesktopSpeedDial* SyncSpeedDialEntries::FindPreviousSpeedDial(const DesktopSpeedDial* current)
{
	INT32 prev_pos = g_speeddial_manager->FindSpeedDial(current);
	if(prev_pos > 0)
	{
		return g_speeddial_manager->GetSpeedDial(prev_pos - 1);
	}
	return NULL;
}

/*************************************************************************
 **
 ** ProcessSyncItem
 **
 ** Called when synchronization has completed. Updates "local" synced items
 **
 **************************************************************************/

OP_STATUS SyncSpeedDialEntries::ProcessSyncItem(OpSyncItem *item, BOOL& dirty)
{
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	if (!g_sync_manager->SupportsType(SyncManager::SYNC_SPEEDDIAL))
	{
		return OpStatus::OK;
	}

	OpString data;

	OpString title;
	OpString custom_title;
	OpString partner_id;
	OpString extension_id;
	OpString url;
	OpString unique_id;
	OpString previous;

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, unique_id));

	const DesktopSpeedDial *sd = g_speeddial_manager->GetSpeedDialByID(unique_id);

	SpeedDialData::ReloadPolicy reload_policy = sd ? sd->GetReloadPolicy() : SpeedDialData::Reload_NeverSoft;
	INT32 reload_interval       = sd ? sd->GetReloadTimeout() : SPEED_DIAL_RELOAD_INTERVAL_DEFAULT; 
	BOOL reload_only_if_expired = sd ? sd->GetReloadOnlyIfExpired() : SPEED_DIAL_RELOAD_ONLY_IF_EXPIRED_DEFAULT;

	OP_STATUS status = item->GetData(OpSyncItem::SYNC_KEY_TITLE, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(title.Set(data));
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_CUSTOM_TITLE, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(custom_title.Set(data));
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_URI, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(url.Set(data));
	}
	status = item->GetData(OpSyncItem::SYNC_KEY_PREV, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(previous.Set(data));
	}
	status = item->GetData(OpSyncItem::SYNC_KEY_PARTNER_ID, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(partner_id.Set(data));
	}
	status = item->GetData(OpSyncItem::SYNC_KEY_EXTENSION_ID, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(extension_id.Set(data));
	}
	status = item->GetData(OpSyncItem::SYNC_KEY_RELOAD_INTERVAL, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		reload_interval = uni_atoi(data.CStr());
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_RELOAD_ONLY_IF_EXPIRED, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		reload_only_if_expired = uni_atoi(data.CStr());
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_RELOAD_ENABLED, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		UINT32 i = uni_atoi(data.CStr());
		if (i == 1)
		{
			reload_policy = SpeedDialData::Reload_UserDefined;
		}
		else
		{
			reload_policy = SpeedDialData::Reload_NeverSoft;
		}
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_RELOAD_POLICY, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		if (!data.CompareI(UNI_L("undefined")))
			reload_policy = SpeedDialData::Reload_NeverSoft;
		else if (!data.CompareI(UNI_L("user")))
			reload_policy = SpeedDialData::Reload_UserDefined;
		else if (!data.CompareI(UNI_L("page")))
			reload_policy = SpeedDialData::Reload_PageDefined;
		else if (!data.CompareI(UNI_L("never")))
			reload_policy = SpeedDialData::Reload_NeverHard;
	}

	QuickSyncLock lock(m_lock_entries, TRUE, FALSE);

	DesktopSpeedDial tmp_sd;
	const DesktopSpeedDial* previous_sd = previous.HasContent() ? g_speeddial_manager->GetSpeedDialByID(previous.CStr()) : NULL;

	if(sd)
	{
		RETURN_IF_ERROR(tmp_sd.Set(*sd));
	}

	// the previous is invalid and requires special measures if:
	// - previous attribute is missing (from the description of OpSyncItem::GetData: "the data will be set
	//   to empty, i.e. data.CStr() will be NULL, if no value was found"), see DSK-359666,
	// - or previous is set, but no item is found.
	BOOL invalid_previous = previous.CStr() == NULL || (previous.HasContent() && previous_sd == NULL);
	switch(item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
		if (!sd  && extension_id.HasContent())
		{
			// extensions cell might have so called mock-ups on other link client (thumbnail placeholder),
			// which should be replaced in case real extension is installed. 
			// Temporary feature, which should be removed together with DSK-335264
			for (unsigned i = 0; i < g_speeddial_manager->GetTotalCount(); ++i)
			{
				const DesktopSpeedDial* cur_sd = g_speeddial_manager->GetSpeedDial(i);
				if (cur_sd->GetExtensionID() == extension_id && 
					cur_sd->GetURL() == url)
				{
					BOOL dev_mode = FALSE;
					if (OpStatus::IsSuccess(ExtensionUtils::IsDevModeExtension(cur_sd->GetExtensionWUID(), dev_mode))
							&& !dev_mode)
					{
						sd = cur_sd;
						break;
					}
				}
			}
		}
		// fall thought?

	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
		{
			RETURN_IF_ERROR(tmp_sd.SetURL(url));

			if (custom_title.HasContent())
				RETURN_IF_ERROR(tmp_sd.SetTitle(custom_title, TRUE));
			else
				RETURN_IF_ERROR(tmp_sd.SetTitle(title, FALSE));

			tmp_sd.SetReload(reload_policy, reload_interval, reload_only_if_expired);
			RETURN_IF_ERROR(tmp_sd.SetPartnerID(partner_id));
			RETURN_IF_ERROR(tmp_sd.SetUniqueID(unique_id));

			OpStringC wuid = sd ? sd->GetExtensionWUID() : NULL;
			RETURN_IF_ERROR(tmp_sd.SetExtensionID(wuid.HasContent() ? wuid : extension_id));

			if(sd)
			{
				// we're updating an item

				if (sd->GetUniqueID().Compare(tmp_sd.GetUniqueID()) == 0 && sd->GetExtensionWUID().HasContent())
				{
					// title and URL attribues of an extension SD cell are managed by extension and not updated
					// via Link
					status = OpStatus::OK;
				}
				else
				{
					status = g_speeddial_manager->ReplaceSpeedDial(sd, &tmp_sd, true);
				}

				if (OpStatus::IsSuccess(status))
				{
					// get it again
					sd = g_speeddial_manager->GetSpeedDialByID(unique_id);

					// compute new position and move speed dial if it is different

					int sd_pos = g_speeddial_manager->FindSpeedDial(sd);
					int new_sd_pos = sd_pos;

					if (previous_sd)
					{
						// previous_sd exists - move sd after it
						int previous_sd_pos = g_speeddial_manager->FindSpeedDial(previous_sd);
						OP_ASSERT(previous_sd_pos >= 0);
						// sd will be first removed and then inserted at the new position, so new_sd_pos
						// must be calculated for list of speed dials without sd
						if (sd_pos <= previous_sd_pos)
							new_sd_pos = previous_sd_pos;
						else
							new_sd_pos = previous_sd_pos + 1;
					}
					else if (invalid_previous)
					{
						// previous is present in the data but points to an item that doesn't exist - move item to the end (DSK-330664)
						UINT32 count = g_speeddial_manager->GetTotalCount();
						new_sd_pos = count > 0 ? count - 1 : 0;
						// but before the plus button
						if (new_sd_pos > 0 && g_speeddial_manager->IsPlusButtonShown())
							-- new_sd_pos;
					}
					else if (previous.IsEmpty())
					{
						// move to the beginning
						new_sd_pos = 0;
					}
					else
					{
						OP_ASSERT(!"If we got here something is wrong!");
					}

					if (new_sd_pos != sd_pos)
					{
						status = g_speeddial_manager->MoveSpeedDial(sd_pos, new_sd_pos);
					}

					if (OpStatus::IsSuccess(status) && invalid_previous)
					{
						// received previous is invalid - send sd back to the server with the new previous
						status = SpeedDialChangedNoLock(*sd, OpSyncDataItem::DATAITEM_ACTION_MODIFIED, FALSE, TRUE);
					}
				}
			}
			else
			{
				// we're adding a new item
				if(previous_sd)
				{
					status = g_speeddial_manager->InsertSpeedDial(g_speeddial_manager->FindSpeedDial(previous_sd) + 1, &tmp_sd, false);
				}
				else
				{
					// add it at the beginning (previous == "") or at the end (previous attribute is invalid or missing) - see DSK-359666
					UINT32 count = g_speeddial_manager->GetTotalCount();
					int pos = invalid_previous && count ? count - 1 : 0;
					status = g_speeddial_manager->InsertSpeedDial(pos, &tmp_sd, false);
					if(invalid_previous && OpStatus::IsSuccess(status))
					{
						// invalid previous even on a regular add, so make sure the server gets the new item
						sd = g_speeddial_manager->GetSpeedDialByID(unique_id.CStr());
						OP_ASSERT(sd);
						if(sd)
						{
							// send it back to the server with the new previous
							status = SpeedDialChangedNoLock(*sd, OpSyncDataItem::DATAITEM_ACTION_MODIFIED, FALSE, TRUE);
						}
					}
				}
			}
		}
		break;

	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
		// ignore errors on unknown items
		OpStatus::Ignore(g_speeddial_manager->RemoveSpeedDial(sd, false));
		break;

	default:
		OP_ASSERT(FALSE);
		break;
	}
	return status;
}

/***********************************************************************
 **
 ** SendAllItems
 **
 ** Send all non-empty speed dials to sync module
 ** Used on first login and when doing dirty sync
 **
 **********************************************************************/

OP_STATUS SyncSpeedDialEntries::SendAllItems(BOOL dirty)
{
	for (unsigned pos = 0; pos < g_speeddial_manager->GetTotalCount(); pos++)
	{
		const DesktopSpeedDial* speed_dial = g_speeddial_manager->GetSpeedDial(pos);
		
		if (speed_dial)
		{
			// SpeedDialChanged must be changed to return an error
			// speed_dial should be a SpeedDial, not a SpeedDialData
			SpeedDialChanged(*speed_dial, OpSyncDataItem::DATAITEM_ACTION_ADDED, dirty, TRUE);
		}
	}

	return OpStatus::OK;
}


/***********************************************************************
 **
 ** SpeedDialChanged / SpeedDialChangedNoLock
 **
 ** Send SyncItem for this speed dial to sync module
 **
 **
 **********************************************************************/
OP_STATUS SyncSpeedDialEntries::SpeedDialChangedNoLock(const DesktopSpeedDial &sd,
                                                       OpSyncDataItem::DataItemStatus status,
                                                       BOOL dirty,
                                                       BOOL send_previous)
{
	if (!g_sync_manager->SupportsType(SyncManager::SYNC_SPEEDDIAL))
	{
		return OpStatus::OK;
	}
	BOOL deleted = status == OpSyncDataItem::DATAITEM_ACTION_DELETED;
	OpSyncItem *item;
	OP_STATUS s;
	OpString interval;

	if (sd.IsEmpty())
	{
		return OpStatus::OK;
	}

	// Must have a GUID
	if (sd.GetUniqueID().IsEmpty())
	{
		OP_ASSERT(!"No guid!");
		return OpStatus::ERR;
	}


	if (sd.GetExtensionWUID().HasContent())
	{
		BOOL dev_mode = FALSE;
		RETURN_IF_ERROR(ExtensionUtils::IsDevModeExtension(sd.GetExtensionWUID(), dev_mode));
		if (dev_mode)
			return OpStatus::OK;
	}

	s = g_sync_coordinator->GetSyncItem(&item, OpSyncDataItem::DATAITEM_SPEEDDIAL_2, OpSyncItem::SYNC_KEY_ID, sd.GetUniqueID().CStr());

	if(OpStatus::IsSuccess(s))
	{
		item->SetStatus(status);

		// we don't need to send lots of info for deleted items
		if(!deleted)
		{
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_TITLE, sd.GetTitle().CStr()), item);
			
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_CUSTOM_TITLE, 
				sd.IsCustomTitle() ? sd.GetTitle().CStr(): UNI_L("")), item);

			if (sd.GetExtensionWUID().HasContent() && sd.HasInternalExtensionURL())
			{
				OpString url;
				SYNC_RETURN_IF_ERROR(g_desktop_extensions_manager->GetExtensionFallbackUrl(sd.GetExtensionWUID(),url),item);
				SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_URI, url), item);
			}
			else
			{
				SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_URI, sd.GetURL().CStr()), item);
			}
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PARTNER_ID, sd.GetPartnerID().CStr()), item);
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_EXTENSION_ID, sd.GetExtensionID().CStr()), item);

			if (send_previous)
			{
				const DesktopSpeedDial* previous_sd = FindPreviousSpeedDial(&sd);
				if(previous_sd)
				{
					SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PREV, previous_sd->GetUniqueID().CStr()), item);
				}
				else
				{
					SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_PREV, NULL), item);
				}
			}
			interval.Empty();
			interval.AppendFormat(UNI_L("%d"), sd.GetReloadEnabled() ? 1 : 0);
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_ENABLED, interval), item);

			interval.Empty();
			SpeedDialData::ReloadPolicy reload_policy = sd.GetReloadPolicy();
			if (reload_policy == SpeedDialData::Reload_NeverSoft)
				interval.AppendFormat(UNI_L("undefined"));
			else if (reload_policy == SpeedDialData::Reload_UserDefined)
				interval.AppendFormat(UNI_L("user"));
			else if (reload_policy == SpeedDialData::Reload_PageDefined)
				interval.AppendFormat(UNI_L("page"));
			else if (reload_policy == SpeedDialData::Reload_NeverHard)
				interval.AppendFormat(UNI_L("never"));
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_POLICY, interval), item);

			int reload_interval = sd.GetReloadTimeout();
			interval.Empty();
			interval.AppendFormat(UNI_L("%d"), reload_interval);
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_INTERVAL, interval), item);

			interval.Empty();
			interval.AppendFormat(UNI_L("%d"), sd.GetReloadOnlyIfExpired() ? 1 : 0);
			SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_RELOAD_ONLY_IF_EXPIRED, interval), item);

			Image image = g_favicon_manager->Get(sd.GetURL().CStr());
			if(!image.IsEmpty())
			{
				OpBitmap *bitmap = image.GetBitmap(NULL);
				if(bitmap)
				{
					TempBuffer base64;
					if(OpStatus::IsSuccess(GetOpBitmapAsBase64PNG(bitmap, &base64)))
					{
						SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_ICON, 
							base64.GetStorage()), item);
					}
					image.ReleaseBitmap();
				}
			}
			else
			{
				SYNC_RETURN_IF_ERROR(item->SetData(OpSyncItem::SYNC_KEY_ICON, UNI_L("")), item);
			}
		}
		SYNC_RETURN_IF_ERROR(item->CommitItem(dirty, !dirty), item);
	}
	g_sync_coordinator->ReleaseSyncItem(item);

	return OpStatus::OK;
}

OP_STATUS SyncSpeedDialEntries::SpeedDialChanged(const DesktopSpeedDial &sd, 
												 OpSyncDataItem::DataItemStatus status, 
												 BOOL dirty,
												 BOOL send_previous)
{
	if (m_lock_entries)
	{
		return OpStatus::OK;
	}
	return SpeedDialChangedNoLock(sd, status, dirty, send_previous);
}

/***********************************************************************
 **
 ** OnSpeedDialAdded
 **
 **
 **********************************************************************/

void SyncSpeedDialEntries::OnSpeedDialAdded(const DesktopSpeedDial &sd)
{
	SpeedDialChanged(sd, OpSyncDataItem::DATAITEM_ACTION_ADDED, FALSE, TRUE);
	sd.AddListener(*this);
}


/***********************************************************************
 **
 ** OnSpeedDialReplaced
 **
 **
 **********************************************************************/

void SyncSpeedDialEntries::OnSpeedDialReplaced(const DesktopSpeedDial &old_sd, const DesktopSpeedDial &new_sd)
{
	old_sd.RemoveListener(*this);
	new_sd.AddListener(*this);

	BOOL sd_added = old_sd.IsEmpty();
	// If new sd is added (old sd was empty) then previous should be sent.
	SpeedDialChanged(new_sd, OpSyncDataItem::DATAITEM_ACTION_MODIFIED, FALSE, sd_added);
}



/***********************************************************************
 **
 ** OnSpeedDialRemoving
 **
 **
 **********************************************************************/

void SyncSpeedDialEntries::OnSpeedDialRemoving(const DesktopSpeedDial &sd)
{
	SpeedDialChanged(sd, OpSyncDataItem::DATAITEM_ACTION_DELETED);
	sd.RemoveListener(*this);
}


/***********************************************************************
 **
 ** OnSpeedDialMoved
 **
 **
 **********************************************************************/

void SyncSpeedDialEntries::OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry)
{
	SpeedDialChanged(to_entry, OpSyncDataItem::DATAITEM_ACTION_MODIFIED, FALSE, TRUE);
}

void SyncSpeedDialEntries::OnSpeedDialPartnerIDAdded(const uni_char* partner_id) 
{
	if (m_lock_entries || !g_sync_coordinator->GetSupports(SYNC_SUPPORTS_SPEEDDIAL_2))
	{
		return;
	}
	if(partner_id)
	{
		SpeedDialPartnerIDChanged(partner_id, OpSyncDataItem::DATAITEM_ACTION_ADDED, FALSE);
	}
}

void SyncSpeedDialEntries::OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) 
{
	if (m_lock_entries || !g_sync_coordinator->GetSupports(SYNC_SUPPORTS_SPEEDDIAL_2))
	{
		return;
	}
	if(partner_id)
	{
		SpeedDialPartnerIDChanged(partner_id, OpSyncDataItem::DATAITEM_ACTION_DELETED, FALSE);
	}
}

void SyncSpeedDialEntries::OnSpeedDialsLoaded()
{
	for (UINT32 i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
	{
		const DesktopSpeedDial& entry = *g_speeddial_manager->GetSpeedDial(i);
		entry.AddListener(*this);
	}
}

OP_STATUS SyncSpeedDialEntries::SpeedDialPartnerIDChanged(const uni_char* partner_id, OpSyncDataItem::DataItemStatus status, BOOL dirty)
{
	OpSyncItem *item;

	// TODO: Add blacklist entries
	OP_STATUS s = g_sync_coordinator->GetSyncItem(&item, OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS, OpSyncItem::SYNC_KEY_PARTNER_ID, partner_id);

	if(OpStatus::IsSuccess(s))
	{
		item->SetStatus(status);

		s = item->CommitItem(dirty, FALSE);

		g_sync_coordinator->ReleaseSyncItem(item);
	}
	return s;
}

/*************************************************************************
 **
 ** ProcessBlacklistSyncItem
 **
 ** Called when synchronization has completed and blacklist items has been received
 **
 **************************************************************************/

OP_STATUS SyncSpeedDialEntries::ProcessBlacklistSyncItem(OpSyncItem *item, BOOL& dirty)
{
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	if (!g_sync_manager->SupportsType(SyncManager::SYNC_SPEEDDIAL))
	{
		return OpStatus::OK;
	}
	OpString data;
	OP_STATUS status = OpStatus::OK;

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PARTNER_ID, data));

	switch(item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
		{
			if(!g_speeddial_manager->IsInDeletedList(data.CStr()))
			{
				status = g_speeddial_manager->AddToDeletedIDs(data.CStr(), false);
			}
		}
		break;

	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
		// ignore errors on unknown items
		g_speeddial_manager->RemoveFromDeletedIDs(data.CStr(), false);
		break;

	default:
		OP_ASSERT(FALSE);
		break;
	}
	return status;
}

#endif // SUPPORT_SYNC_SPEED_DIAL
#endif // SUPPORT_DATA_SYNC
