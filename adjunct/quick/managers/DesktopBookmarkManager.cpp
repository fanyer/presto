/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author karie
 * 
 */

#include "core/pch.h"

#include "adjunct/quick/controller/BookmarkPropertiesController.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#include "adjunct/quick/dialogs/BookmarkDialog.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/RedirectionManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/hotlistparser.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmark_sync.h"
#include "modules/bookmarks/bookmark_ini_storage.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"

#include "adjunct/quick/models/BookmarkAdrStorage.h"

// from HotlistManager
extern void ReplaceIllegalSinglelineText( OpString& text );

DesktopBookmarkManager::~DesktopBookmarkManager()
{
	if (m_model)
	{
		m_model->RemoveModelListener(this);	
	}

	OP_DELETE(m_clipboard);

	// note that if a operation is pending SetStorageProvider refuse to do anything.
	// so we cancel pending operation first so that bookmark manager won't try to use deleted storage provider later
	g_bookmark_manager->CancelPendingOperation(); 
	g_bookmark_manager->SetStorageProvider(NULL);
}

OP_STATUS DesktopBookmarkManager::SaveDirtyModelToDisk()
{
	// nothing to save if the model is not dirty
	if (!m_model || !m_model->IsDirty())
		return OpStatus::OK;

	// model could be dirty before it actually finish loading
	// e.g. Bookmark::OnHistoryItemAccessed change the visited time attribute
	// but don't try to save in this case
	if (!m_model->Loaded())
		return OpStatus::ERR_YIELD;

	// the bookmarks file path for the error dialog
	OpFile hotlistfile;
	OpString hotlistfile_path;
	RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
	RETURN_IF_ERROR(hotlistfile_path.Set(hotlistfile.GetFullPath()));
	OP_STATUS save_result;

	// loop until the file is saved without errors, or the user decides that he's ok
	// for example, if the file is on a removable drive, he can try to reconnect it
	do
	{		
		save_result = g_bookmark_manager->SaveImmediately();
	}
	while (OpStatus::IsError(save_result) && g_hotlist_manager->ShowSavingFailureDialog(hotlistfile_path));
	return save_result;
}

/*************************************************************************
 *
 * BookmarkInit
 *
 * Starts loading of bookmarks file
 *
 * Upgrade default bookmarks 
 *
 *************************************************************************/

OP_STATUS DesktopBookmarkManager::Init()
{
	m_edit_folder_tree_view = NULL;
	m_clipboard = OP_NEW(BookmarkClipboard, ());
	if (!m_clipboard)
		return OpStatus::ERR_NO_MEMORY;
	m_clipboard->Init();
	OP_ASSERT(g_bookmark_manager);

	m_model = g_hotlist_manager->GetBookmarksModel();
	m_model->AddModelListener(this);

	// Bookmarks will be loaded into model when OpBootManager gets
	// name of user's region. This may be delayed for a few seconds
	// if Opera needs to contact the autoupdate server to get IP-based
	// country code (DSK-351304).

#ifdef DEBUG_ENABLE_OPASSERT
	m_broadcasting_loaded = FALSE;
#endif // DEBUG_ENABLE_OPASSERT

	return OpStatus::OK;
}

