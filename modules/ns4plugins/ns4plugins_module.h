/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_NS4PLUGINS_NS4PLUGINS_MODULE_H
#define MODULES_NS4PLUGINS_NS4PLUGINS_MODULE_H

#ifdef _PLUGIN_SUPPORT_

#include "modules/hardcore/opera/module.h"
#ifndef HAS_COMPLEX_GLOBALS
typedef struct _NPNetscapeFuncs NPNetscapeFuncs;
#endif // !HAS_COMPLEX_GLOBALS

class Plugin;
class PluginHandler;
class PluginMemoryHandler;
class PluginScriptData;

#ifdef USE_PLUGIN_EVENT_API
class HTML_Element;
#endif // USE_PLUGIN_EVENT_API

/** Class to do ns4plugins module specific startup and shutdown procedures.
	Implements the OperaModule interface. */

class Ns4pluginsModule : public OperaModule
{
public:
	Ns4pluginsModule();

	/** OperaModule interface method implementation. */
	void InitL(const OperaInitInfo& info);

	/** OperaModule interface method implementation. */
	void Destroy();

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	/** OperaModule interface method implementation.
		Every time Opera starts, ClearTempStreamFiles() will
		delete the cached stream files folder. */
	void ClearTempStreamFiles();
#endif // NS4P_STREAM_FILE_WITH_EXTENSION_NAME

	/** OperaModule interface method implementation.
		FreeCachedData() is a predefined method for the OOM handling mechanism.
		Ns4pluginsModule version was implemented as a fix for CORE-34621.
		FreeCachedData() destroys all loading streams associated with plugins on OOM.
		Be aware that if the OOM happens on a page where the primary content is
		plugin based, it will leave the page completely useless. */
	BOOL FreeCachedData(BOOL toplevel_context);

	/** The global PluginHandler object pointing to the interface for OpNS4PluginHandler.
		Creation and deletion of the PluginHandler instance is performed when initializing 
		and deleting the ns4plugins module. */
	PluginHandler* plugin_handler;

	/** The global PluginMemoryHandler object pointing to the interface for PluginMemoryHandler.
	    Creation of the object is performed first time fetching the handler.
		Deletion of the object is performed when the PluginHandler instance is deleted. */
	PluginMemoryHandler* plugin_memory_handler;

	/** The global PluginScriptData object pointing to the interface for PluginScriptData
	    which provides access to OpNPIdentifiers used in the scripting interface,
		as well as OpNPObjects used internally (ES_Object) and externally (NPObject).
	    Creation of the object is handled when intializing the ns4plugins module.
		Deletion of the object is performed when the PluginHandler instance is deleted. */
	PluginScriptData* plugin_script_data;

	/** The global string variable used when identifying the browser through the NPAPI.
		The value varies, depending on the windowless mode. */
	char agent[256]; /* ARRAY OK 2011-03-02 bratell */

	/** The global INT variable is incremented when the plugin has initiated a synchronous script evaluation.
		The variable is used during general Opera window message processing, to delay Flash window messages,
		as well as during MSG_PLUGIN_CALL processing, to delay calling plugin NPAPI functions.
		The variable is 0 when no syncronous script evaluation is ongoing. */
	INT plugin_in_synchronous_loop;

	/** The global struct pointer used when Opera is initializing an NPObject.
		Creation and deletion of the struct is performed when initializing 
		and deleting the ns4plugins module. */
	struct NPClass* plugin_internal_object_class;

#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
	/** This global BOOL variable keeps track of when we're executing plugin
	    code and when we are not.  We use this when deciding whether to
	    suppress runtime errors (crashers) or not. Whenever we call into a
	    plugin we set this to TRUE, whenever the call return we set if to
	    FALSE. Whenever a plugin calls Opera code we set it to FALSE and
	    when we return to the plugin code we set it to TRUE. */
	BOOL is_executing_plugin_code;
#endif // NS4P_TRY_CATCH_PLUGIN_CALL

	/** The global BOOL variable used when a mousemove event is sent to the windowless plugin.
		The variable is used in the windows platform code and by default set to FALSE.
		The variable is set to TRUE in Plugin::HandleEvent when the mouse event is ONMOUSEMOVE, 
		and a "fake" WM_SETCURSOR message is sent to the plugin.
		When the variable is TRUE, the next real WM_SETCURSOR message is not sent to the view listener.*/
	BOOL mouse_is_over_plugin;

	/** The global Plugin object pointing to the windowless plugin with focus.
		The variable is used in the windows platform code when key events are handled.
		When the windowless plugin has focus, the variable is TRUE and the key event is sent to the plugin.
		When the windowless plugin is out of focus, the variable is FALSE and the key event is handled by Opera. */
	Plugin* plugin_that_should_receive_key_events;

#ifndef HAS_COMPLEX_GLOBALS
	NPNetscapeFuncs* m_operafuncs;
#endif // !HAS_COMPLEX_GLOBALS
};

#define g_pluginhandler g_opera->ns4plugins_module.plugin_handler
#define g_pluginmemoryhandler g_opera->ns4plugins_module.plugin_memory_handler
#define g_pluginscriptdata g_opera->ns4plugins_module.plugin_script_data
#define g_agent g_opera->ns4plugins_module.agent
#define g_in_synchronous_loop g_opera->ns4plugins_module.plugin_in_synchronous_loop
#define g_plugin_internal_object_class g_opera->ns4plugins_module.plugin_internal_object_class
#define g_mouse_is_over_plugin g_opera->ns4plugins_module.mouse_is_over_plugin
#define g_plugin_that_should_receive_key_events g_opera->ns4plugins_module.plugin_that_should_receive_key_events

#ifndef HAS_COMPLEX_GLOBALS
# define g_operafuncs *(g_opera->ns4plugins_module.m_operafuncs)
#endif // !HAS_COMPLEX_GLOBALS

#define NS4PLUGINS_MODULE_REQUIRED

#endif // _PLUGIN_SUPPORT_

#endif // !MODULES_NS4PLUGINS_NS4PLUGINS_MODULE_H
