/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#include "core/pch.h"

#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/BookmarkFolder.h"
#include "adjunct/quick/models/BookmarkSeparator.h"
#include "modules/bookmarks/bookmark_ini_storage.h"
#include "adjunct/quick/models/BookmarkAdrStorage.h"

#include "adjunct/quick/hotlist/hotlistparser.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/img/imagedump.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/SyncManager.h"

BookmarkItemData::BookmarkItemData() :
	direct_image_pointer(NULL),
	personalbar_position(-1),
	panel_position(-1),
	move_is_copy(FALSE),
	separators_allowed(TRUE),
	subfolders_allowed(TRUE),
	deletable(TRUE),
	active(FALSE),
	expanded(FALSE),
	max_count(-1), /* == no limit */
	small_screen(FALSE),
	type(FOLDER_NO_FOLDER)
{
}

BookmarkItemData::BookmarkItemData(const BookmarkItemData& right)
{
	name.Set(right.name);
	url.Set(right.url);
	image.Set(right.image);
	folder.Set(right.folder);
	description.Set(right.description);
	shortname.Set(right.shortname);
	created.Set(right.created);
	visited.Set(right.visited);
	unique_id.Set(right.unique_id);
	target.Set(right.target);
	partner_id.Set(right.partner_id);
	display_url.Set(right.display_url);

	direct_image_pointer	= right.direct_image_pointer;
	personalbar_position	= right.personalbar_position; 
	panel_position			= right.panel_position; 
	move_is_copy			= right.move_is_copy;
	separators_allowed		= right.separators_allowed;
	subfolders_allowed		= right.subfolders_allowed;
	deletable				= right.deletable;
	max_count				= right.max_count;
	active					= right.active;
	expanded				= right.expanded;
	small_screen			= right.small_screen;
	type					= right.type;
}


/**********************************************************************************
 *
 * BookmarkModel
 *
 *
 *********************************************************************************/

BookmarkModel::BookmarkModel() :
	HotlistModel(BookmarkRoot,FALSE),
	m_trash_folder(0),
	m_provider(0),
	m_ini_provider(0),
	m_initialized(FALSE),
	m_loaded(FALSE),
	m_delete_old_ini_bookmark(FALSE)
{ 
	g_favicon_manager->AddListener(this);
}

/**
 * Destructor
 * 
 */
BookmarkModel::~BookmarkModel()
{
	m_trash_folder = NULL;

	if( g_favicon_manager )
	{
		g_favicon_manager->RemoveListener(this);
	}

	g_bookmark_manager->UnregisterBookmarkManagerListener(this);

	OP_DELETE(m_provider);
}

/******************************************************************
 *
 * Init
 *
 * 
 ******************************************************************/
OP_STATUS BookmarkModel::Init()
{
	g_bookmark_manager->RegisterBookmarkManagerListener(this);

	OpFile hotlistfile;
	OpString path;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
	path.Set(hotlistfile.GetFullPath());
	
	if (g_desktop_bookmark_manager->IsIniFile(path))
	{
		// Import bookmarks and delete the file and pref "Hot List File Ver2"
		m_delete_old_ini_bookmark = TRUE;
		m_ini_provider = OP_NEW(BookmarkIniStorage, (g_bookmark_manager));
		if (!m_ini_provider)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(m_ini_provider->OpenLoad(path, OPFILE_ABSOLUTE_FOLDER)))
		{
			RETURN_IF_MEMORY_ERROR(g_bookmark_manager->SetStorageProvider(m_ini_provider));
			if (OpStatus::IsError(g_bookmark_manager->LoadBookmarks(BOOKMARK_INI, m_ini_provider)))
			{
				OP_DELETE(m_ini_provider);
				m_ini_provider = NULL;
				g_bookmark_manager->SetStorageProvider(NULL);
			}
			return OpStatus::OK;
		}
		else
		{
			// the ini file might be missing.
			OP_DELETE(m_ini_provider);
			m_ini_provider = NULL;

			OpString tmp_storage;
			path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage));
			path.Append(UNI_L("bookmarks.adr"));	
		}
	}
	
	return InitStorageProvider(path);
}

/******************************************************************
 *
 * InitStorageProvider
 *
 * 
 ******************************************************************/
