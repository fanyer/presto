/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author:
 * 
 */
#ifndef _DESKTOP_BOOKMARK_MANAGER_H_
#define _DESKTOP_BOOKMARK_MANAGER_H_

#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/BookmarkClipboard.h"
#include "modules/bookmarks/bookmark_storage_provider.h"
#include "modules/util/adt/oplisteners.h"

#define g_desktop_bookmark_manager (DesktopBookmarkManager::GetInstance())

class DesktopBookmarkListener
{
public:
	virtual void OnBookmarkModelLoaded() = 0;
};

class DesktopBookmarkManager : public DesktopManager<DesktopBookmarkManager>
							 , public HotlistModelListener
{
public:

	enum SensitiveTag
	{
		VisitedTime=0x01, CreatedTime=0x02
	};
	
	OP_STATUS Init();

	/** Starts loading bookmarks into the model. This process is asynchronous,
	 * listeners are notified by OnBookmarkModelLoaded when it is finished.
	 *
	 * @param copy_default_bookmarks if true model is initailized using default bookmarks from
	 *                               the package and/or bookmarks imported from the default browser;
	 *                               ignored if bookmarks.adr file in user's profile already exists
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other errors
	 */
	OP_STATUS Load(bool copy_default_bookmarks);

	BOOL DropItem(const BookmarkItemData& item_data, 
				  INT32 onto, 
				  DesktopDragObject::InsertType insert_type, 
				  BOOL execute, 
				  OpTypedObject::Type drag_object_type, 
				  INT32* first_id = 0);
	
	OP_STATUS EditItem(INT32 id, DesktopWindow* parent);

	BOOL Rename( OpTreeModelItem *item, OpString& text );

	// add new bookmarks
	
	// FIXME: Until merge after 7.30 (htm_elm.cpp)
	BOOL NewBookmark( const HotlistManager::ContactData& cd, INT32 parent_id, DesktopWindow* parent, BOOL interactive );
	BOOL NewBookmark( INT32 parent_id, DesktopWindow* parent_window );
	BOOL NewBookmark( BookmarkItemData& item_data, INT32 parent_id );
	BOOL NewBookmark( BookmarkItemData& item_data, INT32 parent_id, DesktopWindow* parent_window, BOOL interactive );
	
	BOOL NewSeparator(INT32 parent_id);

	BOOL NewBookmarkFolder(const OpStringC& name, INT32 parent_id, OpTreeView* treeView = NULL);

	void Delete( OpINT32Vector &id_list, BOOL real_delete = FALSE, BOOL handle_target_folder = TRUE);
	void CopyItems(OpINT32Vector& id_list, BOOL handle_target_folder = TRUE);
	void CutItems(OpINT32Vector& id_list);
	OP_STATUS PasteItems(HotlistModelItem* at, DesktopDragObject::InsertType insert_type = DesktopDragObject::INSERT_INTO);

	BOOL GetItemValue(INT32 id, 
					  BookmarkItemData& data/*, INT32& flag_changed */ )
	{
		return GetItemValue(GetItemByID(id),data);
	}
	
	BOOL GetItemValue(OpTreeModelItem* item, 
					  BookmarkItemData& data/*, INT32& flag_changed*/ );
		
	BOOL SetItemValue(INT32 id, 
					  const BookmarkItemData& data, 
					  BOOL validate, 
					  UINT32 flag_changed )
	{
		return SetItemValue(GetItemByID(id),data,validate, flag_changed);
	}
	
	BOOL SetItemValue(OpTreeModelItem *item, 
					  const BookmarkItemData& data, 
					  BOOL validate, 
					  UINT32 flag_changed );


	HotlistModelItem* GetItemByID(INT32 id) { return m_model->GetItemByID(id); }

	// Copy to clipboard
	void AddToClipboard( const BookmarkItemData &cd, BOOL append) { if (m_clipboard) m_clipboard->AddItem(cd,append); }

	/**
	 * Clears some of the tags in the lists for sensitive data. If there is a change in the 
	 * data structures the new state is written to disk.
	 *
	 * @param flag Use a combination of values from enum SensitiveTag to control what should be cleared.
	 * @param always_write_to_disk Force writing to disk after data structures have been examimed.
	 */
	void ClearSensitiveSettings(int flag, BOOL always_write_to_disk);

	DesktopBookmarkManager() :
		m_model(0),
		m_clipboard(0),
		m_edit_folder_tree_view(0),
		m_import_format(HotlistModel::NoFormat) {}

	~DesktopBookmarkManager();

	OP_STATUS SaveDirtyModelToDisk();

	BookmarkModel* GetBookmarkModel() { return m_model; }
	BookmarkClipboard* GetClipboard() { return m_clipboard; }

	// HotlistModelListener
	virtual void OnHotlistItemAdded(HotlistModelItem* item);
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN) {}
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}

	/**
	 * Adds bookmark listener
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK of success, otherwise an error code
	 */
	OP_STATUS AddBookmarkListener(DesktopBookmarkListener *ml) { return m_bookmark_listeners.Add(ml);}

	/**
	 * Removes bookmark listener
	 *
	 * @param listener The listener
	 *
	 * @return OpStatus::OK of success, otherwise an error code
	 */
	OP_STATUS RemoveBookmarkListener(DesktopBookmarkListener *ml) 
	{
		OP_ASSERT(!m_broadcasting_loaded && "Do not remove yourself as listener in OnBookmarkModelLoaded(), all listeners will not be called then!");
		return m_bookmark_listeners.Remove(ml);
	}

	void BroadcastBookmarkLoaded();

	OP_STATUS ImportAdrBookmark(const uni_char* path);

	BOOL IsIniFile(const OpString& name);

	// Is this bookmark or folder already in user profile
	// @param partner_id partner id of the bookmark
	HotlistModelItem* FindDefaultBookmark(OpStringC partner_id);

	// Was this bookmark deleted by the user
	// @param partner_id partner id of the bookmark
	BOOL IsDeletedDefaultBookmark(OpStringC partner_id);

	void AddDeletedDefaultBookmark(OpStringC partner_id);
	const OpAutoVector<OpString>& DeletedDefaultBookmarks() { return m_deleted_default_bookmarks;}

	// Functions to find a default bookmark (that has display url) by url or display url
	HotlistModelItem* FindDefaultBookmarkByURL(OpStringC url);
	OpINT32Vector& GetDefaultBookmarks() {return m_default_bookmarks;}

	static OP_STATUS AddCoreItem(BookmarkItem* core_item, BookmarkItem* core_previous, BookmarkItem* core_parent);
	static OP_STATUS AddCoreItem(BookmarkItem* item, HotlistModelItem* previous = NULL, HotlistModelItem* parent = NULL, BOOL last = FALSE);
	static OP_STATUS AddBookmark(const BookmarkItemData* item_data, HotlistModelItem* previous, HotlistModelItem* parent = NULL);
	static OP_STATUS AddBookmarkLast(const BookmarkItemData* item_data, HotlistModelItem* parent = NULL);

