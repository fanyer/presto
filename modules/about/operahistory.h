/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if defined HISTORY_SUPPORT && !defined NO_URL_OPERA && !defined OPERAHISTORY_H
#define OPERAHISTORY_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:history document.
 */
class OperaHistory : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the history list page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaHistory(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * Generate the opera:history document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
};


#endif
