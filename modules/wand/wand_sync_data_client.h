/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */


#ifndef WANDSYNCDATACLIENT_H_
#define WANDSYNCDATACLIENT_H_

#ifdef SYNC_HAVE_PASSWORD_MANAGER

#include "modules/sync/sync_coordinator.h"

class WandSyncItem;

/*
 * This class holds common code for OpSyncDataItem::DATAITEM_PM_HTTP_AUTH and
 * OpSyncDataItem::DATAITEM_PM_FORM_AUTH sync types.
 */

class WandSyncDataClient: public OpSyncDataClient
{
public:
	WandSyncDataClient(){}
	virtual ~WandSyncDataClient(){}

	virtual WandSyncItem* 	FindWandSyncItem(const uni_char* sync_id, int &index) = 0;
	virtual WandSyncItem* 	FindWandSyncItemWithSameUser(WandSyncItem *item, int &index) = 0;
	virtual void 		 	DeleteWandSyncItem(int index) = 0;
	virtual OP_STATUS 		StoreWandSyncItem(WandSyncItem *item) = 0;
	virtual void			SyncWandItemsSyncStatuses() = 0;

	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty) = 0;

	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error);
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);

	virtual OP_STATUS SyncWandItemAvailable(OpSyncItem* item, OpSyncDataError& data_error);
};

#endif // SYNC_HAVE_PASSWORD_MANAGER

#endif /* WANDSYNCDATACLIENT_H_ */
