/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef MODULES_ABOUT_OPXMLERRORPAGE_H
#define MODULES_ABOUT_OPXMLERRORPAGE_H

#include "modules/about/opgenerateddocument.h"

class XMLParser;

/**
 * Generator for the XML parsing error document.
 */
class OpXmlErrorPage : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the XML parsing error page generator.
	 *
	 * @param url URL to write to.
	 * @param xml_parser XML parser to retrieve error information from.
	 */
	OpXmlErrorPage(URL &url, XMLParser *xml_parser)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_xml_parser(xml_parser)
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
	XMLParser *m_xml_parser;		///< Source of error message.
};

#endif
