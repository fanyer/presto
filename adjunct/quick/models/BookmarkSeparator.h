/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _BOOKMARK_SEPARATOR_H_
#define _BOOKMARK_SEPARATOR_H_

#include "adjunct/quick/models/DesktopBookmark.h"

class BookmarkItem;
class BookmarkModel;

class BookmarkSeparator : public SeparatorModelItem, public CoreBookmarkWrapper
{
  
 public:
	explicit BookmarkSeparator(BookmarkItem* core_item)
		: CoreBookmarkWrapper(core_item)
	{
		if (core_item)
			OP_ASSERT(core_item->GetFolderType() == FOLDER_SEPARATOR_FOLDER);
	}
	
	virtual const OpStringC&  GetUniqueGUID() const	  	{ const_cast<OpString&>(m_unique_guid).Set(m_core_item->GetUniqueId()); return m_unique_guid; }

	~BookmarkSeparator() {}

	virtual HotlistModelItem* CastToHotlistModelItem() {return this;}
	HotlistModelItem* GetDuplicate() {return OP_NEW(BookmarkSeparator,(*this)); }

private:
	// Not implemented
	BookmarkSeparator& operator=(const BookmarkSeparator& item);

};

#endif // _BOOKMARK_SEPARATOR_H
