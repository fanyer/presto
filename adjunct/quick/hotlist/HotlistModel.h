/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 *
 */

#pragma once

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModel.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/pi/OpDragManager.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"


class FileImage;
class HotlistModelItem;
class HotlistModelListener;
class BookmarkModelItem;
class SeparatorModelItem;
class Image;
class URL;


#define HOTLIST_HASHTABLE


/*************************************************************************
 *
 * HotlistModel
 *
 *
 **************************************************************************/

class HotlistModel
:	public TreeModel<HotlistModelItem>,
	public OpTimerListener,
	public CollectionModel
{

public:

	enum
	{
		FLAG_NONE                   =   0x0,
		FLAG_GUID                   =   0x00001,
		FLAG_NICK                   =	0x00002,		// Set if nick has changed
		FLAG_NAME                   =	0x00004,		// Set if name has changed
		FLAG_URL                    =	0x00008,		// Set if url has changed
		FLAG_DESCRIPTION            =	0x00010,		// Set if description has changed
		FLAG_ICON                   =   0x00020,
		FLAG_CREATED                =	0x00040,		//
		FLAG_VISITED                =	0x00080,		//
		FLAG_CHILDREN               =	0x00100,		//
		FLAG_SIBLINGS               =	0x00200,		//
		FLAG_PERSONALBAR            =   0x00400,
		FLAG_PANEL                  =   0x00800,
		FLAG_SMALLSCREEN            =   0x01000,
		FLAG_TRASH                  =   0x02000,
		FLAG_ADDED                  =   0x04000,
		FLAG_REMOVED                =   0x08000,
		FLAG_MOVED_FROM             =   0x10000,		// dreprecated
		FLAG_MOVED_TO               =   0x20000,
		FLAG_PARTNER_ID             =   0x40000,
		FLAG_UNKNOWN				=	0xFFFFF,	// Set if change unknown (sets all of the flags)
	};

	enum ListRoot
	{
		// These must match the array order in Hotlist manager. Update
		// @ref GetIsRootFolder() if this list is modified
		BookmarkRoot=1,
		ContactRoot,
		NoteRoot,
		UniteServicesRoot,
		MaxRoot=10
	};

	enum DataFormat
	{
		NoFormat=0,
		OperaBookmark=1,
		OperaContact=2,
		NetscapeBookmark=3,
		ExplorerBookmark=4,
		KDE1Bookmark=5,
		KonquerorBookmark=6,
		OperaNote=7,
		OperaGadget=8
	};

	enum DataFlags
	{
		Personalbar=0x01,
		Panel=0x02,
		Trash=0x04
	};

public:
	/**
	 * Initializes an off-screen hotlist treelist
	 */
	HotlistModel(INT32 type, BOOL temp = FALSE );

	/**
	 * Releases any allocated resouces belonging to this object
	 */
	virtual ~HotlistModel();


	/////////////////////////////////////////////////////////////
	// Template methods for subclasses to implement
	////////////////////////////////////////////////////////////

	virtual BOOL EmptyTrash() = 0;

	/**
	 * Returns the current trash folder
	 */
	virtual HotlistModelItem* GetTrashFolder() = 0;

	/**
	 * Sets the trashfolder id. Only to be used when parsing items from file.
	 */
	virtual	void SetTrashfolderId( INT32 id ) = 0;

	/**
	 * Called when the model changes, save changes here
	 */
	virtual void	 OnChange();

	/**
	 * Get the model type (ListRoot)
	 */
	INT32 GetModelType() {return m_type;}

	/**
	 * Adds a listener (notifications about adds, removes etc and save requests)
	 */
	void AddModelListener(HotlistModelListener *ml) {   if (m_model_listeners.Find(ml) == -1) m_model_listeners.Add(ml);}
	OP_STATUS RemoveModelListener(HotlistModelListener *ml) { return m_model_listeners.RemoveByItem(ml);}

	/**
	 * Sets the first suitable item as active node if there is no item
	 * tagged as active.
	 */
	void AddActiveFolder();


	/**
	 * Returns TRUE if this item is descendant of item
	 **/
	BOOL GetIsDescendantOf(HotlistModelItem* descendant, HotlistModelItem* item);

	// SET

	/**
	 * Sets the parent index. Only to be used when parsing items from file.
	 */
	void SetParentIndex( INT32 index )
	{
		m_parent_index = index;
	}

	/**
	 * Sets the active item id. The active item is the item that
	 * contains the focused node in the list.
	 *
	 * @param id The item id
	 * @param save_change Post a save request to disk if TRUE when Id has
	 *        changed.
	 */
	void SetActiveItemId( INT32 id, BOOL save_change );

	/**
	 * Sets or clears the dirty flag of the model. A dirty flag equal to
	 * TRUE indicates that the list should be saved to disk
	 */
	virtual void SetDirty( BOOL state, INT32 timeout_ms=5000 );

	/**
	 * This timeout function is activated when the data in the list
	 * should be saved
	 *
	 * @param timer The timer that triggered the timeout
	 */
	void OnTimeOut(OpTimer* timer);


	/**
	 * Returns the current parent index
	 */
	INT32 GetParentIndex() const { return m_parent_index; }

	// Get'ers

	/**
	 * Returns the list of index entries that represent the
	 * node at 'index' and any children at 'index'
	 *
	 * @param index The start index
	 * @param index_list List of matched items
	 * @param test_sibling Test siblings of 'index' when TRUE
	 * @param probe_depth Indicates how many levels to test. -1 will go through the entire sub tree from the position.
	 * @param type The item type to match
	 */
	void GetIndexList( INT32 index, OpINT32Vector& index_list, BOOL test_sibling, INT32 probe_depth, OpTypedObject::Type type );

	/**
	 * Returns the parent folder of the node if such exist
	 *
	 * @param id The node id
	 *
	 * @return The parent folder or 0
	 */
	HotlistModelItem *GetParentFolder( INT32 id );
	HotlistModelItem *GetParentFolderByIndex( INT32 id );

	/**
	 * Returns TRUE if the node at index is a folder
	 *
	 * @param index Index of node that may be a folder.
	 *
	 * @return TRUE if the node at index is a folder, otherwise FALSE
	 */
	BOOL IsFolderAtIndex( INT32 index );

	/**
	 * Returns TRUE if the node is a child (or grandchild etc) of
	 * the parent node.
	 *
	 * @param parent_index Parent node
	 * @param candidate_child_index Child node to investigate
	 *
	 * @return TRUE if the node is a child, otherwise FALSE
	 */
	BOOL IsChildOf( INT32 parent_index, INT32 candidate_child_index );

	// GetBy ...

	/**
	 * Returns the first bookmark item that matches the URL
	 *
	 * @param address The URL to locate
	 *
	 * @param A pointer to the first item in the list that match or 0 on
	 *        no match
	 */
	HotlistModelItem *GetByURL( const URL &document_url );
	HotlistModelItem* GetByName( const OpStringC& name );


	/**
	 * Returns the first item that holds the same nick name (short name)
	 * as the argument
	 *
	 * @param nick The nick name to locate
	 *
	 * @param A pointer to the first item in the list that match or NULL on
	 *        no match
	 */
	HotlistModelItem *GetByNickname(const OpStringC &nick);

	/**
	 * Returns the first item with the given unique ID
	 *
	 * @param unique_id The 128-bit unique id to locate
	 *
	 * @param A pointer to the first item in the list that match or NULL on
	 *        no match
	 */
	HotlistModelItem *GetByUniqueGUID(const OpStringC &unique_guid);

	HotlistModelItem *GetByUniqueGUID(const OpStringC8 &unique_guid);

	/**
	 * Returns the identifier of the active item or -1 if there are no
	 * active item.
	 */
	INT32 GetActiveItemId() const { return m_active_folder_id; }

	/**
	 * Returns the dirty flag state
	 */
	BOOL IsDirty() const { return m_is_dirty; }

	/**
	 * Returns a HotlistModelItem object if the item type is BOOKMARK_TYPE,
	 * CONTACT_TYPE, NOTE_TYPE or FOLDER_TYPE
	 */
	static HotlistModelItem *GetItemByType( OpTreeModelItem *item );

	/**
	 * Returns TRUE if the 'id' represents a root folder. The known root
	 * folders are listed in enum ListRoot
	 *
	 * @param id The node identifier
	 *
	 * @return See above
	 */
	static BOOL GetIsRootFolder( INT32 id );


	// Functions to move items in model
	/**
	 * Deletes the entire tree and its contents
	 */
	virtual void Erase();

#ifdef HOTLIST_USE_MOVE_ITEM
	INT32 MoveItem( HotlistModel* from_model, INT32 from_index, HotlistModelItem* to, InsertType insert_type);
#endif

	// TreeModel
	INT32 CompareItems( int column, OpTreeModelItem* item1, OpTreeModelItem* item2 );

	/**
	 *
	 * @return TRUE if model contains 'normal' items (non-folders and non-separators)
	 **/
	BOOL HasItems();
	BOOL HasItems(INT32 item_type);
	BOOL HasItemsOrFolders(INT32 item_type);

	/**
	 * Makes a sorted list of items.
	 *
	 * @param index The index of the item where the lists starts. All
	 *        siblings of the item are included in the list
	 * @param start_at This parameter can be used to clip the list
	 *        in the front. start_at=3 means that the list is shifted
	 *        three elemets towards the beginning before it is returned
	 * @param column The column, or sort key, that determines how to
	 *        sort.
	 * @param ascending if FALSE the sorted list will be reversed.
	 * @param The list that will be populated.
	 *
	 */
	void GetSortedList( INT32 index, INT32 start_at, INT32 column, BOOL ascending, OpVector<HotlistModelItem>& items);

#ifndef HOTLIST_USE_MOVE_ITEM

	/**
	 * Tell model we are moving items within it
	 *
	 * This means notifications to listeners will about internal adds and deletes
	 * will not be sent, instead we send OnHotlistItemChanged notifications
	 *
	 */
	void SetIsMovingItems(BOOL state) {
		m_is_moving_items = state;
	}
	BOOL GetIsMovingItems() { return m_is_moving_items; }
#endif

	// Broadcast changes to listeners
	virtual void BroadcastHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flags = FLAG_UNKNOWN);
	virtual void BroadcastHotlistItemTrashed(HotlistModelItem* item);
	virtual void BroadcastHotlistItemRemoving(HotlistModelItem* item, BOOL removed_as_child);
	virtual void BroadcastHotlistItemAdded(HotlistModelItem* item, BOOL moved_as_child);
	virtual void BroadcastHotlistItemMovedFrom(HotlistModelItem* hotlist_item);

	BOOL GetIsTemporary() { return m_temporary; }

	void ItemUnTrashed(HotlistModelItem* item);

