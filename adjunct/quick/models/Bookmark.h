/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 *
 */

#ifndef _BOOKMARK_H_
#define _BOOKMARK_H_

#include "adjunct/quick/models/DesktopBookmark.h"

class HistoryPage;
class BookmarkItem;
class BookmarkModel;

class BookmarkListener
{
public:
	virtual ~BookmarkListener(){}
	virtual void OnBookmarkDestroyed() = 0;
};

class Bookmark : public DesktopBookmark<GenericModelItem>, public HistoryItem::Listener
{

 public:
	explicit Bookmark(BookmarkItem* core_item);
	~Bookmark();

	// OpTreeModelItem interface
	virtual Type GetType() { return BOOKMARK_TYPE; }

	// Subclassing
	virtual const char* GetImage() const;
	virtual Image GetIcon();

	Bookmark*				CastToBookmark()		{return this;}

	// HistoryItem::Listener
	virtual OpTypedObject::Type GetListenerType() {return GetType();}
	virtual void OnHistoryItemDestroying() { RemoveHistoryItem(); }
	virtual void OnHistoryItemAccessed(time_t acc);

	/////
	void SetHistoryItem( HistoryPage* item );
	void RemoveHistoryItem();
	HistoryPage* GetHistoryItem() {return m_history_item;}

	// Sync the BOOKMARK_FAVICON_FILE attribute with the favicon file on disk.
	// @param save_or_read	True write the icon data to favicon file
	//						False read the favicon file and update the attribute
	void SyncIconAttribute(BOOL save_or_read);

	HotlistModelItem* GetDuplicate() {return OP_NEW(Bookmark,(*this)); }
	
	void SetListener(BookmarkListener* listener);

private:
	Bookmark& operator=(const Bookmark& item);

	BOOL m_saving_favicon;
	HistoryPage* m_history_item; // Used for lookups
	BookmarkListener* m_listener;
};

#endif // _BOOKMARK_H
