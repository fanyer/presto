/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGDROPDATAUTILS_H
# define DRAGDROPDATAUTILS_H

# if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD

class OpDragObject;
class URL;
class OpFile;

#  define FILE_DATA_FORMAT        (UNI_L("text/x-opera-files"))
#  define TEXT_DATA_FORMAT        (UNI_L("text/plain"))
#  define HTML_DATA_FORMAT        (UNI_L("text/html"))
#  define URI_LIST_DATA_FORMAT    (UNI_L("text/uri-list"))
#  define DESCRIPTION_DATA_FORMAT (UNI_L("text/x-opera-description"))

/** Utility class encapsulating operations on the data store. */
class DragDrop_Data_Utils
{
public:
	/** Adds given url to the data store's uri list. */
	static OP_STATUS		AddURL(OpDragObject* object, URL& url);
	/** Adds given url to the data store's uri list. */
	static OP_STATUS		AddURL(OpDragObject* object, OpStringC url_str);
	/** Sets the text data (text/plain format) in the data store. */
	static OP_STATUS		SetText(OpDragObject* object, const uni_char* text);
	/** Sets the special description text data (text/x-opera-description) in the data store.
	 * It's used to store e.g. image's alt text.
	 */
	static OP_STATUS		SetDescription(OpDragObject* object, const uni_char* description);
	/** Checks if the data of given format is present in the data store. */
	static BOOL				HasData(OpDragObject* object, const uni_char* format, BOOL is_file = FALSE);
	/** Gets string data of given format from the data store. */
	static const uni_char*	GetStringData(OpDragObject* object, const uni_char* format);
	/** Clears string data of given format from the data store. */
	static void				ClearStringData(OpDragObject* object, const uni_char* format);
	/** Gets file data of given format from the data store. */
	static const OpFile*	GetFileData(OpDragObject* object, const uni_char* format);
	/** Clears file data of given format from the data store. */
	static void				ClearFileData(OpDragObject* object, const uni_char* format);
	/** Checks if text data is present in the data store. */
	static BOOL				HasText(OpDragObject* object) { return HasData(object, TEXT_DATA_FORMAT, FALSE); }
	/** Checks if the data store contains any URL. */
	static BOOL				HasURL(OpDragObject* object) { return HasData(object, URI_LIST_DATA_FORMAT, FALSE); }
	/** Checks if file data is present in the data store. */
	static BOOL				HasFiles(OpDragObject* object);
	/** Gets first URL from the data store's url-list if only_first == TRUE. The whole uri-list otherwise. */
	static OP_STATUS		GetURL(OpDragObject* object, TempBuffer* url, BOOL only_first = TRUE);
	/** Returns URLs being in the 'text/uri-list' as an array of string. */
	static OP_STATUS		GetURLs(OpDragObject* object, OpVector<OpString> &urls);
	/** Gets the text data (text/plain format) from the data store. */
	static const uni_char*	GetText(OpDragObject* object) { return GetStringData(object, TEXT_DATA_FORMAT); }
	/** Gets the special description text data (text/x-opera-description) from the data store. */
	static const uni_char*	GetDescription(OpDragObject* object) { return GetStringData(object, DESCRIPTION_DATA_FORMAT); }
	/** Gets all mime types in a form described by http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#dom-datatransfer-types */
	static OP_STATUS		DOMGetFormats(OpDragObject* object, OpVector<OpString>& formats);
};

# endif // DRAG_SUPPORT || USE_OP_CLIPBOARD

#endif // DRAGDROPDATAUTILS_H