OP_STATUS BookmarkModel::InitStorageProvider(const OpString &path)
{
	RETURN_IF_ERROR(g_bookmark_manager->SetStorageProvider(NULL));

	OP_DELETE(m_provider);

	m_provider = OP_NEW(BookmarkAdrStorage, (g_bookmark_manager, path));
	if (!m_provider)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_MEMORY_ERROR(g_bookmark_manager->SetStorageProvider(m_provider));
	if (OpStatus::IsError(g_bookmark_manager->LoadBookmarks(BOOKMARK_ADR, m_provider)))
	{
		OP_DELETE(m_provider);
		m_provider = NULL;
		g_bookmark_manager->SetStorageProvider(NULL);
	}

	return OpStatus::OK;
}

// ----------- BookmarkManagerListener --------------------------

void BookmarkModel::OnBookmarksLoaded(OP_STATUS ret, UINT32 count) 
{
	// stop trying to read or write if reading fails
	if (OpStatus::IsError(ret))
	{
		OP_STATUS status = g_bookmark_manager->SetStorageProvider(NULL);
		OP_ASSERT(OpStatus::IsSuccess(status));
		OP_DELETE(m_provider);
		OP_DELETE(m_ini_provider);
		m_provider = NULL;
		m_ini_provider = NULL;
		return;
	}

	if (m_ini_provider)
	{
		// use the default location for adr file
		OpString path;
		OpString tmp_storage;
		path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage));
		path.Append(UNI_L("bookmarks.adr"));			

		m_provider = OP_NEW(BookmarkAdrStorage, (g_bookmark_manager, path));
		if (m_provider && OpStatus::IsSuccess(g_bookmark_manager->SetStorageProvider(m_provider)))
		{
			OP_DELETE(m_ini_provider);
			m_ini_provider = NULL;
		}
		else
		{
			OP_ASSERT(FALSE);
		}
	}

	if(!m_loaded)
	{
		m_loaded = TRUE;
		g_desktop_bookmark_manager->BroadcastBookmarkLoaded();
	}
}

void BookmarkModel::OnBookmarksSaved(OP_STATUS ret, UINT32 count)
{
	if (OpStatus::IsError(ret))
	{
		if (g_hotlist_manager->ShowSavingFailureDialog(m_provider->GetFilename()))
			SetDirty(TRUE, 0);
	}
	else if (m_delete_old_ini_bookmark)
	{
		BOOL exist;
		OpFile hotlistfile;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
		if (OpStatus::IsSuccess(hotlistfile.Exists(exist)) && exist)
		{
			RETURN_VOID_IF_ERROR(g_sync_manager->BackupFile(hotlistfile, UNI_L(".old")));
			hotlistfile.Delete();
		}
		TRAP(rc, g_pcfiles->ResetFileL(PrefsCollectionFiles::HotListFile));
		m_delete_old_ini_bookmark = FALSE;
	}
}

/******************************************************************
 *
 * OnBookmarkAdded
 *
 * @param BookmarkItem* item - bookmarkItem that was added to
 *                             core bookmarks.
 * 
 * Adds a CoreBookmarkWrapperItem wrapping the core BookmarkItem
 * to m_model
 *
 * @pre
 * @post
 *
 ******************************************************************/
