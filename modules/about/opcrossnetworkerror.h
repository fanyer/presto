/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPCROSSNETWORKERROR_H
#define OPCROSSNETWORKERROR_H

#ifdef ERROR_PAGE_SUPPORT

#include "modules/about/opgenerateddocument.h"
#include "modules/locale/locale-enum.h"

#define OPERA_CROSS_NETWORK_ERROR_URL "opera:crossnetworkwarning"

/**
 * Generate a cross network error page.
 */
class OpCrossNetworkError : public OpGeneratedDocument
{
public:
	/**
	 * Constructor.
	 * @param url          URL to write to.
	 * @param error        String enum with proper error message for the page.
	 * @param blocked_url  URL that was blocked by this error page.
	 */
	OpCrossNetworkError(const URL& url, Str::LocaleString error, const URL& blocked_url);

	/**
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

private:
	URL m_ref_url;
	Str::LocaleString m_error;
};

#endif // ERROR_PAGE_SUPPORT

#endif // OPCROSSNETWORKERROR_H
