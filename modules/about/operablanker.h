/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPERABLANKER_H && !defined NO_URL_OPERA && defined SELFTEST
#define MODULES_ABOUT_OPERABLANKER_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:blanker (empty) document.
 */
class OperaBlanker : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the empty page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaBlanker(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * Generate the opera:blanker document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
};

#endif
