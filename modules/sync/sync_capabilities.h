/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Petter Nilsen
 */

// Has RemoveSyncListener() and SetSyncListener() returns OP_STATUS now
#define SY_CAP_HAS_REMOVESYNCLISTENER

// OnSyncCompleted() had changed signature and will send the collection of finished synced items
#define SY_CAP_ONSYNCCOMPLETED_SYNCED_ITEMS

// OnSyncError() has a second argument for the error message
#define SY_CAP_ONSYNCERROR_ERRORMSG

// OpSyncState added as argument to multiple methods
#define SY_CAP_SYNCSTATE_ADDED

// OpSyncCoordinator has Cleanup()
#define SY_CAP_HAS_CLEANUP

// CreateAccount() and OnSyncCreateAccount() has a OpSyncRegistrationInformation argument
#define SY_CAP_HAS_OPSYNCREGISTRATIONINFORMATION

// OpSyncCoordinator has SetSupports()
#define SY_CAP_HAS_SETSUPPORTS

// OnSyncCompleted() has a OpSyncServerInformation argument
#define SY_CAP_ONSYNCCOMPLETED_HAS_SERVERINFO

// has OpSyncItem and associated methods
#define SY_CAP_HAS_SYNCITEM

// has SetSystemInformation()
#define SY_CAP_HAS_SETSYSTEMINFORMATION

// has SetTimeout()
#define SY_CAP_HAS_SETTIMEOUT

// OpSyncCoordinator has ClearSupports()
#define SY_CAP_HAS_CLEARSUPPORTS

// OpSyncCoordinator has SetSyncActive()
#define SY_CAP_HAS_SETSYNCACTIVE

// OpSyncCoordinator::SetSupports() has a BOOL argument for toggling support
#define SY_CAP_HAS_SETSUPPORTS_TOGGLE

// All authentication related methods and callbacks have moved out of the module
#define SY_CAP_AUTH_MOVED

// Has the class OpSyncAction with associated methods
#define SY_CAP_HAS_SYNCACTION

// OpSyncListener is obsolete and has been split into OpSyncUIListener and OpSyncDataListener
#define SY_CAP_SYNCLISTENER_CHANGED

// OpSyncDataListener has changed to OpSyncDataClient and method signatures have changed
#define SY_CAP_SYNCDATALISTENER_CHANGED

// CommitItem() has a second argument for whether it will be inserted ordered or not.
#define SY_CAP_COMMITITEM_ORDERED

// OpSyncCoordinator::HasQueuedItems() is available
#define SY_CAP_HASQUEUEDITEMS
