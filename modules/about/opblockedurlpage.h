/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MODULES_ABOUT_OPBLOCKEDURLPAGE_H
#define MODULES_ABOUT_OPBLOCKEDURLPAGE_H

#ifdef ERROR_PAGE_SUPPORT
#ifdef URL_FILTER

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the inline error page when a url is blocked.
 */
class OpBlockedUrlPage : public OpGeneratedDocument
{
public:
	/**
	 * First-phase constructor for error page generator.
	 *
	 * @param url URL to write to.
	 * @param ref_url URL error message corresponds to.
	 */
	OpBlockedUrlPage(URL &url, URL *ref_url)
		: OpGeneratedDocument(url)
		, m_ref_url(ref_url)
		{}

	/**
	 * Generate the error document to the specified internal URL. This will
	 * overwrite anything that has already been written to the URL, so the
	 * caller must make sure we do not overwrite anything important.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	URL *m_ref_url;					///< URL error message corresponds to.
};

#endif // URL_FILTER
#endif // ERROR_PAGE_SUPPORT

#endif // MODULES_ABOUT_OPBLOCKEDURLPAGE_H