#ifdef HOTLIST_HASHTABLE
	/**
	 * Adds a hotlistitem to the hash table which speeds up searching.
	 *
	 * @param HotlistItem to add
	 *
	 */
	OP_STATUS AddUniqueGUID(HotlistModelItem* item);
	/**
	 * Removes a hotlistitem from the hash table which speeds up searching.
	 *
	 * @param HotlistItem to remove
	 *
	 */
	OP_STATUS RemoveUniqueGUID(HotlistModelItem* item);
#endif // HOTLIST_HASHTABLE

	///////////// SYNC RELATED ////////////////////////////

	/**
	 * Tell model we are moving items within it
	 */
	void SetIsSyncing(BOOL is_syncing) { m_is_syncing = is_syncing; }
	BOOL GetIsSyncing() { return m_is_syncing; }

	// functionality needed for new/open bookmark file
	// Is this model (and adr file) synced
	BOOL GetModelIsSynced()
	{
		return m_model_is_synced;
	}

	void SetModelIsSynced(BOOL is_synced)
	{
		m_model_is_synced = is_synced;
	}

	// Functions used for state handling

	/**
	 * Returns the current node. This is only used during install/import
	 */
	HotlistModelItem* GetCurrentNode() const { return m_current_item; }

	/**
	 * Clears the current node. Should be done before install/import
	 */
	void ResetCurrentNode() { m_current_item = 0; }

