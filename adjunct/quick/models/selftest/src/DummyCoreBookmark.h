/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_manager.h"

class DummyCoreBookmark
{

public:
	static BookmarkItem* CreateBookmark(const OpStringC& title, 
										const OpStringC& url, 
										const OpStringC& description, 
										const OpStringC& shortname,
										const OpStringC& created, 
										const OpStringC& visited)
	{
		BookmarkItem* bookmark = OP_NEW(BookmarkItem, ());
		BookmarkAttribute attribute;
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(url.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_URL, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(title.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(description.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(shortname.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(created.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_CREATED, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(visited.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_VISITED, &attribute));
		
		bookmark->SetFolderType(FOLDER_NO_FOLDER);
		return bookmark;
	}

	static BookmarkItem* AddBookmark(const OpStringC& title, 
									 const OpStringC& url, 
									 const OpStringC& description, 
									 const OpStringC& shortname,
									 const OpStringC& created, 
									 const OpStringC& visited)
	{
		BookmarkItem* bookmark = DummyCoreBookmark::CreateBookmark(title, url, description, shortname, created, visited);
		if (OpStatus::IsSuccess(g_bookmark_manager->AddBookmark(bookmark, g_bookmark_manager->GetRootFolder())))
			return bookmark;
		return 0;
	}

	static BookmarkItem* CreateFolder(const OpStringC& title, 
								   const OpStringC& description,
								   const OpStringC& shortname) 
	{
		BookmarkItem* bookmark = OP_NEW(BookmarkItem, ());
		BookmarkAttribute attribute;
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(title.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(description.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attribute));
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(shortname.CStr())))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attribute));
		
		bookmark->SetFolderType(FOLDER_NORMAL_FOLDER);
		return bookmark;
	}
										
	static BookmarkItem* AddFolder(const OpStringC& title, 
								   const OpStringC& description,
								   const OpStringC& shortname) 
	{

		BookmarkItem* bookmark = CreateFolder(title, description, shortname);
		if (OpStatus::IsSuccess(g_bookmark_manager->AddBookmark(bookmark, g_bookmark_manager->GetRootFolder())))
			return bookmark;

		return 0;
	}

	static BookmarkItem* AddTrashFolder()
	{
		BookmarkItem* bookmark = OP_NEW(BookmarkItem, ());
		BookmarkAttribute attribute;
		
		if (OpStatus::IsSuccess(attribute.SetTextValue(UNI_L("Trash"))))
			OpStatus::IsSuccess(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));
		
		bookmark->SetFolderType(FOLDER_TRASH_FOLDER);

		if (OpStatus::IsSuccess(g_bookmark_manager->AddBookmark(bookmark, g_bookmark_manager->GetRootFolder())))
			return bookmark;

		return 0;
		
	}
};
