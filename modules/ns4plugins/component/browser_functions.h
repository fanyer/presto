/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_PLUGIN_COMPONENT_BROWSER_FUNCTIONS_H
#define NS4P_PLUGIN_COMPONENT_BROWSER_FUNCTIONS_H

#include "modules/ns4plugins/component/plugin_component_instance.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"

/**
 * NPAPI browser functions made available to the plug-in library by the plug-in component.
 *
 * These functions are called by the plug-in, and either access a local cache and
 * return directly, or send a message to the browser's document component, nest the
 * message loop while waiting, and return once an authorative answer has been received.
 */
class BrowserFunctions
{
public:
	static NPError      NPN_GetValue(NPP instance, NPNVariable variable, void *ret_value);
	static NPError      NPN_SetValue(NPP instance, NPPVariable variable, void *value);
	static NPError      NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData);
	static NPError      NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData);
	static NPError      NPN_GetURL(NPP instance, const char* url, const char* window);
	static NPError      NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file);
	static NPError      NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
	static NPError      NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
	static int32_t      NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer);
	static NPError      NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
	static void         NPN_Status(NPP instance, const char* message);
	static const char*  NPN_UserAgent(NPP instance);
	static void*        NPN_MemAlloc(uint32_t size);
	static void         NPN_MemFree(void* ptr);
	static uint32_t     NPN_MemFlush(uint32_t size);
	static void         NPN_ReloadPlugins(NPBool reloadPages);
	static void*        NPN_GetJavaEnv();
	static void*        NPN_GetJavaPeer(NPP instance);
	static void         NPN_InvalidateRect(NPP instance, NPRect *rect);
	static void         NPN_InvalidateRegion(NPP instance, NPRegion region);
	static void         NPN_ForceRedraw(NPP instance);
	static NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name);
	static void         NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers);
	static NPIdentifier NPN_GetIntIdentifier(int32_t intid);
	static bool         NPN_IdentifierIsString(NPIdentifier identifier);
	static NPUTF8*      NPN_UTF8FromIdentifier(NPIdentifier identifier);
	static int32_t      NPN_IntFromIdentifier(NPIdentifier identifier);
	static NPObject*    NPN_CreateObject(NPP npp, NPClass *aClass);
	static NPObject*    NPN_RetainObject(NPObject *obj);
	static void         NPN_ReleaseObject(NPObject *obj);
	static bool         NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result);
	static bool         NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
	static bool         NPN_Evaluate(NPP npp, NPObject *obj, NPString *script, NPVariant *result);
	static bool         NPN_GetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, NPVariant *result);
	static bool         NPN_SetProperty(NPP npp, NPObject *obj, NPIdentifier propertyName, const NPVariant *value);
	static bool         NPN_RemoveProperty(NPP npp, NPObject *obj, NPIdentifier propertyName);
	static bool         NPN_HasProperty(NPP npp, NPObject *obj, NPIdentifier propertyName);
	static bool         NPN_HasMethod(NPP npp, NPObject *obj, NPIdentifier propertyName);
	static void         NPN_ReleaseVariantValue(NPVariant *variant);
	static void         NPN_SetException(NPObject *obj, const NPUTF8 *message);
	static void         NPN_PushPopupsEnabledState(NPP npp, NPBool b);
	static void         NPN_PopPopupsEnabledState(NPP npp);
	static bool         NPN_Enumerate(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count);
	static void         NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void *), void *userData);
	static bool         NPN_Construct(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
	static NPError      NPN_GetValueForURL(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len);
	static NPError      NPN_SetValueForURL(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len);
	static NPError      NPN_GetAuthenticationInfo(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen);
	static uint32_t     NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
	static void         NPN_UnscheduleTimer(NPP instance, uint32_t timerID);
	static NPError      NPN_PopUpContextMenu(NPP instance, NPMenu* menu);
	static NPBool       NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);
	static NPBool       NPN_HandleEvent(NPP instance, void* event, NPBool handled);
	static NPBool       NPN_UnfocusInstance(NPP instance, NPFocusDirection direction);
	static void         NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow);

	/**
	 * Look up PluginComponentInstance from NPP.
	 *
	 * @param npp Plug-in instance identifier.
	 * @param allow_other_thread If false, this call will return NULL if it is not made on the right plug-in component thread.
	 *
	 * @return Pointer to PluginComponentInstance or NULL if none found (or thread verification failed.)
	 */
	static PluginComponentInstance* GetPluginComponentInstance(NPP instance, bool allow_other_thread = false);
	static void InitializeFunctionTable(NPNetscapeFuncs* browser_functions);

private:
	template<class T>
	static bool HandleInvoke(CountedPluginComponentInstance &pci,
	                         OpAutoPtr<T> &message,
	                         const NPVariant* args,
	                         uint32_t argCount,
	                         NPVariant* result);
};

#endif // NS4P_PLUGIN_COMPONENT_BROWSER_FUNCTIONS_H