void BookmarkModel::OnBookmarkAdded(BookmarkItem* item)
{
	if (!item)
		return;

	OP_ASSERT(!item->GetListener());

	CoreBookmarkWrapper* bookmark = 0;

	if (item->GetFolderType() == FOLDER_NO_FOLDER)
	{
		bookmark = OP_NEW(Bookmark, (item));
	}
	else if (item->GetFolderType() == FOLDER_NORMAL_FOLDER)
	{
		bookmark = OP_NEW(BookmarkFolder, (item));
	}
	else if (item->GetFolderType() == FOLDER_TRASH_FOLDER)
	{
		if (m_trash_folder)
		{
			m_trash_folder->Update();
		}
		else
		{
			m_trash_folder = OP_NEW(TrashFolder, (item));
			bookmark = m_trash_folder;
		}
	}
	else if (item->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
	{
		bookmark = OP_NEW(BookmarkSeparator, (item));
	}
	else
	{
		OP_ASSERT(FALSE);
	}

	OpString title;

	if (bookmark)
	{
		if (AddItem(bookmark->CastToHotlistModelItem()))
		{
#ifdef SELFTEST
			//if (m_listener)
			//	m_listener->OnBookmarkAdded(bm_item);
#endif
		}
	}
}

/*******************************************************************
 *
 * GetTrashFolder
 *
 * @return the single trash folder
 *
 *******************************************************************/

TrashFolder* BookmarkModel::GetTrashFolder()
{
    return m_trash_folder;
}

/********************************************************************
 *
 * GetInfoText
 *
 * @param BookmarkItemData& item_data
 *
 *
 ********************************************************************/

void BookmarkModel::GetInfoText( const BookmarkItemData& item_data, OpInfoText &text )
{
	OpString str;
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, str);

	if (item_data.url.HasContent())
	{
		if( item_data.url.Find(UNI_L("@")) == KNotFound )
		{
			text.SetStatusText(item_data.url.CStr());
			text.AddTooltipText(str.CStr(), item_data.url.CStr());
		}
		else
		{
			URL	url = g_url_api->GetURL(item_data.url.CStr());
			OpString url_string;
			url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
			text.SetStatusText(url_string.CStr());
			text.AddTooltipText(str.CStr(), url_string.CStr());
		}
	}

	OpString nick;
	if (item_data.shortname.HasContent())
	{
		g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, str);
		text.AddTooltipText(str.CStr(), item_data.shortname.CStr());
	}
	OpString desc;
	if (item_data.description.HasContent())
	{
		g_languageManager->GetString(Str::DI_ID_EDITURL_DESCRIPTIONLABEL, str);
		text.AddTooltipText(str.CStr(), item_data.description.CStr());
	}
	OpString created;
	if (item_data.created.HasContent())
	{
		g_languageManager->GetString(Str::DI_IDLABEL_HL_CREATED, str);
		text.AddTooltipText(str.CStr(), item_data.created.CStr());
	}
	
	if (item_data.visited.HasContent())
	{
		g_languageManager->GetString(Str::DI_IDLABEL_HL_LASTVISITED, str);
		text.AddTooltipText(str.CStr(), item_data.visited.CStr());
	}
}

// ------------------------------------------------------------------------
// ------------------ Functions to handle that item was moved or added ----
// ------------------ Update UI as core bookmarks changes -----------------

/*************************************************************************
 *
 * OnMoveItem
 * 
 * Core item was moved, update the quick model correspondingly
 *
 ************************************************************************/

BOOL BookmarkModel::OnMoveItem(CoreBookmarkWrapper* item_to_move) 
{
	HotlistModelItem* item = item_to_move->CastToHotlistModelItem();

	// Get the new parent and previous
	BookmarkItem* core_parent   = item_to_move->GetCoreItem()->GetParentFolder();
	BookmarkItem* core_previous = item_to_move->GetCoreItem()->GetPreviousItem();
	
	HotlistModelItem* previous   = GetByCoreItem(core_previous);
	HotlistModelItem*  parent    = GetByCoreItem(core_parent);

	Move(item->GetIndex(),parent ? parent->GetIndex() : -1, previous ? previous->GetIndex() : -1);

	BroadcastItemMoved(item);

	// trash folder might need to update. text should be normal when empty and bold when not
	if (GetTrashFolder())
		GetTrashFolder()->Change();

	return TRUE;
}


/**********************************************************************
 *
 * AddItem
 *  
 * @param CoreBookmarkWrapper* item - 
 *
 *
 ************************************************************************/
BOOL BookmarkModel::AddItem(HotlistModelItem* item)
{
	CoreBookmarkWrapper* wrapper_item = BookmarkWrapper(item);
	if (!item || item->GetModel() || !wrapper_item || !wrapper_item->GetCoreItem())
		return FALSE;

	BookmarkItem* core_parent   = wrapper_item->GetCoreItem()->GetParentFolder();
	BookmarkItem* core_previous = wrapper_item->GetCoreItem()->GetPreviousItem();
	
	HotlistModelItem* previous  = GetByCoreItem(core_previous);
	HotlistModelItem* parent    = GetByCoreItem(core_parent);

	if (previous)
	{
		InsertAfter(item, previous->GetIndex());
	}
	else if (parent)
	{
		parent->AddChildFirst(item);
		parent->Change();
	}
	else
	{
		AddFirst(item);
	}

	// so that when the bookmark is moved we know what message to send.(trashed or untrashed)
	BookmarkWrapper(item)->SetWasInTrash(item->GetIsInsideTrashFolder());

	if(item->GetIsActive())
		SetActiveItemId(item->GetID(),FALSE);

	BroadcastHotlistItemAdded(item,FALSE);	

	return TRUE;
}



