/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _BOOKMARK_FOLDER_H_
#define _BOOKMARK_FOLDER_H_

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/models/DesktopBookmark.h"

class BookmarkModel;

/**********************************************************************
 *
 * BookmarkFolder
 *
 *
 **********************************************************************/

class BookmarkFolder : public DesktopBookmark<FolderModelItem>
{
 public:
	 explicit BookmarkFolder(BookmarkItem* core_item) : DesktopBookmark<FolderModelItem>(core_item) {}
	~BookmarkFolder() {}
	
	BookmarkFolder*			CastToBookmarkFolder()	{return this;}

	virtual void SetTarget( const uni_char* str ) {SetAttribute(BOOKMARK_TARGET, str); }
	virtual const OpStringC &GetTarget() const { GetStringAttribute(BOOKMARK_TARGET, const_cast<OpString&>(this->m_target)); return this->m_target; }

	HotlistModelItem* GetDuplicate() {return OP_NEW(BookmarkFolder,(*this)); }

private:
	// Not implemented
	BookmarkFolder();
	BookmarkFolder& operator=(const BookmarkFolder& item);
};

// This class is more than a simple wrapper of an core trash item, in case
// there are multiple core trash folders this class server as a virtual folder
// which contains all trashed items to avoid having multiple trash on UI
class TrashFolder : public BookmarkFolder
{
public:
	 explicit TrashFolder(BookmarkItem* core_item) : BookmarkFolder(core_item) {}
	~TrashFolder() {}

	HotlistModelItem* GetDuplicate() {OP_ASSERT(FALSE); return NULL; }

	// update when core main trash folder changes
	void Update();	

	// BookmarkItem::Listener
	virtual void OnBookmarkDeleted();
private:

	OpAutoVector<OpString> m_trash_folder_ids;
};

#endif // _BOOKMARK_FOLDER_H_
