/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domruntime.h"

#ifdef DOM_JIL_API_SUPPORT
#	include "modules/dom/src/domjil/utils/jilutils.h"
#endif // DOM_JIL_API_SUPPORT
#ifdef DOM_WEBWORKERS_SUPPORT
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#ifndef HAS_COMPLEX_GLOBALS
extern void init_g_sql_error_strings(); // in dom/src/storage/sqlresult.cpp
#endif // HAS_COMPLEX_GLOBALS

#ifdef WEBSOCKETS_SUPPORT
#include "modules/dom/src/websockets/domwebsocket.h"
#endif //WEBSOCKETS_SUPPORT

#ifdef EXTENSION_SUPPORT
# include "modules/dom/src/extensions/domextensionmanager.h"
#endif // EXTENSION_SUPPORT

#include "modules/dom/src/domclonehandler.h"

DomModule::DomModule()
	: data(NULL)
#ifdef _DEBUG
	, debug_has_warned_about_navigator_online(FALSE)
#endif // _DEBUG
	, constructors(NULL)
	, next_file_reader_id(0)
#ifdef DOM_JIL_API_SUPPORT
	, jilUtils(NULL)
#endif // DOM_JIL_API_SUPPORT
#ifdef DOM_WEBWORKERS_SUPPORT
	, web_workers(NULL)
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	, extension_manager(NULL)
#endif // EXTENSION_SUPPORT
{
}

void
DomModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(DOM_Environment::CreateStatic());

#ifdef DOM_JIL_API_SUPPORT
	jilUtils = OP_NEW_L(JILUtils, ());
	jilUtils->ConstructL();
#endif // DOM_JIL_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	LEAVE_IF_ERROR(DOM_WebWorkers::Init());
#endif // DOM_WEBWORKERS_SUPPORT

#ifndef HAS_COMPLEX_GLOBALS
# ifdef DATABASE_STORAGE_SUPPORT
	CONST_ARRAY_INIT(g_sql_error_strings);
# endif //  DATABASE_STORAGE_SUPPORT
#endif // HAS_COMPLEX_GLOBALS

#ifdef WEBSOCKETS_SUPPORT
	DOM_WebSocket::Init();
#endif //WEBSOCKETS_SUPPORT

#ifdef EXTENSION_SUPPORT
	LEAVE_IF_ERROR(DOM_ExtensionManager::Init());
#endif //EXTENSION_SUPPORT

	OpString8HashTable<DOM_ConstructorInformation> *ctors = OP_NEW_L(OpString8HashTable<DOM_ConstructorInformation>, (TRUE));

	constructors = ctors;

	DOM_Runtime::InitializeConstructorsTableL(ctors, longest_constructor_name);

	g_ecmaManager->RegisterHostObjectCloneHandler(OP_NEW_L(DOM_CloneHandler, ()));
}

void
DomModule::Destroy()
{
	OpString8HashTable<DOM_ConstructorInformation> *ctors = static_cast<OpString8HashTable<DOM_ConstructorInformation> *>(constructors);

	if(ctors)
	{
		ctors->DeleteAll();
		OP_DELETE(ctors);
	}

#ifdef DOM_JIL_API_SUPPORT
	OP_DELETE(jilUtils);
	jilUtils = NULL;
#endif // DOM_JIL_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	if (g_webworkers)
	{
		DOM_WebWorkers::Shutdown(g_webworkers);
		g_webworkers = NULL;
	}
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef EXTENSION_SUPPORT
	if (g_extension_manager)
	{
		DOM_ExtensionManager::Shutdown(g_extension_manager);
		g_extension_manager = NULL;
	}
#endif // EXTENSION_SUPPORT

	DOM_Environment::DestroyStatic();
}

