/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPNS4PLUGINHANDLER_H
#define OPNS4PLUGINHANDLER_H

#ifdef _PLUGIN_SUPPORT_

class Plugin;
class FramesDocument;
class OpNS4Plugin;
#ifndef USE_PLUGIN_EVENT_API
class EmbedContent;
#endif // !USE_PLUGIN_EVENT_API
class PluginHandler;
class HTML_Element;
class ES_Value;
class ES_Runtime;
class ES_Object;
class OpPluginWindow;
class OpDLL;

#ifdef _PRINT_SUPPORT_
class VisualDevice;
#endif // _PRINT_SUPPORT_

#ifndef NP_EMBED
#define NP_EMBED 1
#endif

#ifndef NP_FULL
#define NP_FULL 2
#endif

/** The browser's interface to the PluginHandler. */

/** Class for handling MSG_PLUGIN_CALL messages posted to the g_main_message_handler */
class OpNS4PluginHandler : public MessageObject
{
public:

	/** Init() needs to be called as part of InitializeL(). Creates the OpNS4PluginHandler singleton. Will return NULL if creation failed (OOM). */
	static OP_STATUS Init();

	/** Destroy() needs to be called as part of Terminate(). Deletes the OpNS4PluginHandler singleton and the pluginmemoryhandler, if any. */
	static void Destroy();

	/** GetHandler() retrieves a pointer to the OpNS4PluginHandler singleton. */
	static OpNS4PluginHandler* GetHandler() { return (OpNS4PluginHandler *)g_pluginhandler; }

	/** Implementation of the MessageObject interface. This method is called to handle all MSG_PLUGIN_CALL messages. */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam) = 0;

#ifdef USE_PLUGIN_EVENT_API
	/** New() initiates a plugin and returns a pointer to the plugin */
	virtual OpNS4Plugin* New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, OpStringC &mimetype, uint16 mode, HTML_Element* htm_elm = NULL, URL* url = NULL, BOOL embed_url_changed = FALSE) = 0;
#else
	virtual OpNS4Plugin* New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, void* plugin_window, OpStringC &mimetype, uint16 mode, int16 argc, const uni_char *argn[], const uni_char *argv[], HTML_Element* htm_elm = NULL, URL* url = NULL, BOOL embed_url_changed = FALSE) = 0;
#endif // USE_PLUGIN_EVENT_API


	/** Destroy() disables the plugin */
	virtual void Destroy(OpNS4Plugin* plugin) = 0;

	/** Returns the pointer to the plugin object associated with OpPluginWindow. If
	    return_deleted is set to TRUE, a deleted plugin can also be returned. */
	virtual OpNS4Plugin* FindOpNS4Plugin(OpPluginWindow* oppw, BOOL return_deleted = FALSE) = 0;

#ifdef _PRINT_SUPPORT_
	/** Print() calls the plugin's NP_EMBED print functionality */
	virtual void Print(const VisualDevice* vd, const uni_char* src_url, const int x, const int y, const unsigned int width, const unsigned int height) = 0;
#endif // _PRINT_SUPPORT_

	/** Since the plugin to be scripted has not been created yet, the script is suspended until the scriptable object is available.
		The return value is the plugin restart object. */
	virtual OP_STATUS Suspend(FramesDocument* doc, HTML_Element *element, ES_Runtime* runtime, ES_Value* value, BOOL wait_for_full_plugin_init) = 0;

	/** If the script has been suspended because the scriptable object was not created,
		this method should be used to resume the script execution.
		The input parameter plugin_failed indicates that the plugin object was never created
		due to loading error and as such cannot be scriptable.
		The input parameter include_threads_waiting_for_init controls if threads waiting for
		the plugin to init are resumed. This should only be TRUE if the plugin is fully inited.
		Suspend() returns with error for every later attempt to get this scriptable object. */
	virtual void Resume(FramesDocument* frames_doc, HTML_Element* element, BOOL plugin_failed, BOOL include_threads_waiting_for_init) = 0;

	/** Returns TRUE if the plugin script execution has been suspended. */
	virtual BOOL IsSuspended(FramesDocument* frames_doc, HTML_Element* element) = 0;

	/** Returns TRUE if the input parameter is a plugin restart object of type PLUGIN_RESTART*. Will
	    return FALSE for other types of plugin restart objects such as PLUGIN_PUT or objects that
	    are not known to the plugin module at all. */
	virtual BOOL IsPluginStartupRestartObject(ES_Object* restart_object) = 0;

	/** Returns TRUE if the input parameter is a plugin restart object of a known type. */
	virtual BOOL IsSupportedRestartObject(ES_Object* restart_object) = 0;
};

#endif // _PLUGIN_SUPPORT_
#endif // !OPNS4PLUGINHANDLER_H
