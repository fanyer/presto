/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULES_ABOUT_OPERAWEBSTORAGE_H_
#define _MODULES_ABOUT_OPERAWEBSTORAGE_H_

#if defined DATABASE_ABOUT_WEBDATABASES_URL || defined DATABASE_ABOUT_WEBSTORAGE_URL
# include "modules/about/opgenerateddocument.h"
# ifdef OPGENERATEDDOCUMENT_H
#  include "modules/database/webstorage_data_abstraction.h" // WebStorageType

#define HAS_OPERA_WEBSTORAGE_PAGE

/**
 * Generator for the opera:webstorage and opera:webdatabases document.
 */
class OperaWebStorage : public OpGeneratedDocument
{
public:
	enum PageType {
#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
		WEB_STORAGE,
#endif // DATABASE_ABOUT_WEBSTORAGE_URL
#ifdef DATABASE_ABOUT_WEBDATABASES_URL
		WEB_DATABASES,
#endif // DATABASE_ABOUT_WEBDATABASES_URL
		NONE
	};
	/**
	 * Constructor for the webstorage admin page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaWebStorage(URL &url, PageType type)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5, TRUE)
		, m_section_id(0)
		, m_type(type){}
	
	~OperaWebStorage(){}
	/**
	 * Generate the opera:history document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

private:
#ifdef DATABASE_ABOUT_WEBDATABASES_URL
	OP_STATUS GenerateWebSQLDatabasesData();
#endif // DATABASE_ABOUT_WEBDATABASES_URL
#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	OP_STATUS GenerateWebStorageData(WebStorageType ws_t, Str::LocaleString title,
			const uni_char* clear_fn, const uni_char* prefs_obj_name);
#endif // DATABASE_ABOUT_WEBSTORAGE_URL
private:
	int m_section_id;
	PageType m_type;
};

#endif // OPGENERATEDDOCUMENT_H
#endif // defined DATABASE_ABOUT_WEBDATABASES_URL || defined DATABASE_ABOUT_WEBSTORAGE_URL

#endif // _MODULES_ABOUT_OPERAWEBSTORAGE_H_
