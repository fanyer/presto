/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#include "core/pch.h"
#include "adjunct/quick/models/NavigationModel.h"

NavigationModel::NavigationModel()
	: m_recent(NULL)
	, m_bookmarks(NULL)
	, m_trash(NULL)
	, m_bookmark_model(NULL)
	, m_navigation_model_listener(NULL)
{
}

NavigationModel::~NavigationModel()
{
	if (g_desktop_bookmark_manager)
		g_desktop_bookmark_manager->RemoveBookmarkListener(this);

	if (m_bookmark_model)
	{
		m_bookmark_model->RemoveModelListener(this);
		m_bookmark_model->RemoveListener(this);
	}
}

OP_STATUS NavigationModel::Init()
{
	m_bookmark_model = g_hotlist_manager->GetBookmarksModel();
	RETURN_VALUE_IF_NULL(m_bookmark_model, OpStatus::ERR_NULL_POINTER);

	RETURN_IF_ERROR(m_bookmark_model->AddListener(this));

	RETURN_IF_ERROR(AddRecentFolder());
	RETURN_IF_ERROR(AddSeparator(TRUE));
	RETURN_IF_ERROR(AddBookmarksFolder());
	RETURN_IF_ERROR(AddSeparator(FALSE));

	if (m_bookmark_model->Loaded())
	{
		RETURN_IF_ERROR(AddDeletedFolder());
		OpStatus::Ignore(InitBookmarkFolders());
		m_bookmark_model->AddModelListener(this);
	}
	else 
		g_desktop_bookmark_manager->AddBookmarkListener(this);

	return OpStatus::OK;
}

OP_STATUS NavigationModel::InitBookmarkFolders()
{
	HotlistModel* bookmarks = g_hotlist_manager->GetBookmarksModel();
	INT32 count = bookmarks->GetCount();
	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem* item = bookmarks->GetItemByIndex(i);
		if (item && item->IsFolder() && !item->GetIsTrashFolder())
			RETURN_IF_ERROR(AddBookmarkFolder(item));
	}
	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddFolder(NavigationItem* item, const OpStringC& name)
{
	if (name.HasContent())
		RETURN_IF_ERROR(item->SetName(name));

	if (AddLast(item) < 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddRecentFolder()
{
	OpAutoPtr<NavigationItem>item(OP_NEW(NavigationRecentItem, (m_bookmark_model)));
	RETURN_OOM_IF_NULL(item.get());

	OpString loc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_RECENTLY_ADDED_FOLDER, loc_str));
	RETURN_IF_ERROR(AddFolder(item.get(), loc_str));
	m_recent = item.release();
	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddSeparator(BOOL is_first)
{
	OpAutoPtr<NavigationSeparator>sep(OP_NEW(NavigationSeparator, ()));
	RETURN_OOM_IF_NULL(sep.get());
	RETURN_IF_ERROR(AddFolder(sep.get(), OpStringC()));
	m_first_separator = sep.release();
	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddBookmarksFolder()
{
	OpAutoPtr<NavigationItem>item(OP_NEW(NavigationFolderItem, (m_bookmark_model, TRUE)));
	RETURN_OOM_IF_NULL(item.get());

	OpString loc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ALL_BOOKMARKS_FOLDER, loc_str));
	RETURN_IF_ERROR(AddFolder(item.get(), loc_str));
	m_bookmarks = item.release();
	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddDeletedFolder()
{
	OpAutoPtr<NavigationItem>item(OP_NEW(BookmarkNavigationFolder, (m_bookmark_model->GetTrashFolder(), TRUE)));
	RETURN_OOM_IF_NULL(item.get());

	OpString loc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DELETED_FOLDER, loc_str));
	RETURN_IF_ERROR(AddFolder(item.get(), loc_str));
	m_trash = item.release();
	return OpStatus::OK;
}

