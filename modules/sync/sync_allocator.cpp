/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "modules/sync/sync_allocator.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_dataqueue.h"
#include "modules/sync/sync_factory.h"

OpSyncAllocator::OpSyncAllocator(OpSyncFactory* factory)
	: m_factory(factory)
{
	OP_NEW_DBG("OpSyncAllocator::OpSyncAllocator()", "sync");
}

OpSyncAllocator::~OpSyncAllocator()
{
	OP_NEW_DBG("OpSyncAllocator::~OpSyncAllocator()", "sync");
}

OP_STATUS OpSyncAllocator::CreateSyncItemCollection(OpSyncDataCollection& sync_data_items_in, OpSyncCollection& sync_items_out)
{
	OP_NEW_DBG("OpSyncAllocator::CreateSyncItemCollection()", "sync");
	OpSyncParser* parser;
	RETURN_IF_ERROR(m_factory->GetParser(&parser, NULL));

	for (OpSyncDataItemIterator item(sync_data_items_in.First()); *item; ++item)
	{
		OpSyncDataItem::DataItemType type = item->GetType();
		OP_DBG(("") << "received item " << (*item) << " of type " << type);

		OpSyncItem::Key primary_key = parser->GetPrimaryKey(type);
		const uni_char* key_value = 0;
		if (OpSyncItem::SYNC_KEY_NONE != primary_key)
		{
			key_value = item->FindData(parser->GetPrimaryKeyName(type));
			OP_ASSERT(key_value);
		}

		OpSyncItem* sync_item=0;
		OP_STATUS res = GetSyncItem(&sync_item, type, primary_key, key_value);
		if (OpStatus::IsSuccess(res))
		{
			OP_DBG(("add item to output"));
			sync_item->SetDataSyncItem(*item);
			sync_items_out.AddItem(sync_item);
		}
		else if (res != OpStatus::ERR_OUT_OF_RANGE)
			return res;
	}
	return OpStatus::OK;
}

OP_STATUS OpSyncAllocator::GetSyncItem(OpSyncItem** item, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data)
{
	OP_NEW_DBG("OpSyncAllocator::GetSyncItem()", "sync");
	OP_ASSERT(m_factory != NULL);
	OP_ASSERT(item != NULL);
	OP_ASSERT(key_data != NULL || primary_key == OpSyncItem::SYNC_KEY_NONE);

	if (!(m_factory && item &&
		  (key_data || primary_key == OpSyncItem::SYNC_KEY_NONE)))
		return OpStatus::ERR_NULL_POINTER;

	*item = 0;
	OpSyncParser* parser;
	RETURN_IF_ERROR(m_factory->GetParser(&parser, NULL));
	OpSyncDataQueue* data_queue;
	RETURN_IF_ERROR(m_factory->GetQueueHandler(&data_queue, NULL, TRUE));

	OpAutoPtr<OpSyncItem> sync_item(OP_NEW(OpSyncItem, (type)));
	RETURN_OOM_IF_NULL(sync_item.get());
	RETURN_IF_ERROR(sync_item->Construct());
	sync_item->SetDataQueue(data_queue);
	if (key_data)
		RETURN_IF_ERROR(sync_item->SetPrimaryKey(primary_key, key_data));
	*item = sync_item.release();
	OP_DBG(("new item: %p", *item));
	return OpStatus::OK;
}

#endif // SUPPORT_DATA_SYNC
