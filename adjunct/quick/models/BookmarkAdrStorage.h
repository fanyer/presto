/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _BOOKMARK_ADR_STORAGE_H_
#define _BOOKMARK_ADR_STORAGE_H_

#include "modules/bookmarks/bookmark_storage_provider.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "adjunct/quick/hotlist/hotlistfileio.h"
#include "adjunct/quick/models/BookmarkModel.h"

#include "adjunct/quick/models/FolderStack.h"

/**************************************************************
 *
 * BookmarkAdrStorage
 * 
 * Only supports _reading_ adr files.
 * For backwards compatibility and import.
 *
 *
 *************************************************************/

class BookmarkAdrStorage : public BookmarkStorageProvider
{
public:
	BookmarkAdrStorage(BookmarkManager *manager, const uni_char* path, BOOL sync = TRUE, BOOL merge_trash = FALSE);
	~BookmarkAdrStorage() { Close(); }

	// --- BookmarkStorageProvider -------
	virtual OP_STATUS ClearStorage();
	virtual OP_STATUS Close();
	
	virtual OP_STATUS UseFormat(BookmarkFormatType format);
	virtual OP_STATUS SaveBookmark(BookmarkItem *bookmark);
	virtual OP_STATUS FolderBegin(BookmarkItem *folder);
	virtual OP_STATUS FolderEnd(BookmarkItem *folder);
	virtual void RegisterListener(BookmarkStorageListener *l);
	virtual void UnRegisterListener(BookmarkStorageListener *l);

	virtual OP_STATUS LoadBookmark(BookmarkItem *bookmark);
	virtual BOOL MoreBookmarks();

	// Read the data but don't add it to core
	OP_STATUS ReadBookmarkData(BookmarkItemData& item_data);

	const OpString& GetFilename(){ return m_filename; }

protected:

	void      SetFilename(const uni_char* filename) { m_filename.Set(filename); }
	OP_STATUS ParseHeader();
	INT32     GetItemType(const OpString& line);
	INT32     GetItemField(const OpString& line, OpString& value);
	BOOL      GetNextField(BookmarkItemData& item_data);
	BOOL      Advance();
	int IsKey(const uni_char* candidate, int candidateLen,
			  const char* key, int keyLen);
	BOOL IsYes(const OpStringC value) { return value.Compare(UNI_L("YES")) == 0; }
	OP_STATUS DecodeUniqueId(uni_char *uid, char *new_uid);
	OP_STATUS OpenSave();
	OP_STATUS OpenRead();

	BOOL m_sync; // sync changes, FALSE when importing
	BOOL m_merge_trash; // should we merge all trashes into one when loading, set to TRUE when importing
	OpString m_filename;
	HotlistFileWriter* m_writer;
	HotlistFileReader* m_reader;
	FolderStack<BookmarkItem> m_stack;
	OpString m_last_line;
	BookmarkItem* m_last_item;
	BookmarkStorageListener *m_listener;
};

// Parse the default bookmarks file when upgrading opera and
// 1 add newly added default bookmarks to user profile
// 2 udpate existing default bookmarks in user profile
class DefaultBookmarkUpgrader : protected BookmarkAdrStorage
{
public:
	DefaultBookmarkUpgrader();
	OP_STATUS Run();

private:
	OP_STATUS UpgradeBookmark(BookmarkItemData& item);
	OP_STATUS UpdateExistingBookmark(BookmarkItem* item, BookmarkItemData& item_data);

	// Record folders we added and remove empty ones at the end
	OpAutoVector<OpString> m_added_folders;
};

#endif // BOOKMARK_ADR_STORAGE_H_
