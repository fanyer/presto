/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#if !defined MODULES_ABOUT_OPERADEBUGNET_H && !defined NO_URL_OPERA && defined  _OPERA_DEBUG_DOC_
#define MODULES_ABOUT_OPERADEBUGNET_H

#include "modules/about/opgenerateddocument.h"
#include "modules/url/url2.h"

/**
 * Generator for the opera:memdebug document.
 */
class OperaDebugNet : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the list document generator wrapper.
	 *
	 * @param url The URL to generate document to.
	 */
	OperaDebugNet(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * Generate the opera:debug_doc document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
};

#endif
