/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * psmaas - Patricia Aas Oct 2005
 */
#include "core/pch.h"

#include "adjunct/quick/models/DesktopHistoryModel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/util/winutils.h"
#include "modules/search_engine/VisitedSearch.h"

#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/WindowCommanderProxy.h"

#include "modules/debug/debug.h"


/*___________________________________________________________________________*/
/*                         HISTORY MODEL                                     */
/*___________________________________________________________________________*/
/***********************************************************************************
 ** Init - static factory method to ensure proper initialisation of the sigleton
 **          history model.
 **
 **
 ** @return
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::Init()
{
	HistoryModelTimeFolder** timefolders = OP_NEWA(HistoryModelTimeFolder*, NUM_TIME_PERIODS);

	if(!timefolders)
		return OpStatus::ERR_NO_MEMORY;

	// initialize the array so if it fails later, it won't crash in the destructor
	op_memset(timefolders, 0, sizeof(*timefolders) * NUM_TIME_PERIODS);

	OpHistoryModel* model = g_globalHistory;

	if(!model)
		return OpStatus::ERR;

	INT32 i;
	for(i = 0; i < NUM_TIME_PERIODS; i++)
	{
		timefolders[i] = OP_NEW(HistoryModelTimeFolder, (model->GetTimeFolder((TimePeriod)i)));

		if(!(timefolders[i]))
			break;
	}

	if(i < NUM_TIME_PERIODS)
	{
        //Out of memory - delete folders

		for(INT32 j = 0; j < i; j++)
			OP_DELETE(timefolders[j]);

		OP_DELETEA(timefolders);

		return OpStatus::ERR_NO_MEMORY;
	}

	m_model = model;
	m_timefolders = timefolders;

	m_model->AddListener(this);

	INT32 mode = g_pcui->GetIntegerPref(PrefsCollectionUI::HistoryViewStyle);

	m_mode = (mode >= 0 && mode < NUM_MODES) ? (History_Mode) mode : TIME_MODE;

	SetSortListener(this);
	OP_ASSERT(OpStatus::IsSuccess(g_favicon_manager->AddListener(this)));

	m_CoreValid = TRUE;

	m_bookmark_model = (g_hotlist_manager) ? g_hotlist_manager->GetBookmarksModel() : 0;

	if(m_CoreValid && m_bookmark_model)
	{
		m_bookmark_model->AddModelListener(this);

		// History is created _after_ hotlist parsing, 
		// so we need to force add of all bookmarks 
		AddBookmarks();
	}


	g_application->GetDesktopWindowCollection().AddListener(this);

	return OpStatus::OK;
}

/***********************************************************************************
 ** HistoryModel - Constructor
 **
 ** @param
 **
 **
 ***********************************************************************************/
DesktopHistoryModel::DesktopHistoryModel()
	: m_model(0),
	  m_timefolders(0)
{
}

/***********************************************************************************
 **
 ** AddBookmarks
 **
 ** DesktopHistoryModel is created _after_ parsing of bookmarks is finished,
 ** so the notifications about items added won't reach DesktopHistoryModel,
 ** therefore this function is needed to add the bookmarks
 **
 ***********************************************************************************/