/***********************************************************************************
 *
 * BookmarkModel::IsDuplicateURL
 *
 * @param item (not yet registered in hotlist?)
 * @param history_item Returns history_item corresponding to url of item, if any.
 *
 * @return TRUE if there is another bookmark with same url.
 *
 ***********************************************************************************/
// TODO: Use Bookmark module / history integration
BOOL BookmarkModel::IsDuplicateURL(Bookmark& item) 
{
	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem*  model_item = GetItemByIndex(i);
		if (model_item->IsBookmark())//GetType() == BOOKMARK_TYPE)
		{
			Bookmark* bookmark = static_cast<Bookmark*>(model_item);
			if (bookmark->GetResolvedUrl().Compare(item.GetResolvedUrl()) == 0)
			{
				return TRUE;
			}
		}
		else
		{
		}
	}

	//FIXME shuais
	return FALSE;
}

/**********************************************************************************
 *
 * DeleteItem
 *
 *
 *********************************************************************************/


BOOL BookmarkModel::DeleteItem(HotlistModelItem* item, BOOL real_delete/* = FALSE */)
{
#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = real_delete;
#endif

	// Remove from core
	// This will delete all items in the folder too.
	// If there is a trash folder, this will move item to trash folder, else it will do a real delete
	BOOL success = OpStatus::IsSuccess(BookmarkWrapper(item)->RemoveBookmark(real_delete, TRUE));

#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = FALSE;
#endif

	return success;
}

/***********************************************************************
 *
 * EmptyTrash
 *
 * 
 ***********************************************************************/

BOOL BookmarkModel::EmptyTrash()
{
	HotlistModelItem* trash = GetTrashFolder();
	OP_STATUS status(OpStatus::OK);

	if (trash)
	{
		ModelLock lock(this);

		HotlistModelItem* child = 0;
		while ((child = trash->GetChildItem()) != 0)
		{
			if (!DeleteItem(child, TRUE)) // FALSE ok too since in trash
				break;
		}

		OP_ASSERT(trash->GetChildCount() == 0);
		trash->Change();	
	}

	return OpStatus::IsSuccess(status);
}

INT32 BookmarkModel::GetColumnCount()
{
	return 6;
}

OP_STATUS BookmarkModel::GetColumnData(ColumnData* column_data)
{
	OpString name, nickname, address;

	g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, nickname);
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, address);
	g_languageManager->GetString(Str::DI_ID_CL_PROP_NAME_LABEL, name);

	if( column_data->column == 0 )
	{
		column_data->text.Set( name );
	}
	else if( column_data->column == 1 )
	{
		column_data->text.Set( nickname );
	}
	else if( column_data->column == 2 )
	{
		column_data->text.Set( address );
	}

	column_data->custom_sort = TRUE;

	return OpStatus::OK;
}

/******************************************************************************
 *
 * HotlistGenericModel::GetItemList
 *
 * Used by history model
 *
 ******************************************************************************/

void BookmarkModel::GetBookmarkItemList(OpVector<Bookmark>& items)
{
	INT32 count = GetItemCount();

	for( INT32 i = 0; 0 <= i && i < count; )
	{
		HotlistModelItem *hli = GetItemByIndex(i);

		if( hli->GetIsTrashFolder() )
		{
			i = GetSiblingIndex(i);
		}
		else if (hli->IsBookmark()) // Don't add folders and separators
		{
			items.Add(static_cast<Bookmark*>(hli));
			i++;
		}
		else
		{
			i++;
		}
	}
}


/******************************************************************************
 *
 * BookmarkModel::OnFavIconAdded
 *
 * Notify UI and Link about favicon add
 *
 ******************************************************************************/