OP_STATUS DesktopBookmarkManager::Load(bool copy_default_bookmarks)
{
	if (copy_default_bookmarks)
	{
		OpFile hotlistfile;
		RETURN_IF_LEAVE(g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, hotlistfile));
		BOOL exists;
		if (OpStatus::IsSuccess(hotlistfile.Exists(exists)) && !exists)
		{
			// Provide initital content of bookmarks.adr file.
			// 1. If another instance of Opera is set as default browser and its bookmarks store uses
			//    adr format then copy it. Partner bookmarks will be updated when model is loaded.
			// 2. Otherwise copy bookmarks.adr file for current region/language from package.
			//    Bookmarks from the default browser will be imported after model is loaded.
			//    Model will contain current partner bookmarks, so no update in this case.

			if (!g_pcui->GetIntegerPref(PrefsCollectionUI::DisableBookmarkImport) &&
				!g_commandline_manager->GetArgument(CommandLineManager::WatirTest)) // DSK-361259
			{
				DesktopOpSystemInfo* system_info = static_cast<DesktopOpSystemInfo*>(g_op_system_info);
				DesktopOpSystemInfo::DefaultBrowser default_browser = system_info->GetDefaultBrowser();
				switch (default_browser)
				{
				case DesktopOpSystemInfo::DEFAULT_BROWSER_OPERA:
					m_import_format = HotlistModel::OperaBookmark;     // import now
					break;
				case DesktopOpSystemInfo::DEFAULT_BROWSER_FIREFOX:
					m_import_format = HotlistModel::NetscapeBookmark; // will be imported after model is loaded
					break;
				case DesktopOpSystemInfo::DEFAULT_BROWSER_IE:
					m_import_format = HotlistModel::ExplorerBookmark; // will be imported after model is loaded
					break;
				default:
					m_import_format = HotlistModel::NoFormat;
					break;
				}
				if (m_import_format != HotlistModel::NoFormat)
				{
					if (OpStatus::IsError(system_info->GetDefaultBrowserBookmarkLocation(default_browser, m_import_path)) ||
						m_import_path.IsEmpty())
					{
						m_import_format = HotlistModel::NoFormat;
					}
				}
				if (m_import_format == HotlistModel::OperaBookmark)
				{
					OpFile srcfile;
					if (!IsIniFile(m_import_path) &&
						OpStatus::IsSuccess(srcfile.Construct(m_import_path)) &&
						OpStatus::IsSuccess(hotlistfile.CopyContents(&srcfile, FALSE)))
					{
						copy_default_bookmarks = false;

						// leave m_import_format set - this will trigger upgrade
						// of partner bookmarks after model is loaded
					}
					else
					{
						m_import_format = HotlistModel::NoFormat;
					}
					// no longer needed
					m_import_path.Empty();
				}
			}
			if (copy_default_bookmarks)
			{
				OpFile srcfile;
				if (OpStatus::IsSuccess(ResourceSetup::GetDefaultPrefsFile(DESKTOP_RES_PACKAGE_BOOKMARK, srcfile)))
				{
					OpStatus::Ignore(hotlistfile.CopyContents(&srcfile, FALSE));
				}
			}
		}
	}

	return m_model->Init();
}

/************************************************************************
 *
 * DropItem
 *
 *
 ************************************************************************/
BOOL DesktopBookmarkManager::DropItem(const BookmarkItemData& item_data, 
									  INT32 onto, 
									  DesktopDragObject::InsertType insert_type, 
									  BOOL execute, 
									  OpTypedObject::Type drag_object_type, 
									  INT32* first_id)
{
	if (!m_model->Loaded()) // DSK-351304
		return FALSE;

	if( execute )
	{
		HotlistModelItem* to = GetItemByID(onto);
		CoreBookmarkPos pos(to, insert_type);
		AddBookmark(&item_data, pos.Previous(), pos.Parent());
	}

	return TRUE;
}

OP_STATUS DesktopBookmarkManager::EditItem(INT32 id, DesktopWindow* parent)
{
	HotlistModelItem* item = m_model->GetItemByID(id);
	RETURN_VALUE_IF_NULL(item, OpStatus::ERR_NULL_POINTER);

	if (!item->IsFolder())
	{
		BookmarkPropertiesController* controller = OP_NEW(BookmarkPropertiesController, (item));
		RETURN_OOM_IF_NULL(controller);
		RETURN_IF_ERROR(ShowDialog(controller, g_global_ui_context, parent));
	}
	else
	{
		EditBookmarkDialog* dialog = OP_NEW(EditBookmarkDialog, (TRUE));
		RETURN_OOM_IF_NULL(dialog);
		RETURN_IF_ERROR(dialog->Init(parent, item));
	}

	return OpStatus::OK;
}

