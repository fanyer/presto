/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage Sync module
 *
 * This is the auto-generated API documentation for the sync module.  For more
 * information about the module, see the module's <a
 * href="https://wiki.oslo.opera.com/developerwiki/Modules/sync">Wiki
 * page</a>.
 *
 * Information about Opera Link such as protocol specification can be
 * found <a
 * href="https://wiki.oslo.opera.com/developerwiki/Opera_Link">here</a>.
 *
 * @section api API
 *
 * The public API consists of the class OpSyncCoordinator and the two
 * listener classes OpSyncUIListener and OpSyncDataClient.
 *
 * The OpSyncCoordinator exists in one global object called
 * g_sync_coordinator created at module initialization. This object is
 * used for starting and stopping sync, provide items to sync and
 * register listeners.
 *
 * To start sync first OpSyncCoordinator::Init is called to initialize
 * the object and then OpSyncCoordinator::SetLoginInformation must be
 * called to provide login information. Finally
 * OpSyncCoordinator::SetSyncActive is called to start the first
 * sync. This function is also called to stop sync. When exiting
 * OpSyncCoordinator::Cleanup must be called.
 *
 * To provide data to sync OpSyncCoordinator::GetSyncItem is called to
 * instantiate an OpSyncItem object. Then call OpSyncItem::SetData to
 * set different attributes on the data item to sync. When receiving
 * sync items the function OpSyncItem::GetData is used instead.
 *
 * Receiving data is done by register data listeners by calling
 * OpSyncCoordinator::SetSyncDataClient for each data type that should
 * be synced. This listener should be a subclass of OpSyncDataClient
 * then when receiving a new item SyncDataAvailable is called by the
 * sync module and an OpSyncItem is provided. Call OpSyncItem::GetData
 * to get the different attributes.
 *
 * There is also a listener called OpSyncUIListener which contain the
 * OnSyncError function. This is called when for example the login
 * information was incorrect.
 *
 */
