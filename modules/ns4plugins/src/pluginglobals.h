/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGINGLOBALS_H_INC_
#define _PLUGINGLOBALS_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plugincommon.h"

/* I have found no single platform that didn't define this to nothing. It is
 * *possible* that some platforms have been referring to a platform-specific
 * keyword though. 16-bit Visual C++ for example...
 *
 * "The __export keyword provided with the Visual C++ for Windows compiler is
 * obsolete with the Microsoft Visual C++ 32-bit compiler." */
/* TODO Just remove all occurences of __export... */
#ifndef __export
#define __export
#endif

NPError	__export NPN_GetURL(NPP instance, const char* url, const char* window);
NPError __export NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file);
NPError	__export NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
NPError	__export NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
int32_t __export NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer);
NPError __export NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
void __export NPN_Status(NPP instance, const char* message);
const char*	__export NPN_UserAgent(NPP instance);
void* __export NPN_MemAlloc(uint32_t size);
void __export NPN_MemFree(void* ptr);
uint32_t __export NPN_MemFlush(uint32_t size);
void __export NPN_ReloadPlugins(NPBool reloadPages);
void* __export NPN_GetJavaEnv(void);
void* __export NPN_GetJavaPeer(NPP instance);
NPError	__export NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData);
NPError __export NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData);
NPError __export NPN_GetValue(NPP instance, NPNVariable variable, void *value);
NPError __export NPN_SetValue(NPP instance, NPPVariable variable, void *value);
void __export NPN_InvalidateRect(NPP instance, NPRect *invalidRect);
void __export NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion);
void __export NPN_ForceRedraw(NPP instance);
void __export NPN_PushPopupsEnabledState(NPP instance, NPBool enabled);
void __export NPN_PopPopupsEnabledState(NPP instance);
NPError __export NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len);
NPError __export NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len);
NPError __export NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen);
uint32_t __export NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID));
void __export NPN_UnscheduleTimer(NPP instance, uint32_t timerID);
NPError __export NPN_PopUpContextMenu(NPP instance, NPMenu* menu);
NPBool __export NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);
NPBool __export NPN_HandleEvent(NPP instance, void *event, NPBool handled);
NPBool __export NPN_UnfocusInstance(NPP instance, NPFocusDirection direction);

// Opera currently doesn't have this function implemented. For discussion on
// the matter, please see: http://critic.oslo.osa/showcomment?chain=3639
//void __export NPN_URLRedirectResponse(NPP instance, void* notifyData, NPBool allow);

#define NP_VERSION_MAJOR_11 0
#define NP_VERSION_MINOR_11 11
#define NP_VERSION_MAJOR_14 0
#define NP_VERSION_MINOR_14 14
#define NP_VERSION_MINOR_16 16

#define PLUGIN_URL      0
#define PLUGIN_POST     1
#define PLUGIN_NOTIFY   2

#ifdef HAS_COMPLEX_GLOBALS
extern const NPNetscapeFuncs g_operafuncs;
#endif // HAS_COMPLEX_GLOBALS

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINGLOBALS_H_INC_
