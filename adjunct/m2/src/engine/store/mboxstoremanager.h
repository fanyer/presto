// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef MBOXSTOREMANAGER_H
#define MBOXSTOREMANAGER_H

#include "adjunct/m2/src/engine/store/mboxstore.h"
#include "modules/util/OpHashTable.h"

class Message;

/** @brief Manager for different types of mbox storage
  * @author Arjan van Leeuwen
  *
  * The store is able to handle different formats for 'raw' mail storage.
  * The manager can be used to get a store of the right type.
  */
class MboxStoreManager
{
public:
	/** Gets a store of a specific type
	  * @param type Type of store to get (should be one of MboxType)
	  * @param store Where to get the store
	  * @param required_features Features needed (bitwise OR of MboxStore::StoreFeatures)
	  * @return OpStatus::OK if store was found, error otherwise
	  */
	OP_STATUS GetStore(int type, int required_features, MboxStore*& store);

	/** Tries to find a store that contains specified message and gets message
	  * @param mbx_data mbx_data for message
	  * @param message Message to find and output
	  * @param store Store where message was found
	  * @param override Whether to override data already found in message with data found in store
	  * @return OpStatus::OK if message was found, error otherwise
	  */
	OP_STATUS GetMessage(INT64 mbx_data, Message& message, MboxStore*& store, BOOL override);

	/** Sets the path to mbox storage
	  * @param path path to storage (default mail root, this method sets it to something else)
	  */
	OP_STATUS SetStorePath(const OpStringC& path) { return m_store_path.Set(path); }

	/** Commits pending data to all opened stores
	  */
	OP_STATUS CommitData();

protected:
	/** Creates a store of a specific type
	  * @param type Type of store to create (should be one of MboxType)
	  * @param store Where to create the store
	  */
	OP_STATUS CreateStore(int type, MboxStore*& store);

	OpAutoINT32HashTable<MboxStore>	m_stores;

	OpString 						m_store_path;
};

#endif // MBOXSTOREMANAGER_H
