/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#if !defined OPERAWIDGETS_H && (defined OPERAWIDGETS_URL || defined OPERAEXTENSIONS_URL)
#define OPERAWIDGETS_H

#include "modules/about/opgenerateddocument.h"

class OperaWidgets : public OpGeneratedDocument
{
public:
	/** Type of page to generate. */
	enum PageType
	{
		  Widgets			///< Generate opera:widgets
#ifdef OPERAEXTENSIONS_URL
		, Extensions		///< Generate opera:extensions
#endif
	};

	OperaWidgets(URL url, PageType type = Widgets)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_pagetype(type)
		{}

	virtual ~OperaWidgets() {};

	/**
	* Generate the opera:widgets document to the specified internal URL.
	*
	* @return OK on success, or any error reported by URL or string code.
	*/
	OP_STATUS GenerateData();

protected:
	PageType m_pagetype;
};

#endif // !OPERAWIDGETS_H
