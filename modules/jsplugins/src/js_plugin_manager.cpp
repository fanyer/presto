/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef JS_PLUGIN_SUPPORT
#include "modules/jsplugins/jsplugins_module.h"
#include "modules/jsplugins/src/js_plugin_manager.h"
#include "modules/jsplugins/src/js_plugin_context.h"
#include "modules/jsplugins/src/js_plugin_object.h"
#include "modules/jsplugins/jsplugin.h"
#include "modules/pi/OpDLL.h"
#include "modules/util/opfile/opfile.h"
#include "modules/doc/frm_doc.h"
#include "modules/url/url_man.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domobj.h"

extern "C" {

int cb_create_function(const jsplugin_obj *refobj,
					   jsplugin_getter *g,
					   jsplugin_setter *s,
					   jsplugin_function *f_call,
					   jsplugin_function *f_construct,
					   const char *f_signature,
					   jsplugin_destructor *d,
					   jsplugin_gc_trace *t,
					   jsplugin_obj **result);

int cb_create_object(const jsplugin_obj *refobj,
					 jsplugin_getter *g,
					 jsplugin_setter *s,
					 jsplugin_destructor *d,
					 jsplugin_gc_trace *t,
					 jsplugin_obj **result);

int cb_eval(const jsplugin_obj *refobj,
			const char *code,
			void *user_data,
			jsplugin_async_callback *callback);

int cb_set_poll_interval(const jsplugin_obj *global_ctx,
						 unsigned interval,
						 jsplugin_poll_callback *callback);

int cb_call_function(const jsplugin_obj *global_ctx,
					 const jsplugin_obj *this_obj,
					 const jsplugin_obj *function_obj,
					 int argc,
					 jsplugin_value *argv,
					 void *user_data,
					 jsplugin_async_callback *callback);

int cb_getter(const jsplugin_obj *global_context,
			  const jsplugin_obj *obj,
			  const char *name,
			  void *user_data,
			  jsplugin_async_callback *callback);

int cb_setter(const jsplugin_obj *global_context,
			  const jsplugin_obj *obj,
			  const char *name,
			  jsplugin_value *value,
			  void *user_data,
			  jsplugin_async_callback *callback);

int cb_add_unload_listener(const jsplugin_obj *target,
						   jsplugin_notify *listener);

int cb_set_attr_change_listener(jsplugin_obj *target, jsplugin_attr_change_listener *listener);

int cb_set_param_set_listener(jsplugin_obj *target, jsplugin_param_set_listener *listener);

int cb_add_tv_visual(jsplugin_tv_become_visible *become_visible,
					 jsplugin_tv_position *position)
{
#ifdef JS_PLUGIN_ATVEF_VISUAL
	// Must define both or neither
	if (!become_visible == !position)
	{
		g_jsPluginManager->SetTvCallbacks(become_visible, position);
		return 0;
	}
#endif

	// Not supported here
	return -1;
}

int cb_get_window_identifier(const jsplugin_obj *object, long int *identifier);

int cb_resume(const jsplugin_obj *reference);

int cb_get_object_host(const jsplugin_obj *obj, char **host);

void cb_gcmark(const jsplugin_obj *obj);

} // extern "C"

#ifdef JS_PLUGIN_ATVEF_VISUAL
int
JS_Plugin_Manager::SetTvCallbacks(jsplugin_tv_become_visible *become_visible,
									  jsplugin_tv_position *position)
{
	// This must be called during setup.
	// We set the temporary pointers to point to dummy addresses
	// afterwards to catch bad calls.
	if (tmp_become_visible == NULL)
	{
		tmp_become_visible = become_visible;
		tmp_position = position;
		return 0;
	}

	// Out of sequence call
	return -1;
}
#endif

OP_STATUS
JS_Plugin_SecurityDomain::Construct(const char *new_protocol, const char *new_domain, int new_port)
{
	protocol = urlManager->MapUrlType(new_protocol);
	if (protocol == URL_UNKNOWN)
		return OpStatus::ERR;

	port = new_port;
	if (port < 0 || port > 65535)
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetStr(domain, new_domain));

	return OpStatus::OK;
}