void BookmarkModel::OnFavIconAdded(const uni_char* document_url, 
								  const uni_char* image_path)
{
	if (!document_url)
	{
		return;
	}

	// Need to do a broadcastitemchanged on the relevant bookmark if any
	HistoryPage* history_item = 0;
	
	OP_STATUS status = g_globalHistory->GetItem(document_url, history_item);
	
	if (OpStatus::IsSuccess(status) && history_item && history_item->IsBookmarked())
	{
		OpVector<HistoryItem::Listener> result;
		
		history_item->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result);
		
		for (UINT32 i = 0; i < result.GetCount(); i++)
		{
			HistoryItem::Listener* listener = result.Get(i);
			
			OP_ASSERT(listener);
			
			HotlistModelItem* bm_item = static_cast<Bookmark*>(listener);
			OP_ASSERT(bm_item->GetModel() && !bm_item->GetIsInsideTrashFolder());
			
			if( bm_item->IsBookmark() && bm_item->GetResolvedUrl().Compare(document_url) == 0 )
			{
				// Triggers update in bookmark list if icon is not there yet (bug #174635) [espen 2008-01-07]
				bm_item->Change();
				
				static_cast<Bookmark*>(bm_item)->SyncIconAttribute(FALSE);

				// Trigger syncing the icon
				BroadcastHotlistItemChanged(bm_item, FALSE, HotlistGenericModel::FLAG_ICON);
			}
		}
	}
}


/***********************************************************************************
 *
 * BookmarkModel::IsDuplicateURL
 *
 * @param item (not yet registered in hotlist?)
 * @param history_item Returns history_item corresponding to url of item, if any.
 *
 * @return TRUE if there is another bookmark with same url.
 *
 ***********************************************************************************/
BOOL BookmarkModel::IsDuplicateURL(HotlistModelItem *item,
								  HistoryPage** history_item,
								  Bookmark** bookmark )
{

	HotlistModelItem* hmi = GetItemByType( item );

	if (history_item) *history_item = 0;

	if (hmi && hmi->IsBookmark())
	{
		HistoryPage* page = 0;

		OP_STATUS status = g_globalHistory->GetItem(hmi->GetResolvedUrl().CStr(), page);

		if (OpStatus::IsSuccess(status) && page && page->IsBookmarked())
		{
			if (history_item) *history_item = page;

			// Get the bookmark
			if (bookmark)
			{
				OpVector<HistoryItem::Listener> result;

				OP_STATUS status = page->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result);
				if ( OpStatus::IsSuccess(status) && result.GetCount() > 0)
				{
					UINT32 i;
					for (i = 0; i < result.GetCount(); i++)
					{
						if ((static_cast<Bookmark*>(result.Get(i))->GetType()) == OpTypedObject::BOOKMARK_TYPE)
						{
							if ((static_cast<Bookmark*>(result.Get(i)))->GetParentFolder() == item->GetParentFolder())
							{
								*bookmark = static_cast<Bookmark*>(result.Get(i));
							}
						}
					}
				}
			}

			return TRUE;
		}
	}

	return FALSE;
}

/**************************************************************************
 *
 *
 * BookmarkModel::IsDuplicateURL
 *
 * @param url - url to check if is already bookmarked
 * @param history_item - holds ref. to history item of the duplicate bookmark
 *                       at return, if any.
 *
 * @return TRUE if there is another bookmark with same url
 *
 **************************************************************************/

BOOL BookmarkModel::IsDuplicateURL(const OpStringC& url, 
								  HistoryPage** history_item)
{

	if (!url.IsEmpty())
	{
		HistoryPage* page = 0;

		OP_STATUS status = g_globalHistory->GetItem(url.CStr(), page);
		if (OpStatus::IsSuccess(status) && page && page->IsBookmarked())
		{
			if (history_item)
				*history_item = page;
			return TRUE;
		}
	}
	return FALSE;
}

/**************************************************************************************
 *
 * BookmarkModel::IsDuplicateURLInSameFolder
 *
 * @param const OpString& url - url to check if is already bookmarked
 * @param HotlistModelItem* folder - Folder to check for duplicate in
 * @param CoreBookmarkWrapper** bookmark - If duplicate in same folder is found,
 *                               bookmark is set to the duplicate at return
 *
 * @return TRUE if there is another Bookmark with same url in folder,
 *     or if folder is NULL, and there is another such bookmark in root folder.
 *
 **************************************************************************************/

