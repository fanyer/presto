/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_SYNC_MODULE_H
#define MODULES_SYNC_MODULE_H

#ifdef SUPPORT_DATA_SYNC
class OpSyncCoordinator;
class SyncEncryptionKeyManager;

struct SyncAcceptedFormat;
struct MyOperaItemTable;

class SyncModule : public OperaModule
{
public:
	SyncModule();
	virtual ~SyncModule(){};

	void InitL(const OperaInitInfo& info);
	void Destroy();
	BOOL FreeCachedData(BOOL toplevel_context);

	OpSyncCoordinator* sync;

	SyncAcceptedFormat* m_accepted_format;
	MyOperaItemTable* m_myopera_item_table;
	void InitAcceptedFormatL();
	void InitItemTableL();

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	const SyncEncryptionKeyManager* GetEncryptedKeyManager() const {
		return m_encryption_key_manager; }
	SyncEncryptionKeyManager* GetEncryptedKeyManager() {
		return m_encryption_key_manager; }

private:
	SyncEncryptionKeyManager* m_encryption_key_manager;
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
};

#define g_sync_coordinator g_opera->sync_module.sync
#define g_sync_accepted_format g_opera->sync_module.m_accepted_format
#define g_sync_myopera_item_table g_opera->sync_module.m_myopera_item_table
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
#define g_sync_encryption_key_manager g_opera->sync_module.GetEncryptedKeyManager()
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

#define SYNC_MODULE_REQUIRED

#endif // SUPPORT_DATA_SYNC

#endif // !MODULES_SYNC_MODULE_H
