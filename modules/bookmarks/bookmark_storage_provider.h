/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_STORAGE_PROVIDER_H
#define BOOKMARK_STORAGE_PROVIDER_H

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/bookmarks/bookmark_item.h"

/**
 * The different supported data formats the bookmarks may be saved in.
 *
 * BOOKMARK_VERBOSE: an INI-file format.
 * BOOKMARK_BINARY:
 * BOOKMARK_BINARY_COMPRESSED:
 */
enum BookmarkFormatType
{
	BOOKMARK_VERBOSE,
	BOOKMARK_BINARY,
	BOOKMARK_BINARY_COMPRESSED,
	BOOKMARK_INI,
	BOOKMARK_ADR,
	BOOKMARK_NO_FORMAT
};

/*
 *
 */
class BookmarkStorageListener
{
public:
	virtual ~BookmarkStorageListener() { }
	virtual void BookmarkIsLoaded(BookmarkItem *bookmark, OP_STATUS res) = 0;
	virtual void BookmarkIsSaved(BookmarkItem *bookmark, OP_STATUS res) = 0;
};

/**
 * A class which provides functionality for save and load information in some
 * format.
 */
class BookmarkStorageProvider
{
public:
	virtual ~BookmarkStorageProvider() {}

	/**
	 * Close the file after a reading or writing.
	 */
	virtual OP_STATUS Close()=0;

	/**
	 * Specify in what format the bookmarks should be saved.
	 */
	virtual OP_STATUS UseFormat(BookmarkFormatType format)=0;

	/**
	 * Save bookmark on storage.
	 */
	virtual OP_STATUS SaveBookmark(BookmarkItem *bookmark)=0;

	/**
	 * Clear the bookmark storage, that is all stored bookmarks are
	 * removed.
	 */
	virtual OP_STATUS ClearStorage()=0;

	/**
	 * Load a bookmark from storage. The parameter bookmark must be
	 * allocated.

	 * @return OpStatus::ERR_OUT_OF_RANGE if not all bookmark
	 * attributes are specified or if they have too wide values or
	 * there are too many bookmarks in folder or too large folder
	 * depth. OpStatus::ERR_NO_MEMORY if OOM and OpStatus::ERR on
	 * other errors, OpStatus::OK otherwise.
	 */
	virtual OP_STATUS LoadBookmark(BookmarkItem *bookmark)=0;

	/**
	 * Mark that bookmarks saved after this point are saved in this
	 * bookmark folder.
	 */
	virtual OP_STATUS FolderBegin(BookmarkItem *folder)=0;

	/**
	 * Mark that bookmarks saved after this point are saved in the
	 * parent bookmark folder of this bookmark folder.
	 */
	virtual OP_STATUS FolderEnd(BookmarkItem *folder)=0;

	/**
	 * @return TRUE if there are more bookmarks to load, FALSE
	 * otherwise.
	 */
	virtual BOOL MoreBookmarks()=0;

	/**
	 * Register listener to call when finished loading or saving a
	 * bookmark.
	 */
	virtual void RegisterListener(BookmarkStorageListener *l)=0;
	virtual void UnRegisterListener(BookmarkStorageListener *l)=0;
protected:
	BookmarkStorageProvider(BookmarkManager *manager) :
		m_manager(manager) {}

	BookmarkManager *m_manager;
};

#endif // CORE_BOOKMARKS_SUPPORT

#endif // BOOKMARK_STORAGE_PROVIDER_H
