/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Library entry points.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include <algorithm>
#include <cassert>
#include "common.h"

/**
 * Global library data.
 */
NPNetscapeFuncs* g_browser = 0;
bool g_crashed = false;
std::map<std::string, char*> g_cookies;
ObjectInstanceMap g_object_instance_map;
#ifdef XP_WIN
WindowInstanceMap g_window_instance_map;
#endif // XP_WIN

#ifdef XP_MACOSX
/**
 * Required for Mac to make methods externally visible.
 * This feels slightly wrong because npfunctions.h
 * should theoretically take care of it (but it doesn't).
 */
extern "C"
{
  NPError OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs);
  NPError OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
  NPError OSCALL NP_Shutdown();
}
#endif // XP_MACOSX

/**
 * Initialize and supply library with browser entry points.
 *
 * @See https://developer.mozilla.org/en/NP_Initialize.
 */
#if defined(XP_WIN) || defined(XP_MACOSX)
NPError OSCALL NP_Initialize(NPNetscapeFuncs* browser_functions)
{
	if (!browser_functions)
		return NPERR_INVALID_FUNCTABLE_ERROR;

#else // !XP_WIN && !XP_MACOSX
extern "C" NPError NP_Initialize(NPNetscapeFuncs* browser_functions, NPPluginFuncs* plugin_functions)
{
	if (!browser_functions || !plugin_functions)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if (plugin_functions->size < sizeof(*plugin_functions))
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	initialize_plugin_function_structure(plugin_functions);
#endif // !XP_WIN && !XP_MACOSX

	if (browser_functions->version < 21)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	if (!(g_browser = static_cast<NPNetscapeFuncs*>(browser_functions->memalloc(sizeof(*g_browser)))))
		return NPERR_OUT_OF_MEMORY_ERROR;

	memset(g_browser, 0, sizeof(*g_browser));
	/* Note parentheses around std::min to prevent macro expansion. */
	memcpy(g_browser, browser_functions, (std::min)(static_cast<long unsigned int>(browser_functions->size),
				static_cast<long unsigned int>(sizeof(*g_browser))));

	return NPERR_NO_ERROR;
}


/**
 * Make library release all resources.
 *
 * @See https://developer.mozilla.org/en/NP_Shutdown.
 */
#if defined(XP_UNIX) || defined(XP_GOGI)
extern "C" NPError OSCALL NP_Shutdown()
#else // !XP_UNIX && !XP_GOGI
NPError OSCALL NP_Shutdown()
#endif // !XP_UNIX && !XP_GOGI
{
	for (std::map<std::string, char*>::iterator i = g_cookies.begin(); i != g_cookies.end(); ++i)
		g_browser->memfree(i->second);
	g_cookies.clear();
	if (g_browser)
		g_browser->memfree(g_browser);

	return NPERR_NO_ERROR;
}


/**
 * Called by browser to retrieve MIME types supported by this library.
 *
 * Not used on Windows, where the MIME type is read from the DLL version information.
 *
 * @See https://developer.mozilla.org/en/NP_GetMimeDescription.
 */
#if defined(XP_UNIX) || defined(XP_GOGI)
extern "C" const char* OSCALL NP_GetMIMEDescription()
{
	return MIME_TYPE ":opera-test-plugin:Opera Test Plugin";
}
#endif // XP_UNIX || XP_GOGI

/**
 * Called by browser to retrieve complete list of library entry points.
 *
 * Not used on UNIX, where the data is retrieved using NP_Initialize.
 *
 */
#ifndef XP_UNIX
NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* plugin_functions)
{
	if (!plugin_functions)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	initialize_plugin_function_structure(plugin_functions);

	return NPERR_NO_ERROR;
}
#endif // !XP_UNIX

/**
 * Called by browser to retrieve the name of this plugin.
 *
 * Not used on Windows, where the name resides in the DLL meta data.
 *
 */
#if defined(XP_UNIX) || defined(XP_GOGI)
extern "C" NPError OSCALL NP_GetValue(void* /* future */, NPPVariable variable, void* value)
{
	if (!value)
		return NPERR_INVALID_PARAM;

	switch (variable)
	{
		case NPPVpluginNameString:
			*static_cast<char**>(value) = const_cast<char*>("Opera Test Plugin");
			break;

		case NPPVpluginDescriptionString:
			*static_cast<char**>(value) = const_cast<char*>("Opera Test Plugin 1.2.0");
			break;

		case NPPVpluginNeedsXEmbed:
			*static_cast<bool*>(value) = true;
			break;

		default:
			return NPERR_INVALID_PARAM;
	};

	return NPERR_NO_ERROR;
}
#endif // XP_UNIX || XP_GOGI
