/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPERAUNITEADMINWARNING_H
#define OPERAUNITEADMINWARNING_H

#include "modules/about/opgenerateddocument.h"

#ifdef WEBSERVER_SUPPORT
/**
 * Generator for the suppressed URL document.
 */
class OperaUniteAdminWarningURL : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the unite warning document generator.
	 *
	 * @param url URL to write to.
	 * @param unite URL we're warning about
	 */
	OperaUniteAdminWarningURL(URL &url, URL &unite_url, BOOL is_blocking_access)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		, m_unite_url(unite_url), m_is_blocked(is_blocking_access)
		{}

	/**
	 * Generate the suppressed info document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

protected:
	URL m_unite_url;				///< URL that was suppressed.
	BOOL m_is_blocked;
};

#endif // WEBSERVER_SUPPORT


#endif /* OPERAUNITEADMINWARNING_H */
