/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_parser_myopera.h"
#include "modules/sync/sync_encryption_key.h"
#include "modules/sync/sync_module.h"

SyncModule::SyncModule()
	: sync(NULL)
	, m_accepted_format(NULL)
	, m_myopera_item_table(NULL)
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	, m_encryption_key_manager(0)
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
{
}

void SyncModule::InitL(const OperaInitInfo& info)
{
	sync = OP_NEW_L(OpSyncCoordinator, ());
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	m_encryption_key_manager = OP_NEW_L(SyncEncryptionKeyManager, ());
	m_encryption_key_manager->InitL(sync);
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
	InitAcceptedFormatL();
	InitItemTableL();
}

void SyncModule::Destroy()
{
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	OP_DELETE(m_encryption_key_manager); m_encryption_key_manager = 0;
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
	OP_DELETE(sync);
	sync = NULL;
	OP_DELETEA(m_accepted_format);
	OP_DELETEA(m_myopera_item_table);
}

BOOL SyncModule::FreeCachedData(BOOL toplevel_context)
{
	if (sync)
		sync->FreeCachedData(toplevel_context);
	return TRUE;
}

void SyncModule::InitAcceptedFormatL()
{
	extern void init_AcceptedFormat();
	m_accepted_format = OP_NEWA_L(SyncAcceptedFormat, OpSyncDataItem::DATAITEM_MAX+1);
	init_AcceptedFormat();
}

void SyncModule::InitItemTableL()
{
	/* OpSyncItem::SYNC_KEY_NONE is the last enum value. The MyOperaItemTable*
	 * has an item for each enum value - and the last item is an empty item, so
	 * we allocate one more item here: */
	m_myopera_item_table = OP_NEWA_L(MyOperaItemTable, OpSyncItem::SYNC_KEY_NONE+1);
	/* This function is defined in sync_parser_myopera.cpp: */
	extern void init_MyOperaItemTable();
	init_MyOperaItemTable();
}

#endif // SUPPORT_DATA_SYNC
