/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined OPERACONFIG_H && defined OPERACONFIG_URL
#define OPERACONFIG_H

#include "modules/url/url2.h"
#include "modules/about/opgenerateddocument.h"

struct prefssetting;

/**
 * Interface class to generate the opera:config document.
 */
class OperaConfig
	: public OpGeneratedDocument
{
public:
	OperaConfig(URL url) :
		OpGeneratedDocument(url, OpGeneratedDocument::HTML5, TRUE),
		m_settings(NULL)
		{}
	virtual ~OperaConfig();

	/**
	 * Generate the opera:config document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

private:
	prefssetting *m_settings; ///< List of settings.
};

#endif
