/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#if !defined MODULES_ABOUT_OPPRIVACYMODE_H
#define MODULES_ABOUT_OPPRIVACYMODE_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:private document.
 */
class OpPrivacyModePage : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the opera:private page generator.
	 *
	 * @param url URL to write to.
	 * @param enabled Whether privacy mode is enabled or not.
	 */
	OpPrivacyModePage(URL &url, BOOL enabled)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_enabled(enabled)
		{}

	/**
	 * Generate the privacy mode document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	BOOL m_enabled; ///< Privacy mode enabled flag.
};

#endif