void DesktopHistoryModel::AddBookmarks()
{
	OpVector<Bookmark> items;

	m_bookmark_model->GetBookmarkItemList(items);

	for(UINT32 i = 0; i < items.GetCount(); i++)
	{
		Bookmark * item = items.Get(i);
		AddBookmark(item);
		AddBookmarkNick(item->GetShortName(), item);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::AddBookmarkNick(const OpStringC & nick, Bookmark * item)
{
	if(item && item->IsBookmark())
	{
		if(m_CoreValid)
		{
			HistoryPage * core_bookmark_item = AddBookmark(item);

			if(!core_bookmark_item)
				return OpStatus::ERR;

			if(nick.IsEmpty())
				return OpStatus::OK;

			return m_model->AddBookmarkNick(nick, *core_bookmark_item);
		}
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryPage * DesktopHistoryModel::AddBookmark(Bookmark * item)
{
	if(item && item->IsBookmark())
	{
		if(m_CoreValid)
		{
			OpString url, display_url;
			url.Set(item->GetUrl()); //Try the url itself
			if (item->GetHasDisplayUrl())
				display_url.Set(item->GetDisplayUrl());

			if(url.IsEmpty())
			{
				return NULL;
			}

			if(!m_model->IsAcceptable(url.CStr()))
			{
				url.Set(item->GetResolvedUrl()); //If it doesn't work try the resolved url
			}

			if(m_model->IsAcceptable(url.CStr()))
			{
				OpStringC name      = item->GetName();
				OpStringC shortname = item->GetShortName();

				OpString title;

				if(name.CStr())
				{
					title.Set(name.CStr());
					title.Append(" ");
				}

				if(shortname.CStr())
				{
					title.Append("(");
					title.Append(shortname.CStr());
					title.Append(")");
				}

				if(title.IsEmpty())
					title.Set(url.CStr());

				HistoryPage * history_item = 0, *dummy_item = 0;

				OP_ASSERT(url.HasContent());
				OP_STATUS status = m_model->AddBookmark(url.CStr(), title.CStr(), history_item);

				// Also mark the display url as bookmarked if any, to prevent it appearing in history
				// result when searching(it's already searched as a bookmark)
				if (display_url.HasContent())
					OpStatus::Ignore(m_model->AddBookmark(display_url.CStr(), title.CStr(), dummy_item));

				if (OpStatus::IsSuccess(status) && history_item)
				{
					item->SetHistoryItem(history_item);
				}

				return (HistoryPage *) history_item;
			}
		}
	}
	return NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::RemoveBookmark(Bookmark * item)
{
	if(item)
	{
		HistoryPage* core_item = item->GetHistoryItem();
		if (core_item)
		{
			// Do not listen to the history item any more
			core_item->RemoveListener(item);	

			// Get the address of the history item
			OpString address;
			core_item->GetAddress(address);

			// Only RemoveBookmark from core history if no more bookmarks listen to this item.
			BOOL last_bookmark = FALSE;

			OpVector<HistoryItem::Listener> result;
			if(OpStatus::IsSuccess(core_item->GetListenersByType(OpTypedObject::BOOKMARK_TYPE, result)))
				last_bookmark = (result.GetCount() == 0);

			if(last_bookmark)
			{
				m_model->RemoveBookmark(address.CStr());
			}

			// Remove referance to history item
			item->SetHistoryItem(NULL);
		}
	}
}


/***********************************************************************************
 ** HistoryModel - Destructor
 **
 **
 **
 **
 ***********************************************************************************/
DesktopHistoryModel::~DesktopHistoryModel()
{
	// Delete items that are not in the ui :
	while(m_deletable_items.GetCount())
	{
		HistoryModelPage * item = m_deletable_items.Remove(0);

		if(!item->IsInHistory())
		{
			DeleteItem(item, TRUE);
		}
	}

	// Delete items in the ui :
	ClearModel();

	if(m_timefolders)
	{
		for(INT32 i = 0; i < NUM_TIME_PERIODS; i++)
		{
			if(m_timefolders[i])
			{
				m_timefolders[i]->Remove();
				OP_DELETE(m_timefolders[i]);
			}
		}
		OP_DELETEA(m_timefolders);
	}
	if(m_CoreValid && m_model) m_model->RemoveListener(this);
	if(g_favicon_manager) 
	{
		OP_ASSERT(OpStatus::IsSuccess(g_favicon_manager->RemoveListener(this)));
	}

	m_model = 0;
	m_CoreValid = FALSE;

	g_pcui->WriteIntegerL(PrefsCollectionUI::HistoryViewStyle, m_mode);

	g_application->GetDesktopWindowCollection().RemoveListener(this);

	if (m_bookmark_model)
		m_bookmark_model->RemoveModelListener(this);
}

/***********************************************************************************
 ** ChangeMode
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::ChangeMode(INT32 mode)
{
	if(mode == TIME_MODE     ||
	   mode == SERVER_MODE   ||
#ifdef HISTORY_BOOKMARK_VIEW
	   mode == BOOKMARK_MODE ||
#endif // HISTORY_BOOKMARK_VIEW
	   mode == OLD_MODE)
	{
		GenericTreeModel::ModelLock lock(this);

		CleanUp();

		m_mode        = (DesktopHistoryModel::History_Mode) mode;
		m_initialized = FALSE;

		switch(m_mode)
		{
			case TIME_MODE     : MakeTimeVector();     break;
			case SERVER_MODE   : MakeServerVector();   break;
			case OLD_MODE      : MakeOldVector();      break;
#ifdef HISTORY_BOOKMARK_VIEW
			case BOOKMARK_MODE : MakeBookmarkVector(); break;
#endif // HISTORY_BOOKMARK_VIEW
		}

		m_initialized = TRUE;
	}
}

/***********************************************************************************
 ** CleanUp
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::CleanUp()
{
	for(INT32 i = 0; i < GetCount(); i++)
	{
		HistoryModelItem* item = GetItemByIndex(i);

		if(item->GetType() == OpTypedObject::FOLDER_TYPE)
		{
			HistoryModelFolder* folder = (HistoryModelFolder*) item;

			if(folder->GetFolderType() == PREFIX_FOLDER)
			{
				folder->Remove(FALSE);
				OP_DELETE(folder);
				i--;
			}
		}
	}
	RemoveAll();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnHotlistItemAdded(HotlistModelItem* item)
{
	if(item->IsBookmark() && !item->GetIsInsideTrashFolder())
	{
		Bookmark * bookmark_item = (Bookmark *) item;
		AddBookmark(bookmark_item);
		AddBookmarkNick(bookmark_item->GetShortName(), bookmark_item);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag)
{
	if(item->IsBookmark() && !item->GetIsInsideTrashFolder())
	{
		Bookmark * bookmark_item = static_cast<Bookmark*>(item);
		if(changed_flag & HotlistModel::FLAG_NICK)
		{
			AddBookmarkNick(bookmark_item->GetShortName(), bookmark_item);
		}
		else if(changed_flag & HotlistModel::FLAG_NAME || changed_flag & HotlistModel::FLAG_URL)
		{
			g_hotlist_manager->RemoveBookmarkFromHistory(item);
			OnHotlistItemAdded(item);
		}
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
	if(item->IsBookmark() && !item->GetIsInsideTrashFolder())
	{
		Bookmark * bookmark_item = (Bookmark *) item;
		RemoveBookmark(bookmark_item);
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnHotlistItemTrashed(HotlistModelItem* item)
{
	if(item->IsBookmark())
	{
		Bookmark * bookmark_item = (Bookmark *) item;
		RemoveBookmark(bookmark_item);
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnHotlistItemUnTrashed(HotlistModelItem* item)
{
	if(item->IsBookmark() && !item->GetIsInsideTrashFolder())
	{
		Bookmark * bookmark_item = (Bookmark *) item;
		AddBookmark(bookmark_item);
		AddBookmarkNick(bookmark_item->GetShortName(), bookmark_item);
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnFavIconAdded(const uni_char* document_url,
										 const uni_char* image_path)
{
	if (!document_url) return;

	HistoryModelPage* item = GetItemByUrl(document_url);

	if(item)
	{
		item->UpdateSimpleItem();
		item->Change( TRUE ); 
	}
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnFavIconsRemoved()
{
	// TODO : what to do?
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelPage* DesktopHistoryModel::GetItemByDesktopWindow(DesktopWindow* window)
{
	if(!window || window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return NULL;

	DocumentDesktopWindow* document_window = static_cast<DocumentDesktopWindow*>(window);

	URL url = WindowCommanderProxy::GetMovedToURL(document_window->GetWindowCommander());

	OpString url_string;
	RETURN_VALUE_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, url_string, FALSE), NULL);
	return GetItemByUrl(url_string.CStr());
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnDesktopWindowAdded(DesktopWindow* window)
{
	HistoryModelPage* item = GetItemByDesktopWindow(window);

	if(item)
		item->SetOpen(TRUE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnDesktopWindowRemoved(DesktopWindow* window)
{
	HistoryModelPage* item = GetItemByDesktopWindow(window);

	if(item)
		item->SetOpen(FALSE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,
													 const OpStringC& url)
{
	HistoryModelPage* old_item = GetItemByDesktopWindow(document_window);

	if(old_item)
		old_item->SetOpen(FALSE);

	HistoryModelPage* new_item = GetItemByUrl(url);

	if(new_item)
		new_item->SetOpen(TRUE);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelPage* DesktopHistoryModel::GetPage(HistoryPage* core_item)
{
	HistoryModelPage* item = static_cast<HistoryModelPage*>(GetListener(core_item));

#ifdef DEBUG
	//Item should have listener
	BOOL listener_status_valid =
		(item                      ||
		 core_item->IsBookmarked() ||
		 core_item->IsOperaPage()  ||
		 core_item->IsNick());
	
	OP_ASSERT(listener_status_valid);
#endif

	if(item)
		return item;

	item = OP_NEW(HistoryModelPage, (core_item));
	if (!item)
		return NULL;

	if (!item->GetSimpleItem() ||	// OOM during construction
		OpStatus::IsError(m_deletable_items.Add(item)))
	{
		OP_DELETE(item);
		return NULL;
	}

	return item;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryItem::Listener* DesktopHistoryModel::GetListener(HistoryItem* core_item)
{
	if(!core_item)
		return 0;

	return core_item->GetListenerByType(core_item->GetType());
}


/***********************************************************************************
 ** OnItemAdded
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnItemAdded(HistoryPage* core_item, BOOL save_session)
{
	HistoryItem::Listener* listener = GetListener(core_item);
	HistoryModelPage* item = 0;

	if(!listener)
	{
		item = OP_NEW(HistoryModelPage, (core_item));

		if (!item)
			return;

		if (!item->GetSimpleItem())	// OOM during construction
		{
			OP_DELETE(item);
			return;
		}
	}
	else
	{
		item = (HistoryModelPage*) listener;
		item->UpdateSimpleItem();
	}

	HistoryModelSiteFolder* site = item->GetSiteFolder();

	if(m_mode == OLD_MODE)
	{
		if(!item->GetModel())
			AddItem_To_OldVector(item, TRUE);
	}
	else if(m_mode == TIME_MODE)
	{
		if(!item->GetModel())
			AddItem_To_TimeVector(item);
	}
	else if(m_mode == SERVER_MODE)
	{
		if(!site->GetModel())
			AddSorted(site);

		if(!item->GetModel())
		{
			INT32 site_ind = GetIndexByItem(site);
			AddSorted(item, site_ind);
		}
	}

	if (save_session)
		if (g_session_auto_save_manager)
			g_session_auto_save_manager->SaveCurrentSession();

	if(item->m_moving)
	{
		item->m_old_parent_id = 0;
		item->m_moving = FALSE;
	}
}

/* ------------------ HistoryModel::Listener interface ------------------------*/

/***********************************************************************************
 ** OnItemRemoving - implementing the HistoryModel::Listener interface
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnItemRemoving(HistoryItem* core_item)
{
	HistoryItem::Listener* listener = GetListener(core_item);

	if(!listener)
		return;

	DeleteItem((HistoryModelItem*) listener, FALSE);
}

/***********************************************************************************
 ** OnItemDeleting - implementing the HistoryModel::Listener interface
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnItemDeleting(HistoryItem* core_item)
{
	HistoryItem::Listener* listener = GetListener(core_item);

	if(!listener)
		return;

	DeleteItem((HistoryModelItem*) listener, TRUE);
}


/***********************************************************************************
 ** OnItemMoving - implementing the HistoryModel::Listener interface
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnItemMoving(HistoryItem* core_item)
{
	HistoryItem::Listener* listener = GetListener(core_item);
	if(!listener)
		return;

	HistoryModelItem* item  = (HistoryModelItem*) listener;

	if(!item)
		return;

	HistoryModelItem* folder = item->GetParentItem();

	if(folder) item->m_old_parent_id = folder->GetID();
	item->m_moving = TRUE;

	BOOL remove_item = TRUE;

/*
  Time:
  If this is time mode and this item is in a today folder and is
  the last child in this folder - do not remove it - since this
  will result in the folder being closed.

  Server:
  If this is the last child of this folder - do not remove it -
  since this will result in the folder being closed
*/
	if(m_mode == TIME_MODE)
	{
		HistoryModelSiteFolder * site  = item->GetSiteFolder();

		if(site) //Should always be true
		{
			HistoryModelSiteSubFolder * today = site->GetSubFolder(TODAY);

			if(today && folder && today == folder && folder->GetChildCount() == 1)
			{
				remove_item = FALSE;
			}
		}
	}
	else if(m_mode == SERVER_MODE)
	{
		if(folder && folder->GetChildCount() == 1)
		{
			remove_item = FALSE;
		}
	}

	if(remove_item)
		Remove(item);

	if(m_mode == TIME_MODE && folder && folder->GetChildCount() == 0)
		folder->Remove();
}

/***********************************************************************************
 ** OnModelChanging - implementing the HistoryModel::Listener interface
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnModelChanging()
{
	CleanUp();
}

/***********************************************************************************
 ** OnModelChanged - implementing the HistoryModel::Listener interface
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnModelChanged()
{
	ChangeMode(GetMode());
}


/***********************************************************************************
 ** OnModelDeleted - implementing the HistoryModel::Listener interface
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::OnModelDeleted()
{
	ClearModel();
	m_model = 0;
	m_CoreValid = FALSE;
}

/* --------------------------------------------------------------------------------*/


/***********************************************************************************
 ** DeleteItem
 **
 ** @param
 **
 **
 *********************************************************************************/
void DesktopHistoryModel::DeleteItem(HistoryModelItem* item, BOOL deleteParent)
{
	HistoryModelSiteFolder* parent = 0;
	HistoryModelItem* folder = item->GetParentItem();

	if(item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
		parent = item->GetSiteFolder(FALSE);

	Remove(item);

	if(item->IsDeletable())
	{
		if(item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
		{
			m_deletable_items.RemoveByItem((HistoryModelPage*) item);
		}

		BroadcastHistoryModelItemDestroyed(item);

		OP_DELETE(item);
	}

	if(parent && deleteParent && parent->IsEmpty())
		DeleteItem((HistoryModelSiteFolder*) parent, FALSE);

 	if(m_mode == TIME_MODE && folder && folder->GetChildCount() == 0)
 		folder->Remove();
}


/***********************************************************************************
 ** Delete
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::Delete(HistoryModelItem* item)
{
	if(m_CoreValid)
		m_model->Delete(item->GetCoreItem());
}


/***********************************************************************************
 ** Remove
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::Remove(HistoryModelItem* item)
{
	if(item->GetType() == OpTypedObject::FOLDER_TYPE)
		RemoveFolder((HistoryModelFolder *) item);
	else if(item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
		RemoveItem(item);
}


/***********************************************************************************
 ** RemoveFolder
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::RemoveFolder(HistoryModelFolder* folder)
{
	if(!folder->IsSticky())
		folder->Remove(FALSE);
}


/***********************************************************************************
 ** RemoveItem
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::RemoveItem(HistoryModelItem* item)
{
	if(item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
	{
		item->Remove();
	}
}




/***********************************************************************************
 ** ClearModel
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::ClearModel()
{
	//If the model contains sticky folders they have to be removed

	if(m_mode == TIME_MODE)
	{
		for(INT32 i = 0; i < NUM_TIME_PERIODS; i++)
		{
			m_timefolders[i]->Remove(FALSE);
		}
	}

	for(INT32 i = 0; i < GetCount(); i++)
	{
		HistoryModelItem* item = GetItemByIndex(i);

		if(item->GetType() == OpTypedObject::FOLDER_TYPE)
		{
			HistoryModelFolder* folder = (HistoryModelFolder*) item;

			if(folder->GetFolderType() == PREFIX_FOLDER)
			{
				folder->Remove(FALSE);
				OP_DELETE(folder);
				i--;
			}
		}
	}

	while(GetCount())
	{
		HistoryModelItem* item = GetItemByIndex(0);
		DeleteItem(item, TRUE);
	}
}


/***********************************************************************************
 ** FindItem
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
HistoryModelItem* DesktopHistoryModel::FindItem(INT32 id)
{
	return GetItemByID(id);
}

/***********************************************************************************
 ** GetItemByUrl
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
HistoryModelPage* DesktopHistoryModel::GetItemByUrl(const OpStringC & search_url,
											 BOOL do_guessing)
{
	if(search_url.IsEmpty())
		return 0;

	if(m_CoreValid)
	{
		HistoryModelPage * page = LookupUrl(search_url);

		if(page)
		{
			// Do exact search first
			return page;
		}
		else if(do_guessing)
		{
			// Append a slash and try again
			OpString tmp;
			tmp.AppendFormat(UNI_L("%s/"), search_url.CStr());

			return LookupUrl(tmp);
		}
	}

	return 0;
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModelPage* DesktopHistoryModel::LookupUrl(const OpStringC & search_url)
{
	if(m_CoreValid)
	{
		if(m_model->IsAcceptable(search_url.CStr()))
		{
			HistoryPage* core_item = 0;

			m_model->GetItem(search_url, core_item);

			if(core_item)
			{
				HistoryItem::Listener* listener = GetListener(core_item);

				return (HistoryModelPage*) listener;
			}
		}
	}

	return 0;
}

/***********************************************************************************
 ** SearchContent
 **
 ** @param search_text  - the text to search for
 ** @param items        - the items that matched the search text
 ** @param content_text - the array of the matched text matches the above items item for item
 **
 ***********************************************************************************/
// OP_STATUS DesktopHistoryModel::SearchContent(const OpStringC & search_text,
// 									  OpVector<OpString> & words,
// 									  OpVector<HistoryModelItem> & items,
// 									  OpVector<OpString> & content_text)
// {
	
// 	RETURN_IF_ERROR(g_visited_search->Search(search_text.CStr(),
// 											 VisitedSearch::RankSort,
// 											 500,
// 											 UNI_L("<b>"),
// 											 UNI_L("</b>"),
// 											 16,
// 											 this));

// 	return OpStatus::OK;
// }


/***********************************************************************************
 ** MergeSort (copied and modified from optreemodel)
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::MergeSort(UINTPTR* array, UINTPTR* temp_array, INT32 left, INT32 right)
{
	if (left < right)
	{
		INT32 center = (left + right) / 2;
		MergeSort(array, temp_array, left, center);
		MergeSort(array, temp_array, center + 1, right);
		Merge(array, temp_array, left, center + 1, right);
	}
}


/***********************************************************************************
 **  Merge
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::Merge(UINTPTR* array, UINTPTR* temp_array, INT32 left, INT32 right, INT32 right_end)
{
	INT32 left_end = right - 1;
	INT32 temp_pos = left;
	INT32 count = right_end - left + 1;

	while (left <= left_end && right <= right_end)
	{
		HistoryModelPage* left_item  = (HistoryModelPage*) array[left];
		HistoryModelPage* right_item = (HistoryModelPage*) array[right];

		temp_array[temp_pos++] = ComparePopularity(left_item, right_item) <= 0 ?
			array[left++] :
			array[right++];
	}

	while (left <= left_end)
	{
		temp_array[temp_pos++] = array[left++];
	}

	while (right <= right_end)
	{
		temp_array[temp_pos++] = array[right++];
	}

	for (INT32 i = 0; i < count; i++, right_end--)
	{
		array[right_end] = temp_array[right_end];
	}
}


/***********************************************************************************
 ** ComparePopularity
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
INT32 DesktopHistoryModel::ComparePopularity(HistoryModelPage* item_1, HistoryModelPage* item_2)
{
	if(item_1->GetAverageInterval() < item_2->GetAverageInterval())
		return -1;

	if(item_1->GetAverageInterval() > item_2->GetAverageInterval())
		return 1;

	return 0;
}


/***********************************************************************************
 ** GetTopItems
 **
 ** @param
 **
 ** @return OpStatus::OK (or OpStatus::ERR_NO_MEMORY)
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::GetTopItems(OpVector<HistoryModelPage> &itemVector, INT32 num_items)
{
	if(m_CoreValid)
	{
		INT32  table_count = m_model->GetCount();
		UINTPTR* sort_array  = OP_NEWA(UINTPTR, table_count * 2);
		if (!sort_array)
			return OpStatus::ERR_NO_MEMORY;

		UINTPTR* temp_array  = sort_array + table_count;

		HistoryPage* core_item = 0;

		INT32 i;

		for(i = 0; i < table_count; i++)
		{
			core_item = m_model->GetItemAtPosition(i);
			sort_array[i] = 0;

			if(core_item)
			{
				HistoryItem::Listener* listener = GetListener(core_item);
				HistoryModelPage* item;

				if(!listener)
				{
					item = OP_NEW(HistoryModelPage, (core_item));

					if (!item)
						break;

					if (!item->GetSimpleItem())	// OOM during construction
					{
						OP_DELETE(item);
						break;
					}
				}
				else
				{
					item = (HistoryModelPage*) listener;
				}

				sort_array[i] = (UINTPTR) item;
			}
			else
				break;
		}

		//Number of items retrieved should be equal the number of items in the model
		OP_ASSERT(i == table_count);

		MergeSort(sort_array, temp_array, 0, table_count - 1);

		for(INT32 j = 0, index = 0; j < num_items && index < i; index++)
		{
			HistoryModelPage* item = (HistoryModelPage*) sort_array[index];

			if(item->GetAverageInterval() >= 0)
			{
				itemVector.Add(item);
				j++;
			}
		}

		OP_DELETEA(sort_array);
	}
	return OpStatus::OK;
}

OP_STATUS DesktopHistoryModel::GetSimpleItems(const OpStringC& search_text, OpVector<HistoryAutocompleteModelItem> &itemVector)
{
	OpVector<HistoryPage> result;

	RETURN_IF_ERROR(g_globalHistory->GetItems(search_text.CStr(), result));

	HistoryModelPage* page = 0;
	
	for(UINT i = 0; i < result.GetCount(); i++)
	{
		HistoryPage* history_page = result.Get(i);
		page = GetPage(history_page);

		if(!history_page || !page)
			break;

		// This is the display url, it is handled elsewhere.
		if(history_page->IsBookmarked() && !history_page->GetListenerByType(OpTypedObject::BOOKMARK_TYPE))
			continue;

		HistoryAutocompleteModelItem* item = page->GetSimpleItem();

		if (history_page->IsBookmarked() || history_page->IsNick())
		{
			item->SetSearchType(HistoryAutocompleteModelItem::BOOKMARK);
			RETURN_IF_ERROR(itemVector.Add(item));
		}
		else
		{
			item->SetSearchType(PageBasedAutocompleteItem::HISTORY);
			RETURN_IF_ERROR(itemVector.Add(item));
		}
	}

	return OpStatus::OK;
}

OP_STATUS DesktopHistoryModel::GetOperaPages(const OpStringC& search_page, OpVector<PageBasedAutocompleteItem>& opera_pages)
{
	OpString whole_string;
	RETURN_IF_ERROR(whole_string.Set("opera:"));
	RETURN_IF_ERROR(whole_string.Append(search_page));
	OpVector<HistoryPage> items;
	RETURN_IF_ERROR(g_globalHistory->GetItems(whole_string.CStr(), items));
	for(UINT i = 0; i < items.GetCount(); ++i)
	{
		HistoryPage* history_page = items.Get(i);
		HistoryModelPage* page = GetPage(history_page);
		if (page && history_page->IsOperaPage())
		{
			RETURN_IF_ERROR(opera_pages.Add(page->GetSimpleItem()));
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::DeleteAllItems()
{
	if(m_CoreValid)
	{
		m_model->DeleteAllItems();
		m_model->Save(TRUE);

		if(g_url_api)
		{
			URL history_url = g_url_api->GetURL("opera:history");
			history_url.Unload();
		}
	}
}

/***********************************************************************************
 ** Save
 **
 **
 **
 ** @return OpStatus::OK (or result of core history Save())
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::Save(BOOL force)
{
	if(m_CoreValid)
	{
		return m_model->Save(force);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL DesktopHistoryModel::IsLocalFileURL(const OpStringC & url, OpString & stripped)
{
	if(m_CoreValid)
	{
		return m_model->IsLocalFileURL(url, stripped);
	}

	return FALSE;
}

/***********************************************************************************
 ** OnCompareItems
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
INT32 DesktopHistoryModel::OnCompareItems(OpTreeModel* tree_model,
										  OpTreeModelItem* item0,
										  OpTreeModelItem* item1)
{
	if(item0->GetType() != OpTypedObject::FOLDER_TYPE ||
	   item1->GetType() != OpTypedObject::FOLDER_TYPE )
		return 0;

	HistoryModelItem *folder0 = static_cast<HistoryModelItem*>(item0);
	HistoryModelItem *folder1 = static_cast<HistoryModelItem*>(item1);

	return uni_stricmp(folder0->GetTitle().CStr(), folder1->GetTitle().CStr());
}


/***********************************************************************************
 ** GetTimeFolder
 **
 ** @param
 **
 ** @return
 ***********************************************************************************/
HistoryModelTimeFolder* DesktopHistoryModel::GetTimeFolder(TimePeriod period)
{
	return m_timefolders[period];
}

/***********************************************************************************
 ** AddItem_To_TimeVector
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::AddItem_To_TimeVector(HistoryModelPage* item)
{
	HistoryModelSiteFolder* site = item->GetSiteFolder();

    //Item must have site
	OP_ASSERT(site);

	if(!site)
		return;

    //Site must be a folder
	OP_ASSERT(site->GetType() == OpTypedObject::FOLDER_TYPE);

	if(site->GetType() != OpTypedObject::FOLDER_TYPE)
		return;

	time_t now = g_timecache->CurrentTime();
	time_t acc = item->GetAccessed();
	time_t elapsed = now - acc;

	time_t today_sec      = m_model->SecondsSinceMidnight();
	time_t yesterday_sec  = today_sec + 24*60*60;
	time_t last_week_sec  = yesterday_sec + 7*24*60*60;
	time_t last_month_sec = last_week_sec + 28*24*60*60;

	TimePeriod period;

	if (elapsed < today_sec)  //Since midnight
	{
		period = TODAY;
	}
	else if (elapsed < yesterday_sec) //Yesterday
	{
		period = YESTERDAY;
	}
	else if (elapsed < last_week_sec) //The week before yesterday
	{
		period = WEEK;
	}
	else if (elapsed < last_month_sec)  //Last 4 weeks before last week
	{
		period = MONTH;
	}
	else //Older
	{
		period = OLDER;
	}

	HistoryModelSiteSubFolder* subfolder = site->GetSubFolder(period);
	if (subfolder)		// GetSubFolder returns NULL on OOM
	{
		if(!subfolder->GetModel())
		{
			INT32 index = GetIndexByItem(GetTimeFolder(period));
			AddSorted(subfolder, index);
		}
		subfolder->AddChildFirst(item);
	}
}


/***********************************************************************************
 ** AddItem_To_OldVector
 **
 ** @param
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::AddItem_To_OldVector(HistoryModelPage* item, BOOL first)
{

	BOOL test = FALSE;

	if(test)
	{
		time_t now = g_timecache->CurrentTime();
		time_t acc = item->GetAccessed();
		time_t elapsed = now - acc;

		time_t today_sec      = m_model->SecondsSinceMidnight();
		time_t yesterday_sec  = today_sec + 86400;
		time_t last_week_sec  = yesterday_sec + 604800;
		time_t last_month_sec = last_week_sec + 2419200;

		if (elapsed < today_sec)  //Since midnight
		{
			if(first)
				GetTimeFolder(TODAY)->AddChildFirst(item);
			else
				GetTimeFolder(TODAY)->AddChildLast(item);
		}
		else if (elapsed < yesterday_sec) //Yesterday
		{
			GetTimeFolder(YESTERDAY)->AddChildLast(item);
		}
		else if (elapsed < last_week_sec) //The week before yesterday
		{
			GetTimeFolder(WEEK)->AddChildLast(item);
		}
		else if (elapsed < last_month_sec) //Last 4 weeks before last week
		{
			GetTimeFolder(MONTH)->AddChildLast(item);
		}
		else //Older
		{
			GetTimeFolder(OLDER)->AddChildLast(item);
		}
	}
	else
		AddFirst(item);
}


/***********************************************************************************
 ** MakeTimeVector
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::MakeTimeVector()
{
	if(m_CoreValid)
	{
		HistoryPage*      core_item = 0;
		HistoryModelPage* item      = 0;

		AddLast(GetTimeFolder(TODAY));
		AddLast(GetTimeFolder(YESTERDAY));
		AddLast(GetTimeFolder(WEEK));
		AddLast(GetTimeFolder(MONTH));
		AddLast(GetTimeFolder(OLDER));

		INT32 i;

		for(i = 0; i < m_model->GetCount(); i++)
		{
			core_item = m_model->GetItemAtPosition(i);

			if(core_item)
			{
				HistoryItem::Listener* listener = GetListener(core_item);

				if(!listener)
				{
					item = OP_NEW(HistoryModelPage, (core_item));

					if (!item)
						continue;

					if (!item->GetSimpleItem())	// OOM during construction
					{
						OP_DELETE(item);
						continue;
					}
				}
				else
				{
					item = (HistoryModelPage*) listener;
				}

				item->UpdateSimpleItem();

				AddItem_To_TimeVector(item);
			}
			else
				break;
		}
	}
}


/***********************************************************************************
 ** MakeServerVector
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::MakeServerVector()
{
	if(m_CoreValid)
	{
		HistoryPage* core_item = 0;
		OpVector<HistoryPage>  loc_vector;

		HistoryModelPage*     item      = 0;
		m_model->GetHistoryItems(loc_vector);

		for(UINT32 j = 0; j < loc_vector.GetCount(); j++)
		{
			core_item = (HistoryPage*) loc_vector.Get(j);

			if(core_item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
			{
				HistoryItem::Listener* listener = GetListener(core_item);

				if(!listener)
				{
					item = OP_NEW(HistoryModelPage, (core_item));

					if (!item)
						continue;

					if (!item->GetSimpleItem())	// OOM during construction
					{
						OP_DELETE(item);
						continue;
					}
				}
				else
				{
					item = (HistoryModelPage*) listener;
				}

				HistoryModelSiteFolder * folder = item->GetSiteFolder();

				OP_ASSERT(folder); // All items should have a site folder

				if(!folder) // Recovery
					continue;

				if(!folder->GetModel())
				{
					AddLast(folder);
				}

				item->UpdateSimpleItem();

				folder->AddChildLast(item);
			}
		}
	}
}

#ifdef HISTORY_BOOKMARK_VIEW
/***********************************************************************************
 ** MakeBookmarkVector
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::MakeBookmarkVector()
{
	if(m_CoreValid)
	{
		HistoryPage* core_item = 0;
		OpVector<HistoryPage>  loc_vector;

		HistoryModelPage*     item      = 0;

		m_model->GetBookmarkItems(loc_vector);

		for(UINT32 j = 0; j < loc_vector.GetCount(); j++)
		{
			core_item = (HistoryPage*) loc_vector.Get(j);

			if(core_item->GetType() == OpTypedObject::HISTORY_ELEMENT_TYPE)
			{
				HistoryItem::Listener* listener = GetListener(core_item);

				if(!listener)
				{
					item = OP_NEW(HistoryModelPage, (core_item));

					if (!item)
						continue;

					if (!item->GetSimpleItem())	// OOM during construction
					{
						OP_DELETE(item);
						continue;
					}
				}
				else
				{
					item = (HistoryModelPage*) listener;
				}

				HistoryModelSiteFolder * folder = item->GetSiteFolder();

				OP_ASSERT(folder); // All items should have a site folder

				if(!folder) // Recovery
					continue;

				if(!folder->GetModel())
				{
					AddLast(folder);
				}

				item->UpdateSimpleItem();

				folder->AddChildLast(item);
			}
		}
	}
}
#endif //HISTORY_BOOKMARK_VIEW


/***********************************************************************************
 ** MakeOldVector
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::MakeOldVector()
{
	if(m_CoreValid)
	{
		HistoryModelPage * item = 0;
		HistoryPage * core_item = 0;

		for(INT32 i = 0; i < m_model->GetCount(); i++)
		{
			core_item = (HistoryPage*) m_model->GetItemAtPosition(i);
			HistoryItem::Listener* listener = GetListener(core_item);

			if(!listener)
			{
				item = OP_NEW(HistoryModelPage, (core_item));

				if (!item)
					continue;

				if (!item->GetSimpleItem())	// OOM during construction
				{
					OP_DELETE(item);
					continue;
				}
			}
			else
			{
				item = (HistoryModelPage*) listener;
			}

			if(item)
			{
				item->UpdateSimpleItem();
				AddLast(item);
			}
			else
				break;
		}
	}
}

/* ------------------ OpTreeModel interface ---------------------------------------*/

/***********************************************************************************
 ** GetColumnData - Implementing the OpTreeModel interface
 **
 ** @param column_data
 **
 ** @return OP_STATUS
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::GetColumnData(ColumnData* column_data)
{
	Str::LocaleString str_id = Str::NOT_A_STRING;
	switch(column_data->column)
	{
	case 0: str_id = Str::DI_ID_HLFILEPROP_FNAME_LABEL; break;
	case 1: str_id = Str::DI_ID_EDITURL_URLLABEL; break;
	case 2: str_id = Str::DI_IDLABEL_HL_LASTVISITED; break;
	case 3: str_id = Str::S_POPULARITY; break;
	default:
		return OpStatus::ERR;
	}
	return g_languageManager->GetString(str_id, column_data->text);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_HISTORY, type_string);
}
#endif

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::AddDesktopHistoryModelListener(DesktopHistoryModelListener* listener)
{
	RETURN_IF_ERROR(m_listeners.Add(listener));
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopHistoryModel::RemoveDesktopHistoryModelListener(DesktopHistoryModelListener* listener)
{
	return m_listeners.Remove(listener);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DesktopHistoryModel::BroadcastHistoryModelItemDestroyed(HistoryModelItem* item)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopHistoryModelItemDeleted(item);
	}
}

