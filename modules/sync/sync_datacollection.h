/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_DATACOLLECTION_H_INCLUDED_
#define _SYNC_DATACOLLECTION_H_INCLUDED_

#include "modules/sync/sync_dataitem.h"
#include "modules/util/OpHashTable.h"

class OpSyncParser;

class OpSyncDataCollection : private Head
{
public:
	OpSyncDataCollection() {}
	virtual ~OpSyncDataCollection();

	OpSyncDataItem*	First() const { return static_cast<OpSyncDataItem*>(Head::First()); }
	/**
	 * Searches in this collection for an OpSyncDataItem with the same primary
	 * key and value as specified. If there is no such item in this collection,
	 * NULL is returned.
	 * @param type is the base-type of the OpSyncDataItem to find (see
	 *  OpSyncDataItem::GetBaseType()).
	 * @param key is the primary key to search.
	 * @param value is the value of the specified primary key to find.
	 * @return the found OpSyncDataItem instance or 0 if no item was found.
	 */
	virtual OpSyncDataItem* FindPrimaryKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value);
	UINT GetCount() const { return Cardinal(); }
	UINT Cardinal() const { return Head::Cardinal(); }
	BOOL IsEmpty() const { return Head::Empty(); }
	BOOL HasItems() const { return !IsEmpty(); }
	void Clear() {
		while (HasItems())
			RemoveItem(First());
	}

	void AppendItems(OpSyncDataCollection* items) {
		if (items && items != this)
			while (items->First())
				AddItem(items->First());
	}

	/**
	 * Adds the specified instance to the end of this OpSyncDataCollection and
	 * increases the item's reference counter. If the item is currently in some
	 * other collection, it is removed from that other collection.
	 */
	void AddItem(OpSyncDataItem* item) {
		EnsureRefCountAndRemovedFromCollection(item);
		item->Into(this);
	}

	/**
	 * Adds the specified instance to the start of this OpSyncDataCollection and
	 * increases the item's reference counter. If the item is currently in some
	 * other collection, it is removed from that other collection.
	 */
	void AddFirst(OpSyncDataItem* item) {
		EnsureRefCountAndRemovedFromCollection(item);
		item->IntoStart(this);
	}

	/**
	 * Adds the specified item after the OpSyncDataItem after_this into this
	 * OpSyncDataCollection. The reference counter of the specified item is
	 * increased. If the specified item is currently in some other collection,
	 * it is removed from that other collection.
	 * @param item is the OpSyncDataItem to add after after_this.
	 * @param after_this is the OpSyncDataItem after which the specified item is
	 *  added. It must be in this OpSyncDataCollection.
	 */
	void AddAfter(OpSyncDataItem* item, OpSyncDataItem* after_this) {
		OP_ASSERT(item && after_this && HasLink(after_this));
		EnsureRefCountAndRemovedFromCollection(item);
		item->Follow(after_this);
	}

	/**
	 * Adds the specified item before the OpSyncDataItem before_this into this
	 * OpSyncDataCollection. The reference counter of the specified item is
	 * increased. If the specified item is currently in some other collection,
	 * it is removed from that other collection.
	 * @param item is the OpSyncDataItem to add before before_this.
	 * @param before_this is the OpSyncDataItem before which the
	 *  specified item is added. It must be in this OpSyncDataCollection.
	 */
	void AddBefore(OpSyncDataItem* item, OpSyncDataItem* before_this) {
		OP_ASSERT(item && before_this && HasLink(before_this));
		EnsureRefCountAndRemovedFromCollection(item);
		item->Precede(before_this);
	}

	void RemoveItem(OpSyncDataItem* item) {
		OP_ASSERT(item && HasLink(item));
		if (item && HasLink(item))
		{
			item->Out();
			OnItemRemoved(item);
			item->DecRefCount();
		}
	}

protected:
	virtual void OnItemAdded(OpSyncDataItem* item) {}
	virtual void OnItemRemoved(OpSyncDataItem* item) {}

private:
	/**
	 * Ensures that the ref-count for the specified item is set correctly (for
	 * this item being in this collection) and that the item is removed from its
	 * current OpSyncDataCollection.
	 *
	 * So after calling this method !item->InList() is true.
	 *
	 * This method is called before inserting the specified item into this
	 * collection.
	 *
	 * If the item was not in this collection, OnItemAdded() is called to allow
	 * an OpSyncDataHashedCollection to hash the item (if it was already in the
	 * queue, the hash-entry already exists).
	 */
	void EnsureRefCountAndRemovedFromCollection(OpSyncDataItem* item) {
		if (HasLink(item))
			/* This item is already in this collection, so no need to change
			 * the ref-count, only remove the item from this collection, it will
			 * be inserted again into this collection by the caller. */
			item->Out();
		else
		{	/* Increase the ref-count for the item, because it will be
			 * referenced by this collection (the caller will insert it): */
			item->IncRefCount();
			if (item->InList())
				/* And ensure to remove the item from the collection it is
				 * currently in: */
				item->GetList()->RemoveItem(item);
			/* Allow OpSyncDataHashedCollection to add the item to the
			 * hash-table as well: */
			OnItemAdded(item);
		}
	}
};

