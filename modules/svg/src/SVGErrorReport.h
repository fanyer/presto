/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ERROR_REPORT_H
#define SVG_ERROR_REPORT_H

#ifdef SVG_SUPPORT
#ifdef SVG_LOG_ERRORS

class HTML_Element;
class SVG_DOCUMENT_CLASS;

class SVGErrorReport
{
public:
	/**
	 *  Will get doc from elm's SVGDocumentContext if NULL.
	 */
	SVGErrorReport(HTML_Element* elm, SVG_DOCUMENT_CLASS* doc = NULL) :
			m_doc(doc),
			m_elm(elm) {}

	void Report(const uni_char* format, ...) const;

	/**
	 * Will create a report with the specified message + an appended
	 * url from the specified elements xlink:href attribute.
	 */
	void ReportWithHref(const uni_char* format, ...) const;

private:
	SVG_DOCUMENT_CLASS* m_doc;
	HTML_Element* m_elm;
};

#endif // SVG_LOG_ERRORS
#endif // SVG_SUPPORT
#endif // SVG_ERROR_REPORT_H