BOOL DesktopBookmarkManager::NewBookmark( const HotlistManager::ContactData& cd, INT32 parent_id, DesktopWindow* parent, BOOL interactive )
{
	BookmarkItemData item_data;
	item_data.name.Set(cd.name.CStr());
	item_data.url.Set(cd.url.CStr());
	item_data.panel_position = cd.in_panel ? 0 : -1;

	return NewBookmark(item_data, parent_id, parent, interactive );
}

BOOL DesktopBookmarkManager::NewBookmark( INT32 parent_id, DesktopWindow* parent_window )
{
	BookmarkItemData item_data; 
	return NewBookmark(item_data, parent_id, parent_window, TRUE); 
}

BOOL DesktopBookmarkManager::NewBookmark( BookmarkItemData& item_data, INT32 parent_id )
{
	return NewBookmark(item_data, parent_id, 0, FALSE); 
}

/************************************************************************
 *
 * NewBookmark
 *
 ************************************************************************/
BOOL DesktopBookmarkManager::NewBookmark( BookmarkItemData& item_data, INT32 parent_id, DesktopWindow* parent_window, BOOL interactive )
{
	// trying to add new bookmark before loading bookmarks.adr?
	OP_ASSERT(m_model->Loaded());

	HotlistModelItem* parent = m_model->GetItemByID(parent_id);

	if(interactive)
	{
		HotlistModelItem* item = m_model->GetByURL(item_data.url.CStr(), FALSE);
		
		// Check for duplicate
		if (item && !item->GetIsInsideTrashFolder())
		{
			EditBookmarkDialog* dialog = OP_NEW(EditBookmarkDialog, (TRUE));
			if (dialog)
				dialog->Init(parent_window, item);
		}
		else
		{
			AddBookmarkDialog* dialog = OP_NEW(AddBookmarkDialog, ());
			if (dialog)
				dialog->Init( parent_window, &item_data, parent);
		}
	}
	else
	{
		// Since we do not use a dialog we fill in the url address as name.
		if( !item_data.name.HasContent() )
		{
			item_data.name.Set(item_data.url);
		}

		AddBookmarkLast(&item_data, GetItemByID(parent_id));
	}

	return TRUE;
}

BOOL DesktopBookmarkManager::NewSeparator(INT32 parent_id)
{
	// trying to add new separator before loading bookmarks.adr?
	OP_ASSERT(m_model->Loaded());

	BookmarkItem* separator = OP_NEW(BookmarkItem, ());
	if (!separator)
		return FALSE;

	CoreBookmarkPos pos(GetItemByID(parent_id), DesktopDragObject::INSERT_INTO);
	return OpStatus::IsSuccess(g_bookmark_manager->AddSeparator(separator, pos.previous, pos.parent));
}

/************************************************************************
 *
 * NewBookmarkFolder
 *
 *
 ************************************************************************/
BOOL DesktopBookmarkManager::NewBookmarkFolder(const OpStringC& name, INT32 parent_id, OpTreeView* treeView )
{
	// trying to add new folder before loading bookmarks.adr?
	OP_ASSERT(m_model->Loaded());

	m_edit_folder_tree_view = treeView;

	HotlistModelItem* parent_folder = GetItemByID( parent_id );
	BookmarkItemData item_data;
	item_data.name.Set(name);
	item_data.type = FOLDER_NORMAL_FOLDER;
	AddBookmarkLast(&item_data, parent_folder);

	// Make sure treeview isn't matching text, to prevent the newly created item
	// being invisible
	if (treeView)
		OpStatus::Ignore(treeView->SetText(0));

	return TRUE;
}


/************************************************************************
 *
 * Rename
 *
 *
 ************************************************************************/

BOOL DesktopBookmarkManager::Rename( OpTreeModelItem *item, OpString& text )
{
	if (!item) return FALSE;
	HotlistModelItem* bookmark = static_cast<HotlistModelItem*>(item);
	
	bookmark->SetName( text.CStr() );
	
	return TRUE;
}

/************************************************************************
 *
 * CopyItems
 *
 * Copy all items to clipboard
 * 
 ************************************************************************/