JS_Plugin_Manager::~JS_Plugin_Manager()
{
	plugin_list.Clear();
	plugin_allowed_from_list.Clear();
	dlls.Clear();
	tempbuf.Clear();
}

void
JS_Plugin_Manager::Setup()
{
	// Initialize callbacks structure
	callbacks.create_function = cb_create_function;
	callbacks.create_object = cb_create_object;
	callbacks.eval = cb_eval;
	callbacks.set_poll_interval = cb_set_poll_interval;
	callbacks.call_function = cb_call_function;
	callbacks.get_property = cb_getter;
	callbacks.set_property = cb_setter;
	callbacks.add_unload_listener = cb_add_unload_listener;
	callbacks.set_attr_change_listener = cb_set_attr_change_listener;
	callbacks.set_param_set_listener = cb_set_param_set_listener;
	callbacks.add_tv_visual = cb_add_tv_visual;
	callbacks.get_window_identifier = cb_get_window_identifier;
	callbacks.resume = cb_resume;
	callbacks.get_object_host = cb_get_object_host;
	callbacks.gcmark = cb_gcmark;

	TRAPD(err, ScanAndLoadPluginsL());
	if (OpStatus::IsMemoryError(err))
	{
		g_memory_manager->RaiseCondition(err);
	}
}

class JS_Plugin_AllowedFromElm
	: public Link
{
public:
	virtual
	~JS_Plugin_AllowedFromElm() { OP_DELETEA(plugin_id); OP_DELETE(security_domain); }

	char *plugin_id;
	JS_Plugin_SecurityDomain *security_domain;
};

class JS_Plugin_SoItem
	: public Link
{
public:
	const char* plugin_id;
};

JS_OpDLL_Item::~JS_OpDLL_Item()
{
	dll->Unload();
	OP_DELETE(dll);
}

/** Scan the directory for the plugins and load the ones that are found by
  * loading the DLL and getting the capability record.  The records are stored
  * in an array.
  */

