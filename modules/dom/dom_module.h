/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DOM_DOM_MODULE_H
#define DOM_DOM_MODULE_H

#include "modules/hardcore/opera/module.h"
#if !defined(HAS_COMPLEX_GLOBALS) && defined(DATABASE_STORAGE_SUPPORT)
# include "modules/dom/src/storage/sqlresult.h" // for the DOM_SQLError::ErrorCode enum
#endif // !HAS_COMPLEX_GLOBALS && DATABASE_STORAGE_SUPPORT

class DOM_GlobalData;
class DOM_WindowCommander;

#if defined(DOM_USER_JAVASCRIPT) && defined(OPERA_CONSOLE)
# include "modules/util/adt/opvector.h"
#endif // DOM_USER_JAVASCRIPT && OPERA_CONSOLE

#ifdef DOM_WEBWORKERS_SUPPORT
class DOM_WebWorkers;
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
class JILUtils;
#endif// DOM_JIL_API_SUPPORT

#ifdef EXTENSION_SUPPORT
class DOM_ExtensionManager;
#endif // EXTENSION_SUPPORT

class DomModule
	: public OperaModule
{
public:
	DomModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	DOM_GlobalData *data;

#if defined(DOM_USER_JAVASCRIPT) && defined(OPERA_CONSOLE)
	OpINT32Vector missing_userjs_files;
#endif // DOM_USER_JAVASCRIPT && OPERA_CONSOLE

#ifdef _DEBUG
	BOOL debug_has_warned_about_navigator_online;
#endif // _DEBUG

#if !defined(HAS_COMPLEX_GLOBALS) && defined(DATABASE_STORAGE_SUPPORT)
	const uni_char*		sql_error_strings[DOM_SQLError::LAST_SQL_ERROR_CODE];
#endif // !HAS_COMPLEX_GLOBALS && DATABASE_STORAGE_SUPPORT

	void *constructors;
	/**< Really 'OpString8HashTable<DOM_ConstructorInformation>', but kept
	     generic to avoid having to expose DOM_ConstructorInformation to the
	     world. */
	unsigned longest_constructor_name;
	/**< Longest key in the 'constructors' table, to avoid lookups of things
	     that aren't there.  JS_Window::GetName() does the lookups, so it makes
	     sense to optimize. */

	unsigned next_file_reader_id;
	/**< We need to keep track of ongoing FileReaders and we use an id number for that. */

#ifdef DOM_JIL_API_SUPPORT
	// TODO: why not keep them on stack?
	JILUtils *jilUtils;
#endif// DOM_JIL_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	DOM_WebWorkers *web_workers;
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT
	DOM_ExtensionManager *extension_manager;
#endif // EXTENSION_SUPPORT
};

#if !defined(HAS_COMPLEX_GLOBALS) && defined(DATABASE_STORAGE_SUPPORT)
# define g_sql_error_strings g_opera->dom_module.sql_error_strings
#endif // !HAS_COMPLEX_GLOBALS && DATABASE_STORAGE_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
# define g_DOM_jilUtils								(g_opera->dom_module.jilUtils)
#endif// DOM_JIL_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
# define g_webworkers (g_opera->dom_module.web_workers)
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT
# define g_extension_manager (g_opera->dom_module.extension_manager)
#endif // EXTENSION_SUPPORT

#define DOM_MODULE_REQUIRED

#endif // DOM_DOM_MODULE_H
