/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SUPPORT_SPEED_DIAL
#include "adjunct/quick/models/SpeedDialSuggestions.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/HistoryModelItem.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#include "modules/history/direct_history.h"
#include "modules/locale/oplanguagemanager.h"

#define MAX_ITEM_COUNT 3


namespace
{
	// these urls populate model if we don't have meaningful suggestions yet
	// e.g. during first start of browser or after clearing of history
	const char *LIST_OF_FALLBACK_URLS[] = 
	{
		"http://redir.opera.com/speeddialsuggestions/0/",
		"http://redir.opera.com/speeddialsuggestions/1/",
		"http://redir.opera.com/speeddialsuggestions/2/"
	};

	// these urls should never appear in speeddial suggestions
	const char* LIST_OF_BLACKLISTED_URLS[] = 
	{
		// these two land in suggestions through open/closed tabs during
		// first start/upgrade of opera
		"http://www.opera.com/browser/",
		"http://www.opera.com/portal/startup/"
	};
}


SpeedDialSuggestionsModelItem::SpeedDialSuggestionsModelItem(ItemType type) :
	m_item_type(type)
{
	m_id = OpTypedObject::GetUniqueID();	//needed for anything?
}

OP_STATUS SpeedDialSuggestionsModelItem::GetItemData(ItemData* item_data)
{
	switch (item_data->query_type)
	{
		case INIT_QUERY:
			{
				if (GetItemType() == FOLDER_TYPE)
				{
					item_data->flags |= FLAG_INITIALLY_OPEN;
				}
				break;
			}
		case COLUMN_QUERY:
			{
				if (GetItemType() == FOLDER_TYPE)
				{
					item_data->flags |=  FLAG_BOLD;
				}

				item_data->column_query_data.column_text->Set(m_title);
				// skin image
				if (m_image.HasContent())
				{
					item_data->column_query_data.column_image = m_image.CStr();
				}
				else if (m_url.HasContent() && GetItemType() == LINK_TYPE)
				{
					// use bookmarks icon if it has a url
					item_data->column_query_data.column_image = "Bookmark Visited";
				}

				item_data->column_bitmap = m_bitmap;
				break;
			}

		case SKIN_QUERY:
			if (GetItemType() == LINK_TYPE || GetItemType() == LINK_TYPE)
			{
				item_data->skin_query_data.skin = "Speed Dial Dialog Treeview Item Skin";
			}
			break;

		default:
			break;
	}
	return OpStatus::OK;
}


OP_STATUS SpeedDialSuggestionsModelItem::SetLinkData(const OpStringC &title, const OpStringC &url, const OpStringC &display_url)
{
	OP_ASSERT(m_item_type == LINK_TYPE);	//SpeedDialSuggestionsModelItem doesn't handle on the fly changing of type

	RETURN_IF_ERROR(m_title.Set(title));
	RETURN_IF_ERROR(m_url.Set(url));
	RETURN_IF_ERROR(m_display_url.Set(display_url));
	return OpStatus::OK;
}

INT32 SpeedDialSuggestionsModel::AddSuggestion(const OpStringC &title, const OpStringC &url, const INT32 parent)
{
	OpString actual_url, display_url;
	actual_url.Set(url);

	// Look up for the display url or redirect url if any
	HotlistModelItem* bookmark = g_desktop_bookmark_manager->FindDefaultBookmarkByURL(url);
	if (bookmark)
	{
		actual_url.Set(bookmark->GetUrl());
		display_url.Set(bookmark->GetDisplayUrl());
	}

	//Removes duplicated URLs from the treeview list
	if (parent == -1)
	{
		SpeedDialSuggestionsModelItem *item;
		for (INT32 index = 0; index < GetItemCount() && (item = GetItemByIndex(index)); index++)
		{
			if (item->GetURL().Compare(actual_url) == 0 || item->GetURL().Compare(display_url) == 0)
				return -1;
		}
	}

	// Don't add urls already in speed dial	
	if (g_speeddial_manager->SpeedDialExists(actual_url) || g_speeddial_manager->SpeedDialExists(display_url))
		return -1;

	for (UINT32 i = 0; i < ARRAY_SIZE(LIST_OF_BLACKLISTED_URLS); i++)
		if (url == LIST_OF_BLACKLISTED_URLS[i])
			return -1;

	SpeedDialSuggestionsModelItem* item = OP_NEW(SpeedDialSuggestionsModelItem, (SpeedDialSuggestionsModelItem::LINK_TYPE));
	if (item)
	{
		if (OpStatus::IsSuccess(item->SetLinkData(title, actual_url, display_url)))			
		{
			Image favico = g_favicon_manager->Get(url.CStr());
			item->SetBitmap(favico);
			INT32 idx = AddLast(item, parent);
			if (idx != -1)
			{
				return idx;
			}
		}
		OP_DELETE(item);
	}
	return -1;
}

