/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _STATIC_PLUGIN_H_INC_
#define _STATIC_PLUGIN_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plug-inc/npfunctions.h"

class OpPluginWindow;

// Define NP_InitializeP and NP_ShutdownP
#ifdef NS4P_OPDLL_SELF_INIT
class OpDLL;
typedef NPError (* NP_InitializeP)(OpDLL*, NPNetscapeFuncs*, NPPluginFuncs*);
typedef int (* NP_ShutdownP)(OpDLL*);
#else
#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI)
typedef NPError (WINAPI* NP_InitializeP)(NPNetscapeFuncs* funcs);
typedef int (WINAPI* NP_ShutdownP)();
#else // MSWIN
#ifdef __cplusplus
extern "C" {
#endif
#if defined (ANDROID)
typedef NPError (* NP_InitializeP)(NPNetscapeFuncs*, NPPluginFuncs*, JNIEnv*);
typedef NPError (* NP_ShutdownP)();
#elif defined(_MACINTOSH_)
typedef NPError (* NP_InitializeP)(NPNetscapeFuncs*);
typedef NPError (* NP_ShutdownP)();
#else
typedef NPError (* NP_InitializeP)(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError (* NP_ShutdownP)();
#endif
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // MSWIN
#endif // NS4P_OPDLL_SELF_INIT

#ifdef _MACINTOSH_
#define WINAPI
#endif

// Define NP_GetEntryPointsP
#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI) || defined(_MACINTOSH_)
typedef NPError (WINAPI* NP_GetEntryPointsP)(NPPluginFuncs* pFuncs);
#endif // MSWIN

#ifdef STATIC_NS4PLUGINS
struct StaticPlugin
{
	StaticPlugin() :
		init_function(0),
		shutdown_function(0) {}

    OpString plugin_name;
    NP_InitializeP init_function;
    NP_ShutdownP   shutdown_function;
};
#endif // STATIC_NS4PLUGINS

# if defined(NS4P_SUPPORT_PROXY_PLUGIN) || defined(NS4P_COMPONENT_PLUGINS)
class ProxyPlugin
{
public:
	static OpPluginWindow * GetPluginWindow(NPP instance);
};
# endif // defined(NS4P_SUPPORT_PROXY_PLUGIN) || defined(NS4P_COMPONENT_PLUGINS)

#endif // _PLUGIN_SUPPORT_
#endif // !_STATIC_PLUGIN_H_INC_
