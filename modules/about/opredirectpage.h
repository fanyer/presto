/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef MODULES_ABOUT_OPREDIRECTPAGE_H
#define MODULES_ABOUT_OPREDIRECTPAGE_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the redirection document.
 */
class OpRedirectPage : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the redirection page generator.
	 *
	 * @param url URL to write to.
	 * @param target_url URL the document is redirected to.
	 */
	OpRedirectPage(URL &url, URL *target_url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_target_url(target_url)
		{}

	/**
	 * Generate the redirection document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	URL *m_target_url;				///< URL the document is redirected to.
};

#endif
