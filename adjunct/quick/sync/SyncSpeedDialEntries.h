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

#ifndef __SYNC_SPEEDDIAL_ENTRIES_H__
#define __SYNC_SPEEDDIAL_ENTRIES_H__

#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_coordinator.h"

#ifdef SUPPORT_DATA_SYNC
#ifdef SUPPORT_SYNC_SPEED_DIAL

/*************************************************************************
 **
 ** SyncSpeedDialEntries
 **
 **
 **************************************************************************/

class SyncSpeedDialEntries : public SpeedDialListener
						  , public OpSyncDataClient
						  , public SpeedDialConfigurationListener
						  , public SpeedDialEntryListener

{
public:
	SyncSpeedDialEntries();
	virtual ~SyncSpeedDialEntries();


public:
	// SpeedDialListener. 
	virtual void OnSpeedDialAdded(const DesktopSpeedDial &sd);
	virtual void OnSpeedDialRemoving(const DesktopSpeedDial &sd);
	virtual void OnSpeedDialReplaced(const DesktopSpeedDial &old_sd, const DesktopSpeedDial &new_sd);
	virtual void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry);
	virtual void OnSpeedDialPartnerIDAdded(const uni_char* partner_id);
	virtual void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id);
	virtual void OnSpeedDialsLoaded();

	OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);

	// SpeedDialConfigurationListener
	virtual void OnSpeedDialConfigurationStarted(const DesktopWindow& window) {}
	virtual void OnSpeedDialLayoutChanged();
	virtual void OnSpeedDialBackgroundChanged() {}
	virtual void OnSpeedDialColumnLayoutChanged() {}
	virtual void OnSpeedDialScaleChanged() {}

	// SpeedDialEntryListener
	virtual void OnSpeedDialDataChanged(const DesktopSpeedDial &sd);

	BOOL IsProcessingIncomingItems(){ return m_lock_entries; }

protected:
	OP_STATUS ProcessSyncItem(OpSyncItem* item, BOOL& dirty);
	virtual void EnableSpeedDialListening(BOOL enable, BOOL config_listener);

private:
	OP_STATUS SendAllItems(BOOL dirty);

	OP_STATUS SpeedDialPartnerIDChanged(const uni_char* partner_id, OpSyncDataItem::DataItemStatus status, BOOL dirty = FALSE);
	OP_STATUS SpeedDialChanged(const DesktopSpeedDial &sd, OpSyncDataItem::DataItemStatus status, BOOL dirty = FALSE, BOOL send_previous = FALSE);
	OP_STATUS ProcessBlacklistSyncItem(OpSyncItem *item, BOOL& dirty);

	// Doesn't check the sync lock
	OP_STATUS SpeedDialChangedNoLock(const DesktopSpeedDial &sd, OpSyncDataItem::DataItemStatus status, BOOL dirty, BOOL send_previous = FALSE);

	// find the previous speed dial to current in the list (or vector), if it exists
	const DesktopSpeedDial* FindPreviousSpeedDial(const DesktopSpeedDial* current);

	BOOL m_lock_entries;
};


#endif // SUPPORT_SYNC_SPEED_DIAL
#endif // SUPPORT_DATA_SYNC
#endif // __SYNC_SPEEDDIAL_ENTRIES_H__
