/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_ALLOCATOR_H_INCLUDED_
#define _SYNC_ALLOCATOR_H_INCLUDED_

#include "modules/sync/sync_datacollection.h"

// Current class size on Windows 32-bit: 48 bytes
class OpSyncAllocator
{
	friend class OpSyncFactory;

private:
	/** The constructor is private, only certain classes are allowed
	 * to create an item! */
	OpSyncAllocator(OpSyncFactory* factory);

public:
	virtual ~OpSyncAllocator();

	OP_STATUS CreateSyncItemCollection(OpSyncDataCollection& sync_data_items_in, OpSyncCollection& sync_items_out);

	/**
	 * Creates a new OpSyncItem. This is the only valid way to get a new
	 * OpSyncItem and provides an easier way to get and add a new item to be
	 * synchronised.
	 *
	 * When you are done adding data using the OpSyncItem::SetData() method, you
	 * must call OpSyncItem::CommitItem() to signal the sync module that it can
	 * commit the changes into the queue for synchronising.
	 *
	 * You must free the item using OP_DELETE() method after calling
	 * CommitItem().
	 *
	 * @param item Receives on success a new OpSyncItem instance that can be
	 *  used for synchronisation.
	 * @param type a OpSyncDataItem::DataItemType definition for the type of
	 *  item to get.
	 * @param primary_key a OpSyncItem::Key definition of the unique primary key
	 *  for this type of data.
	 * @param key_data The data associated with the primary_key given. This will
	 *  be used by the sync module for identifying unique items.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS GetSyncItem(OpSyncItem** item, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data);

protected:
	OpSyncFactory* m_factory;
};

#endif //_SYNC_ALLOCATOR_H_INCLUDED_
