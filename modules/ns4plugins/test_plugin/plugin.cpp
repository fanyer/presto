/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Plugin entry points and associated helper functions.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include <cassert>
#include <cstdio>

#include "common.h"


/**
 * Create new plugin instance.
 *
 * @See https://developer.mozilla.org/en/NPP_New.
 */
NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16_t mode, int16_t argc, char** argn, char** argv, NPSavedData* /*data*/)
{
	/* Don't allow new instances after crash. Crash again to send a message? */
	if (g_crashed)
		return NPERR_INVALID_PLUGIN_ERROR;

	/* Verify mime type. */
	if (!plugin_type || strcmp(plugin_type, MIME_TYPE))
		return NPERR_INVALID_PARAM;

	/* Only accept embedded mode (as opposed to exist without a document). */
	if (mode != NP_EMBED)
		return NPERR_INVALID_PARAM;

	/* Default to windowed and no onload function. */
	bool windowless = false;
	bool windowless_param_found = false;
	const char* onload = 0;
	const char* bgcolor = 0;

	for (int i = 0; i < argc; i++)
	{
		if (!windowless_param_found && (windowless_param_found = !STRCASECMP(argn[i], "windowless")))
			windowless = !STRCASECMP(argv[i], "true") ? true : false;
		else if (!STRCASECMP(argn[i], "_onload"))
			onload = argv[i];
		else if (!STRCASECMP(argn[i], "bgcolor"))
			bgcolor = argv[i];
	}

#ifdef XP_GOGI
	/* For GOGI we would like to run the plugin as windowless even if the
	 * client specifically has requested to use windowed mode. The reason for
	 * this is that we would like to be able to run all the TC's for the plugin
	 * even if we haven't implemented windowed mode. */
	if (!windowless)
	  windowless = true;
#endif // XP_GOGI

#ifdef XP_MACOSX
	/* Only windowless plugins are supported on Mac. */
	if (windowless_param_found && !windowless)
		return NPERR_INVALID_PARAM;

	/* When no windowless param specified, default to windowless. */
	windowless = true;
#endif // XP_MACOSX

	/* Create new plugin instance. */
	PluginInstance* pi;

	if (windowless)
		pi = new WindowlessInstance(instance, bgcolor);
	else
		pi = new WindowedInstance(instance, bgcolor);

	if (!pi)
		return NPERR_OUT_OF_MEMORY_ERROR;

	if (!pi->Initialize())
	{
		delete pi;
		return NPERR_INVALID_PARAM;
	}

	/* Verify arguments. */
	NPError err = pi->CheckArguments(argc, argn, argv);
	if (err != NPERR_NO_ERROR)
	{
		delete pi;
		return err;
	}

	instance->pdata = pi;

	/*
	 * The following evaluation may induce many calls to the newly
	 * created instance and all will take place prior to this function
	 * completing. Be aware.
	 */
	if (onload)
	{
		if (NPObject* global = pi->GetGlobalObject())
		{
			NPString script;
			script.UTF8Characters = onload;
			script.UTF8Length = static_cast<uint32_t>(strlen(onload));

			NPVariant return_value;
			if (g_browser->evaluate(instance, global, &script, &return_value))
				g_browser->releasevariantvalue(&return_value);
		}
	}

	return NPERR_NO_ERROR;
}


/**
 * Destroy plugin instance.
 *
 * @See https://developer.mozilla.org/en/NPP_Destroy.
 */
NPError NPP_Destroy(NPP instance, NPSavedData** /*data*/)
{
	if (instance && instance->pdata)
		static_cast<PluginInstance*>(instance->pdata)->SafeDelete();

	return NPERR_NO_ERROR;
}


/**
 * Window create, move, resize or destroy notification.
 *
 * @See https://developer.mozilla.org/en/NPP_SetWindow.
 */
NPError NPP_SetWindow(NPP instance, NPWindow* window)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->SetWindow(window);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * New data stream notification.
 *
 * @See https://developer.mozilla.org/en/NPP_NewStream.
 */
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->NewStream(type, stream, seekable, stype);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Data stream close or destroy notification.
 *
 * @See https://developer.mozilla.org/en/NPP_DestroyStream.
 */
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->DestroyStream(stream, reason);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Provide a local file name for the data from a stream.
 *
 * @See https://developer.mozilla.org/en/NPP_StreamAsFile.
 */
