/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPILLEGALHOSTPAGE_H && defined ERROR_PAGE_SUPPORT
#define MODULES_ABOUT_OPILLEGALHOSTPAGE_H

#include "modules/about/opgenerateddocument.h"
#include "modules/url/url_lop_api.h"

/**
 * Generator for the illegal hostname error page.
 */
class OpIllegalHostPage : public OpGeneratedDocument
{
public:
	enum IllegalHostKind {
		/** Hostname contained illegal characters, like "*" */
		KIllegalCharacter,
		/** The portnumber in the URL was outside the range of 1 to 65535, inclusive, which is the permitted range  */
		KIllegalPort
	};
	/**
	 * Constructor for the inline illegal hostname error page generator.
	 *
	 * @param url               URL to write to.
	 * @param bad_url_string    The illegal URL to report. The string pointed to may not be HTMLified and is not taken over
	 * @param bad_kind          The reason the host is considered bad.
	 */
	OpIllegalHostPage(URL &url, const uni_char *bad_url_string, IllegalHostKind bad_kind = KIllegalCharacter)
		: OpGeneratedDocument(url),
		m_bad_url(bad_url_string),
		m_bad_kind(bad_kind)
		{}

	virtual ~OpIllegalHostPage() {}

	/**
	 * Generate the error document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	const uni_char *m_bad_url;		///< URL to report.
	IllegalHostKind m_bad_kind;		///< Type of problem with the URL
};

class OpIllegalHostPageURL_Generator : public OperaURL_Generator
{
public:
	virtual GeneratorMode GetMode() const { return KQuickGenerate; }
	virtual OP_STATUS QuickGenerate(URL &url, OpWindowCommander*);
};

#endif
