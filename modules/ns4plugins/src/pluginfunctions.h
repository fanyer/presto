/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_SRC_PLUGIN_FUNCTIONS_H
#define NS4P_SRC_PLUGIN_FUNCTIONS_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/src/generated/g_proto_ns4plugins_messages.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/ns4plugins/src/plug-inc/npruntime.h"
#include "modules/ns4plugins/src/plug-inc/npfunctions.h"
#include "modules/inputmanager/inputmanager_constants.h"

class PluginInstance;

/**
 * Plug-in entry points when NS4P_COMPONENT_PLUGINS is enabled.
 */
class PluginFunctions
{
public:
	static NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
	static NPError NPP_Destroy(NPP instance, NPSavedData** save);
	static NPError NPP_SetWindow(NPP instance, NPWindow* window);
	static NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
	static NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
	static int32   NPP_WriteReady(NPP instance, NPStream* stream);
	static int32   NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
	static void    NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
	static void    NPP_Print(NPP instance, NPPrint* platformPrint);
	static int16   NPP_HandleEvent(NPP instance, void* event);
	static void    NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
	static NPError NPP_GetValue(NPP instance, NPPVariable variable, void *ret_alue);
	static NPError NPP_SetValue(NPP instance, NPNVariable variable, void *ret_alue);
	static NPBool  NPP_GotFocus(NPP instance, NPFocusDirection direction);
	static void    NPP_LostFocus(NPP instance);
	static void    NPP_URLRedirectNotify(NPP instance, const char* url, int32_t status, void* notifyData);
	static NPError NPP_ClearSiteData(const char* site, uint64_t flags, uint64_t maxAge);
	static char**  NPP_GetSitesWithData();

	static NPObject* NPO_Allocate(NPP npp, NPClass* aClass);
	static void      NPO_Deallocate(NPObject* npobj);
	static void      NPO_Invalidate(NPObject* npobj);
	static bool      NPO_HasMethod(NPObject* npobj, NPIdentifier name);
	static bool      NPO_Invoke(NPObject* npobj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
	static bool      NPO_InvokeDefault(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);
	static bool      NPO_HasProperty(NPObject* npobj, NPIdentifier name);
	static bool      NPO_GetProperty(NPObject* npobj, NPIdentifier name, NPVariant* result);
	static bool      NPO_SetProperty(NPObject* npobj, NPIdentifier name, const NPVariant* value);
	static bool      NPO_RemoveProperty(NPObject* npobj, NPIdentifier name);
	static bool      NPO_Enumerate(NPObject* npobj, NPIdentifier** identifier, uint32_t* count);
	static bool      NPO_Construct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result);

	typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;
	static void SetPluginIdentifier(PluginIdentifier& out_identifier, NPIdentifier identifier);

	static void InitializeFunctionTable(NPPluginFuncs* plugin_functions);
	static void InitializeClass(NPClass* npclass);

	static NPError SetWindow(NPP instance, NPWindow* window, OpChannel* adapter_channel);
	static OP_STATUS HandleFocusEvent(NPP npp, bool focus, FOCUS_REASON reason, bool* is_event_handled);

	/**
	 * Trigger a windowless paint.
	 *
	 * Sends a message to the plugin component which instructs it to have
	 * the windowless plugin paint itself to whatever target that was supplied
	 * via NPP_SetWindow(inst,NPWindow) at some earlier stage.
	 */
	static OP_STATUS HandleWindowlessPaint(NPP npp, OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect, bool transparent, OpWindow* parent_opwindow);
	static OP_STATUS HandleWindowlessWindowPosChanged(NPP npp, bool* is_event_handled);
	static OP_STATUS HandleWindowlessMouseEvent(NPP npp, OpPluginEventType event_type, const OpPoint& point, int button_or_key_or_delta, ShiftKeyState modifiers, bool* is_event_handled);
	static OP_STATUS HandleWindowlessKeyEvent(NPP npp, const OpKey::Code key, const uni_char *key_value, const OpPluginKeyState key_state, const ShiftKeyState modifiers, UINT64 platform_data_1, UINT64 platform_data_2, INT32* is_event_handled);

	static PluginInstance* GetPluginInstance(NPP instance);
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_SRC_PLUGIN_FUNCTIONS_H