void
JS_Plugin_Manager::ScanAndLoadPluginsL()
{
	OpFile file; ANCHOR(OpFile, file);
	LEAVE_IF_ERROR(file.Construct(UNI_L("jsplugins.ini"), OPFILE_JSPLUGIN_FOLDER));
	LEAVE_IF_ERROR(file.Open(OPFILE_READ | OPFILE_TEXT));

	ReadPermissionsFileL(&file);
	file.Close();

#ifdef DYNAMIC_JSPLUGINS
	// Only dynamic plugins scanned.
	// Create a unique list of plugin names to load:
	Head so_list;

	for (JS_Plugin_AllowedFromElm* elm = static_cast<JS_Plugin_AllowedFromElm *>(plugin_allowed_from_list.First()) ; elm ; elm = static_cast<JS_Plugin_AllowedFromElm *>(elm->Suc()))
	{
		BOOL plugin_seen = FALSE;
		for (JS_Plugin_SoItem* so_elm = static_cast<JS_Plugin_SoItem *>(so_list.First()) ; so_elm ;
			 so_elm = static_cast<JS_Plugin_SoItem *>(so_elm->Suc()))
		{
			if (op_strcmp(so_elm->plugin_id, elm->plugin_id) == 0)
			{
				// the plugin has already been seen
				plugin_seen = TRUE;
				break;
			}
		}
		if (plugin_seen == FALSE)
		{
			JS_Plugin_SoItem* item = OP_NEW(JS_Plugin_SoItem, ());
			if (item == NULL)
			{
				so_list.Clear();
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}
			item->plugin_id = elm->plugin_id;
			item->Into(&so_list);
		}
	}

	// iterate over all plugins
	JS_Plugin_SoItem* volatile so_elm = NULL;
	for (so_elm = static_cast<JS_Plugin_SoItem *>(so_list.First()) ; so_elm ; so_elm = static_cast<JS_Plugin_SoItem *>(so_elm->Suc()))
	{
		const uni_char* volatile uni_plugin_id = make_doublebyte_in_tempbuffer(so_elm->plugin_id, op_strlen(so_elm->plugin_id));
		OpFile dll_file;

		OP_STATUS err = dll_file.Construct(uni_plugin_id, OPFILE_JSPLUGIN_FOLDER);
		if (OpStatus::IsError(err))
		{
			so_list.Clear();
			LEAVE(err);
		}

		// no leave from here
		OpDLL* dll;
		OP_STATUS ret_val = OpDLL::Create(&dll);
		if (OpStatus::IsSuccess(ret_val))
		{
			OP_STATUS ret_val = dll->Load(dll_file.GetFullPath());
			if (OpStatus::IsSuccess(ret_val))
			{
#ifdef DLL_NAME_LOOKUP_SUPPORT
				void* sym_addr = dll->GetSymbolAddress("jsplugin_capabilities");
#else
				void* sym_addr = dll->GetSymbolAddress(1);
#endif // DLL_NAME_LOOKUP_SUPPORT
				if (sym_addr != NULL)
				{
					JS_OpDLL_Item* dll_item = OP_NEW(JS_OpDLL_Item, ());
					if (!dll_item)
					{
						dll->Unload();
						OP_DELETE(dll);
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						break;
					}
					dll_item->dll = dll;
					dll_item->Into(&dlls);

#ifdef JS_PLUGIN_ATVEF_VISUAL
					// Prepare for callbacks during setup
					tmp_become_visible = NULL;
					tmp_position = NULL;
#endif

					// we recognize this one
					jsplugin_cap *capptr = NULL;
					const jsplugin_capabilities_fn plugin_capabilities = reinterpret_cast<const jsplugin_capabilities_fn>(sym_addr);
					int r = plugin_capabilities(&capptr, &callbacks);
					if (r == 0)
					{
						JS_Plugin_Item* item = OP_NEW(JS_Plugin_Item, ());
						if (item != NULL)
						{
							item->plugin_id = NULL;

							if (OpStatus::IsMemoryError(SetStr(item->plugin_id, so_elm->plugin_id)))
							{
								g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
								OP_DELETE(item);
							}
							else
							{
								item->plugin_cap = capptr;
#ifdef JS_PLUGIN_ATVEF_VISUAL
								item->plugin_cap_ext.cb_tv_become_visible = tmp_become_visible;
								item->plugin_cap_ext.cb_tv_position = tmp_position;
#endif
								item->Into(&plugin_list);
							}
						}
					}
				}
				else
				{
					dll->Unload();
					OP_DELETE(dll);
				}
			}
			else
			{
				OP_DELETE(dll);
			}
		// Now it's ok to leave again
		}
	}
	so_list.Clear();
#endif // DYNAMIC_JSPLUGINS

#ifdef JS_PLUGIN_ATVEF_VISUAL
	// Seed with dummy pointers to catch out-of-sequence calls
	tmp_become_visible = (jsplugin_tv_become_visible *) 0xDEADBEEF;
	tmp_position = (jsplugin_tv_position *) (0xDEADBEEF);
#endif

}

OP_STATUS
JS_Plugin_Manager::AddStaticPlugin(const char *name, jsplugin_capabilities_fn jsplugin_capabilities)
{
#ifdef STATIC_JSPLUGINS
	// Check that this plugin is allowed by rules in jsplugin.ini
	// This shouldn't be nessecary from a security perspective and
	// only tells the caller that this plugin will not be loaded.
	BOOL plugin_allowed = FALSE;
	for (JS_Plugin_AllowedFromElm* elm = static_cast<JS_Plugin_AllowedFromElm *>(plugin_allowed_from_list.First()) ; elm ; elm = static_cast<JS_Plugin_AllowedFromElm *>(elm->Suc()))
	{
		if (op_strcmp(name, elm->plugin_id) == 0)
		{
			// The plugin has already been seen
			plugin_allowed = TRUE;
			break;
		}
	}

	if (!plugin_allowed)
	{
		return OpStatus::ERR;
	}

#ifdef JS_PLUGIN_ATVEF_VISUAL
	// Prepare for callbacks during setup
	tmp_become_visible = NULL;
	tmp_position = NULL;
#endif

	jsplugin_cap *capptr = NULL;
	int r = jsplugin_capabilities(&capptr, &callbacks);
	if (r == 0)
	{
		JS_Plugin_Item* item = OP_NEW(JS_Plugin_Item, ());
		if (item != NULL)
		{
			item->plugin_id = NULL;

			if (OpStatus::IsMemoryError(SetStr(item->plugin_id, name)))
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				OP_DELETE(item);
				return OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				item->plugin_cap = capptr;
#ifdef JS_PLUGIN_ATVEF_VISUAL
				item->plugin_cap_ext.cb_tv_become_visible = tmp_become_visible;
				item->plugin_cap_ext.cb_tv_position = tmp_position;
#endif
				item->Into(&plugin_list);
			}
		}
	}

