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


#include "adjunct/quick/models/BookmarkClipboard.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/models/BookmarkModel.h"

#include "modules/pi/OpClipboard.h"

// OBS: This is lots of crap that needs to be fixed :-)

/***********************************************************************
 *
 * Destructor 
 *
 * Clear the core repr of clipboard
 *
 ***********************************************************************/

BookmarkClipboard::~BookmarkClipboard()
{
	Clear();
	OP_DELETE(m_manager);
}
	

/***********************************************************************
 *
 * Init
 *
 * Creates the core BookmarkManager for the clipboard bookmark items 
 *
 *
 ***********************************************************************/
void BookmarkClipboard::Init()
{
	if (!m_initialized)
	{
		m_manager = OP_NEW(BookmarkManager, ());
		if (m_manager)
		{
			m_manager->Init();
			OP_ASSERT(m_manager->GetRootFolder());
			// Disable syncing for clipboard
#ifdef SUPPORT_DATA_SYNC
			m_manager->GetRootFolder()->SetAllowedToSync(FALSE);
#endif
			m_initialized = TRUE;
		}
	}
}


/***********************************************************************
 *
 * GetNextElm
 *
 * 
 ***********************************************************************/
BookmarkListElm* BookmarkClipboard::GetNextElm(BookmarkListElm* elm)
{
	Link* l = elm->Suc();
	if (l)
	{
		return static_cast<BookmarkListElm*>(l);
    }
	return 0;
}


/***********************************************************************
 *
 * PasteToModel
 *
 * 
 ***********************************************************************/
// TODO: Move to BookmarkModel?? 
// Delete list
OP_STATUS BookmarkClipboard::PasteToModel(BookmarkModel& model, HotlistModelItem* at, DesktopDragObject::InsertType insert_type)
{
	Head* bookmark_list = OP_NEW(Head, ());
	BookmarkItem* previous = NULL;
	BookmarkItem* parent =  NULL;

	if(at)
	{
		if(at->IsFolder())
		{
			parent = BookmarkWrapper(at)->GetCoreItem();
		}
		else if (at->GetParentFolder())
		{
			parent = BookmarkWrapper(at->GetParentFolder())->GetCoreItem();
			previous = BookmarkWrapper(at)->GetCoreItem();
		}
		else
		{
			previous = BookmarkWrapper(at)->GetCoreItem();
		}
	}

	if (OpStatus::IsSuccess(m_manager->GetList(bookmark_list)) && bookmark_list->First())
	{
		if (insert_type == DesktopDragObject::INSERT_INTO || insert_type == DesktopDragObject::INSERT_AFTER)
		{
			BookmarkListElm* elem =  static_cast<BookmarkListElm*>(bookmark_list->First());
			
			for (; elem; elem = GetNextElm(elem))
			{
				// only handle top level elements, CopyItem copy the children as well
				if(elem->GetBookmark()->GetParentFolder() != m_manager->GetRootFolder())
					continue;

				if(at)
				{
					if (elem->GetBookmark()->GetFolderType() == FOLDER_SEPARATOR_FOLDER && !at->GetSeparatorsAllowed()
						|| elem->GetBookmark()->GetFolderType() == FOLDER_NORMAL_FOLDER && !at->GetSubfoldersAllowed())
						continue;
				}

				previous = CopyItem(g_bookmark_manager,elem->GetBookmark(), previous, parent);
			}
		}
		else
		{
			OP_ASSERT(FALSE);
		}
		
    }

	OP_DELETE(bookmark_list);

	return OpStatus::OK;
}


/***********************************************************************
 *
 * Clear
 *
 * Clear the Clipboard.
 *
 * NOTE: Calls RemoveAllBookmarks and then Init since RemoveAllBookmarks
 *       removes also the Root folder
 * 
 ***********************************************************************/
void BookmarkClipboard::Clear()
{
	m_manager->RemoveAllBookmarks(); // TODO: Doesnt delete them??
	m_manager->Init(); 
}

/***********************************************************************
 *
 * CopyCoreItem
 *
 * 
 ***********************************************************************/