BOOL BookmarkModel::IsDuplicateURLInSameFolder(const OpStringC& url, 
											  HotlistModelItem* folder, 
											  Bookmark** bookmark)
{

	HistoryPage* history_item = 0;
	if (IsDuplicateURL(url, &history_item))
	{
		// Check folder
		if (history_item)
		{
			OpVector<HistoryItem::Listener> result;

			history_item->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result);

			for (UINT32 i = 0; i < result.GetCount(); i++)
			{
				HistoryItem::Listener* listener = result.Get(i);
				
				OP_ASSERT(listener);
				
				Bookmark* bm_item = static_cast<Bookmark*>(listener);
				
				INT32 parent_id = bm_item->GetParentID();

				if ((folder && parent_id == folder->GetID()) || (!folder && parent_id == -1))
				{
					// Update bookmark to be set to the correct item in the correct folder
					if (bm_item &&
						bm_item->GetParentFolder() == folder && bookmark && *bookmark)
						{
							*bookmark = bm_item;
						}
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

/**************************************************************************************
 *
 * BookmarkModel::IsDuplicateURL
 *
 * @return TRUE if there is another Bookmark with same url in folder,
 *     or if folder is NULL, and there is another such bookmark in root folder.
 *
 **************************************************************************************/

BOOL BookmarkModel::IsDuplicateURLInSameFolder(HotlistModelItem* item, 
											  HotlistModelItem* folder, 
											  Bookmark** bookmark)
{
	/* If bookmark - Check if duplicate (bookmarks with same url in same folder) */
	BOOL duplicate = FALSE;
	HistoryPage* history_item = 0;

	BOOL global_duplicate = IsDuplicateURL(item, &history_item, bookmark);

	if (global_duplicate && history_item)
	{
		OpVector<HistoryItem::Listener> result;

		history_item->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result);

		for (UINT32 i = 0; i < result.GetCount(); i++)
		{
			HistoryItem::Listener* listener = result.Get(i);

			OP_ASSERT(listener);

			Bookmark* bm_item = static_cast<Bookmark*>(listener);

			INT32 parent_id = bm_item->GetParentID();

			if ((folder && parent_id == folder->GetID()) || (!folder && parent_id == -1))
			{
				duplicate = TRUE;
				// Update bookmark to be set to the correct item in the correct folder
				if (bm_item &&
					bm_item->GetParentFolder() == folder && bookmark && *bookmark)
				{
					*bookmark = bm_item;
				}
				break;
			}
		}
	}
	return duplicate;
}

void BookmarkModel::OnChange()
{
	// don't intiate a save request when the model is still loading
	if (m_loaded)
		SetDirty(TRUE);
}

void BookmarkModel::BroadcastItemMoved(HotlistModelItem* item)
{
	BOOL from_trash = BookmarkWrapper(item)->GetWasInTrash();
	BOOL to_trash = item->GetIsInsideTrashFolder();

	BookmarkWrapper(item)->SetWasInTrash(to_trash);

	BroadcastHotlistItemChanged(item, FALSE, HotlistModel::FLAG_MOVED_TO);

	if(!from_trash && to_trash)
		BroadcastHotlistItemTrashed(item);
	else if(from_trash && !to_trash)
		ItemUnTrashed(item);
}

void BookmarkModel::OnBookmarkRemoved(HotlistModelItem* item)
{
	// trash folder might be deleted for some reason. e.g. Sync
	if (item == m_trash_folder)
		m_trash_folder = NULL;

	// Assuming remove is really remove, and move is used everywhere else
	BOOL old_value = m_bypass_trash_on_delete;
	m_bypass_trash_on_delete = TRUE;
	BroadcastHotlistItemRemoving(item, FALSE);
	m_bypass_trash_on_delete = old_value;
}

void BookmarkModel::Erase()
{
	m_trash_folder = NULL;
	m_active_folder_id = -1;
	m_is_dirty = FALSE;

	// Erase the UI model first, so we don't receive a lot of remove messages from core in next step
	HotlistModel::Erase();

	// Erase the core bookmarks
	g_bookmark_manager->RemoveAllBookmarks();

	// This removes also the root folder in core bookmark_manager, so do Init
	g_bookmark_manager->Init();
}

void BookmarkModel::SetDirty( BOOL state, INT32 timeout_ms )
{
	g_bookmark_manager->SetSaveBookmarksTimeout(BookmarkManager::DELAY_AFTER_FIRST_CHANGE, timeout_ms);
	
	if (state)
		g_bookmark_manager->SetSaveTimer();

	m_is_dirty = state;
}


HotlistModelItem* BookmarkModel::GetByURL(const uni_char* url, bool trashed /*= true*/, bool use_display_url /*= false*/)
{
	if (!url)
		return NULL;

	int len_of_arg_url = uni_strlen(url);
	int len_to_cmp = KAll;

	for (INT32 i = 0; i < GetItemCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (!item->IsSeparator() && item->GetUrl().HasContent())
		{
			if (trashed || !item->GetIsInsideTrashFolder())
			{
				if (use_display_url && item->GetHasDisplayUrl() && item->GetDisplayUrl().Compare(url) == 0)
					return item;
				else
				{
					/* What you are seeing is a solution to fix a bizarre situation 
					in which URLs mentioned in below example are consider same.

					Example:
					a. http://www.hostname.no and http://www.hostname.no/ are treated identical, but 
					b. http://www.hostname.no/xyz.html and http://www.hostname.no/xyz.html/ are not same.

					Though it is not a complete solution, but it captures most of the usable cases on web.
					*/
					int len_of_bm_url = item->GetUrl().Length();
					if (abs(static_cast<int>(len_of_bm_url - len_of_arg_url)) == 1)
					{
						int min_len = min(len_of_bm_url, len_of_arg_url);
						if (item->GetUrl().DataPtr()[min_len] == UNI_L('/') || url[min_len] == UNI_L('/'))
							len_to_cmp = min_len;
					}

					if (item->GetUrl().Compare(url, len_to_cmp) == 0)
						return item;

					len_to_cmp = KAll;
				}
			}
		}
	}
	return NULL;
}

HotlistModelItem* BookmarkModel::GetByCoreItem(BookmarkItem* item)
{
	if (!item)
		return NULL;

	if (item->GetFolderType() == FOLDER_TRASH_FOLDER)
		return m_trash_folder;
	else
		return GetByUniqueGUID(item->GetUniqueId());
}

void BookmarkModel::DeleteItem(INT32 id)
{
	CollectionModelItem* item = GetModelItemByID(id);
	if (!item)
		return;

	DeleteItem(static_cast<HotlistModelItem*>(item));
}

OP_STATUS BookmarkModel::EditItem(INT32 id)
{
	return g_desktop_bookmark_manager->EditItem(id, g_application->GetActiveDesktopWindow());
}

CoreBookmarkPos::CoreBookmarkPos(HotlistModelItem* onto, DesktopDragObject::InsertType type)
{
	parent = g_bookmark_manager->GetRootFolder();
	previous = NULL;

	if (onto)
	{
		if(type == DesktopDragObject::INSERT_BEFORE)
		{
			previous = BookmarkWrapper(onto)->GetCoreItem()->GetPreviousItem();
			parent   = BookmarkWrapper(onto)->GetCoreItem()->GetParentFolder();
		}
		else if(type == DesktopDragObject::INSERT_AFTER || type == DesktopDragObject::INSERT_INTO && !onto->IsFolder())
		{
			previous = BookmarkWrapper(onto)->GetCoreItem();
			parent   = previous->GetParentFolder();
		}
		else if(type == DesktopDragObject::INSERT_INTO)
		{
			parent   = BookmarkWrapper(onto)->GetCoreItem();
		}
	}
}

HotlistModelItem* CoreBookmarkPos::Previous()
{
	return previous ? g_hotlist_manager->GetBookmarksModel()->GetByCoreItem(previous) : NULL;
}

HotlistModelItem* CoreBookmarkPos::Parent()
{
	return parent && parent != g_bookmark_manager->GetRootFolder()? 
		g_hotlist_manager->GetBookmarksModel()->GetByCoreItem(parent) : NULL;
}
