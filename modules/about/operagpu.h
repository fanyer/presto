/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/


#ifndef OPERAGPU_H
#define OPERAGPU_H

#include "modules/about/opgenerateddocument.h"

class OperaGPU : public OpGeneratedDocument
{
public:
	/**
	 * @param url URL to write to.
	 */
	OperaGPU(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * See documentation in OpGeneratedDocument.
	 */
	virtual OP_STATUS GenerateData();

	/**
	 * Create a paragraph containing str.
	 * NOTE: should only be used directly when str doesn't need translation.
	 * @param str the contents of the paragraph
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Paragraph(const OpStringC &str);
	/**
	 * Create a heading.
	 * NOTE: should only be used directly when str doesn't need translation.
	 * @param str the heading text
	 * @param level the heading level (eg 1 for h1, 2 for h2 etc)
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Heading(const OpStringC &str, unsigned level = 1);
	/**
	 * Create a list entry in a previously opened definition list -
	 * should only be called between a call to OpenDefinitionList and
	 * CloseDefinitionList.
	 * NOTE: should only be used when term, def and details don't need
	 * translation.
	 * @param term the term to be defined
	 * @param def the definition of the term
	 * @param details any additional details (will be italicized)
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS ListEntry(const OpStringC &term, const OpStringC &def, const OpStringC &details = 0);
	/**
	 * Create a hyperlink.
	 * NOTE: should only be used when title doesn't need translation.
	 * @param title the title of the hyperlink
	 * @param url the href
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Link(const OpStringC &title, const OpStringC &url);

	/**
	 * Create a heading.
	 * @param heading the heading text
	 * @param level the heading level (eg 1 for h1, 2 for h2 etc)
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Heading(Str::LocaleString heading, unsigned level = 1);
	/**
	 * Open a definition list. After creating the list entries can be
	 * added using ListEntry(). Should be closed by calling
	 * CloseDefinitionList().
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS OpenDefinitionList();
	/**
	 * Create a list entry in a previously opened definition list -
	 * should only be called between a call to OpenDefinitionList and
	 * CloseDefinitionList.
	 * @param term the term to be defined
	 * @param def the definition of the term
	 * @param details any additional details (will be italicized)
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS ListEntry(Str::LocaleString term, Str::LocaleString def, const OpStringC &details = 0);
	/**
	 * Close a previously opened definition list - should only be
	 * called after a successful call to OpenDefinitionList().
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS CloseDefinitionList();

private:
	OP_STATUS GenerateBackendInfo();
};

#endif // !OPERAGPU_H
