/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if defined _PLUGIN_SUPPORT_ && !defined NO_URL_OPERA && !defined OPERAPLUGINS_H
#define OPERAPLUGINS_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:plugins document.
 */
class OperaPlugins : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the plug-ins list page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaPlugins(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5, TRUE)
		{}

	/**
	 * Generate the opera:plugins document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
};


#endif
