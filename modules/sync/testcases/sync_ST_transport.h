/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_ST_TRANSPORT_H_INCLUDED_
#define _SYNC_ST_TRANSPORT_H_INCLUDED_

#ifdef SELFTEST

#include "modules/sync/sync_transport.h"

class ST_SyncNullInternalSyncListener : public OpInternalSyncListener
{
public:
	ST_SyncNullInternalSyncListener() {}
	virtual ~ST_SyncNullInternalSyncListener() {}
	virtual void OnSyncStarted(BOOL items_sending) {}
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_message) {}
	virtual void OnSyncCompleted(OpSyncCollection* new_items, OpSyncState& sync_state, OpSyncServerInformation& sync_server_info) {}
	virtual void OnSyncItemAdded(BOOL new_item) {}
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	virtual void OnEncryptionKeyCreated() {}
	virtual void OnSyncReencryptEncryptionKeyFailed(OpSyncUIListener::ReencryptEncryptionKeyContext* context) {}
	virtual void OnSyncReencryptEncryptionKeyCancel(OpSyncUIListener::ReencryptEncryptionKeyContext* context) {}
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
};

class ST_SyncTransportProtocol : public OpSyncTransportProtocol
{
public:
	ST_SyncTransportProtocol(OpSyncFactory* factory, OpInternalSyncListener* listener);
	virtual ~ST_SyncTransportProtocol();

	virtual OP_STATUS Connect(OpSyncDataCollection& items_to_sync, OpSyncState& sync_state);

private:
	OP_STATUS ParseData(const OpStringC& filename);
};

#endif // SELFTEST
#endif //_SYNC_ST_TRANSPORT_H_INCLUDED_