class OpSyncDataItemHashed;

/**
 * This class extends an OpSyncDataCollection with a hash-table.
 *
 * The OpSyncDataItem that are added to the OpSyncDataCollection are in addition
 * also stored in string-hash where the type and the primary key are used as a
 * unique hash key. Storing the items in a hash-table requires more memory, but
 * it improves the performance of finding an item in the collection by its
 * primary key significantly (see FindPrimaryKey()). If there is not enough
 * memory to use a hash-table, the hash-table is disabled and finding an item
 * will still work with O(n) (where n is the size of the collection).
 *
 * Using this class is especially useful on processing a dirty sync when a lot
 * of items from the server and the client are merged (see
 * OpSyncCoordinator::MergeDirtySyncItems()).
 */
class OpSyncDataHashedCollection : public OpSyncDataCollection
{
public:
	OpSyncDataHashedCollection() : m_hash_table(0), m_oom(false) {}
	virtual ~OpSyncDataHashedCollection();

	/**
	 * Searches in this collection for an OpSyncDataItem with the same primary
	 * key and value as specified. If there is no such item in this collection,
	 * NULL is returned.
	 * @param type is the base-type of the OpSyncDataItem to find (see
	 *  OpSyncDataItem::GetBaseType()).
	 * @param key is the primary key to search.
	 * @param value is the value of the specified primary key to find.
	 * @return the found OpSyncDataItem instance or 0 if no item was found.
	 */
	virtual OpSyncDataItem* FindPrimaryKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value);

protected:
	/**
	 * Is called if the specified OpSyncDataItem is added to this collection.
	 * This implementation adds the item also to the internal hash-table such
	 * that FindPrimaryKey() has a fast access to that item.
	 * @param item is the OpSyncDataItem that was added.
	 */
	virtual void OnItemAdded(OpSyncDataItem* item);
	/**
	 * Is called if the specified OpSyncDataItem is removed from this
	 * collection.
	 * This implementation remove the item also from the internal hash-table.
	 * @param item is the OpSyncDataItem that was removed.
	 */
	virtual void OnItemRemoved(OpSyncDataItem* item);

private:
	/**
	 * Calculates the hash-key for the specified OpSyncDataItem. The hash-key is
	 * used to add the OpSyncDataItem to the internal hash-table or to find the
	 * item in the internal hash-table.
	 * @param item is the OpSyncDataItem for which to get the hash-key.
	 * @param hash_key receives on success the hash-key for the specified item.
	 */
	OP_STATUS GetHashKey(const OpSyncDataItem* item, OpString& hash_key) const;
	/**
	 * Calculates the hash-key for an OpSyncDataItem with the specified type,
	 * primary key and value. The hash-key is used to add an OpSyncDataItem to
	 * the internal hash-table or to find an item with this data in the internal
	 * hash-table.
	 * @param type is the OpSyncDataItem::DataItemType of the OpSyncDataItem for
	 *  which to get the hash-key.
	 * @param key is the primary key of the OpSyncDataItem for which to get the
	 *  hash-key.
	 * @param value is the primary key's value of the OpSyncDataItem for which
	 *  to get the hash-key.
	 * @param hash_key receives on success the hash-key for the specified item.
	 */
	OP_STATUS GetHashKey(OpSyncDataItem::DataItemType type, const OpStringC& key, const OpStringC& value, OpString& hash_key) const;

	void ClearHashTable();

	/**
	 * The internal hash-table that keeps all added OpSyncDataItem instances.
	 * The instance is created on adding the first item.
	 */
	OpStringHashTable<OpSyncDataItemHashed>* m_hash_table;

	/**
	 * Is called when OnItemAdded() fails to add the OpSyncDataItem to the
	 * internal hash table. In this case the internal hash-table will be
	 * disabled and no longer used. Finding an OpSyncDataItem is still possible,
	 * but not as fast as with the hash-table.
	 */
	void SetOOM();

	/** Initialised with false and set to true (by SetOOM()) if there is not
	 * enough memory to maintain the hash-table. If this attribute is true,
	 * FindPrimaryKey() cannot use the hash-table to find an item. */
	bool m_oom;
};

class OpSyncCollection : private AutoDeleteHead
{
public:
	OpSyncCollection() {}
	OpSyncItem*	Last() const { return static_cast<OpSyncItem*>(Head::Last()); }
	OpSyncItem*	First() const { return static_cast<OpSyncItem*>(Head::First()); }
	UINT GetCount() const { return Head::Cardinal(); }
	UINT Cardinal() const { return Head::Cardinal(); }
	BOOL IsEmpty() const { return Head::Empty(); }
	BOOL HasItems() const { return !IsEmpty(); }
	void Clear() { Head::Clear(); }
	void AppendItems(OpSyncCollection* items) { Head::Append(items); }
	void AddItem(OpSyncItem* item) { item->Into(this); }
	void AddFirst(OpSyncItem* item) { item->IntoStart(this); }

	void Remove(OpSyncItem* item) {
		OP_ASSERT(HasLink(item));
		if (HasLink(item))
			item->Out();
	}
};

#endif //_SYNC_DATACOLLECTION_H_INCLUDED_
