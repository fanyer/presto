/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_INI_STORAGE_H
#define BOOKMARK_INI_STORAGE_H

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_storage_provider.h"


/**
 * An implementation of the BookmarkStorageProvider API for storing
 * bookmarks in an INI-file using prefsfile.
 */
class BookmarkIniStorage : public BookmarkStorageProvider
{
public:
	BookmarkIniStorage(BookmarkManager *manager);
	virtual ~BookmarkIniStorage();

	/**
	 * Set which file to load bookmarks from. Need only to be called
	 * once.
	 */
	virtual OP_STATUS OpenLoad(const uni_char *filename, OpFileFolder folder);

	/**
	 * Specify which file to save bookmarks to. Need not be called if
	 * this file is the same as set by call to OpenLoad.
	 */
	virtual OP_STATUS OpenSave(const uni_char *filename, OpFileFolder folder);
	virtual OP_STATUS Close();
	virtual OP_STATUS UseFormat(BookmarkFormatType format);
	virtual OP_STATUS SaveBookmark(BookmarkItem *bookmark);
	virtual OP_STATUS ClearStorage();
	virtual OP_STATUS LoadBookmark(BookmarkItem *bookmark);
	virtual OP_STATUS FolderBegin(BookmarkItem *folder);
	virtual OP_STATUS FolderEnd(BookmarkItem *folder);
	virtual BOOL MoreBookmarks();
	virtual void RegisterListener(BookmarkStorageListener *l);
	virtual void UnRegisterListener(BookmarkStorageListener *l);

	void AddToPool(BookmarkListElm *elm);
	void RemoveFromPool(uni_char *uid);
	BookmarkListElm* FetchFromPool(uni_char *uid);
	BookmarkListElm* FetchParentFromPool(uni_char *puid);
private:
    /**
	 * Check if file with name filename exists in folder. If it do not
	 * then create it.
	 * @return OpStatus::OK if everything is ok. 
	 */
	OP_STATUS CheckIfExists(const uni_char *filename, OpFileFolder folder);
	OP_STATUS LoadAttribute(BookmarkAttributeType attribute, const PrefsSection *section_name, const uni_char *entry_name, BookmarkItem *bookmark);
	OP_STATUS LoadIntAttribute(BookmarkAttributeType attribute, const PrefsSection *section_name, const uni_char *entry_name, BookmarkItem *bookmark);
	OP_STATUS SaveAttribute(BookmarkAttributeType attribute, const uni_char *section_name, const uni_char *entry_name, BookmarkItem *bookmark);
	OP_STATUS SaveIntAttribute(BookmarkAttributeType attribute, const uni_char *section_name, const uni_char *entry_name, BookmarkItem *bookmark);
	OP_STATUS SaveTargetFolderValues(const uni_char *section_name, BookmarkItem *bookmark);
	void LoadTargetFolderValues(const PrefsSection *section, BookmarkItem *bookmark);

	uni_char *m_filename;
	OpFileFolder m_folder;

	OpFile *m_bookmark_file;
	PrefsFile *m_bookmark_prefs;

	UINT m_current_section;
	UINT m_index;

	BookmarkItem *m_current_folder;
	BookmarkFormatType m_format;
	OpString_list m_sections;
	BookmarkStorageListener *m_listener;

	Head m_pool;
};

#endif // CORE_BOOKMARKS_SUPPORT

#endif // BOOKMARK_INI_STORAGE_H
