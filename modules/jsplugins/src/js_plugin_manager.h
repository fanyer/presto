/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#ifndef JS_PLUGIN_MANAGER_H
#define JS_PLUGIN_MANAGER_H

#ifdef JS_PLUGIN_SUPPORT
#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_version.h"
#include "modules/util/simset.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/tempbuf.h"

class JS_Plugin_Context;
class OpDLL;

#ifdef JS_PLUGIN_ATVEF_VISUAL
/** Extended capability struct for visual jsplugins. */
struct jsplugin_cap_ext
{
	jsplugin_tv_become_visible *cb_tv_become_visible;	///< TV callback.
	jsplugin_tv_position *cb_tv_position;				///< TV callback.
};
#endif

/** JavaScript plugin manager's plugin information wrapper. */
class JS_Plugin_Item
	: public Link
{
public:
	~JS_Plugin_Item() { OP_DELETEA(plugin_id); }

	char *plugin_id;
	jsplugin_cap* plugin_cap;
#ifdef JS_PLUGIN_ATVEF_VISUAL
	jsplugin_cap_ext plugin_cap_ext;
#endif
};

class JS_Plugin_SecurityDomain
{
public:
	JS_Plugin_SecurityDomain()
		: protocol(URL_UNKNOWN), domain(0), port(0), use_callback(FALSE)
	{
	}

	~JS_Plugin_SecurityDomain()
	{
		OP_DELETEA(domain);
	}

	OP_STATUS
	Construct(const char *protocol, const char *domain, int port);
	
	void
	ConstructCallback() { use_callback = TRUE; }

	URLType
	GetProtocol() { return protocol; }
	
	const char *
	GetDomain() { return domain; }

	int
	GetPort() { return port; }
	
	BOOL
	UseCallback() { return use_callback; }

protected:
	URLType protocol;
	char *domain;
	int port;
	BOOL use_callback;
};

class JS_OpDLL_Item 
	: public Link
{
public:
	~JS_OpDLL_Item();
	OpDLL *dll;
};

// Need to declare internal function here due to linkage specifier.
extern "C" int cb_add_tv_visual(jsplugin_tv_become_visible *become_visible,
								jsplugin_tv_position *position);

/** Global JavaScript plug-in manager. */
class JS_Plugin_Manager
{
private:
	Head plugin_list;
	Head plugin_allowed_from_list;
	Head dlls;
	TempBuffer tempbuf;
	ES_ValueString tempstring;
	jsplugin_callbacks callbacks;

	BOOL
	IsAllowedFrom(JS_Plugin_Item *item, ES_Runtime *runtime);

#ifdef JS_PLUGIN_ATVEF_VISUAL
	friend int
	cb_add_tv_visual(jsplugin_tv_become_visible *become_visible, jsplugin_tv_position *position);

	/** Set TV visualization callbacks from plug-in. */
	int
	SetTvCallbacks(jsplugin_tv_become_visible *become_visible,
				   jsplugin_tv_position *position);

	jsplugin_tv_become_visible *tmp_become_visible;	///< Temporary used in callback.
	jsplugin_tv_position *tmp_position;				///< Temporary used in callback.
#endif

	void
	ScanAndLoadPluginsL();
	
	void
	ReadPermissionsFileL(class OpFile* permissionsFile);

public:
	JS_Plugin_Manager() {}
	~JS_Plugin_Manager();
	void Setup();			///< Init; scan for plugins

	/** Create a jsplugin context. There is one context per DOM environment. */
	JS_Plugin_Context *
	CreateContext(ES_Runtime *runtime, EcmaScript_Object *global_object);
	
	/** Destroy a jsplugin context. */
	void
	DestroyContext(JS_Plugin_Context *context);

	OP_STATUS
	AllowPluginFrom(const char *plugin_id, JS_Plugin_SecurityDomain *security_domain);

	OP_STATUS
	PluginEval(JS_Plugin_Context *context, const char *code, void *user_data, jsplugin_async_callback *callback);

	OP_STATUS
	CallFunction(JS_Plugin_Context *context, ES_Object *fo, ES_Object *to, int argc, ES_Value *argv, void *user_data, jsplugin_async_callback *callback);

	OP_STATUS
	GetSlot(JS_Plugin_Context *context, ES_Object *obj, const char *name, void *user_data, jsplugin_async_callback *callback);
	
	OP_STATUS
	SetSlot(JS_Plugin_Context *context, ES_Object *obj, const char *name, ES_Value *value, void *user_data, jsplugin_async_callback *callback);

	TempBuffer *
	GetTempBuffer() { return &tempbuf; }

	ES_ValueString *
	GetTempValueString() { return &tempstring; }

	OP_STATUS
	AddStaticPlugin(const char *name, jsplugin_capabilities_fn jsplugin_capabilities);
	
	void
	RemoveStaticPlugin(const char *name);
};

#endif // JS_PLUGIN_SUPPORT
#endif // JS_PLUGIN_MANAGER_H
