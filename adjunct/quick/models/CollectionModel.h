/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#ifndef _COLLECTION_MODEL_H_
#define _COLLECTION_MODEL_H_

class CollectionModel;

/**
 * CollectionModelItem
 *
 * Interface to implement for items shown in CollectionView/ViewPane
 * of the Collection Manager
 *
 */
class CollectionModelItem
{
public:
	CollectionModelItem() : m_matched(FALSE) {}
	virtual ~CollectionModelItem() {}

public:
	/**
	 * @return Title to be used in view
	 */
	virtual const uni_char* GetItemTitle() const = 0;

	/**
	 * Fetches url of item
	 *
	 * @param[out] url_str contains string url of item
	 * @param display_url if true, item's display url is fetched, otherwise regular url.
	 */
	virtual OP_STATUS GetItemUrl(OpString& url_str, bool display_url = true) const = 0;

	/**
	 * @return ID of item that can be used to uniquely represent it
	 */
	virtual int GetItemID() const = 0;

	/**
	 * @return TRUE if this item should be excluded from the view
	 *
	 */
	virtual bool IsExcluded() const = 0;

	/**
	 * @return TRUE if item is to be shown as a special folder item
	 */
	virtual bool IsItemFolder() const = 0;

	/**
	 * @return parent of item, if any
	 */
	virtual CollectionModelItem* GetParent() const  { return NULL; }

	/**
	 * @return TRUE if item is deleted
	 */
	virtual bool IsInDeletedFolder() const { return FALSE; }

	/**
	 * @return TRUE if this item is the Deleted Folder
	 */
	virtual bool IsDeletedFolder() { return FALSE; }

	/**
	 * @return the model this item is in
	 */
	virtual CollectionModel* GetItemModel() const = 0;

	/**
	 * Set an item as matched in search or similar
	 */
	virtual void SetMatched(bool match) { m_matched = match; }

	/**
	 * @return TRUE if the item matched in search
	 */
	virtual bool IsMatched() { return m_matched; }

	// m_matched is used to track if an item is match by a current search
	bool m_matched;
};

/**
 * Iterates subtree from GetFolder() or all bookmarks. Deleted items
 * are included only when GetFolder() == Deleted
 */
class CollectionModelIterator
{
public:
	enum SortType
	{
		SORT_BY_CREATED,
		SORT_BY_NAME,
		SORT_NONE
	};

	virtual ~CollectionModelIterator() {}

	/**
	 * Init does any initialization setup needed for iterator
	 */
	virtual void Init() {}

	/**
	 * Resets the iterator
	 */
	virtual void Reset() = 0;

	/**
	 * @return Next item in deflated list
	 */
	virtual CollectionModelItem* Next() = 0;

	/**
	 * @return Model the iterator iterates
	 */
	virtual CollectionModel* GetModel() const = 0;

	/**
	 * @return number of items in an iteration
	 *
	 */
	virtual int GetCount(bool include_folders = FALSE) = 0;

	/**
	 * @return the folder represented, if NULL, iterates whole model
	 */
	virtual CollectionModelItem* GetFolder() const { return NULL; }

	/**
	 * @return TRUE if Iterator it representas same model and folder as this Iterator.
	 */
	virtual bool Equals(CollectionModelIterator* it)
	{
		return it->GetModel() == GetModel() && it->GetFolder() == GetFolder() && it->IsRecent() == IsRecent() && it->GetSortType() == GetSortType();
	}

	/**
	 * @return Position of item in the model it belongs to (position refers to the order items are stored in and 'my order')
	 */
	virtual int GetPosition(CollectionModelItem* item) { return -1; }

	/**
	 * @®eturn TRUE if this iterator iterates the Recent items folder
	 */
	virtual bool IsRecent() const { return false;}

	/**
	 * @return TRUE if this iterator iterates the Deleted folder
	 */
	virtual bool IsDeleted() const { return false; }

	/**
	 * @return SortType
	 */
	SortType GetSortType() const { return m_sort_type; }

protected:
	CollectionModelIterator(SortType sort_type = SORT_NONE) : m_sort_type(sort_type) { }

private:
	SortType m_sort_type;
};

/**
 * CollectionModel
 *
 */
class CollectionModel
{
public:
	virtual ~CollectionModel() {}

	/**
	 * @param folder restricts iteration to given folder item
	 * @return CollectionModelItemIterator for this Model
	 */
	virtual CollectionModelIterator* CreateModelIterator(CollectionModelItem* folder, CollectionModelIterator::SortType, bool recent) = 0;

	/**
	 * @return item with the given ID
	 */
	virtual CollectionModelItem* GetModelItemByID(int id) = 0;

	/**
	 * Delete the item with id 'id'
	 */
	virtual void DeleteItem(int id) { }

	/**
	 * Edit item with id 'id'
	 */
	virtual OP_STATUS EditItem(int id) { return OpStatus::ERR; }
};
#endif // _COLLECTION_MODEL_H_