void DesktopBookmarkManager::CopyItems(OpINT32Vector& id_list, BOOL handle_target_folder ) // Duh. Change name
{
	OpString global_clipboard_text;
	INT32 count =  id_list.GetCount();

	if (count > 0) 
		GetClipboard()->Clear();


	for (INT32 i = 0; i < count; i++)
	{
		// The easiest way to do this is if the id_list only contains top level items
		// and copying one means do the whole tree,
		// -- NO, there can be holes in the list
		HotlistModelItem* bookmark = GetItemByID(id_list.Get(i));

		if(bookmark && !bookmark->GetIsTrashFolder())
		{
			if (!handle_target_folder && bookmark->GetTarget().HasContent())
				continue;
			// If this is the first in the list. clipboard should be cleared
			// then this item should be added first (addlast)

			// OBS: This must add a copy of the item !!
			GetClipboard()->CopyItem(GetClipboard()->m_manager,BookmarkWrapper(bookmark)->GetCoreItem());

			// If this is not the first in the list, the item should be added relative
			// to its core parent and previous - 
			// the parent is not necessarily relevant if it's not also copied
			// the previous is neccessarily there
			// m_clipboard->AddNewItem(bookmark); // Add single, or relative to its parent and previous? Make two func.s for this!

			OpInfoText text;
			text.SetAutoTruncate(FALSE);

			bookmark->GetInfoText(text);

			if (text.HasStatusText())
			{
				if (global_clipboard_text.HasContent())
				{
					global_clipboard_text.Append(UNI_L("\n"));
				}

				global_clipboard_text.Append(text.GetStatusText().CStr());
			}
		}
	}
	
	if (global_clipboard_text.HasContent())
	{
		g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
		// Mouse selection
		g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken(), true);
#endif
	}

}

/************************************************************************
 *
 * Delete
 *
 *
 ************************************************************************/
void DesktopBookmarkManager::Delete(OpINT32Vector& id_list, BOOL real_delete, BOOL handle_target_folder)
{
	OpINT32Vector target_folders;

	// the lock will update the whole tree in EndChange, which
	// make it even slower when operating small amount of items
	BOOL use_model_lock = id_list.GetCount() > 50;

	if (use_model_lock)
		m_model->BeginChange();

	for(UINT32 i = 0; i < id_list.GetCount(); i++)
	{
		HotlistModelItem* item = m_model->GetItemByID(id_list.Get(i));

		// item might has been deleted when it's parent folder got deleted
		if(item)
		{
			// If they don't want to delete just stop.
			if (item->GetTarget().HasContent())
			{
				// currently the tree view model is not in sync with the BookmarkModel, 
				// don't do any UI stuff here, showing a dialog will run a message loop
				// and possibly paint the tree view thus crash!
				target_folders.Add(id_list.Get(i));
			}
			else
				m_model->DeleteItem(item, real_delete);
		}
	}
	
	if (use_model_lock)
		m_model->EndChange();

	for(UINT32 i = 0; i < target_folders.GetCount(); i++)
	{
		HotlistModelItem* item = m_model->GetItemByID(target_folders.Get(i));

		if (handle_target_folder && g_hotlist_manager->ShowTargetDeleteDialog(item))
		{
			m_model->DeleteItem(item, TRUE);
		}
	}
}

/************************************************************************
 *
 * CutItems
 *
 *
 ************************************************************************/
// OBS, TODO: Need a way to not delete the item when it is removed from core list of bookmarks?
void DesktopBookmarkManager::CutItems(OpINT32Vector& id_list)
{
	// OBS: TODO: This should be changed to just move the items over to the clipboard, instead of copying
	// But then it must still remove the wrappers from the quick model
	CopyItems(id_list, FALSE);
	Delete(id_list, TRUE, FALSE);
}


OP_STATUS DesktopBookmarkManager::PasteItems(HotlistModelItem* at, DesktopDragObject::InsertType insert_type)
{
	FolderModelItem* max_item_parent = at ? at->GetIsInMaxItemsFolder() : NULL;

	if(max_item_parent && max_item_parent->GetMaximumItemReached(GetClipboard()->GetCount()))
	{
		g_hotlist_manager->ShowMaximumReachedDialog(max_item_parent);
		return OpStatus::ERR;
	}

	return GetClipboard()->PasteToModel(*m_model, at, insert_type);
}