#ifdef WEBSERVER_SUPPORT
	UniteServiceModelItem* GetRootService() { return m_root_service; }
	void SetRootService(UniteServiceModelItem* root) { m_root_service = root; }
#endif

	// CollectionModel
	virtual CollectionModelIterator* CreateModelIterator(CollectionModelItem* folder, CollectionModelIterator::SortType, bool recent);
	virtual CollectionModelItem* GetModelItemByID(int id) { return GetItemByID(id); }

private:
	/**
	 * Sort function that are called by @ref GetSortedList
	 *
	 * @param item1 First element to be compared (** pointer to HotlistModelItem)
	 * @param item2 Second element to be compared (** pointer to HotlistModelItem)
	 *
	 * @return  An integer less than, equal to, or greater than zero if the first
	 *          argument is considered to be respectively less than, equal to, or
     *          greater than the second.
	 */
	static INT32 CompareHotlistModelItem( const void *item1, const void *item2);
	static INT32 CompareHotlistModelItem( const void *item1, const void *item2, INT32 model_type);
	static INT32 CompareHotlistModelItem( int column, HotlistModelItem* hli1, HotlistModelItem* hli2 , INT32 model_type);
	static INT32 CompareHotlistModelItem( int column, HotlistModelItem* hli1, HotlistModelItem* hli2 );

protected:
	HotlistModelItem* m_current_item; // Only to be used during install/import
	BOOL  m_is_dirty;
#ifndef HOTLIST_USE_MOVE_ITEM
	BOOL  m_is_moving_items;
	BOOL  m_bypass_trash_on_delete;
