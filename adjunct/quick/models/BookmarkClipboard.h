/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _BOOKMARK_CLIPBOARD_H_
#define _BOOKMARK_CLIPBOARD_H_

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/locale/oplanguagemanager.h"


/*********************************************************************
 *
 * BookmarkClipboard
 *
 *
 *********************************************************************/

class BookmarkClipboard {
public:
	BookmarkClipboard() : m_manager(0), m_initialized(0) {}
	~BookmarkClipboard();

	void Init();
	OP_STATUS PasteToModel(BookmarkModel& model, HotlistModelItem* at, DesktopDragObject::InsertType insert_type);

	void Clear();
	INT32 GetCount() { return m_manager->GetCount();}

	BOOL IsInitialized() { return m_initialized; }

	void AddItem(const BookmarkItemData& item_data, BOOL append);

	static BookmarkItem* CopyCoreItem(BookmarkItem* item);
	static BookmarkItem* CopyItem(BookmarkManager* to, BookmarkItem* bookmark,BookmarkItem* previous = NULL,BookmarkItem* parent = NULL); // and all children

	BookmarkManager* m_manager;

private:
	// Not implemented
	BookmarkClipboard(const BookmarkClipboard&);
	BookmarkClipboard& operator=(const BookmarkClipboard& item);


	// Paster top level item(and its children)
	// @ret new item
	BookmarkItem* PasteItem(BookmarkItem* item, BookmarkItem* previous);

	// Helper functions
	BookmarkListElm* GetNextElm(BookmarkListElm* elm);

	// BookmarkManager for the clipboard items
	BOOL m_initialized;

};

#endif // BOOKMARK_CLIPBOARD_H