private:
	void CreateTrashFolderIfNeeded();

	BookmarkModel*           m_model;
	BookmarkClipboard*       m_clipboard;

	OpTreeView*			     m_edit_folder_tree_view; // next added folder will be in edit mode if TRUE. FIXME

	// default bookmarks in user profile, may include bookmarks that
	// was default bookmark but then edited so not default anymore
	OpINT32Vector			m_default_bookmarks;
	OpAutoVector<OpString>  m_deleted_default_bookmarks;	// uuids of deleted partner bookmarks

	// default bookmarks that have (had) non-empty Display URL
	OpINT32Vector			m_default_bookmarks_with_display_url;

	OpListeners<DesktopBookmarkListener> m_bookmark_listeners;

	// Format of bookmarks store of system's default browser. Used when
	// auto-importing bookmarks to fresh profile.
	HotlistModel::DataFormat	m_import_format;
	// Path to default browser's bookmarks store. Used when auto-importing
	// bookmarks to fresh profile.
	OpString					m_import_path;

#ifdef DEBUG_ENABLE_OPASSERT
	BOOL					m_broadcasting_loaded;	// Set to TRUE when the BroadcastBookmarkLoaded() method is broadcasting loaded. 
#endif // DEBUG_ENABLE_OPASSERT

	// Re-add default bookmarks to user profile(when upgrading)
	OP_STATUS UpgradeDefaultBookmarks();
};

#endif // DESKTOP_BOOKMARK_MANAGER
