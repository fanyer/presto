/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/mboxstoremanager.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/store/mboxcursor.h"
#include "adjunct/m2/src/engine/store/mboxpermail.h"
#include "adjunct/m2/src/engine/store/mboxmonthly.h"
#include "adjunct/m2/src/engine/store/mboxnone.h"


/***********************************************************************************
 ** Gets a store of a specific type
 **
 ** MboxStoreManager::GetStore
 ** @param type Type of store to create (should be one of MboxType)
 ** @param required_features Features needed (bitwise OR of MboxStore::StoreFeatures)
 ** @param store Where to get the store
 **
 ***********************************************************************************/
OP_STATUS MboxStoreManager::GetStore(int type, int required_features, MboxStore*& store)
{
	store = NULL;

	// Check if we have a store of this type
	if (OpStatus::IsError(m_stores.GetData(type, &store)))
	{
		// Store of this type not found, create a new one
		RETURN_IF_ERROR(CreateStore(type, store));
	}

	if (required_features && !(store->GetFeatures() & required_features))
		return GetStore(type + 1, required_features, store);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Tries to find a store that contains specified message and gets message
 **
 ** MboxStoreManager::GetMessage
 ** @param mbx_data mbx_data for message
 ** @param message Message to find and output
 ** @param store Store where message was found
 ** @param override Whether to override data already found in message with data found in store
 ** @return OpStatus::OK if message was found, error otherwise
 **
 ***********************************************************************************/
OP_STATUS MboxStoreManager::GetMessage(INT64 mbx_data, Message& message, MboxStore*& store, BOOL override)
{
	for (int i = AccountTypes::MBOX_TYPE_COUNT - 1; i > AccountTypes::MBOX_NONE; i--)
	{
		if (OpStatus::IsSuccess(GetStore(i, 0, store)))
			if (OpStatus::IsSuccess(store->GetMessage(mbx_data, message, override)))
				return OpStatus::OK;
	}

	return OpStatus::ERR_FILE_NOT_FOUND;
}


/***********************************************************************************
 ** MboxStoreManager::CommitData
 **
 ***********************************************************************************/
OP_STATUS MboxStoreManager::CommitData()
{
	for (INT32HashIterator<MboxStore> it(m_stores); it; it++)
	{
		RETURN_IF_ERROR(it.GetData()->CommitData());
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Creates a store of a specific type
 **
 ** MboxStoreManager::CreateStore
 ** @param type Type of store to create (should be one of MboxType)
 ** @param store Where to create the store
 **
 ***********************************************************************************/
OP_STATUS MboxStoreManager::CreateStore(int type, MboxStore*& store)
{
	OpAutoPtr<MboxStore> new_store;

	// Check type of store and create new object
	switch(type)
	{
		case AccountTypes::MBOX_NONE:
			new_store = OP_NEW(MboxNone, ());
			break;
		case AccountTypes::MBOX_MONTHLY:
			new_store = OP_NEW(MboxMonthly, ());
			break;
		case AccountTypes::MBOX_PER_MAIL:
			new_store = OP_NEW(MboxPerMail, ());
			break;
		case AccountTypes::MBOX_CURSOR:
			new_store = OP_NEW(MboxCursor, ());
			break;
		default:
			OP_ASSERT(!"Unknown mbox type, this should never happen");
			return OpStatus::ERR;
	}

	if (!new_store.get())
		return OpStatus::ERR_NO_MEMORY;

	// Initialize new store
	RETURN_IF_ERROR(new_store->Init(m_store_path));

	// Add store to our hash table
	RETURN_IF_ERROR(m_stores.Add(type, new_store.get()));

	store = new_store.release();

	return OpStatus::OK;
}

#endif // M2_SUPPORT
