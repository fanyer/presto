/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPSUPPRESSEDURL_H
#define MODULES_ABOUT_OPSUPPRESSEDURL_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the suppressed URL document.
 */
class OpSuppressedURL : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the suppressed URL document generator.
	 *
	 * @param url URL to write to.
	 * @param suppressed URL that was suppressed.
	 * @param downloadlink
	 *    Display as a download link instead of as a MIME suppressed link.
	 */
	OpSuppressedURL(URL &url, URL &suppressed, BOOL downloadlink = FALSE)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_suppressed(suppressed)
		, m_downloadlink(downloadlink)
		{}

	/**
	 * Generate the suppressed info document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	URL m_suppressed;				///< URL that was suppressed.
	BOOL m_downloadlink;			///< Whether to present link as a download link.
};

#endif