INT32 SpeedDialSuggestionsModel::AddFolder(const OpStringC &title, const OpStringC8 &skin_image)
{
	SpeedDialSuggestionsModelItem* item = OP_NEW(SpeedDialSuggestionsModelItem, (SpeedDialSuggestionsModelItem::FOLDER_TYPE));
	if (item)
	{
		if (OpStatus::IsSuccess(item->SetTitle(title)) &&
			OpStatus::IsSuccess(item->SetImage(skin_image)))
		{
			INT32 idx = AddLast(item);
			if (idx != -1)
				return idx;
		}
		OP_DELETE(item);
	}
	return -1;
}

INT32 SpeedDialSuggestionsModel::AddLabel(const OpStringC &title, const INT32 parent)
{
	SpeedDialSuggestionsModelItem* item = OP_NEW(SpeedDialSuggestionsModelItem, (SpeedDialSuggestionsModelItem::LABEL_TYPE));
	if (item)
	{
		if (OpStatus::IsSuccess(item->SetTitle(title)))
		{
			INT32 idx = AddLast(item, parent);
			if (idx != -1)
				return idx;
		}
		OP_DELETE(item);
	}
	return -1;
}

void SpeedDialSuggestionsModel::Populate(BOOL items_under_parent_folder /*= FALSE*/)
{
	AddOpenTabs          (items_under_parent_folder, MIN(3, MAX_ITEM_COUNT - GetItemCount()));
	AddClosedTabs        (items_under_parent_folder, MIN(3, MAX_ITEM_COUNT - GetItemCount()));
	AddHistorySuggestions(items_under_parent_folder,		MAX_ITEM_COUNT - GetItemCount() );
	AddOperaSites        (items_under_parent_folder,        MAX_ITEM_COUNT - GetItemCount() );
}


void SpeedDialSuggestionsModel::AddHistorySuggestions(BOOL items_under_parent_folder /*= FALSE*/, INT32 num_of_item_to_add/* = -1*/)
{
	if (num_of_item_to_add == 0)
		return;

	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	if (!history_model)
		return;

	OpVector<HistoryModelPage> top_sites;
	INT32 top10_folder_idx = -1;
	
	// Add topten folder
	if (items_under_parent_folder)
	{
		OpString freq_visited_sites;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_FREQUENTLY_VISITED_PAGES, freq_visited_sites));

		OpStringC8 toptenimage("Top10");
		INT32 top10_folder_idx = AddFolder(freq_visited_sites, toptenimage);
		if (top10_folder_idx == -1)
			return;
	}

	// Read more than needed in case some of the urls are alreay in the treeview or speeddial
	history_model->GetTopItems(top_sites, num_of_item_to_add + 20);

	INT32 count = 0;
	for (UINT32 i = 0; i < top_sites.GetCount() && count < num_of_item_to_add; i++)
	{
		OpString address, title;

		HistoryModelPage* item = top_sites.Get(i);
		item->GetAddress(address);
		item->GetTitle(title);

		if (!DocumentView::IsUrlRestrictedForViewFlag(address.CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
			continue;

		if (-1 != AddSuggestion(title, address, top10_folder_idx))
			count ++;
	}

	if (count == 0 && items_under_parent_folder)
	{
		/*
		Add information on 'no browsering history available' only if 'items_under_parent_folder' is set to TRUE and 
		history count is 0.
		*/
		OpString no_items_label;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_NO_BROWSING_HISTORY_AVAILABLE, no_items_label));
		RETURN_VOID_IF_ERROR(AddLabel(no_items_label, top10_folder_idx));
	}
}