/************************************************************************
 *
 * GetItemValue
 *
 *
 ************************************************************************/

BOOL DesktopBookmarkManager::GetItemValue( OpTreeModelItem* item, BookmarkItemData& data/*, INT32& flag_changed*/ )
{
	HotlistModelItem*  hmi = static_cast<HotlistModelItem*>(item);
	
	if( !hmi)
	{
		return FALSE;
    }
	
	OpString tmp;
	data.name.Set(hmi->GetName());
	if (!hmi->IsFolder())
		data.url.Set( hmi->GetUrl() );

	data.image.Set( hmi->GetImage() );

	data.description.Set( hmi->GetDescription() );
	data.shortname.Set( hmi->GetShortName() );
	data.created.Set( hmi->GetCreatedString() );
	if (!hmi->IsFolder() )
		data.visited.Set( hmi->GetVisitedString() );

	data.direct_image_pointer = hmi->GetImage();

	INT32 pos = hmi->GetPersonalbarPos();
	data.personalbar_position = pos;
	if (!hmi->IsFolder())
	{
		pos = ((Bookmark*)hmi)->GetPanelPos();
		data.panel_position = pos;
    }

	if (hmi->GetHasDisplayUrl())
		data.display_url.Set(hmi->GetDisplayUrl());

	return TRUE;
}



/************************************************************************
 *
 * SetItemValue
 *
 *
 ************************************************************************/

BOOL DesktopBookmarkManager::SetItemValue( OpTreeModelItem *item, const BookmarkItemData& data, BOOL validate, UINT32 flag_changed )
{
	HotlistModelItem*  hmi = static_cast<HotlistModelItem*>(item); // ??
  
	if( !hmi )
	{
		return FALSE;
	}
	
	if ( flag_changed & HotlistModel::FLAG_NAME )
	{
		hmi->SetName( data.name.CStr() );
	}
	
	if( !hmi->IsFolder() && (flag_changed & HotlistModel::FLAG_URL))
	{
		if (hmi->GetUrl().Compare(data.url.CStr()) != 0)
		{
			hmi->SetUrl(data.url.CStr());
			// Reset the Partner ID and Display URL when user changed the url
			AddDeletedDefaultBookmark(hmi->GetPartnerID());
			hmi->SetPartnerID(NULL);	
			hmi->SetDisplayUrl(NULL);
		}
		
	}
	if ( flag_changed & HotlistModel::FLAG_DESCRIPTION )
	{
		hmi->SetDescription( data.description.CStr() );
	}
	
	if ( flag_changed & HotlistModel::FLAG_NICK )
	{
		if ( validate )
		{
			OpString tmp;
			tmp.Set( data.shortname );
			ReplaceIllegalSinglelineText( tmp );
			hmi->SetShortName( tmp.CStr());
		}
		else
		{
			hmi->SetShortName( data.shortname.CStr() );
		}
	}

	if ( flag_changed & HotlistModel::FLAG_PERSONALBAR )
	{
		hmi->SetPersonalbarPos( data.personalbar_position );
	}
	
	if ( flag_changed & HotlistModel::FLAG_PANEL )
	{
		hmi->SetPanelPos( data.panel_position );
	}	

	if ( flag_changed & HotlistModel::FLAG_SMALLSCREEN )
	{
		hmi->SetSmallScreen( data.small_screen );
	}	

	hmi->GetModel()->SetDirty( TRUE );
	hmi->Change( TRUE );
	g_bookmark_manager->SetSaveTimer();
	
	hmi->GetModel()->BroadcastHotlistItemChanged(hmi, FALSE, flag_changed);
	return TRUE;
}

