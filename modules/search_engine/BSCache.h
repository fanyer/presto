/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef BLOCKSTORAGECACHE_H
#define BLOCKSTORAGECACHE_H

#include "modules/search_engine/BlockStorage.h"

#define NUR_MASK 0x7FFF
#define NUR_MAX  0x8000

/**
 * @brief Cache over BlockStorage
 * @author Pavel Studeny <pavels@opera.com>
 *
 * If the access to the file isn't completely random, like for ACT or BTree,
 * it makes sense to have some cache. This is an NUR cache, also suitable
 * for prejournalling the data before commit.
 */
class BSCache : public NonCopyable, public SearchGroupable
{
public:
	/**
	 * specifies which Items to release from memory on Flush
	 */
	enum ReleaseSeverity
	{
		ReleaseNo   = 0, /**< just flush */
		ReleaseAll  = 1, /**< release all unreferenced */
		ReleaseNUR  = 2, /**< release not used recently */
		JournalOnly = 3, /**< just journal the modified Items */
		JournalAll  = 4  /**< JournalOnly + reserve all the unsaved items (with disk_id < 0) */
	};

	/**
	 * cacheable item, define its payload and virtual methods in descendants
	 * as well as NewMemoryItem and NewDiskItem
	 */
	struct Item : public NonCopyable
	{
		typedef INT32 DiskId;

		DiskId disk_id;
		Item *previous; // Possible improvement: Use other structure than linked list
		BOOL modified;
		BOOL in_list;
		BOOL journalled;
		DiskId deleted_id;
		int reference_count;
		unsigned short NUR_mark;
		BOOL id_reserved;

		Item(void)
		{
			disk_id = 0;
			previous = NULL;
			modified = FALSE;
			in_list = FALSE;
			journalled = FALSE;
			deleted_id = 0;
			reference_count = 0;
			id_reserved = FALSE;
			NUR_mark = 0;
		}

		virtual ~Item(void) {}

		/**
		 * read data from position specified by disk_id
		 */
		CHECK_RESULT(virtual OP_STATUS Read(BlockStorage *storage)) = 0;

		/**
		 * write data to the storage:
		 * update if disk_id > 0 and create a new record if disk_id < 0;
		 * in this case also update disk_id to the number of the new block;
		 * if disk_id == 0, delete deleted_id from the file
		 */
		CHECK_RESULT(virtual OP_STATUS Flush(BlockStorage *storage)) = 0;

		/**
		 * called whenever disk_id changes (could be in Flush, for example),
		 * the value of Item::disk_id is undefined during this call
		 */
		virtual void OnIdChange(DiskId new_id, DiskId old_id) {}
		
		/**
		 * @return an estimate of the memory used by this data structure
		 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
		virtual size_t EstimateMemoryUsed() const;
#endif
	};

	/**
	 * @param max_cache maximum number of non-referenced Items in memory
	 */
	BSCache(int max_cache);
	virtual ~BSCache(void);

	void SetNURFlush(BOOL nur)
	{
		m_flush_mode = nur ? ReleaseNUR : JournalOnly;
	}

	/**
	 * write all unsaved data to disk
	 * @return OpBoolean::IS_TRUE if the operation completed, OpBoolean::IS_FALSE if time limit was reached
	 */
	CHECK_RESULT(OP_BOOLEAN Flush(ReleaseSeverity severity = ReleaseNo, int max_ms = 0));

	/**
	 * @return number of cached Items
	 */
	int GetItemCount(void) {return m_branch_count;}

	/**
	 * @return direct pointer to the cached file
	 */
	BlockStorage *GetStorage(void) {return &m_storage;}

	/**
	 * Get SearchGroupable group member
	 */
	virtual BlockStorage &GetGroupMember() { return m_storage; }

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
	virtual size_t EstimateMemoryUsed() const;
#endif

protected:
	/**
	 * create a new Item
	 * @param id set disk_id to -id
	 * @param rbranch Item referencing the new Item (if any)
	 * @param rnode element of the referencing Item referencing the new Item (if Item is a kind of array)
	 * @param nur current NUR stamp
	 * @return NULL on error
	 */
	virtual Item *NewMemoryItem(int id, Item *rbranch, int rnode, unsigned short nur) = 0;

	/**
	 * read an item from disk
	 * @param id number of the block to read
	 * @param nur current NUR stamp
	 * @return NULL on error
	 */
	virtual Item *NewDiskItem(OpFileLength id, unsigned short nur) = 0;

	/**
	 * forcibly remove everything from memory without saving
	 */
	void ClearCache(void);

	/**
	 * create a new Item and add it to the list
	 */
	CHECK_RESULT(OP_STATUS Create(Item **t, Item *reference, int ref_node));

	/**
	 * delete the Item from disk
	 */
	void Unlink(Item *t);

	/**
	 * go to Item with given id (both positive or negative)
	 */
	CHECK_RESULT(OP_STATUS Load(Item **t, OpFileLength id));

	/**
	 * just increase the reference counter of b
	 */
	void Load(Item **t, Item *b);

	/**
	 * decrease the reference counter
	 */
	void Release(Item *t);

	Item *m_head;
	int m_branch_count;   // total number if Items in m_head list
	int m_cache_count;    // number of not referenced Items
	int m_journal_count;  // number of Items processed by the PreJournal/Reserve conditions
	int m_max_cache;      // maximum number of not referenced items to store
	BlockStorage m_storage;

protected:
	OpFileLength m_memory_id;
	unsigned short m_NUR_mark;
	ReleaseSeverity m_flush_mode;
	BOOL m_journal_flushed;

#ifdef _DEBUG
	// statistics
public:
	int branches_read;
	int branches_created;
	int branches_cached;
	int branches_written;
	int flush_count;
#endif

#if defined(_DEBUG) || defined(SELFTEST)
public:
	void CheckCache(void)
	{
		Item *tmp;

		for (tmp = m_head; tmp != NULL; tmp = tmp->previous)
		{
			OP_ASSERT(tmp->reference_count == 0);
		}
	}
#endif
};

#endif  // BLOCKSTORAGECACHE_H

