/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#ifndef _NAVIGATION_ITEM_H_
#define _NAVIGATION_ITEM_H_

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/CollectionModel.h"

class NavigationModel;

/**
* Class representing items (folders and separators) used to navigate
* to different real and virtual folders in the CollectionView/ViewPane
*
*/
class NavigationItem : public TreeModelItem<NavigationItem, NavigationModel>
{
public:

	NavigationItem() : m_id(0) { m_id = GetUniqueID(); }
	virtual	~NavigationItem() {}

	/**
	 * Reimplemented from OpTreeModelItem
	 * Returns element data for this object
	 *
	 * @param itemData Data for this object is placed here on return
	 *
	 * @return OP_STATUS value for success of failure
	 */
	virtual OP_STATUS	GetItemData(ItemData* item_data);

	/**
	 * Returns the element type. Must be reimplemented to be of any value.
	 */
	virtual Type GetType() { return OpTypedObject::UNKNOWN_TYPE; }

	/**
	 * Returns the unique element id
	 */
	virtual int GetID() { if (GetCollectionModelItem()) return GetCollectionModelItem()->GetItemID(); else return m_id; }

	/**
	 * Returns name of navigation iteam
	 */
	virtual const OpStringC &GetName() const { return m_name; }

	/**
	 * Returns name associated with image
	 */
	virtual const char* GetImage() const = 0;

	/**
	 * @return TRUE if this is a separator item, else FALSE
	 * Note that there are no user added separators in the NavigationModel
	 */
	virtual BOOL IsSeparator() const { return FALSE; }

	/**
	 * @return TRUE if this is a Folder item, else FALSE
	 */
	virtual BOOL IsFolder() const { return TRUE; }

	/**
	 * @return TRUE if this is the special 'Recent' folder, else FALSE
	 */
	virtual BOOL IsRecentFolder() const { return FALSE;}

	/**
	 * @return TRUE if this is the virtual bookmark root folder, else FALSE
	 */
	virtual BOOL IsAllBookmarks() const { return FALSE;}

	/**
	 * @return TRUE if this is the special 'Deleted' items folder, else FALSE
	 */
	virtual BOOL IsDeletedFolder() const { return FALSE; }

	/**
	 * @return the HotlistModelItem corresponding to this folder, if any.
	 * If this item does not correspond to a HotlistModelItem folder, returns NULL
	 */
	virtual HotlistModelItem* GetHotlistItem(){return NULL;}

	/**
	 * @return the CollectionModelItem corresponding to this folder, if any.
	 * If this item does not correspond to a HotlistModelItem folder, returns NULL
	 */
	virtual CollectionModelItem* GetCollectionModelItem() { return NULL; }
	virtual void GetInfoText(OpInfoText &text);
	virtual OP_STATUS SetName(const uni_char* str) = 0;

	/**
	 * @return the model this NavigationItem represents item(s) from
	 */
	virtual CollectionModel* GetItemModel() { return NULL; }

	/**
	 * @return TRUE if changing the contents of the folder by the user is allowed
	 */
	BOOL IsUserEditable() { return IsAllBookmarks() || IsDeletedFolder() || GetHotlistItem(); }

protected:
	INT32 m_id;
	OpString m_name;

private:
	NavigationItem(const NavigationItem&);
	NavigationItem& operator=(const NavigationItem&);
};


/**
*
* NavigationFolderItem - A FolderItem that does not correspond to a real bookmark folder
*
*/
class NavigationFolderItem : public NavigationItem
{
public:
	NavigationFolderItem(CollectionModel* model, BOOL bookmarks = FALSE) : m_model(model) {}
	NavigationFolderItem() : m_model(0) {}
	virtual	~NavigationFolderItem() {}

	virtual OP_STATUS SetName(const uni_char* str) { return m_name.Set(str); }

	/**
	 * Returns name associated with image
	 */
	virtual const char* GetImage() const { return "Folder"; }