#ifdef JS_PLUGIN_ATVEF_VISUAL
	// Seed with dummy pointers to catch out-of-sequence calls
	tmp_become_visible = (jsplugin_tv_become_visible *) 0xDEADBEEF;
	tmp_position = (jsplugin_tv_position *) (0xDEADBEEF);
#endif

	return OpStatus::OK;
#else //STATIC_JSPLUGINS
	OP_ASSERT(!"Error: You must enable static jsplugins to use this function!");
	return OpStatus::ERR;
#endif
}

void
JS_Plugin_Manager::RemoveStaticPlugin(const char *name)
{
	for (JS_Plugin_Item *item = static_cast<JS_Plugin_Item *>(plugin_list.First()); item; item = static_cast<JS_Plugin_Item *>(item->Suc()))
	{
		if (op_strcmp(item->plugin_id, name) == 0)
		{
			item->Out();
			OP_DELETE(item);
			return;
		}
	}
}

class JS_Plugin_Container 
	: public EcmaScript_Object
{
public:
};

JS_Plugin_Context *
JS_Plugin_Manager::CreateContext(ES_Runtime *runtime, EcmaScript_Object *global_object)
{
	JS_Plugin_Context *ctx = OP_NEW(JS_Plugin_Context, (this, runtime));
	if (!ctx)
		return NULL;

	OpAutoPtr<JS_Plugin_Context> anchor(ctx);
	int private_name_counter = 0;
	JS_Plugin_Container *container = NULL;

	JS_Plugin_Item *item = static_cast<JS_Plugin_Item *>(plugin_list.First());
	while (item)
	{
		if (IsAllowedFrom(item, runtime))
		{
			if (!container)
			{
				container = OP_NEW(JS_Plugin_Container, ());
				if (!container ||
				    OpStatus::IsMemoryError(container->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object", FALSE)) ||
				    OpStatus::IsMemoryError(global_object->PutPrivate(ES_LAST_PRIVATESLOT - 1, *container)))
				{
					OP_DELETE(container);
					return NULL;
				}
			}

			JS_Plugin_Object *obj_opera = OP_NEW(JS_Plugin_Object, (ctx));
			if (!obj_opera)
				return NULL;

			if (OpStatus::IsMemoryError(obj_opera->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "PluginObject", FALSE)) ||
			    OpStatus::IsMemoryError(container->PutPrivate(++private_name_counter, *obj_opera)))
			{
				OP_DELETE(obj_opera);
				return NULL;
			}

			obj_opera->SetCallbacks(item->plugin_cap->global_getter, item->plugin_cap->global_setter, NULL, NULL, NULL, item->plugin_cap->gc_trace);
#ifdef JS_PLUGIN_ATVEF_VISUAL
			obj_opera->SetTvCallbacks(item->plugin_cap_ext.cb_tv_become_visible, item->plugin_cap_ext.cb_tv_position);
#endif
			if (OpStatus::IsMemoryError(ctx->AddPlugin(obj_opera, item->plugin_cap
#ifdef JS_PLUGIN_ATVEF_VISUAL
			                                           , &item->plugin_cap_ext
#endif
				
				)))
			{
				OP_DELETE(obj_opera);
				return NULL;
			}

			if (item->plugin_cap->init)
				item->plugin_cap->init(obj_opera->GetRepresentation());
		}

		item = static_cast<JS_Plugin_Item *>(item->Suc());
	}

	// When running with jsplugins we need to disable 'fast history navigation'
	// as this doesn't work properly with our unload callback
	if (ctx->GetFirstPlugin())
		runtime->GetFramesDocument()->SetCompatibleHistoryNavigationNeeded();

	anchor.release();
	return ctx;
}

