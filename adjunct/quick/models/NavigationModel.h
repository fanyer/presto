/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#ifndef NAVIGATION_MODEL_H
#define NAVIGATION_MODEL_H

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/models/NavigationItem.h"
#include "adjunct/quick/widgets/OpCollectionView.h"

/**
 * Model containing the navigation items for all collection models;
 * that is, folders and virtual folders like Recent, Deleted, All bookmarks.
 */
class NavigationModel
	: public TreeModel<NavigationItem>
	, public OpTreeModel::Listener
	, public HotlistModelListener
	, public DesktopBookmarkListener
{
private:
	NavigationModel(const BookmarkModel&);
	NavigationModel& operator=(const BookmarkModel&);

public:
	NavigationModel();
	virtual ~NavigationModel();

	/**
	 * This class allows observer(listener) to listen change in state 
	 * of subject(NavigationModel).
	 */
	class NavigationModelListener
	{
	public:
		/**
		 * Event triggers after an item is added.
		 * @param item which is just added into the model
		 */
		virtual void ItemAdded(CollectionModelItem* item) = 0;

		/**
		 * Event triggers after an item is removed from a 
		 * model or folder
		 * @param item which is just removed from the model
		 * or folder
		 */
		virtual void ItemRemoved(CollectionModelItem* item) = 0;

		/**
		 * Event triggers after an item is modified.
		 * @param item which is just modified
		 * @param changed_flag identifies type of change has 
		 * taken.
		 */
		virtual void ItemModified(CollectionModelItem* item, UINT32 changed_flag) = 0;

		/**
		 * Event triggers after an item is moved.
		 * @param item which is just moved from one folder
		 * to another
		 */
		virtual void ItemMoved(CollectionModelItem* item) = 0;

		/**
		 * Event triggers after folder is moved
		 * @param item identifies moved folder
		 */
		virtual void FolderMoved(NavigationItem* item) = 0;
	};

	/**
	 * Initializes NavigationModel. Special folders are set up and NavigationFolders
	 * created for the bookmark model folders)
	 */
	OP_STATUS Init();

	/**
	 * @return Recent Items folder
	 */
	NavigationItem* GetRecentFolder() const { return m_recent; }
	
	/**
	 * @return Deleted Items folder
	 */
	NavigationItem* GetDeletedFolder() const { return m_trash; }

	/**
	 * @return Bookmark Root Folder
	 */
	NavigationItem* GetBookmarkRootFolder() const { return m_bookmarks; }

	/**
	 * Remove a folder from the model
	 */
	void RemoveBookmarkFolder(HotlistModelItem* item);

	/** 
	 * Allows subject(this class) to notify of change in state to 
	 * register observer.
	 * @param listener identifies observer
	*/
	void SetListener(NavigationModelListener* listener) { m_navigation_model_listener = listener; }

	// OpTreeModel
	virtual INT32				GetColumnCount() { return 1; }
	virtual OP_STATUS			GetColumnData(ColumnData* column_data);

	// OpTreeModel::Listener
	virtual	void				OnItemAdded(OpTreeModel* tree_model, INT32 item) { }
	virtual	void				OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) { }
	virtual	void				OnItemRemoving(OpTreeModel* tree_model, INT32 item) { }
	virtual	void				OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) { }
	virtual	void				OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) { }
	virtual void				OnTreeChanging(OpTreeModel* tree_model ) { }
	virtual void				OnTreeChanged(OpTreeModel* tree_model) { }
	virtual void				OnTreeDeleted(OpTreeModel* tree_model) { m_bookmark_model = NULL; }

	// TreeModel
	virtual INT32				CompareItems( int column, OpTreeModelItem* item1, OpTreeModelItem* item2 );
	virtual BOOL				IsDraggable(NavigationItem* item) { return item->GetHotlistItem() != NULL; }
	virtual BOOL				IsRecent(NavigationItem* item ) { return item == GetRecentFolder(); }

	// HotlistModelListener methods
	virtual void				OnHotlistItemAdded(HotlistModelItem* item);
	virtual void				OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN);
	virtual void				OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	virtual void				OnHotlistItemTrashed(HotlistModelItem* item);
	virtual void				OnHotlistItemMovedFrom(HotlistModelItem* item) {}
	virtual void				OnHotlistItemUnTrashed(HotlistModelItem* item);

	// DesktopBookmarkListener
	virtual void				OnBookmarkModelLoaded();

private:
	OP_STATUS					InitBookmarkFolders();
	OP_STATUS					AddFolder(NavigationItem* item, const OpStringC& name);
	OP_STATUS					AddRecentFolder();
	OP_STATUS					AddSeparator(BOOL is_first);
	OP_STATUS					AddBookmarksFolder();
	OP_STATUS					AddDeletedFolder();
	OP_STATUS					AddBookmarkFolderSorted(HotlistModelItem* folder);
	OP_STATUS					AddBookmarkFolder(HotlistModelItem* folder);
	void						AddSubtree(HotlistModelItem* item);
	void						AddBookmarkFolderLast(NavigationItem* item);
	NavigationItem*				GetNavigationItemFromHotlistItem(HotlistModelItem* item);

	NavigationItem*						m_recent;
	NavigationItem*						m_bookmarks;
	NavigationItem*						m_trash;
	BookmarkModel*						m_bookmark_model;
	NavigationItem*						m_first_separator;
	NavigationModelListener*			m_navigation_model_listener;
};

#endif // NAVIGATION_MODEL_H