void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		pi->StreamAsFile(stream, fname);
}


/**
 * Retrieve maximum number of bytes plugin is ready to consume.
 *
 * @See https://developer.mozilla.org/en/NPP_WriteReady.
 */
int32_t NPP_WriteReady(NPP instance, NPStream* stream)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->WriteReady(stream);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Deliver stream data.
 *
 * @See https://developer.mozilla.org/en/NPP_Write.
 */
int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buf)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->Write(stream, offset, len, buf);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Request platform specific print operation.
 *
 * @See https://developer.mozilla.org/en/NPP_Print.
 */
void NPP_Print(NPP instance, NPPrint* print_info)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		pi->Print(print_info);
}


/**
 * Deliver a platform specific window event.
 *
 * @See https://developer.mozilla.org/en/NPP_HandleEvent.
 */
int16_t NPP_HandleEvent(NPP instance, void* event)
{
	PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata);
	if (!pi || !pi->IsWindowless())
		return NPERR_INVALID_INSTANCE_ERROR;

	return static_cast<WindowlessInstance*>(pi)->HandleEvent(event);
}


/**
 * URL request completion notification.
 *
 * @See https://developer.mozilla.org/en/NPP_URLNotify.
 */
void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* data)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		pi->URLNotify(url, reason, data);
}


/**
 * Query plugin information.
 *
 * @See https://developer.mozilla.org/en/NPP_GetValue.
 */
NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->GetValue(variable, value);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Set browser information.
 *
 * @See https://developer.mozilla.org/en/NPP_SetValue.
 */
NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->SetValue(variable, value);

	return NPERR_INVALID_INSTANCE_ERROR;
}


/**
 * Set focus.
 *
 * @See https://wiki.mozilla.org/NPAPI:AdvancedKeyHandling.
 */
NPBool NPP_GotFocus(NPP instance, NPFocusDirection direction)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->GotFocus(direction);

	return false;
}


/**
 * Unset focus.
 *
 * @See https://wiki.mozilla.org/NPAPI:AdvancedKeyHandling.
 */
void NPP_LostFocus(NPP instance)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		pi->LostFocus();
}


/**
 * Redirect notification.
 *
 * @See https://wiki.mozilla.org/NPAPI:HTTPRedirectHandling.
 */
void NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status, void* notifyData)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		pi->URLRedirectNotify(url, status, notifyData);
}


/**
 * Test method for NPP_ClearSiteData. The first argument is the site for which
 * to clear data, second argument is the type of data to clear, third argument
 * is maximum age in seconds of the data that should be cleared.
 *
 * First argument can be either NULL to clear all site related data or string
 * pointer to domain of the site whose data is to be cleared.
 *
 * @See https://wiki.mozilla.org/NPAPI:ClearSiteData.
 */
NPError NPP_ClearSiteData(const char* /*site*/, uint64_t /*flags*/, uint64_t /*maxAge*/)
{
	for (std::map<std::string, char*>::iterator i = g_cookies.begin(); i != g_cookies.end(); ++i)
		g_browser->memfree(i->second);
	g_cookies.clear();
	return NPERR_NO_ERROR;
}


/**
 * Get stored private data.
 *
 * @See https://wiki.mozilla.org/NPAPI:ClearSiteData.
 */
char** NPP_GetSitesWithData()
{
	char** sites = static_cast<char**>(g_browser->memalloc((static_cast<uint32_t>(g_cookies.size()) + 1) * sizeof(char*)));
	if (!sites)
		return NULL;

	char** site_iter = sites;
	for (std::map<std::string, char*>::iterator i = g_cookies.begin(); i != g_cookies.end(); ++i)
	{
		const unsigned int len = static_cast<unsigned int>(i->first.length());
		*site_iter = static_cast<char*>(g_browser->memalloc(len + 1));
		if (!*site_iter)
		{
			// Emergency pull out when out of memory
			for (site_iter = sites; *site_iter; ++site_iter)
				g_browser->memfree(*site_iter);
			g_browser->memfree(sites);
			return NULL;
		}
		memcpy(*site_iter, i->first.c_str(), len);
		(*site_iter)[len] = '\0';
		++site_iter;
	}
	*site_iter = 0;
	return sites;
}


