/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Pavel Studeny
*/

#ifdef OPERAHISTORYSEARCH_URL
#define OPHISTORYSEARCH_H

#include "modules/about/opgenerateddocument.h"
#include "modules/search_engine/Vector.h"
#include "modules/search_engine/VisitedSearch.h"
#include "modules/search_engine/WordHighlighter.h"

/**
 * Generator for opera:historysearch, the visited pages search.
 * @author Pavel Studeny <pavels@opera.com>
 */
class OperaHistorySearch : public OpGeneratedDocument
{
public:
	static OperaHistorySearch *Create(URL &url);

	/**
	 * Constructor for the opera:historysearch page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaHistorySearch(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5, TRUE)
		, m_query(NULL)
		, m_res_p_page(0)
		, m_start_res(0)
		{}

	virtual ~OperaHistorySearch();

	OP_STATUS Init();

	virtual OP_STATUS GenerateData();

protected:
	OP_STATUS WriteResult(OpString &html, const VisitedSearch::Result &result, int res_no);
	static OP_STATUS ParseQuery(uni_char **dst, int *rpp, int *sr, const char *src);

	uni_char *m_query;
	WordHighlighter m_highlighter;
	int m_res_p_page;
	int m_start_res;
};

#endif