// This should only be necessary for Copy, not Cut, when core adds
// function to remove an item without deleting it -> TODO: Remove param keep_all
BookmarkItem* BookmarkClipboard::CopyCoreItem(BookmarkItem* item) 
{	
	BookmarkItem* copy = OP_NEW(BookmarkItem, ());
	
	// Copy fields
	OpString url;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_URL, url)) && url.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_URL, url));
	OpString title;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_TITLE, title)) && title.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_TITLE, title));
	
	OpString nick;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_SHORTNAME, nick)) && nick.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_SHORTNAME, nick));
	OpString desc;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_DESCRIPTION, desc)) && desc.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_DESCRIPTION, desc));
	OpString created;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_CREATED, created)) && created.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_CREATED, created));
	OpString visited;
	if (OpStatus::IsSuccess(GetAttribute(item, BOOKMARK_VISITED, visited)) && visited.HasContent())
		OpStatus::Ignore(SetAttribute(copy, BOOKMARK_VISITED, visited));
	
	copy->SetFolderType(item->GetFolderType());
	
	// TODO shuais More attributes ?
	return copy;
}


/***********************************************************************
 *
 * AddNewItem
 *
 * @param CoreBookmarkWrapper* bookmark - item to add to clipboard
 * @ret	  new item 
 *
 *
 ***********************************************************************/
BookmarkItem*  BookmarkClipboard::CopyItem(BookmarkManager* to, BookmarkItem* bookmark,BookmarkItem* previous,BookmarkItem* parent)
{
	if (!bookmark)
		return NULL;
	
	// Need a copy of the core bookmark because when removing
	// it from the list of core bookmarks, it might be deleted.
	// ONLY NEEDED FOR COPY NOT CUT
	
	// Get Last Item
	BookmarkItem* children	= bookmark->GetChildren();
	BookmarkItem* last_item = static_cast<BookmarkItem*>(bookmark->LastChild());
	BookmarkItem* copy_item = CopyCoreItem(bookmark); // What about cut -> Should keep guid etc.
	
	BOOL sync = to == g_bookmark_manager; // don't sync items in clipboard. 

	// Add item
	
	OP_STATUS status(OpStatus::OK);
	if (parent) // if it has a parent
	{
		if (previous)
		{
			status = to->AddBookmark(copy_item, previous, parent, sync);
		}
		else
		{
			status = to->AddNewBookmark(copy_item, parent, sync);
		}
	}
	else // If it doesn't have a parent
	{
		if (previous)
		{
			status = to->AddBookmark(copy_item, previous, to->GetRootFolder(), sync);
		}
		else
		{
			status = to->AddNewBookmark(copy_item, to->GetRootFolder(), sync);
		}
	}

	if (OpStatus::IsError(status))
	{
		OP_DELETE(copy_item);
		return NULL;
	}
	
	// Add children	
	if (children)
	{
		BookmarkItem* prev = 0;
		
		for (BookmarkItem* child = children; child; child = child->GetNextItem())
		{
			prev = CopyItem(to,child,prev,copy_item);
			if(child == last_item)
				break; // last_item->GetNextItem() return itself?
		}
	}

	return copy_item;
}
	

void BookmarkClipboard::AddItem(const BookmarkItemData& item_data, BOOL append)
{
	if( !append )
	{
		g_desktop_clipboard_manager->PlaceText(UNI_L(""), g_application->GetClipboardToken());
		Clear();
	}

	BookmarkItem* item = OP_NEW(BookmarkItem, ());
	item->SetFolderType(FOLDER_NO_FOLDER); // TODO:

	::SetDataFromItemData(&item_data, item);
	m_manager->AddNewBookmark(item, m_manager->GetRootFolder(), false);


	OpInfoText text;
	text.SetAutoTruncate(FALSE);
	g_hotlist_manager->GetBookmarksModel()->GetInfoText(item_data, text);

	if (text.HasStatusText())
	{
		OpString global_clipboard_text;

		g_desktop_clipboard_manager->GetText(global_clipboard_text);

		if (global_clipboard_text.HasContent())
		{
			global_clipboard_text.Append(UNI_L("\n"));
		}

		global_clipboard_text.Append(text.GetStatusText().CStr());

		if (global_clipboard_text.HasContent())
		{
			g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
			// Mouse selection
			g_desktop_clipboard_manager->PlaceText(global_clipboard_text.CStr(), g_application->GetClipboardToken(), true);
#endif
		}
	}
}