void DesktopBookmarkManager::ClearSensitiveSettings(int flag, BOOL always_write_to_disk)
{
	BOOL changed = always_write_to_disk;

	for( INT32 i=0; i<m_model->GetCount(); i++ )
	{
		HotlistModelItem* bm = m_model->GetItemByIndex(i);
		if( bm )
		{
			BOOL item_changed = FALSE;
			if( flag & VisitedTime )
			{
				item_changed |= bm->ClearVisited();
				changed |= item_changed;
			}
			if( flag & CreatedTime )
			{
				item_changed |= bm->ClearCreated();
				changed |= item_changed;
			}

			if( item_changed )
			{
				m_model->BroadcastHotlistItemChanged(bm, HotlistModel::FLAG_CREATED | HotlistModel::FLAG_VISITED);
				m_model->Change(i, FALSE);
			}
		}
	}

	if( changed )
	{
		m_model->SetDirty( TRUE );
	}
}

void DesktopBookmarkManager::OnHotlistItemAdded(HotlistModelItem* item)
{
	if(m_edit_folder_tree_view && item->IsFolder())
	{
		INT32 pos = m_edit_folder_tree_view->GetItemByModelItem(item);
		m_edit_folder_tree_view->EditItem(pos);
		m_edit_folder_tree_view = NULL;
	}

	// Remember the partner bookmarks in user profile
	if (item->GetPartnerID().HasContent() && !FindDefaultBookmark(item->GetPartnerID()))
		m_default_bookmarks.Add(item->GetID());

	if (item->GetDisplayUrl().HasContent() && item->GetDisplayUrl().Compare(item->GetUrl()) != 0)
		m_default_bookmarks_with_display_url.Add(item->GetID());
}

void DesktopBookmarkManager::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
	// When a default bookmark is deleted add it to the black list to prevent
	// bringing it back when upgrading
	if (item->GetPartnerID().HasContent() && item->IsBookmark())
		AddDeletedDefaultBookmark(item->GetPartnerID());

	m_default_bookmarks.RemoveByItem(item->GetID());
	m_default_bookmarks_with_display_url.RemoveByItem(item->GetID());
}

void DesktopBookmarkManager::BroadcastBookmarkLoaded()
{
	CreateTrashFolderIfNeeded();
	UpgradeDefaultBookmarks();

	g_hotlist_manager->ImportCustomFileOnce();

	if (m_import_path.HasContent())
	{
		if (m_import_format != HotlistModel::OperaBookmark) // OperaBookmark is handled in Load
		{
			OP_ASSERT(m_import_format != HotlistModel::NoFormat);
			g_hotlist_manager->Import(0, m_import_format, m_import_path, TRUE, FALSE);
		}
		m_import_path.Empty(); // no longer needed
	}

#ifdef DEBUG_ENABLE_OPASSERT
	m_broadcasting_loaded = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	for (OpListenersIterator iterator(m_bookmark_listeners); m_bookmark_listeners.HasNext(iterator);)
		m_bookmark_listeners.GetNext(iterator)->OnBookmarkModelLoaded();
#ifdef DEBUG_ENABLE_OPASSERT
	m_broadcasting_loaded = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
}

OP_STATUS DesktopBookmarkManager::ImportAdrBookmark(const uni_char* path)
{
	BookmarkAdrStorage adr_parser(g_bookmark_manager, path, FALSE, TRUE);

	OP_STATUS ret = OpStatus::OK;

	// prevent core from automatically issuing any save request when we're busy reading
	g_bookmark_manager->SetSaveBookmarksTimeout(BookmarkManager::NO_AUTO_SAVE, 0);

	{
		GenericTreeModel::ModelLock lock(m_model);
		while (adr_parser.MoreBookmarks())
		{
			BookmarkItem* item = OP_NEW(BookmarkItem, ());
			if (!item)
			{
				ret = OpStatus::ERR_NO_MEMORY;
				break;
			}

			ret = adr_parser.LoadBookmark(item);
			if (OpStatus::IsError(ret))
			{
				OP_DELETE(item);
				if (ret != OpStatus::ERR_OUT_OF_RANGE)
					break;
			}
		}
	}

	m_model->SetDirty(TRUE);

	if (ret != OpStatus::OK && ret != OpStatus::ERR_OUT_OF_RANGE)
		return ret;

	// Force a complete sync
	// the imported items are not added to sync queue one by one since 
	// OpSyncItem::CommitItem(FALSE, TRUE) is too slow to do for a lot of items
	// (OpSyncDataCollection::Find is using a linear search!! )
	if (g_sync_manager->SupportsType(SyncManager::SYNC_BOOKMARKS))
	{
		TRAPD(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::CompleteSync, TRUE));
		g_sync_manager->SyncNow(SyncManager::Now, TRUE);
	}

	return OpStatus::OK;
}

