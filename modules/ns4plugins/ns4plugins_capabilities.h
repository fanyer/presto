/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef NS4PLUGINS_CAPABILITIES_H
#define NS4PLUGINS_CAPABILITIES_H

// Define capabilities supported in current ns4plugins module

// URLLink class moved to URL module (core-2) // Actual move triggered by URL_CAP_URLLINK_CLASS
#define NS4P_CAP_URLLINK_CLASS_MOVED

// Support for printing plugins
#define NS4P_CAP_PRINT_SUPPORT

// OpNS4Plugin API has GetScriptableObject method.
#define NS4P_CAP_GET_SCRIPTABLE_OBJECT

// OpNS4Plugin API has GetFormsvalue method
#define NS4P_CAP_GET_FORMS_VALUE

// PluginLibHandler::OnTimeOut replaces the API call OpNS4PluginHandler::ReleaseUnusedResources().
#define NS4P_CAP_ONTIMEOUT

// OpNS4Plugin API has SetPopupsEnabled method.
#define NS4P_CAP_SET_POPUPS_ENABLED

// Moved functions PluginStaticWinProc() and PluginRegClassDef() into platform code
#define NS4P_CAP_MOVED_PLUGIN_FUNCTIONS_TO_PLATFORM

// OpNS4Plugin API has SetWindowPos method.
#define NS4P_CAP_SET_WINDOW_POS

// Ns4pluginsModule defines g_in_synchronous_loop, g_mouse_is_over_plugin, g_plugin_that_should_receive_key_events

#define NS4P_CAP_MOVED_GLOBAL_VARIABLES

// OpNS4PluginHandler API has PluginHandler::New() with parameter url
#define NS4P_CAP_NEW_URL

// OpNS4PluginHandler API PluginHandler::Destroy() makes sure that the plugin window is destroyed
#define NS4P_CAP_DESTROY_PLUGIN_WINDOW

/** The ns4plugins module can satisfy the requirements of the plug-in event API.
	May be used to toggle import of API_PI_USE_PLUGIN_EVENT_API. */
#define NS4P_CAP_PLUGIN_EVENT_API

/** Supports ES_CAP_PROPERTY_TRANSLATION and the new EcmaScript_Object::{Get,Put}{Index,Name}() signatures. */
#define NS4P_CAP_NEW_GETNAME

/** OpNS4PluginHandler API has the methods Suspend(), Resume(), IsSuspended() and IsPluginRestartObject(). */
#define NS4P_CAP_PLUGINHANDLER_RESTART

// Prepared for WindowCommander::GetSecurityModeFromState() becoming non-static.
#define NS4P_CAP_USE_NONSTATIC_WIC_GETSECMODE

// Prepared for OpNS4Plugin::GetScriptableObject() always called, both before and after the call on NPP_GetValue().
#define NS4P_CAP_GETSCRIPTABLEOBJECT_ALWAYS_CALLED

// Deprecated the OpNS4PluginHandler::SetWmode() and DestroyWmode(), will soon be removed
#define NS4P_CAP_WMODE_DEPRECATED

// Ready to send html element to GetNewPluginWindow
#define NS4P_CAP_SEND_HTM_ELM_TO_GETNEWPLUGINWINDOW

// OpNS4PluginHandler contains the method FindPlugin() based on OpPluginWindow
#define NS4P_CAP_HAS_FINDPLUGIN

// OpNS4Plugin has GetInstace()
#define NS4P_CAP_HAS_GETINSTANCE

// end of capability defines.
#endif // !NS4PLUGINS_CAPABILITIES_H
