/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas Oct 2005
 */
#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/HistoryModelItem.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

#include "modules/history/history_enums.h"
#include "modules/history/OpHistoryModel.h"

#define HISTORY_BOOKMARK_VIEW

class DesktopHistoryModelListener
{
public:
    virtual void OnDesktopHistoryModelItemDeleted(HistoryModelItem* item) = 0;
};

//__________________________ HISTORY MODEL -----------------------------------------

class DesktopHistoryModel 
	: public DesktopManager<DesktopHistoryModel>,
	  public TreeModel<HistoryModelItem>,  
	  public OpTreeModel::SortListener,
	  public OpHistoryModel::Listener,
	  public HotlistModelListener,
	  public FavIconManager::FavIconListener,
	  public DesktopWindowCollection::Listener
{

public:

    //Modes that history can be in (defines the order and elements to be in the vector)
    enum History_Mode
	{
	    TIME_MODE, 
	    SERVER_MODE, 
	    OLD_MODE,  
	    PROTOCOL_MODE,
#ifdef HISTORY_BOOKMARK_VIEW
		BOOKMARK_MODE,
#endif //HISTORY_BOOKMARK_VIEW
		NUM_MODES
	};

	DesktopHistoryModel();
	~DesktopHistoryModel();

/* ------------------ Implementing DesktopManager interface --------------------------*/
	OP_STATUS Init();

/* ------------------ Implementing OpTreeModel interface --------------------------*/
    virtual OP_STATUS GetColumnData(ColumnData* column_data);
    virtual INT32     GetColumnCount() { return 4; }
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS GetTypeString(OpString& type_string);
#endif
/* --------------------------------------------------------------------------------*/

/* ------------------ Implementing HistoryModel::Listener interface -----------*/
	virtual	void      OnItemAdded(HistoryPage* core_item, BOOL save_session);
	virtual	void      OnItemRemoving(HistoryItem* core_item);
	virtual void      OnItemDeleting(HistoryItem* core_item);
	virtual	void      OnItemMoving(HistoryItem* core_item);
	virtual void      OnModelChanging();
	virtual void      OnModelChanged();
	virtual void      OnModelDeleted();
/* --------------------------------------------------------------------------------*/

/* ------------------ Implementing HotlistModel::Listener interface ---------------*/
	// Called by the model when interesting things happen to the hotlist
	// items.
	virtual void OnHotlistItemAdded(HotlistModelItem* item);
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN);
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	virtual void OnHotlistItemTrashed(HotlistModelItem* item);
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item);
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}


/* --------------------------------------------------------------------------------*/

/* ------------------ Implementing FavIconManager::FavIconListener interface ------*/
	virtual void OnFavIconAdded(const uni_char* document_url, const uni_char* image_path);
	virtual void OnFavIconsRemoved();
/* --------------------------------------------------------------------------------*/

/* ------------------ Implementing DesktopWindowCollection::Listener interface-----*/
	virtual void OnDesktopWindowAdded(DesktopWindow* window);
	virtual void OnDesktopWindowRemoved(DesktopWindow* window);
	virtual void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,
											const OpStringC& url);
	virtual void OnCollectionItemMoved(DesktopWindowCollectionItem* item, DesktopWindowCollectionItem* old_parent, DesktopWindowCollectionItem* old_previous) {}
/* --------------------------------------------------------------------------------*/

	//Mode functions:
    INT32             GetMode(){return m_mode;}
    void              ChangeMode(INT32 mode);

	//Initialiation state check:
	BOOL              Initialized() {return m_initialized;}

	//Public Deleting methods
	void		      DeleteAllItems();	//	*** DG-241198
    void              Delete(HistoryModelItem* item);

    //SortListener
    virtual INT32     OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1);

	//Get methods
	OP_STATUS         GetTopItems(OpVector<HistoryModelPage> &itemVector, INT32 num_items);
	OP_STATUS         GetSimpleItems(const OpStringC& search_text, OpVector<HistoryAutocompleteModelItem> &itemVector);
	OP_STATUS         GetOperaPages(const OpStringC& search_text, OpVector<PageBasedAutocompleteItem>& search_page);
	//Add/Remove DesktopHistoryModelListeners
	OP_STATUS         AddDesktopHistoryModelListener(DesktopHistoryModelListener* listener);
	OP_STATUS         RemoveDesktopHistoryModelListener(DesktopHistoryModelListener* listener);

	HistoryModelItem* FindItem(INT32 id);
	HistoryModelPage* GetPage(HistoryPage* page);

	/**
	 * Finds exactly one item if it exists
	 *
	 * @param search_url - the url to search for
	 * @param do_guessing - if TRUE it will do a little url manipulation to try to find it (not recomended)
	 *
	 * @return the history page if found
	 */
	HistoryModelPage* GetItemByUrl(const OpStringC & search_url, BOOL do_guessing = FALSE);

	/**
	 * Finds bookmark by its nickname
	 *
	 * @param [in] nick bookmark's nickname
	 * @param [out] bookmark if bookmark is found it will be written in this variable
	 *
	 * @return status
	 */
	OP_STATUS GetBookmarkByNick(const OpStringC& nick, HistoryPage*& bookmark) { return m_model->GetBookmarkByNick(nick, bookmark); }

	//Returns number of items in core - not includeing folders.
	INT32             GetNumItems() {if(m_CoreValid) return m_model->GetCount(); return 0;}

    //Resize the list to new size
    void		      Resize(INT32 m) {if(m_CoreValid) m_model->Size(m);}

    //Save history functions
	OP_STATUS 		  Save(BOOL force = FALSE);

	BOOL IsLocalFileURL(const OpStringC & url, OpString & stripped);