#endif
	BOOL  m_is_syncing;       // TRUE if is processing items sent from server
	BOOL  m_model_is_synced;  // TRUE if this model is being synced (needed for new/open bookmark file)

	INT32 m_parent_index;
	INT32 m_current_index;      //
	INT32 m_active_folder_id;
	INT32 m_type;
	OpTimer* m_timer;

	// Model Listeners get notifications from Hotlist Model (distinct from TreeModelListeners),
	// currently; History, HotlistManager (uses only OnSaveRequest), Sync
	OpVector<HotlistModelListener> m_model_listeners;

#ifdef HOTLIST_HASHTABLE
	// Hash table for fast GUID lookups
	OpStringHashTable<HotlistModelItem> m_unique_guids;
#endif // HOTLIST_HASHTABLE

	BOOL m_temporary;
	BOOL m_move_as_child;

	static INT32 m_sort_column;
	static BOOL m_sort_ascending;

#ifdef WEBSERVER_SUPPORT
	UniteServiceModelItem* m_root_service;
#endif

};

/*************************************************************************
 *
 * HotlistModelListener
 *
 * HotlistModel is a treemodel, notifications needed for the view
 * goes through the TreeModel::Listener
 *
 * The HotlistModelListener is for clients that need information
 * about the internal state of the hotlist model
 *
 **************************************************************************/

class HotlistModelListener
{
public:

	HotlistModelListener() {}
	virtual ~HotlistModelListener() {}

	/**
	 * Will be called when the model contains changes that need
	 * to be saved
	 *
	 * @param The model the must be saved.
	 *
	 * @return Returns TRUE on success (data saved), otherwise FALSE
	 */
	virtual BOOL OnHotlistSaveRequest( HotlistModel *model ) { return FALSE; }

	/**
	 *
	 *
	 **/
	virtual void OnHotlistItemAdded(HotlistModelItem* item) = 0;
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) = 0;
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child) = 0;

	/**
	 * Called when an item is moved to trash
	 *
	 **/
	virtual void OnHotlistItemTrashed(HotlistModelItem* item) = 0;

	/**
	 * Called when an item is moved out of trash into 'normal' bookmarks
	 * Note that in this case listeners will also receive an
	 * OnHotlistItemChanged notification
	 *
	 **/
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) = 0;


	/**
	 * Called when an item is (re)moved from a position.
	 * Needed for Link
	 * We should be able to remove this and rely on the FLAGs later
	 *
	 **/
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) = 0;

	virtual void OnHotlistModelDeleted() {}
};

class ViewModel : public TreeViewModel
{
public:
	ViewModel() : TreeViewModel(), m_is_trash_folder_or_folder_in_trash(FALSE) {}

	// TreeViewModel
	void Init();

	void SetModelParameters(INT32 start_idx, INT32 count);
	virtual TreeViewModelItem *	GetItemByModelItem(OpTreeModelItem* item);
	void SetIsTrashFolderOrFolderInTrashFolder() { m_is_trash_folder_or_folder_in_trash = TRUE; }

private:
	BOOL m_is_trash_folder_or_folder_in_trash;
	INT32 m_start_idx;
	INT32 m_count;
};


class HotlistModelIterator
	: public CollectionModelIterator
	, public OpTreeModel::SortListener
{
	friend class HotlistModel;

public:
	virtual void Reset();
	virtual CollectionModelItem* Next();
	virtual CollectionModel* GetModel() const {  return m_model; }
	virtual int GetCount(bool include_folders = false);
	virtual CollectionModelItem* GetFolder() const { return m_folder; }
	virtual INT32 OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1);
	void Init();
	int GetPosition(CollectionModelItem* item);

	virtual bool IsRecent() const { return m_recent; }
	virtual bool IsDeleted() const { return m_model ? static_cast<HotlistModelItem*>(GetFolder()) == m_model->GetTrashFolder() : false; }

	~HotlistModelIterator()
	{
		if (m_model)
			m_sorted_model.SetModel(NULL);
	}

protected:
	HotlistModelIterator(HotlistModel* model, HotlistModelItem* folder, CollectionModelIterator::SortType sort_type, bool recent = false) :
		 CollectionModelIterator(sort_type),
			 m_folder(folder),
			 m_model(model),
			 m_sorted_index(0),
			 m_recent_count(10),
			 m_recent_num(0),
			 m_recent(recent)
		 {
		 }

private:
	// If restricted to a folder
	HotlistModelItem* m_folder;
	HotlistModel* m_model;

	// TYPE_DEFLATED WITH SORTING
	int m_sorted_index;
	OpINT32Vector m_sorted_vector;
	ViewModel m_sorted_model;

	// RECENT
	int m_recent_count;
	int m_recent_num;
	bool m_recent;
};