OP_STATUS NavigationModel::AddBookmarkFolderSorted(HotlistModelItem* folder)
{
	if (!g_hotlist_manager->GetBookmarksModel()->Loaded())
		return AddBookmarkFolder(folder);

	OP_ASSERT(GetNavigationItemFromHotlistItem(folder) == NULL);

	OpAutoPtr<BookmarkNavigationFolder>item(OP_NEW(BookmarkNavigationFolder, (folder, FALSE)));
	RETURN_OOM_IF_NULL(item.get());

	INT32 pos = -1;
	HotlistModelItem* sibling_item = folder->GetSiblingItem();
	HotlistModelItem* prev_item = folder->GetPreviousItem();
	if (prev_item && prev_item->IsFolder() && !prev_item->GetIsTrashFolder())
	{
		NavigationItem* prev = GetNavigationItemFromHotlistItem(prev_item);
		if (prev)
			pos = prev->InsertSiblingAfter(item.get());
	}
	else if (sibling_item && sibling_item->IsFolder() && !sibling_item->GetIsTrashFolder())
	{
		NavigationItem* sib = GetNavigationItemFromHotlistItem(folder->GetSiblingItem());
		if (sib)
			pos = sib->InsertSiblingBefore(item.get());
	}
	else if (folder->GetParentFolder())
	{
		NavigationItem* parent = GetNavigationItemFromHotlistItem(folder->GetParentFolder());
		if (parent)
			pos = parent->AddChildLast(item.get());
	}
	else
		pos = m_bookmarks->AddChildLast(item.get());

	if (pos >= 0)
		item.release();

	return pos >=0 ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS NavigationModel::AddBookmarkFolder(HotlistModelItem* folder)
{
	OpAutoPtr<BookmarkNavigationFolder>item(OP_NEW(BookmarkNavigationFolder, (folder, FALSE)));
	RETURN_OOM_IF_NULL(item.get());

	INT32 pos = -1;
	if (folder->GetParentFolder())
	{
		NavigationItem* parent = GetNavigationItemFromHotlistItem(folder->GetParentFolder());
		if (parent)
			 pos = parent->AddChildLast(item.get());
	}
	else
		pos = m_bookmarks->AddChildLast(item.get());

	if (pos >= 0)
		item.release();

	return pos >=0 ? OpStatus::OK : OpStatus::ERR;
}

NavigationItem* NavigationModel::GetNavigationItemFromHotlistItem(HotlistModelItem* item)
{
	if (!item)
		return NULL;

	NavigationItem* lib_item;
	INT32 count = GetCount();
	for (INT32 i = 0; i < count; i++)
	{
		lib_item = GetItemByIndex(i);
		if (lib_item->GetHotlistItem() == item)
			return lib_item;
	}

	return NULL;
}

OP_STATUS NavigationModel::GetColumnData(ColumnData* column_data)
{
	column_data->custom_sort = TRUE;
	return OpStatus::OK;
}

void NavigationModel::RemoveBookmarkFolder(HotlistModelItem* item)
{
	NavigationItem* item_to_remove = GetNavigationItemFromHotlistItem(item);
	if (item_to_remove) item_to_remove->Remove();
}

void NavigationModel::AddBookmarkFolderLast(NavigationItem* item)
{
	m_bookmarks->AddChildLast(item);
}

void NavigationModel::OnBookmarkModelLoaded()
{
	RETURN_VOID_IF_ERROR(AddDeletedFolder()); 

	OpStatus::Ignore(InitBookmarkFolders());

	if (m_bookmark_model)
		m_bookmark_model->AddModelListener(this);
}

void NavigationModel::OnHotlistItemAdded(HotlistModelItem* item)
{
#ifdef LIBRARY_DEBUG
	fprintf(stderr, " OnHotlistItemAdded \n");
#endif

	if (item->IsSeparator())
		return;

	// Don't need to do anything unless it's a folder
	if (item && item->IsFolder())
	{
		if (!item->GetIsTrashFolder())
			OpStatus::Ignore(AddBookmarkFolderSorted(item));
		else 
			RETURN_VOID_IF_ERROR(AddDeletedFolder()); 
	}

	if (m_navigation_model_listener)
		m_navigation_model_listener->ItemAdded(item);
}

void NavigationModel::OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag)
{
#ifdef LIBRARY_DEBUG
	fprintf(stderr, " OnHotlistItemChanged item %x, model = %x\n", item, this);
#endif

	if (item->IsSeparator())
		return;

	if (changed_flag & HotlistModel::FLAG_MOVED_TO && !item->GetIsTrashFolder())
	{
		if (!item->IsFolder())
		{
			if (m_navigation_model_listener)
				m_navigation_model_listener->ItemMoved(item);

			return;
		}
		else
		{
			NavigationItem*	navitem = GetNavigationItemFromHotlistItem(item);
			NavigationItem* parent = item->GetParentFolder() ? GetNavigationItemFromHotlistItem(item->GetParentFolder()) : m_bookmarks;
			if (!navitem || navitem->GetParentItem() == parent) // BookmarkModel notifies twice, avoid
				return;

			if (m_navigation_model_listener)
				m_navigation_model_listener->FolderMoved(navitem);

			navitem->Remove();
			AddSubtree(item);
		}
	}


	NavigationItem* navitem = GetNavigationItemFromHotlistItem(item);
	if (navitem)
		navitem->Change(TRUE);

	// If any other flag than FLAG_MOVED_TO is set
	if (m_navigation_model_listener)
		m_navigation_model_listener->ItemModified(item, changed_flag);

}