#ifdef SELFTEST
	OP_STATUS AddPage(const OpStringC& url_name,
					  const OpStringC& title,
					  time_t acc,
					  BOOL save_session = TRUE,
					  time_t average_interval = -1)
	{
		return m_model->AddPage(url_name, title, acc, save_session, average_interval);
	}
#endif // SELFTEST

private:
	HistoryPage* AddBookmark(Bookmark * item);
	HistoryItem::Listener* GetListener(HistoryItem* core_item);

    //Init functions
    void              initPrefixArray();
    void              initFolderArray();
	void              AddBookmarks();
	OP_STATUS		  AddBookmarkNick(const OpStringC & nick, Bookmark * item);
	void              RemoveBookmark(Bookmark * item);

    //Setting up the vector for the user interface
    void              MakeTimeVector();
    void              MakeServerVector();
    void              MakeOldVector();
#ifdef HISTORY_BOOKMARK_VIEW
	void              MakeBookmarkVector();
#endif //HISTORY_BOOKMARK_VIEW

    //Utility functions
    INT32             StripPrefix(const uni_char* address, uni_char ** stripped);
    INT32             StripProtocol(const uni_char* address, uni_char ** stripped, INT32 prefix_num);
    void              GetServerName(const uni_char* address, uni_char ** stripped);
    void              GetServerAddress(const uni_char* address, uni_char ** stripped);
	HistoryModelPage* LookupUrl(const OpStringC & search_url);
	HistoryModelPage* GetItemByDesktopWindow(DesktopWindow* window);

    //State changing functions
    void              AddItem(HistoryModelItem* item, HistoryModelSiteFolder* site);
	void              Remove(HistoryModelItem* item);
    void              RemoveItem(HistoryModelItem* item);
    void              RemoveFolder(HistoryModelFolder* item);
    void              DeleteItem(HistoryModelItem* item, BOOL deleteParent);

    HistoryModelSiteFolder* AddServer(const uni_char *url_name, INT32 prefix_num);
    void              Link(HistoryModelItem* elm, BOOL save_session);

    //Traversal functions for the trees
    void              getInfix(OpVector<OpTreeModelItem> &vector);
    void              FindInPrefixTrees(const uni_char* in_MatchString, 
										OpVector<OpTreeModelItem> &pvector,
										OpVector<OpTreeModelItem> &vector);
	
    void              AddItem_To_TimeVector(HistoryModelPage* item);
    void              AddItem_To_OldVector(HistoryModelPage* item, BOOL first);
	HistoryModelTimeFolder* GetTimeFolder(TimePeriod period);
	INT32             ComparePopularity(HistoryModelPage* item_1, HistoryModelPage* item_2);

	void              MergeSort(UINTPTR* array, UINTPTR* temp_array, INT32 left, INT32 right);
	void              Merge(UINTPTR* array, UINTPTR* temp_array, INT32 left, INT32 right, INT32 right_end);

	//Deletes all items/folders (except time folders)
	void              ClearModel();

	//Clears the model (only deleting prefixfolders)
	void              CleanUp();

	//Call DesktopHistoryModelListeners
	void              BroadcastHistoryModelItemDestroyed(HistoryModelItem* item);

	BOOL                     m_CoreValid;
	BOOL                     m_initialized;
	OpHistoryModel*          m_model;
	History_Mode             m_mode;
	HistoryModelTimeFolder** m_timefolders;
	BookmarkModel*            m_bookmark_model;

	OpVector<HistoryModelPage> m_deletable_items;
	OpListeners<DesktopHistoryModelListener> m_listeners;
};

#endif // HISTORY_MODEL_H
