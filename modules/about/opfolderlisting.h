/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPFOLDERLISTING_H && (defined _LOCALHOST_SUPPORT_ || !defined NO_FTP_SUPPORT)
#define MODULES_ABOUT_OPFOLDERLISTING_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for file:// and ftp:// folder listings. The body is
 * generated with help from the corresponding loadhandler document.
 */
class OpFolderListing : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the folder listing page generator.
	 *
	 * @param url URL to write to.
	 */
	OpFolderListing(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_list_open(false)
		, m_temp_base(NULL)
		, m_htmlified_url(NULL)
		, m_displayable_base_url(NULL)
		, m_htmlified_displayable_base_url(NULL)
		{}

	virtual ~OpFolderListing();

	/**
	 * Open the document and generate the folder listing head to the
	 * specified internal URL.
	 *
	 * <b>NB!</b>
	 * Unlike most other OpGeneratedDocument subclasses, the document
	 * is NOT closed after the call to this method. The caller need to
	 * call WriteWelcomeMessage() (if needed) and then WriteEntry() for
	 * each file to list. The document will be closed when
	 * EndFolderListing() is called.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

#ifndef NO_FTP_SUPPORT
	/**
	 * Write a welcome message from the server to the folder listing
	 * document. The message is written as a pre-formatted string (any
	 * markup will be shown as raw text in the document), with links
	 * expanded.
	 *
	 * @param message Welcome message to display.
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS WriteWelcomeMessage(const OpStringC &message);
#endif

	/**
	 * Write a directory entry to the folder listing. Will add the proper
	 * start-of-listing HTML code when the first entry is added. The list
	 * will be closed when EndFolderListing() is called.
	 *
	 * @param url URL to the file to add to the listing.
	 * @param name Name of file to add to the listing.
	 * @param ftptype FTP type to use ("i" or nothing).
	 * @param mode File mode (file, directory, symbolic link).
	 * @param size Size of the file (ignored for directories).
	 * @param datetime Time stamp of file.
	 * @return OK on success, or error code.
	 */
	OP_STATUS WriteEntry(const OpStringC &url, const OpStringC &name, char ftptype,
						 OpFileInfo::Mode mode, OpFileLength size,
						 const struct tm *datetime);
	
	/**
	 * Generate the folder listing footer and close the document. Must be
	 * called after GenerateData().
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	OP_STATUS EndFolderListing();

protected:
	bool m_list_open;			///< Flag if there are entries in the listing.
	OpString m_str_directory;	///< Caption for directory.
	OpString m_str_b;			///< Caption for byte.
	OpString m_str_kb;			///< Caption for kilobyte.

	/* Temporary pointers used for formatting. */
	uni_char *m_temp_base;
	uni_char *m_htmlified_url;
	uni_char *m_displayable_base_url;
	uni_char *m_htmlified_displayable_base_url;

	/** Remove formatting characters from displayable strings. */
	void RemoveFormattingCharacters(uni_char *s);
};

#endif
