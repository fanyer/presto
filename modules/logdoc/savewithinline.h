/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SAVEWITHINLINE_H
#define SAVEWITHINLINE_H

#ifdef SAVE_SUPPORT

#include "modules/url/url2.h"
#include "modules/upload/upload.h"

class Window;

#define FILENAME_BUF_SIZE 1024

BOOL SafeTypeToSave(Markup::Type element_type, URL content);

class SavedUrlCache
{
private:
	struct url_and_filename {
		URL			url;
		uni_char	name[14]; /* ARRAY OK 2009-02-05 stighal */
	} m_array[1024]; /* ARRAY OK 2009-02-05 stighal */

	int			m_used;

	uni_char	m_filename_buf[FILENAME_BUF_SIZE]; /* ARRAY OK 2009-05-11 stighal */

	// Points to the end of the relative directory, i.e. where the filename should go
	uni_char*	m_filename_start;

	// Points to the base directory
	uni_char*   m_reldir_start;

public:

					SavedUrlCache(const uni_char* fname);

	const uni_char*	GetSavedFilename(const URL& url, BOOL& new_filename);
	const uni_char*	GetPathname() const { return m_filename_buf; }

	const uni_char* GetFilenameStart() const { return m_filename_start;}
	const uni_char* GetReldirStart() const { return m_reldir_start; }
};

class SaveWithInlineHelper
{
public:
	/**
	 * Save a document and its sub-resources to disk.
	 * @param url The URL of the document to save.
	 * @param displayed_doc Pointer to the FramesDocument representing the
	 *   document we are about to save, or NULL if not available.
	 * @param force_encoding Force document to be written using a specific
	 *   encoding, NULL if you want to use the document encoding.
	 * @param fname File name to save the parent document to.
	 * @param window The window that requested the document to be saved.
	 *   Any errors are reported to its associated WindowCommander.
	 * @param frames_only Only save linked frames if set to TRUE.
	 */
	static OP_STATUS	Save(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, Window *window, BOOL frames_only = FALSE);
	/** @overload */
	static void			SaveL(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, Window *window, BOOL frames_only);
	/** @overload */
	static OP_STATUS	Save(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, SavedUrlCache* su_cache, Window *window, BOOL frames_only = FALSE);
	/** @overload */
	static void			SaveL(URL& url, FramesDocument *displayed_doc, const uni_char* fname, const char *force_encoding, SavedUrlCache* su_cache, Window *window, BOOL frames_only = FALSE, BOOL main_page = TRUE);
};

#ifdef MHTML_ARCHIVE_SAVE_SUPPORT
class SaveAsArchiveHelper
{
public:
	/** Save the URL including all inline files (images, CSS, scripts etc.)
	 *  in one MHTML (MIME multipart encoded) file. Links to other documents
	 *  however are not followed or included.
	 *
	 *	@param	url             URL of the top document to save
	 *	@param	fname			Filename to save the file as. Must be a full path.
	 *	@return OP_STATUS
	 */
	static OP_STATUS Save(URL& url, const uni_char* fname, Window *window);

    /** Save the URL including all inline files (images, CSS, scripts etc.)
	 *  in one MHTML (MIME multipart encoded) file. Links to other documents
	 *  however are not followed or included. Return original page size and
     *  page size after saving.
	 *
	 *	@param	url             URL of the top document to save
	 *	@param	fname			Filename to save the file as. Must be a full path.
	 *  @param	max_size		The maximum size in bytes that the saved page is allowed
	 *							to use. 0 for no limit. If the page would exceed the limit,
	 *							Opera will try to reduce its size, by ommiting parts of the
	 *							document, or fail to save it the page can't be reduced to a
	 *							sufficiently small size.
     *  @param page_size        Pointer to a variable that'll get original page size
     *  @param saved_size       Pointer to a variable that'll get saved page size
	 *	@return OP_STATUS
	 */
    static OP_STATUS SaveAndReturnSize(URL& url, const uni_char* fname, Window *window,
		unsigned int max_size = 0, unsigned int* page_size = NULL, unsigned int* saved_size = NULL);

    /** Save the URL including all inline files (images, CSS, scripts etc.)
	 *  in one MHTML (MIME multipart encoded) file. Links to other documents
	 *  however are not followed or included.
	 *
	 *	@param	url             URL of the top document to save
	 *	@param	fname			Filename to save the file as. Must be a full path.
	 *  @param	max_size		See Save()
     *  @param page_size        Pointer to a variable that'll get original page size
     *  @param saved_size       Pointer to a variable that'll get saved page size
	 */
	static void		SaveL(URL& url, const uni_char* fname, Window *window,
						unsigned int max_size = 0,
						unsigned int* page_size = NULL,
						unsigned int* saved_size = NULL);

	/** Save the URL including all inline files (images, CSS, scripts etc.)
	 *  in one MHTML (MIME multipart encoded) Upload_Multipart archive. Links to other documents
	 *  however are not followed or included.
	 *
	 *	@param	url             URL of the top document to save
	 *	@param	archive			Upload_Multipart where to save the document.
	 *  @param	mail_message	Whether the URL is a mail message or not
	 */
	static void		GetArchiveL(URL& top_url, Upload_Multipart& archive, Window *window, BOOL mail_message = FALSE);

private:
	/** Reorder the files so that the root page comes first, followed by any other
	 *  html/xhtml/xml file, then svg files, then css files, and finaly images,
	 *  scripts, and other files in no specific order. The root page is assumed to
	 *  be the first item in the received parameter.
	 *
	 *  @param archive			The multipart document that should be reordered
	 */
	static void			SortArchive(Upload_Multipart &archive);
};
#endif // MHTML_ARCHIVE_SAVE_SUPPORT
#endif // SAVE_SUPPORT

#endif // SAVEWITHINLINE_H