/**
 * Fill the function pointer structure supplied by the browser.
 *
 * @param Destination structure.
 */
void initialize_plugin_function_structure(NPPluginFuncs*& plugin_functions)
{
	uint16_t version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

	if (plugin_functions->version < 22)
		/* Don't go below version 22 to not break old Opera
		   versions that did not set version of structure. */
		version = 22;
	else if (version > plugin_functions->version)
		/* To be on the safe side, avoid setting
		   version higher than browser supports. */
		version = plugin_functions->version;

	plugin_functions->version = version;
	plugin_functions->newp = NPP_New;
	plugin_functions->destroy = NPP_Destroy;
	plugin_functions->setwindow = NPP_SetWindow;
	plugin_functions->newstream = NPP_NewStream;
	plugin_functions->destroystream = NPP_DestroyStream;
	plugin_functions->asfile = NPP_StreamAsFile;
	plugin_functions->writeready = NPP_WriteReady;
	plugin_functions->write = NPP_Write;
	plugin_functions->print = NPP_Print;
	plugin_functions->event = NPP_HandleEvent;
	if (version < NPVERS_HAS_NOTIFICATION) return;
	plugin_functions->urlnotify = NPP_URLNotify;
	plugin_functions->getvalue = NPP_GetValue;
	plugin_functions->setvalue = NPP_SetValue;
	if (version < NPVERS_HAS_ADVANCED_KEY_HANDLING) return;
	plugin_functions->gotfocus = NPP_GotFocus;
	plugin_functions->lostfocus = NPP_LostFocus;
	if (version < NPVERS_HAS_URL_REDIRECT_HANDLING) return;
	plugin_functions->urlredirectnotify = NPP_URLRedirectNotify;
	if (version < NPVERS_HAS_CLEAR_SITE_DATA) return;
	plugin_functions->clearsitedata = NPP_ClearSiteData;
	plugin_functions->getsiteswithdata = NPP_GetSitesWithData;
}


/**
 * Function called by browser as a response to NPN_PluginThreadAsyncCall.
 */
void async_call(void* arg)
{
	if (AsyncCallRecord* acr = static_cast<AsyncCallRecord*>(arg))
	{
		acr->instance->AsyncCall(acr->reason);
		g_browser->memfree(acr);
	}
}


/**
 * Function called by browser as a response to NPN_ScheduleTimer.
 */
void timeout(NPP instance, uint32_t id)
{
	if (PluginInstance* pi = static_cast<PluginInstance*>(instance->pdata))
		return pi->Timeout(id);
}


#ifdef XP_UNIX
/**
 * Function called by Gtk when one of our widgets receives messages.
 */
gboolean gtk_event(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	if (WindowedInstance* wi = static_cast<WindowedInstance*>(user_data))
		return wi->HandleGtkEvent(widget, event);

	return false;
}
#endif // XP_UNIX


#ifdef XP_WIN
/**
 * Function called by Windows when our window receives messages.
 */
LRESULT win_proc(HWND hwnd, UINT message, WPARAM w, LPARAM l)
{
	WindowInstanceMap::iterator it = g_window_instance_map.find(hwnd);
	if (it != g_window_instance_map.end())
		return it->second->WinProc(hwnd, message, w, l);

	return DefWindowProc(hwnd, message, w, l);
}
#endif // XP_WIN


/**
 * Handy debug log function. Works like printf.
 */
#if defined XP_WIN || defined XP_UNIX
void log(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
#ifdef XP_WIN
	char buf[4096]; /* ARRAY OK terjes 2010-11-16 */
	vsprintf_s<sizeof(buf)>(buf, fmt, ap);
	OutputDebugStringA(buf);
#else // XP_WIN
	vprintf(fmt, ap);
#endif // !XP_WIN
	va_end(ap);
}
#endif // XP_WIN || XP_UNIX
