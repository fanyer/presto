/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Mostyn Bramley-Moore
*/

#if !defined MODULES_ABOUT_OPERACREDITS_H && !defined NO_URL_OPERA
#define MODULES_ABOUT_OPERACREDITS_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:credits (third-party information) document.
 */
class OperaCredits : public OpGeneratedDocument
{
public:

	/**
	 * Fills a string with the third party credit information in opera:about
	 * (and opera:credits, if enabled).
	 *
	 * @param[in] str An OpString to write to.
	 * @param[in] rtl Whether or not the text should be right-to-left.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	static OP_STATUS GetString(OpString &str, BOOL rtl);

#ifdef OPERA_CREDITS_PAGE

	/**
	 * Constructor for the credits page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaCredits(URL &url) : OpGeneratedDocument(url, OpGeneratedDocument::HTML5) {}

	/**
	 * Generate the credits document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

#endif // OPERA_CREDITS_PAGE
};

#endif // !MODULES_ABOUT_OPERACREDITS_H && !NO_URL_OPERA