void SpeedDialSuggestionsModel::AddOpenTabs(BOOL items_under_parent_folder /*= FALSE*/, INT32 num_of_item_to_add /*= -1*/)
{
	if (num_of_item_to_add == 0)
		return;

	INT32 open_pages_folder_idx = -1;

	// Add open tabs folder folder
	if (items_under_parent_folder)
	{
		OpString open_pages;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_CURRENTLY_OPEN_PAGES, open_pages));

		OpStringC8 open_pages_image("Open Pages");
		open_pages_folder_idx = AddFolder(open_pages, open_pages_image);
		if (open_pages_folder_idx == -1)
			return;
	}

	// Get the title and url of last accessed open tabs (only from the active browser window)
	OpWorkspace* workspace = NULL;
	if (BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow())
		workspace = browser->GetWorkspace();
	
	if (!workspace)
		return;

	INT32 count = 0;
	for (INT32 i = 0; i < workspace->GetDesktopWindowCount() && count < num_of_item_to_add; i++)
	{
		DesktopWindow* window = workspace->GetDesktopWindowFromStack(i);
		if (window && window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
		{
			DocumentDesktopWindow* ddw = (DocumentDesktopWindow*)window; 
 
			if (ddw->HasDocument() && !ddw->IsLockedByUser()) 
			{
				OpString title, address; 
				OpStatus::Ignore(title.Set(ddw->GetTitle(FALSE))); 
				OpStatus::Ignore(WindowCommanderProxy::GetMovedToURL(ddw->GetWindowCommander()).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, address));
				
				if (!DocumentView::IsUrlRestrictedForViewFlag(address.CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
					continue;

				if (-1 != AddSuggestion(title, address, open_pages_folder_idx))
					count ++;
			}
		}
	}

	if (count == 0 && items_under_parent_folder)
	{
		/*
		Add information on 'no pages currently open' only if 'items_under_parent_folder' is set to TRUE and 
		tab count is 0.
		*/
		OpString no_items_label;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_NO_PAGES_CURRENTLY_OPEN, no_items_label));
		RETURN_VOID_IF_ERROR(AddLabel(no_items_label, open_pages_folder_idx));
	}
}

void SpeedDialSuggestionsModel::AddOperaSites(BOOL items_under_parent_folder /*= FALSE*/, INT32 num_of_item_to_add /*= -1*/)
{
	if (num_of_item_to_add == 0)
		return;

	OpString title, address;
	INT32 opera_folder_idx = -1;
	
	//Add folder
	if (items_under_parent_folder)
	{
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_OPERA_SPEED_DIAL_SUGGESTIONS, title));

		OpStringC8 image("Opera Logo");
		opera_folder_idx = AddFolder(title, image);
		if (opera_folder_idx == -1)
			return;	
	}

	UINT32 index;
	INT32 count;
	for (index = 0, count = 0; index < ARRAY_SIZE(LIST_OF_FALLBACK_URLS) && count < num_of_item_to_add; index++)
	{
		RETURN_VOID_IF_ERROR(address.Set(LIST_OF_FALLBACK_URLS[index]));
		if ( -1 != AddSuggestion(NULL, address, opera_folder_idx))
			count ++;
	}
}

void SpeedDialSuggestionsModel::AddClosedTabs(BOOL items_under_parent_folder /*= FALSE*/, INT32 num_of_item_to_add /*= -1*/)
{
	if (num_of_item_to_add == 0)
		return;
	
	TRAPD(err, AddClosedTabsHelperL(items_under_parent_folder, num_of_item_to_add));
}

void SpeedDialSuggestionsModel::AddClosedTabsHelperL(BOOL items_under_parent_folder, INT32 num_of_item_to_add)
{
	//Todo: Add folder if parameter 'items_under_parent_folder' is true
	INT32 session_pages_folder_idx = -1;

	OP_ASSERT(g_application->GetActiveBrowserDesktopWindow());
	OpSession* session;
	if ((session = g_application->GetActiveBrowserDesktopWindow()->GetUndoSession()) != NULL)
	{
		INT32 session_wnd_count = session->GetWindowCountL();
		for (INT32 index = static_cast<INT32>(session_wnd_count - 1), count = 0; index >= 0 && count < num_of_item_to_add; index--)
		{
			OpSessionWindow* session_window = session->GetWindowL(index);
			if (session_window->GetTypeL() ==  OpSessionWindow::DOCUMENT_WINDOW)
			{
				UINT32 current_history = session_window->GetValueL("current history");
				if (current_history == 0 && session_window->GetValueL("max history"))
					continue; //Speed-dial in history

				OpAutoVector<OpString> history_url_list; ANCHOR(OpAutoVector<OpString>, history_url_list);
				session_window->GetStringArrayL("history url", history_url_list);
				if (!history_url_list.Get(current_history - 1)) 
					continue;

				if (!DocumentView::IsUrlRestrictedForViewFlag(history_url_list.Get(current_history - 1)->CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL))
					continue;

				OpAutoVector<OpString> history_title_list; ANCHOR(OpAutoVector<OpString>, history_title_list);
				session_window->GetStringArrayL("history title", history_title_list);
				
				if ( -1 != AddSuggestion(history_title_list.Get(current_history - 1)->CStr()
					, history_url_list.Get(current_history - 1) ? history_url_list.Get(current_history - 1)->CStr() : UNI_L("")
					, session_pages_folder_idx))
					count ++;
			}
		}
	}
}

#endif // SUPPORT_SPEED_DIAL