BOOL DesktopBookmarkManager::IsIniFile(const OpString& name)
{
	if (name.Length() < 4 || name.FindI(UNI_L(".ini")) != name.Length() - 4)
		return FALSE;

	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(name), FALSE);

	// This is just in case the file is actually an Adr file which happens
	// sometimes due to sharing profile between different Opera installs
	RETURN_VALUE_IF_ERROR(file.Open(OPFILE_READ), FALSE);
	
	OpString8 line;
	file.ReadLine(line);
	file.Close();

	if (line.FindI("Opera Preferences") != 0)
		return FALSE;

	return TRUE;
}

OP_STATUS DesktopBookmarkManager::UpgradeDefaultBookmarks()
{
	Application::RunType run_type = g_application->DetermineFirstRunType();
	if (run_type == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		run_type == Application::RUNTYPE_FIRST ||
		// bookmarks were copied from the default Opera, which may be an older version
		m_import_format == HotlistModel::OperaBookmark ||
		g_run_type->m_added_custom_folder ||
		// usually when region changes default bookmarks also change, so bookmarks.adr
		// has to be upgraded
		// one exception is the first session - the flag is true (initial region is
		// always "new"), but default bookmarks for the region were copied in Load()
		(g_region_info->m_changed && run_type != Application::RUNTYPE_FIRSTCLEAN))
	{
		DefaultBookmarkUpgrader upgrader;
		return upgrader.Run();
	}
	return OpStatus::OK;
}

HotlistModelItem* DesktopBookmarkManager::FindDefaultBookmark(OpStringC partner_id)
{
	for (UINT32 i=0; i<m_default_bookmarks.GetCount(); i++)
	{
		HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_default_bookmarks.Get(i));
		if (item && item->GetPartnerID().CompareI(partner_id) == 0)
			return item;
	}
	return NULL;
}

BOOL DesktopBookmarkManager::IsDeletedDefaultBookmark(OpStringC partner_id)
{
	for (UINT32 i=0; i<m_deleted_default_bookmarks.GetCount(); i++)
	{
		if (m_deleted_default_bookmarks.Get(i)->CompareI(partner_id) == 0)
			return TRUE;
	}
	return FALSE;
}

void DesktopBookmarkManager::AddDeletedDefaultBookmark(OpStringC partner_id)
{
	if (partner_id.HasContent() && !IsDeletedDefaultBookmark(partner_id))
	{
		OpString* new_id = OP_NEW(OpString, ());
		if (new_id)
		{
			new_id->Set(partner_id);
			m_deleted_default_bookmarks.Add(new_id);
		}
	}
}

HotlistModelItem* DesktopBookmarkManager::FindDefaultBookmarkByURL(OpStringC url)
{
	for (UINT32 i=0; i<m_default_bookmarks_with_display_url.GetCount(); i++)
	{
		HotlistModelItem* item = g_hotlist_manager->GetItemByID(m_default_bookmarks_with_display_url.Get(i));
		
		if (item)
		{
			OpString resolved_url, resolved_display_url, resolved_input;
			TRAPD(err,	g_url_api->ResolveUrlNameL(item->GetUrl(), resolved_url);
						g_url_api->ResolveUrlNameL(item->GetDisplayUrl(), resolved_display_url);
						g_url_api->ResolveUrlNameL(url, resolved_input));

			if (OpStatus::IsSuccess(err) && (resolved_url.Compare(resolved_input) == 0 
				|| resolved_display_url.Compare(resolved_input) == 0))
				return item;
		}
	}
	return NULL;
}

