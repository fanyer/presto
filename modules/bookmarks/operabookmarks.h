/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPERABOOKMARKS_H
#define OPERABOOKMARKS_H

#ifdef OPERABOOKMARKS_URL

#include "modules/url/url2.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/about/opgenerateddocument.h"

/**
 * Interface class to generate the opera:bookmarks document.
 */
class OperaBookmarks : public OpGeneratedDocument
{
public:
	OperaBookmarks(URL url) :
		OpGeneratedDocument(url, OpGeneratedDocument::HTML5) {}
	virtual ~OperaBookmarks() {}
	
	/**
	 * Generate the opera:bookmarks document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
	
private:
};

class ES_Object;
class ES_AsyncInterface;

class OperaBookmarksListener : public BookmarkManagerListener
{
public:
	OperaBookmarksListener();
	virtual ~OperaBookmarksListener();

	void SetAsyncInterface(ES_AsyncInterface *ai) { m_ai = ai; }
	void SetCallback(ES_Object *callback) { m_callback = callback; }
	ES_Object* GetCallback() const { return m_callback; }

	void SetSavedFormValues(const uni_char *form_url, const uni_char *form_title, const uni_char *form_desc, const uni_char *form_sn, const uni_char *form_parent_title);
	uni_char* GetSavedFormUrlValue() { return m_form_url; }
	uni_char* GetSavedFormTitleValue() { return m_form_title; }
	uni_char* GetSavedFormDescriptionValue() { return m_form_desc; }
	uni_char* GetSavedFormShortnameValue() { return m_form_sn; }
	uni_char* GetSavedFormParentTitleValue() { return m_form_parent_title; }

	void OnBookmarksSaved(OP_STATUS ret, UINT32 operation_count);
	void OnBookmarksLoaded(OP_STATUS ret, UINT32 operation_count);
#ifdef SUPPORT_DATA_SYNC
	void OnBookmarksSynced(OP_STATUS ret);
#endif // SUPPORT_DATA_SYNC
	virtual void OnBookmarkAdded(BookmarkItem *bookmark) { }
	virtual void OnBookmarkDeleted(BookmarkItem *bookmark) { }
	virtual void OnBookmarkChanged(BookmarkItem *bookmark) { }
private:
	OP_STATUS SetValue(uni_char **dst, const uni_char *src);

	ES_AsyncInterface *m_ai;
	ES_Object *m_callback;
	uni_char *m_form_url;
	uni_char *m_form_title;
	uni_char *m_form_desc;
	uni_char *m_form_sn;
	uni_char *m_form_parent_title;
};

#endif // OPERABOOKMARKS_URL

#endif // OPERABOOKMARKS_H
