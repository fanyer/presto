/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Shared definitions and declarations.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <string>

#ifdef XP_GOGI
# define TRUE 1
# define FALSE 0
# include "gogi_opera_api.h"
# include "gogi_plugin_api.h"
#endif

#include "platform.h"
#include "npapi.h"
#include "npfunctions.h"
#include "instance.h"
#include "windowless.h"
#include "windowed.h"
#include "variants.h"

#define MIME_TYPE "application/x-vnd.opera-test-plugin"


/**
 * Global plugin data.
 */
extern NPNetscapeFuncs* g_browser;
extern bool g_crashed;
extern std::map<std::string, char*> g_cookies;

typedef std::map<NPObject*, PluginInstance*> ObjectInstanceMap;
extern ObjectInstanceMap g_object_instance_map;

#ifdef XP_WIN
typedef std::map<HWND, WindowedInstance*> WindowInstanceMap;
extern WindowInstanceMap g_window_instance_map;
#endif // XP_WIN

/**
 * Globally accessible function(s).
 */
void initialize_plugin_function_structure(NPPluginFuncs*& plugin_functions);
NPClass* build_object_class();
void async_call(void*);
void timeout(NPP instance, uint32_t id);

#ifdef XP_UNIX
gboolean gtk_event(GtkWidget* widget, GdkEvent* event, gpointer user_data);
#endif //XP_UNIX

#ifdef XP_WIN
LRESULT win_proc(HWND hwnd, UINT message, WPARAM w, LPARAM l);
#endif // XP_WIN

#if defined XP_WIN || defined XP_UNIX
void log(const char* fmt, ...);
#endif // XP_WIN || XP_UNIX

#endif // COMMON_H