	/**
	 * @return TRUE if this is the virtual bookmark root folder, else FALSE
	 */
	virtual BOOL IsAllBookmarks() const
	{
		return const_cast<NavigationFolderItem*>(this)->GetHotlistItem() == NULL
			&& static_cast<BookmarkModel*>(m_model) == g_hotlist_manager->GetBookmarksModel();
	}

	virtual CollectionModel* GetItemModel() { return m_model; }

private:
	NavigationFolderItem(const NavigationFolderItem&);
	NavigationFolderItem& operator=(const NavigationFolderItem&);

protected:
	CollectionModel* m_model;
};


class NavigationRecentItem : public NavigationFolderItem
{
public:
	NavigationRecentItem(CollectionModel* model) : NavigationFolderItem(model) {}
	NavigationRecentItem() {}

	/**
	 * @return TRUE if this is the virtual bookmark root folder, else FALSE
	 */
	virtual BOOL IsAllBookmarks() const { return FALSE; }

	/**
	 * @return TRUE if this is the special 'Recent' folder, else FALSE
	 */
	virtual BOOL IsRecentFolder() const { return TRUE;}

	/**
	 * Returns name associated with image
	 */
	virtual const char* GetImage() const { return "Recently Added"; }
};


class NavigationSeparator : public NavigationItem
{
public:
	NavigationSeparator(){}
	~NavigationSeparator(){}

	// NavigationItem
	/**
	 * Returns name associated with image
	 */
	virtual const char* GetImage() const { return ""; }
	virtual OP_STATUS SetName(const uni_char* str) { return OpStatus::ERR; }

	/**
	 * @return TRUE if this is a separator item, else FALSE
	 * Note that there are no user added separators in the NavigationModel
	 */
	virtual BOOL IsSeparator() const { return TRUE; }


	/**
	 * @return TRUE if this is a Folder item, else FALSE
	 */
	virtual BOOL IsFolder() const { return FALSE; }

private:
	NavigationSeparator(const NavigationSeparator&);
	NavigationSeparator& operator=(const NavigationSeparator&);
};


/**
* BookmarkNavigationFolder - A NavigationFolder that represents
*          and corresponds to a 'real' BookmarkFolder in the Bookmarks Model
*
*/
class BookmarkNavigationFolder : public NavigationFolderItem
{
public:
	BookmarkNavigationFolder(HotlistModelItem* item, BOOL deleted)
		: NavigationFolderItem(), m_hotlist_item(item), m_is_deleted_folder(deleted)
	{
		if (m_hotlist_item)
			m_model = m_hotlist_item->GetModel();
	}
	~BookmarkNavigationFolder() {}

	// NavigationFolderItem
	/**
	 * Returns name associated with image
	 */
	virtual const char* GetImage() const { return m_is_deleted_folder ? "Trash" : "Folder"; }

	/**
	 * Returns name of navigation iteam
	 */
	virtual const OpStringC &GetName() const { return IsDeletedFolder() ? m_name : m_hotlist_item->GetName(); }

	/**
	 * @return the HotlistModelItem corresponding to this folder, if any.
	 * If this item does not correspond to a HotlistModelItem folder, returns NULL
	 */
	virtual HotlistModelItem* GetHotlistItem() { return m_hotlist_item; }

	/**
	 * @return the CollectionModelItem corresponding to this folder, if any.
	 * If this item does not correspond to a HotlistModelItem folder, returns NULL
	 */
	virtual CollectionModelItem* GetCollectionModelItem() { return GetHotlistItem(); }

	/**
	 * @return TRUE if this is the special 'Deleted' items folder, else FALSE
	 */
	virtual BOOL IsDeletedFolder() const { return m_is_deleted_folder; }

private:
	BookmarkNavigationFolder(const BookmarkNavigationFolder&);
	BookmarkNavigationFolder& operator=(const BookmarkNavigationFolder&);

	HotlistModelItem* m_hotlist_item;
	BOOL m_is_deleted_folder;
};

#endif // _NAVIGATION_ITEM_H