void
JS_Plugin_Manager::DestroyContext(JS_Plugin_Context *context)
{
	JS_Plugin_Context::PluginElm *pe = context->GetFirstPlugin();

	while (pe)
	{
		if (pe->cap->destroy)
			pe->cap->destroy(pe->obj);

		pe = static_cast<JS_Plugin_Context::PluginElm *>(pe->Suc());
	}

	OP_DELETE(context);
}

OP_STATUS
JS_Plugin_Manager::AllowPluginFrom(const char *plugin_id, JS_Plugin_SecurityDomain *security_domain)
{
	JS_Plugin_AllowedFromElm *elm = OP_NEW(JS_Plugin_AllowedFromElm, ());
	if (!elm)
		return OpStatus::ERR_NO_MEMORY;

	elm->plugin_id = 0;
	if (OpStatus::IsMemoryError(SetStr(elm->plugin_id, plugin_id)))
	{
		OP_DELETE(elm);
		return OpStatus::ERR_NO_MEMORY;
	}

	elm->security_domain = security_domain;
	elm->Into(&plugin_allowed_from_list);

	return OpStatus::OK;
}

BOOL
JS_Plugin_Manager::IsAllowedFrom(JS_Plugin_Item *item, ES_Runtime *runtime)
{
	URLType document_url_type;
	const char *document_url_type_string;
	const uni_char *document_domain_uni;
	OpString8 document_domain;
	int document_port;

	static_cast<DOM_Runtime *>(runtime)->GetDomain(&document_domain_uni, &document_url_type, &document_port);

	if (OpStatus::IsError(document_domain.SetUTF8FromUTF16(document_domain_uni)))
		return FALSE;
	document_url_type_string = urlManager->MapUrlType(document_url_type);

	JS_Plugin_AllowedFromElm *elm = static_cast<JS_Plugin_AllowedFromElm *>(plugin_allowed_from_list.First());
	while (elm)
	{
		if (op_stricmp(item->plugin_id, elm->plugin_id) == 0)
		{
			if (elm->security_domain->UseCallback())
			{
				if (item->plugin_cap->allow_access(document_url_type_string, document_domain.CStr(), document_port))
					return TRUE;
			}
			else if (document_url_type == elm->security_domain->GetProtocol() &&
					 document_port == elm->security_domain->GetPort())
			{
				const char *security_domain = elm->security_domain->GetDomain();

				if (op_strlen(security_domain) > 6 && strni_eq(security_domain, "[ALL].", 6))
				{
					security_domain += 5;

					int offset = op_strlen(document_domain.CStr()) - op_strlen(security_domain);

					if (offset > 0 && op_stricmp(document_domain.CStr() + offset, security_domain) == 0)
						return TRUE;
				}
				else if (op_stricmp(document_domain.CStr(), security_domain) == 0)
					return TRUE;
			}
		}

		elm = static_cast<JS_Plugin_AllowedFromElm *>(elm->Suc());
	}

	return FALSE;
}

/** Read the plugin security information from file.
  * @param file File to read the permissions from. Must be open for reading.
  */
