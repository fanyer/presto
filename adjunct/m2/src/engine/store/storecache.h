/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef STORE_CACHE_H
#define STORE_CACHE_H

#include "modules/util/OpHashTable.h"
#include "adjunct/desktop_util/mempool/mempool.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/m2/src/include/defs.h"

class DuplicateTable;

struct StoreItem
{
	typedef OpFileLength RowID;

	StoreItem(message_gid_t p_m2_id, RowID p_row_id)
		: m2_id(p_m2_id)
		, row_id(p_row_id)
		, parent_id(0)
		, next_sibling_id(0)
		, sent_date(0)
		, child_sent_date(0)
		, size(0)
		, flags(0)
		, mbx_type(0)
		, master_id(0)
		, thread_root_id(0)
	{}

	bool operator==(const StoreItem& other) const { return op_memcmp(this, &other, sizeof(*this)) == 0; }

	message_gid_t		m2_id;
	RowID				row_id;
	message_gid_t		parent_id;
	message_gid_t		next_sibling_id;
	time_t				sent_date;
	// child_sent_date is the last sent child message date
	// used for sorting by date in threaded views : SORT_BY_THREADED_SENT_DATE
	time_t				child_sent_date;
	INT32				size;
	INT64				flags;
	INT32				mbx_type;
	message_gid_t		master_id; // the master m2 id of all the duplicates of this message
	message_gid_t		thread_root_id; // m2 id of the root message in the thread the message is in
};

class StoreCache
{
public:
	StoreCache() : m_mempool(4096), m_null_item(0, 0) {}

	/** Gets the set of message ids of all the replies to a specified message (all the direct children)
	  * @param message_id Message to find the children for
	  * @param children_ids All messages that are children of the given message
	  */
	OP_STATUS GetChildrenIds(message_gid_t message_id, OpINTSet& children_ids);

	/** Returns a set of all messages in the thread that a specified message is part of
	  * @param message_id Message to find a thread for
	  * @param thread_ids All messages in the thread that contains message_id (includes message_id itself)
	  */
	OP_STATUS GetThreadIds(message_gid_t message_id, OpINTSet& thread_ids);

	/** Retrieve an item from the cache. 
	  * @param id M2 ID of item to retrieve
	  * @return The item if it was found, or a null-item otherwise
	  */
	const StoreItem& GetFromCache(message_gid_t id) const;

	/** @return an iterator that can iterate through all items in this cache
	  */
	INT32HashIterator<StoreItem> GetIterator() { return INT32HashIterator<StoreItem>(m_cache_by_id); }

	/** Update or add an item
	  * @param item Item to update or add. If the M2 ID of this item is unknown, add it, otherwise update.
	  */
	OP_STATUS UpdateItem(const StoreItem& item);

	/** Remove an item from the cache
	  * @param id M2 ID of item to remove
	  */
	void RemoveItem(message_gid_t id);

	/** Remove all items in the cache
	  */
	void Clear();

	/** Add a DuplicateTable so that the store cache can use it to add more items to threads
	  */ 
	void SetDuplicateTable(DuplicateTable* duplicate_table) { m_duplicate_table = duplicate_table; }

private:
	StoreItem* GetById(message_gid_t id) const;
	StoreItem* GetFirstChild(message_gid_t parent_id) const;
	OP_STATUS RemoveFromCacheByParent(StoreItem* item);
	OP_STATUS AddToCacheByParent(StoreItem* item);
	void UpdateThreadSent(StoreItem* item);
	void SetThreadRootIdOnChildren(StoreItem* item, message_gid_t thread_root_id);

	OpINT32HashTable<StoreItem> m_cache_by_id;		///< Cache indexed by ID
	OpINT32HashTable<StoreItem> m_cache_by_parent;	///< Cache indexed by parent ID
	MemPool<StoreItem>			m_mempool;	///< Mempool for cache
	StoreItem					m_null_item;
	DuplicateTable*				m_duplicate_table;
};

#endif // STORE_CACHE_H
