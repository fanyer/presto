/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _BOOKMARK_MODEL_H_
#define _BOOKMARK_MODEL_H_

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/models/Bookmark.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/models/BookmarkFolder.h"

class OpTreeView;
class BookmarkIniStorage;
class BookmarkAdrStorage;

class BookmarkItemData
{
public:
	BookmarkItemData();
	BookmarkItemData(const BookmarkItemData&);

	OpString name;
	OpString url;
	OpString display_url;
	OpString8 image;
	OpString folder;
	const char *direct_image_pointer;
	OpString description;
	OpString shortname;
	OpString created;
	OpString visited;
	OpString target;
	OpString partner_id;
	INT32 personalbar_position; 
	INT32 panel_position; 
	OpString unique_id;
	BOOL move_is_copy;
	BOOL separators_allowed;
	BOOL subfolders_allowed;
	BOOL deletable;
	BOOL active;
	BOOL expanded;
	INT32 max_count;
	BOOL small_screen;
	BookmarkFolderType type;
};




// When drag n drop a bookmark UI use an "onto" item and an InsertType to locate
// the new position of the item, while core use previous item and parent item.
// This class convert the UI concept to Core's
class CoreBookmarkPos
{
public:
	CoreBookmarkPos(HotlistModelItem* onto, DesktopDragObject::InsertType type);

	BookmarkItem* previous;
	BookmarkItem* parent;

	HotlistModelItem* Previous();
	HotlistModelItem* Parent();
};

/*********************************************************************
 *
 * BookmarkModel
 *
 *
 *********************************************************************/

class BookmarkModel : public HotlistModel,
					  public BookmarkManagerListener,
					  public FavIconManager::FavIconListener

{
	friend class OpPersonalbar;

 public:
	BookmarkModel();
	~BookmarkModel();

	OP_STATUS Init();
	OP_STATUS InitStorageProvider(const OpString &path);
	BOOL Loaded(){return m_loaded;}

	// subclassing OpTreeModel
	INT32 GetColumnCount();
	OP_STATUS GetColumnData(ColumnData* column_data);

	// subclassing HotlistModel
	virtual TrashFolder* GetTrashFolder();
	virtual void Erase();
	virtual void SetDirty( BOOL state, INT32 timeout_ms=5000 );

	/**
	 * Sets the trashfolder id. Only to be used when parsing items from file.
	 */
	virtual	void SetTrashfolderId( INT32 id ) {}
	
	/**
	 * Called when the model changes, save changes here
	 */
	virtual void	 OnChange();

	// FavIconManager::FavIconListener ---------------------------------------------
	virtual void OnFavIconAdded(const uni_char* document_url, const uni_char* image_path);
	virtual void OnFavIconsRemoved() {}
	
	// --- BookmarkManagerListener ---------------------------------
	virtual void OnBookmarksSaved(OP_STATUS ret, UINT32 count) ;
	virtual void OnBookmarksLoaded(OP_STATUS ret, UINT32 count) ;
	virtual void OnBookmarkAdded(BookmarkItem* item);
	virtual void OnBookmarksSynced(OP_STATUS ret) {}

	void OnBookmarkRemoved(HotlistModelItem* item);

	BOOL DeleteItem(HotlistModelItem* item, BOOL real_delete = FALSE);
	BOOL EmptyTrash();

	// ---------------------------------------------------
	// Move? Util funcs actually.

	void GetInfoText( const BookmarkItemData& item_data, OpInfoText &text );

	/**
	 * Returns a bookmark list of all items in the model except those in the trash folder
	 * The list is not sorted
	 * 
	 * @param The list that will be populated.
	 */
	void GetBookmarkItemList(OpVector<Bookmark>& items);

	// Update the UI as core changes
	BOOL OnMoveItem(CoreBookmarkWrapper* item_to_move);

	/**
	 * Checks if there already is an item with the same url
	 *
	 * @param item 
	 *
	 * @return TRUE if there is already an item with the given url
	 *         
	 */
	BOOL IsDuplicateURL(Bookmark& item);
	BOOL IsDuplicateURL(HotlistModelItem *item, HistoryPage** history_item, Bookmark** bookmark);
	BOOL IsDuplicateURL(const OpStringC& url, HistoryPage** history_item);
	BOOL IsDuplicateURLInSameFolder(const OpStringC& url, HotlistModelItem* folder, Bookmark** bookmark);
	BOOL IsDuplicateURLInSameFolder(HotlistModelItem* item, HotlistModelItem* folder, Bookmark** bookmark);

	// get the first by url
	// @param in_trash also return the trashed item if true 
	HotlistModelItem* GetByURL(const uni_char* url, bool trashed = true, bool use_display_url = false);

	HotlistModelItem* GetByCoreItem(BookmarkItem* item);

	// CollectionModel
	virtual void DeleteItem(INT32 id);
	virtual OP_STATUS EditItem(INT32 id);

private:
	// Not implemented
	BookmarkModel(const BookmarkModel&);
	BookmarkModel& operator=(const BookmarkModel& item);

	BOOL AddItem(HotlistModelItem* item);

	void BroadcastItemMoved(HotlistModelItem* item);

	TrashFolder*			m_trash_folder;
	BookmarkAdrStorage*		m_provider;
	BookmarkIniStorage*		m_ini_provider;
	BOOL					m_initialized;
	BOOL					m_loaded;
	BOOL					m_delete_old_ini_bookmark;
};

#endif // BOOKMARK_MODEL_H