void
JS_Plugin_Manager::ReadPermissionsFileL(OpFile* file)
{
	if (file == NULL)
	{
		return;
	}

	OpString line; ANCHOR(OpString, line);

	while(!file->Eof())
	{
		LEAVE_IF_ERROR(file->ReadUTF8Line(line));

		line.Strip(TRUE, TRUE);

		// Don't care about comments
		if (line.CStr()[0] == '#')
		{
			continue;
		}

		uni_char* str = OP_NEWA_L(uni_char, line.Length() + 1);
		ANCHOR_ARRAY(uni_char, str);
		uni_strcpy(str, line.CStr());

		OpString plugin_id;  ANCHOR(OpString, plugin_id);
		OpString protocol;   ANCHOR(OpString, protocol);
		OpString server;     ANCHOR(OpString, server);
		OpString portstring; ANCHOR(OpString, portstring);

		plugin_id.SetL(uni_strtok(str, UNI_L(":")));
		protocol.SetL(uni_strtok(NULL, UNI_L(",")));
		server.SetL(uni_strtok(NULL, UNI_L(",")));
		portstring.SetL(uni_strtok(NULL, UNI_L(",")));

		plugin_id.Strip(TRUE, TRUE);
		protocol.Strip(TRUE, TRUE);
		server.Strip(TRUE, TRUE);
		portstring.Strip(TRUE, TRUE);

		// Sorry, no LEAVE from this point in loop.

		if (protocol.Compare("CALLBACK") == 0)
		{
			// Convert to plain char:
			char* c_plugin_id = uni_down_strdup(plugin_id.CStr());

			if (c_plugin_id != NULL) // Here we don't distinguish between oom and an empty string.
			{
				// Now construct the security object (js plugin manager takes ownership of it)

				JS_Plugin_SecurityDomain* securityObject = OP_NEW(JS_Plugin_SecurityDomain, ());

				if (securityObject != NULL)
				{
					securityObject->ConstructCallback();
					if (OpStatus::IsError(AllowPluginFrom(c_plugin_id, securityObject)))
						OP_DELETE(securityObject);
				}
			}

			op_free(c_plugin_id);
		}
		else
		{
			// Convert to plain char:
			char* c_plugin_id = uni_down_strdup(plugin_id.CStr());
			char* c_proto = uni_down_strdup(protocol.CStr());
			char* c_server = uni_down_strdup(server.CStr());

			if (c_plugin_id != NULL && c_proto != NULL && c_server != NULL && portstring.CStr() != NULL) // Here we don't distinguish between oom and an empty string.
			{
				int port = uni_atoi(portstring.CStr());

				// Now construct the security object (js plugin manager takes ownership of it)

				JS_Plugin_SecurityDomain* securityObject = OP_NEW(JS_Plugin_SecurityDomain, ());

				if (securityObject != NULL)
				{
					securityObject->Construct(c_proto, c_server, port);
					if (OpStatus::IsError(AllowPluginFrom(c_plugin_id, securityObject)))
						OP_DELETE(securityObject);
				}
			}

			op_free(c_plugin_id);
			op_free(c_proto);
			op_free(c_server);
		}
	}
}

#include "modules/ecmascript_utils/esasyncif.h"

class JS_Plugin_AsyncCallback 
	: public ES_AsyncCallback
{
private:
	JS_Plugin_Context *context;
	jsplugin_async_callback *callback;
	void *user_data;

public:
	JS_Plugin_AsyncCallback(JS_Plugin_Context *context, void *user_data, jsplugin_async_callback *callback)
		: context(context), callback(callback), user_data(user_data)
	{
	}

	virtual OP_STATUS
	HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		BOOL success = status == ES_ASYNC_SUCCESS;

		jsplugin_value value;

		if (success)
			JS_Plugin_Object::Export(context, &value, &result);
		else
			value.type = JSP_TYPE_UNDEFINED;

		callback(success ? JSP_CALLBACK_OK : JSP_CALLBACK_ERROR, &value, user_data);

		/* Free string allocated by JS_Plugin_Object::Export. */
		JS_Plugin_Object::FreeValue(&value);

		/* Ouch, ugly. */
		OP_DELETE(this);

		return OpStatus::OK;
	}

	void
	CompilationFailed()
	{
		jsplugin_value value;
		value.type = JSP_TYPE_UNDEFINED;

		callback(JSP_CALLBACK_ERROR, &value, user_data);
	}
};