void NavigationModel::AddSubtree(HotlistModelItem* item)
{
	AddBookmarkFolder(item);

	for (HotlistModelItem* child = item->GetChildItem(); child != NULL; child = child->GetSiblingItem())
	{
		if (child->IsFolder())
			AddSubtree(child);
	}
}

void NavigationModel::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
#ifdef LIBRARY_DEBUG
	fprintf(stderr, " OnHotlistItem Removed \n");
#endif

	if (item->IsSeparator())
		return;

	if (item && item->IsFolder() && !item->GetIsTrashFolder())
		RemoveBookmarkFolder(item);

	m_navigation_model_listener->ItemRemoved(item);
}

void NavigationModel:: OnHotlistItemTrashed(HotlistModelItem* item)
{
#ifdef LIBRARY_DEBUG
	fprintf(stderr, " OnHotlistItemTrashed \n");
#endif

	m_navigation_model_listener->ItemRemoved(item);
}

/**
 * Called when an item is moved out of trash into 'normal' bookmarks
 * Note that in this case listeners will also receive an
 * OnHotlistItemChanged notification
 *
 */
void NavigationModel::OnHotlistItemUnTrashed(HotlistModelItem* item)
{
#ifdef LIBRARY_DEBUG
	fprintf(stderr, " OnHotlistItemUnTrashed \n");
#endif
}

INT32 NavigationModel::CompareItems( int column, OpTreeModelItem* item1, OpTreeModelItem* item2)
{
	if (!item1 || !item2)
		return 0;

	NavigationItem* n1 = static_cast<NavigationItem*>(item1);
	NavigationItem* n2 = static_cast<NavigationItem*>(item2);

	if( n1->IsRecentFolder() )
	{
		return -1;
	}
	else if( n2->IsRecentFolder() )
	{
		return 1;
	}

	if( n1->IsDeletedFolder() )
	{
		return 1;
	}
	else if( n2->IsDeletedFolder() )
	{
		return -1;
	}

	if (n1->IsAllBookmarks())
	{
		if (n2 == m_first_separator)
			return 1;
		return -1;
	}
	if (n2->IsAllBookmarks())
	{
		if (n1 == m_first_separator)
			return -1;
		return 1;
	}

	OpString s1, s2;
	const uni_char *p1 = 0;
	const uni_char *p2 = 0;

	p1 = n1->GetName().CStr();
	p2 = n2->GetName().CStr();

	if( !p1 )
	{
		return 1;
	}
	else if( !p2 )
	{
		return -1;
	}
	else
	{
		int ans;
		TRAPD(err, ans = g_oplocale->CompareStringsL(p1, p2, -1, FALSE/*TRUE*/));
		OP_ASSERT(OpStatus::IsSuccess(err)); 
		if (OpStatus::IsError(err))
		{
			ans = uni_stricmp(p1,p2);
		}
		return ans; 
	}
}
