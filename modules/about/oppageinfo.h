/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPPAGEINFO_H && defined DOC_HAS_PAGE_INFO
#define MODULES_ABOUT_OPPAGEINFO_H

#include "modules/about/opgenerateddocument.h"
#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
# include "modules/windowcommander/WritingSystem.h"
#endif

class FramesDocument;
class LogicalDocument;
class URL_InUse;

/**
 * Generator for the page properties document.
 */
class OpPageInfo : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the page properties generator.
	 *
	 * @param url URL to write to.
	 * @param frmdoc Frames document structure to fetch data from.
	 * @param logdoc Logical document structure to fetch data from.
	 * @param security_url URL to point to security properties, if needed.
	 */
	OpPageInfo(URL &url, FramesDocument *frmdoc, LogicalDocument *logdoc, URL_InUse *security_url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_frmdoc(frmdoc)
		, m_logdoc(logdoc)
		, m_security_url(security_url)
		{}

	/**
	 * Generate the info document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

#if defined AB_USE_IMAGE_METADATA && defined IMAGE_METADATA_SUPPORT
	static Str::LocaleString GetExifLocaleId(int meta_data_id);
#endif

protected:
	FramesDocument *m_frmdoc;		///< Source of page information.
	LogicalDocument *m_logdoc;		///< Source of page information.
	URL_InUse *m_security_url;		///< URL to set to security information.

	OP_STATUS OpenTable();
	OP_STATUS AddTableRow(Str::LocaleString id, Str::LocaleString data, const URL *link = NULL);
	OP_STATUS AddTableRow(Str::LocaleString id, const OpStringC &data, const URL *link = NULL);
	OP_STATUS AddTableRow(const OpStringC &heading, const OpStringC &data, const URL *link = NULL);
	OP_STATUS CloseTable();
	OP_STATUS AddListItemLink(const uni_char *urlname, const uni_char *extra = NULL);
#ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC
# ifdef SELFTEST
	friend class ST_aboutmisc;
# endif
	static Str::LocaleString ScriptToLocaleString(WritingSystem::Script script);
#endif
};

#endif