OP_STATUS
JS_Plugin_Manager::PluginEval(JS_Plugin_Context *context, const char *code, void *user_data, jsplugin_async_callback *callback)
{
	if (!context || !context->GetRuntime() || !context->GetRuntime()->GetFramesDocument())
		return OpStatus::ERR;

	uni_char *is;
	RETURN_IF_ERROR(JS_Plugin_Object::ImportString(&is, NULL, code, op_strlen(code)));

	ES_AsyncInterface *asyncif = context->GetRuntime()->GetFramesDocument()->GetESAsyncInterface();
	ES_Thread *current_thread = DOM_Object::GetCurrentThread(context->GetRuntime());
	JS_Plugin_AsyncCallback *cb = NULL;

	if (callback)
	{
		cb = OP_NEW(JS_Plugin_AsyncCallback, (context, user_data, callback));
		if (!cb)
			return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = asyncif->Eval(is, cb, current_thread);

	if (OpStatus::IsError(status))
		OP_DELETE(cb);

	return status;
}

OP_STATUS
JS_Plugin_Manager::CallFunction(JS_Plugin_Context *context, ES_Object *fo, ES_Object *to, int argc, ES_Value *argv, void *user_data, jsplugin_async_callback *callback)
{
	ES_AsyncInterface *asyncif = context->GetRuntime()->GetFramesDocument()->GetESAsyncInterface();
	ES_Thread *current_thread = DOM_Object::GetCurrentThread(context->GetRuntime());
	JS_Plugin_AsyncCallback *cb = NULL;

	if (callback)
	{
		cb = OP_NEW(JS_Plugin_AsyncCallback, (context, user_data, callback));
		if (!cb)
			return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = asyncif->CallFunction(fo, to, argc, argv, cb, current_thread);

	if (OpStatus::IsError(status))
		OP_DELETE(cb);

	return status;
}

OP_STATUS
JS_Plugin_Manager::GetSlot(JS_Plugin_Context *context, ES_Object *obj, const char *name, void *user_data, jsplugin_async_callback *callback)
{
	ES_AsyncInterface *asyncif = context->GetRuntime()->GetFramesDocument()->GetESAsyncInterface();
	ES_Thread *current_thread = DOM_Object::GetCurrentThread(context->GetRuntime());
	JS_Plugin_AsyncCallback *cb = NULL;

	if (callback)
	{
		cb = OP_NEW(JS_Plugin_AsyncCallback, (context, user_data, callback));
		if (!cb)
			return OpStatus::ERR_NO_MEMORY;
	}

	OpString uni_name;
	RETURN_IF_ERROR(uni_name.Set(name));

	OP_STATUS status = asyncif->GetSlot(obj, uni_name.CStr(), cb, current_thread);

	if (OpStatus::IsError(status))
		OP_DELETE(cb);

	return status;
}

OP_STATUS
JS_Plugin_Manager::SetSlot(JS_Plugin_Context *context, ES_Object *obj, const char *name, ES_Value *value, void *user_data, jsplugin_async_callback *callback)
{
	ES_AsyncInterface *asyncif = context->GetRuntime()->GetFramesDocument()->GetESAsyncInterface();
	ES_Thread *current_thread = DOM_Object::GetCurrentThread(context->GetRuntime());
	JS_Plugin_AsyncCallback *cb = NULL;

	if (callback)
	{
		cb = OP_NEW(JS_Plugin_AsyncCallback, (context, user_data, callback));
		if (!cb)
			return OpStatus::ERR_NO_MEMORY;
	}

	OpString uni_name;
	RETURN_IF_ERROR(uni_name.Set(name));

	OP_STATUS status = asyncif->SetSlot(obj, uni_name.CStr(), *value, cb, current_thread);

	if (OpStatus::IsError(status))
		OP_DELETE(cb);

	return status;
}

JspluginsModule::JspluginsModule()
	: manager(NULL)
{
}

void
JspluginsModule::InitL(const OperaInitInfo& info)
{
#ifdef SELFTEST
# ifndef HAS_COMPLEX_GLOBALS
	extern void init_jsp_selftests_global_names();
	CONST_ARRAY_INIT(jsp_selftests_global_names);
	extern void init_jsp_selftests_object_types();
	CONST_ARRAY_INIT(jsp_selftests_object_types);
# endif // HAS_COMPLEX_GLOBALS
	jsp_selftest_bits = 0;
#endif

	manager = OP_NEW_L(JS_Plugin_Manager, ());
	manager->Setup();
}

void
JspluginsModule::Destroy()
{
	OP_DELETE(manager);
}

#endif // JS_PLUGIN_SUPPORT