/************************************************************
 *
 * AddCoreItem
 *
 * Adds a new item to the bookmark tree in core.
 *
 ****************************************************************/
OP_STATUS DesktopBookmarkManager::AddCoreItem(BookmarkItem* core_item,
										HotlistModelItem* previous, 
										HotlistModelItem* parent,
										BOOL last)
{
	if (!core_item)
		return FALSE;

	if (previous && !BookmarkWrapper(previous)->GetCoreItem())
		return FALSE;

	if (parent && !BookmarkWrapper(parent)->GetCoreItem())
		return FALSE;

	// Core allows adding several folders with type trash. 
	if (core_item->GetFolderType() == FOLDER_TRASH_FOLDER && g_bookmark_manager->GetTrashFolder())
		return FALSE;

	// parent shouldn't be trash
	if( parent && (parent->GetIsInsideTrashFolder() || parent->GetIsTrashFolder()) )
	{
		parent = NULL;
		previous = NULL;
	}

	// parent should be a folder, else use it as previous
	if( parent && !parent->IsFolder() )
	{
		if(!previous)
			previous = parent;
		parent = NULL;
	}

	if (core_item->GetFolderType() == FOLDER_NORMAL_FOLDER && parent && !parent->GetSubfoldersAllowed())
		return FALSE;

	BookmarkItem* core_parent	= parent ? BookmarkWrapper(parent)->GetCoreItem() : 0;
	BookmarkItem* core_previous = previous ? BookmarkWrapper(previous)->GetCoreItem() : 0;

	if (last)
	{
		if (core_parent)
			core_previous = static_cast<BookmarkItem*>(core_parent->LastChild());
		else
			core_previous = static_cast<BookmarkItem*>(g_bookmark_manager->GetRootFolder()->LastChild());
	}

	return OpStatus::IsSuccess(AddCoreItem(core_item, core_previous, core_parent));
}

/**********************************************************************************
 *
 * AddCoreItem
 *
 *
 *********************************************************************************/
OP_STATUS DesktopBookmarkManager::AddCoreItem(BookmarkItem* core_item, 
										   BookmarkItem* core_previous, 
										   BookmarkItem* core_parent)
{
	// Core only send ADD message to sync server when calling g_bookmark_manager->AddNewBookmark
	// but that function doesn't support previous, so set these fields ourselves and use AddBookmark
	core_item->SetAdded(TRUE);
	core_item->SetModified(FALSE);
	return g_bookmark_manager->AddBookmark(core_item, core_previous, core_parent ? core_parent : g_bookmark_manager->GetRootFolder());
}


/**********************************************************************************
 *
 * AddBookmark
 *
 *
 *********************************************************************************/
OP_STATUS DesktopBookmarkManager::AddBookmark(const BookmarkItemData* item_data,
									   HotlistModelItem* previous, 
									   HotlistModelItem* parent)
{
	BookmarkItem* core_item = OP_NEW(BookmarkItem, ());
	RETURN_OOM_IF_NULL(core_item);
	::SetDataFromItemData(item_data, core_item);
	return AddCoreItem(core_item, previous, parent);
}

OP_STATUS DesktopBookmarkManager::AddBookmarkLast(const BookmarkItemData* item_data, HotlistModelItem* parent)
{
	BookmarkItem* core_item = OP_NEW(BookmarkItem, ());
	RETURN_OOM_IF_NULL(core_item);
	::SetDataFromItemData(item_data, core_item);
	return AddCoreItem(core_item, NULL, parent, TRUE);
}

void DesktopBookmarkManager::CreateTrashFolderIfNeeded()
{
	static const char* unique_id = "14C645A5B8A3470FB3B52CC32C97E2B8";
	// Create a trash folder if it doesn't exist, but only after loading finished
	if (!m_model->GetTrashFolder())
	{
		BookmarkItemData item_data;
		item_data.type = FOLDER_TRASH_FOLDER;
		item_data.deletable = FALSE;
		if (!m_model->GetByUniqueGUID(unique_id))
			item_data.unique_id.Set(unique_id);
		g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, item_data.name);

		AddBookmarkLast(&item_data);
	}
}
