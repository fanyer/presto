/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** JS_Plugin_Object is an Opera-side object that maps to a jsplugin_obj object
    inside the plugin.
*/

#include "core/pch.h"

#ifdef JS_PLUGIN_SUPPORT
#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_object.h"
#include "modules/jsplugins/src/js_plugin_manager.h"
#include "modules/jsplugins/src/js_plugin_context.h"
#include "modules/jsplugins/src/js_plugin_macros.h"
#include "modules/jsplugins/src/js_plugin_privatenames.h"

#include "modules/doc/frm_doc.h"
#include "modules/dom/domutils.h"
#include "modules/dom/src/domruntime.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/util/tempbuf.h"
#include "modules/display/tvmanager.h"

extern "C" {

/* PROBLEM: these have lost the security context. */
int cb_create_function(const jsplugin_obj *refobj,
					  jsplugin_getter *g,
					  jsplugin_setter *s,
					  jsplugin_function *f_call,
					  jsplugin_function *f_construct,
					  const char *f_signature,
					  jsplugin_destructor *d,
					  jsplugin_gc_trace *t,
					  jsplugin_obj **result)
{
	if (!refobj || !refobj->opera_private)
		return JSP_CREATE_ERROR;

	JS_Plugin_Context *context = GET_JS_PLUGIN_OBJECT(refobj)->GetContext();

	JS_Plugin_Object *obj_opera = OP_NEW(JS_Plugin_Object, (context));
	if (!obj_opera)
		return JSP_CREATE_NOMEM;

	ES_Runtime *runtime = context->GetRuntime();

	ES_Object *global_object = DOM_Utils::GetES_Object(runtime->GetFramesDocument()->GetJSWindow());

	if (OpStatus::IsMemoryError(obj_opera->SetFunctionRuntime(runtime, runtime->GetFunctionPrototype(), "PluginObject", f_signature)) ||
		OpStatus::IsMemoryError(obj_opera->PutPrivate(1, global_object)))
	{
		OP_DELETE(obj_opera);
		return JSP_CREATE_NOMEM;
	}

	obj_opera->SetCallbacks(g, s, f_call, f_construct, d, t);
	*result = obj_opera->GetRepresentation();

	return JSP_CREATE_OK;
}

int cb_create_object(const jsplugin_obj *refobj,
					 jsplugin_getter *g,
					 jsplugin_setter *s,
					 jsplugin_destructor *d,
					 jsplugin_gc_trace *t,
					 jsplugin_obj **result)
{
	if (!refobj || !refobj->opera_private)
		return JSP_CREATE_ERROR;

	JS_Plugin_Context *context = GET_JS_PLUGIN_OBJECT(refobj)->GetContext();

	JS_Plugin_Object *obj_opera = OP_NEW(JS_Plugin_Object, (context));
	if (!obj_opera)
		return JSP_CREATE_NOMEM;

	ES_Runtime *runtime = context->GetRuntime();

	ES_Object *global_object = DOM_Utils::GetES_Object(runtime->GetFramesDocument()->GetJSWindow());

	if (OpStatus::IsMemoryError(obj_opera->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "PluginObject", FALSE)) ||
		OpStatus::IsMemoryError(obj_opera->PutPrivate(1, global_object)))
	{
		OP_DELETE(obj_opera);
		return JSP_CREATE_NOMEM;
	}

	obj_opera->SetCallbacks(g, s, NULL, NULL, d, t);
	*result = obj_opera->GetRepresentation();

	return JSP_CREATE_OK;
}

int cb_eval(const jsplugin_obj *refobj, const char *code, void *user_data, jsplugin_async_callback *callback)
{
	if (!refobj || !refobj->opera_private)
		return JSP_CALLBACK_ERROR;

	JS_Plugin_Context *context = GET_JS_PLUGIN_OBJECT(refobj)->GetContext();

	switch (g_jsPluginManager->PluginEval(context, code, user_data, callback))
	{
	case OpStatus::OK:
		return JSP_CALLBACK_OK;

	case OpStatus::ERR_NO_MEMORY:
		return JSP_CALLBACK_NOMEM;

	default:
		return JSP_CALLBACK_ERROR;
	}
}

int cb_add_unload_listener(const jsplugin_obj *target,
						   jsplugin_notify *listener)
{
	switch (GET_JS_PLUGIN_OBJECT(target)->AddUnloadListener(listener))
	{
	case OpStatus::OK:
		return JSP_CALLBACK_OK;

	case OpStatus::ERR_NO_MEMORY:
		return JSP_CALLBACK_NOMEM;

	default:
		return JSP_CALLBACK_ERROR;
	}
}

int cb_set_attr_change_listener(jsplugin_obj *target, jsplugin_attr_change_listener *listener)
{
	switch (GET_JS_PLUGIN_OBJECT(target)->SetAttrChangeListener(listener))
	{
	case OpStatus::OK:
		return JSP_CALLBACK_OK;

	case OpStatus::ERR_NO_MEMORY:
		return JSP_CALLBACK_NOMEM;

	default:
		return JSP_CALLBACK_ERROR;
	}
}

int cb_set_param_set_listener(jsplugin_obj *target, jsplugin_param_set_listener *listener)
{
	switch (GET_JS_PLUGIN_OBJECT(target)->SetParamSetListener(listener))
	{
	case OpStatus::OK:
		return JSP_CALLBACK_OK;

	case OpStatus::ERR_NO_MEMORY:
		return JSP_CALLBACK_NOMEM;

	default:
		return JSP_CALLBACK_ERROR;
	}
}

int cb_get_window_identifier(const jsplugin_obj *object, long int *identifier)
{
	OpWindow *window =
		GET_JS_PLUGIN_OBJECT(object)
			->GetContext()->GetRuntime()->GetFramesDocument()
			->GetWindow()->GetOpWindow();
	if (window)
	{
		*identifier = reinterpret_cast<long int>(window);
		return JSP_CALLBACK_OK;
	}

	return JSP_CALLBACK_ERROR;
}

int cb_resume(const jsplugin_obj *reference)
{
	JS_Plugin_Context *context =
		GET_JS_PLUGIN_OBJECT(reference)->GetContext();
	OP_ASSERT(context);
	if (context && context->IsSuspended() && OpStatus::IsSuccess(context->Resume()))
		return JSP_CALLBACK_OK;

	return JSP_CALLBACK_ERROR;
}

int cb_get_object_host(const jsplugin_obj *obj, char **host)
{
	JS_Plugin_Object *op_obj = GET_JS_PLUGIN_OBJECT(obj);
	DOM_Runtime *runtime =
		DOM_Utils::GetDOM_Runtime(op_obj->GetContext()->GetRuntime());
	URL url = runtime->GetOriginURL();

	OpString8 url_str;
	if (OpStatus::IsError(url.GetAttribute(URL::KName_Escaped, url_str))
		|| url_str.IsEmpty())
	{
		*host = NULL;
		return JSP_CALLBACK_ERROR;
	}

	TempBuffer *buf = g_jsPluginManager->GetTempBuffer();
	if (OpStatus::IsError(buf->Expand(UNICODE_DOWNSIZE(url_str.Length()) + 1)))
		return JSP_CALLBACK_ERROR;

	*host = reinterpret_cast<char *>(buf->GetStorage());
	op_strcpy(*host, url_str.CStr());
	return JSP_CALLBACK_OK;
}

void cb_gcmark(const jsplugin_obj *obj)
{
	if (!obj)
		return;

	EcmaScript_Object *eso = GET_ECMASCRIPT_OBJECT(obj);
	if (eso)
		eso->GetRuntime()->GCMarkSafe(eso->GetNativeObject());
}

} // extern "C"

///////  Methods on the JS_Plugin_Object

JS_Plugin_Object::JS_Plugin_Object(JS_Plugin_Context *context)
	: context(context)
	, getname(NULL)
	, putname(NULL)
	, call(NULL)
	, construct(NULL)
	, destruct(NULL)
	, attr_change_listener(NULL)
	, param_set_listener(NULL)
#ifdef JS_PLUGIN_ATVEF_VISUAL
	, tv_become_visible(NULL)
	, tv_position(NULL)
#endif
{
#ifdef _DEBUG
	++g_opera->jsplugins_module.jspobj_balance;
#endif // _DEBUG

	self.opera_private = static_cast<EcmaScript_Object *>(this);
	self.plugin_private = NULL;
	context->AddPluginObject(this);
}

JS_Plugin_Object::~JS_Plugin_Object()
{
#ifdef _DEBUG
	--g_opera->jsplugins_module.jspobj_balance;
#endif // _DEBUG

#ifdef JS_PLUGIN_ATVEF_VISUAL
	if (!tv_url.IsEmpty())
	{
		g_tvManager->UnregisterTvListener(this);
	}
#endif

	Out();
	unload_listeners.Clear();

	if (destruct)
		destruct(&self);

	self.opera_private = self.plugin_private = NULL;

}

/* virtual */ void
JS_Plugin_Object::GCTrace()
{
	for (JS_Eval_Elm *e = static_cast<JS_Eval_Elm *>(eval_elms.First()); e; e = static_cast<JS_Eval_Elm *>(e->Suc()))
		GetRuntime()->GCMark(e->GetNativeObject());

	if (this->trace)
		trace(&self);
}

void
JS_Plugin_Object::BeforeUnload()
{
	for (UnloadListenerElm *u = static_cast<UnloadListenerElm *>(unload_listeners.First()); u; u = static_cast<UnloadListenerElm *>(u->Suc()))
	{
		jsplugin_notify *notify = u->listener;
		notify(this->GetRepresentation());
	}
}

OP_STATUS
JS_Plugin_Object::AddUnloadListener(jsplugin_notify *listener)
{
	UnloadListenerElm *ul = OP_NEW(UnloadListenerElm, (listener));
	if (!ul)
		return OpStatus::ERR_NO_MEMORY;

	ul->Into(&unload_listeners);

	return OpStatus::OK;
}

OP_STATUS
JS_Plugin_Object::SetAttrChangeListener(jsplugin_attr_change_listener *listener)
{
	attr_change_listener = listener;
	return OpStatus::OK;
}

void
JS_Plugin_Object::AttributeChanged(const uni_char *name, const uni_char *value)
{
	if (attr_change_listener)
	{
		OpString8 name8;
		if (OpStatus::IsError(name8.SetUTF8FromUTF16(name)))
			return;

		OpString8 value8;
		if (OpStatus::IsError(value8.SetUTF8FromUTF16(value)))
			return;

		attr_change_listener(this->GetRepresentation(), name8.CStr(), value8.CStr());
	}
}


OP_STATUS
JS_Plugin_Object::SetParamSetListener(jsplugin_param_set_listener *listener)
{
	param_set_listener = listener;
	return OpStatus::OK;
}


void
JS_Plugin_Object::ParamSet(const uni_char *name, const uni_char *value)
{
	if (param_set_listener)
	{
		OpString8 name8;
		if (OpStatus::IsError(name8.SetUTF8FromUTF16(name)))
			return;

		OpString8 value8;
		if (OpStatus::IsError(value8.SetUTF8FromUTF16(value)))
			return;

		param_set_listener(this->GetRepresentation(), name8.CStr(), value8.CStr());
	}
}


void
JS_Plugin_Object::SetCallbacks(jsplugin_getter *g, jsplugin_setter *s, jsplugin_function *c,
							   jsplugin_function *ctor, jsplugin_destructor *dtor, jsplugin_gc_trace *t)
{
	getname = g;
	putname = s;
	call = c;
	construct = ctor;
	destruct = dtor;
	trace = t;
}

#ifdef JS_PLUGIN_ATVEF_VISUAL
void
JS_Plugin_Object::SetTvCallbacks(jsplugin_tv_become_visible *become_visible,
								 jsplugin_tv_position *position)
{
	tv_become_visible = become_visible;
	tv_position = position;
}
#endif

/* static */ OP_STATUS
JS_Plugin_Object::ExportNativeObject(JS_Plugin_Context *context, jsplugin_obj *&eo, ES_Object *io)
{
	JS_Plugin_Native *obj = NULL;;

	ES_Value val;
	if (context->GetRuntime()->GetPrivate(io, JS_PLUGIN_PRIVATE_nativeObject, &val) == OpBoolean::IS_TRUE && val.type == VALUE_OBJECT)
	{
		if (op_strcmp(ES_Runtime::GetClass(val.value.object), "JS_Plugin_Native") == 0)
			obj = static_cast<JS_Plugin_Native*>(ES_Runtime::GetHostObject(val.value.object));
	}

	if (!obj)
	{
		RETURN_IF_ERROR(JS_Plugin_Native::Make(obj, context, io));

		val.type = VALUE_OBJECT;
		val.value.object = obj->GetNativeObject();
		RETURN_IF_ERROR(context->GetRuntime()->PutPrivate(io, JS_PLUGIN_PRIVATE_nativeObject, val));
	}

	eo = obj->GetRepresentation();

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Plugin_Object::ExportObject(JS_Plugin_Context *context, jsplugin_obj *&eo, ES_Object *io)
{
	if (op_strcmp(ES_Runtime::GetClass(io), "PluginObject") == 0)
		eo = static_cast<JS_Plugin_Object *>(ES_Runtime::GetHostObject(io))->GetRepresentation();
	else if (op_strcmp(ES_Runtime::GetClass(io), "HTMLObjectElement") == 0)
		eo = static_cast<JS_Plugin_Object *>(ES_Runtime::GetHostObject(DOM_Utils::GetJSPluginObject(DOM_Utils::GetDOM_Object(io))))->GetRepresentation();
	else
		eo = NULL; // Handled by ExportNativeObject

	if (eo && GET_JS_PLUGIN_OBJECT(eo)->GetContext() != context)
		eo = NULL;

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Plugin_Object::ExportString(char **es, int *es_len, const uni_char *is, unsigned is_len)
{
	UTF16toUTF8Converter converter;
	int read;

	int size = converter.BytesNeeded(is, UNICODE_SIZE(is_len));
	converter.Reset();

	*es = OP_NEWA(char, size + 1);
	if (!*es)
		return OpStatus::ERR_NO_MEMORY;

	converter.Convert(reinterpret_cast<const char *>(is), UNICODE_SIZE(is_len), *es, size, &read);
	(*es)[size] = 0;
	if (es_len) *es_len = size;

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Plugin_Object::Export(JS_Plugin_Context *context, jsplugin_value *ev, const ES_Value *iv)
{
	if (ev)
		switch (iv->type)
		{
		case VALUE_NULL:
			ev->type = JSP_TYPE_NULL;
			break;
		case VALUE_UNDEFINED:
			ev->type = JSP_TYPE_UNDEFINED;
			break;
		case VALUE_BOOLEAN:
			ev->type = JSP_TYPE_BOOLEAN;
			ev->u.boolean = iv->value.boolean;
			break;
		case VALUE_NUMBER:
			ev->type = JSP_TYPE_NUMBER;
			ev->u.number = iv->value.number;
			break;
		case VALUE_STRING:
			ev->type = JSP_TYPE_STRING;
			return ExportString(const_cast<char **>(&ev->u.s.string), &ev->u.s.len , iv->value.string, iv->GetStringLength());
		case VALUE_OBJECT:
			ev->type = JSP_TYPE_OBJECT;
			RETURN_IF_ERROR(ExportObject(context, ev->u.object, iv->value.object));
			if (!ev->u.object) // not handled by ExportObject
			{
				ev->type = JSP_TYPE_NATIVE_OBJECT;
				RETURN_IF_ERROR(ExportNativeObject(context, ev->u.object, iv->value.object));
			}
			break;
		}

	return OpStatus::OK;
}

JS_Plugin_Context *
JS_Plugin_Object::GetOriginingContext(ES_Runtime* origining_runtime)
{
	if (origining_runtime)
		return origining_runtime->GetFramesDocument()->GetDOMEnvironment()->GetJSPluginContext();
	else
		return GetContext();
}

OP_STATUS
JS_Plugin_Object::ImportExpression(ES_Value *iv, jsplugin_value *ev)
{
	if (ev->type != JSP_TYPE_EXPRESSION)
		return OpStatus::ERR;

	uni_char *es;
	RETURN_IF_ERROR(ImportString(&es, NULL,  ev->u.string, op_strlen(ev->u.string)));

	JS_Eval_Elm *elm;
	RETURN_IF_ERROR(JS_Eval_Elm::Make(elm, context, es));

	elm->Into(&eval_elms);

	iv->type = VALUE_OBJECT;
	iv->value.object = elm->GetNativeObject();

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Plugin_Object::ImportString(uni_char **is, unsigned *is_len,  const char *es, int es_len, BOOL temp)
{
	UTF8toUTF16Converter converter;
	int read;

	int size = converter.Convert(es, es_len, NULL, INT_MAX, &read);

	TempBuffer *buf = g_jsPluginManager->GetTempBuffer();

	RETURN_IF_ERROR(buf->Expand(UNICODE_DOWNSIZE(size) + 1));
	*is = buf->GetStorage();

	converter.Convert(es, es_len, *is, UNICODE_SIZE(UNICODE_DOWNSIZE(size)), &read);
	(*is)[UNICODE_DOWNSIZE(size)] = 0;
	if (is_len)
		*is_len = UNICODE_DOWNSIZE(size);

	if (!temp)
	{
		uni_char *str = OP_NEWA(uni_char, UNICODE_DOWNSIZE(size) + 1);
		if (!str)
			return OpStatus::ERR_NO_MEMORY;

		uni_strncpy(str, *is, UNICODE_DOWNSIZE(size));
		str[UNICODE_DOWNSIZE(size)] = 0;
		*is = str;
	}

	return OpStatus::OK;
}

OP_STATUS
JS_Plugin_Object::Import(ES_Value *iv, jsplugin_value *ev, BOOL temp)
{
	ES_ValueString *is;
	OP_STATUS stat;
	if (iv)
		switch (ev->type)
		{
		case JSP_TYPE_NULL:
			iv->type = VALUE_NULL;
			break;
		case JSP_TYPE_UNDEFINED:
			iv->type = VALUE_UNDEFINED;
			break;
		case JSP_TYPE_BOOLEAN:
			iv->type = VALUE_BOOLEAN;
			iv->value.boolean = !!ev->u.boolean;
			break;
		case JSP_TYPE_NUMBER:
			iv->type = VALUE_NUMBER;
			iv->value.number = ev->u.number;
			break;
		case JSP_TYPE_STRING:
			iv->type = VALUE_STRING;
			return ImportString(const_cast<uni_char **>(&iv->value.string), NULL, ev->u.string, op_strlen(ev->u.string), temp);
		case JSP_TYPE_STRING_WITH_LENGTH:
			iv->type = VALUE_STRING_WITH_LENGTH;
			if (temp)
				is = g_jsPluginManager->GetTempValueString();
			else
			{
				is = OP_NEW(ES_ValueString, ());
				if (!is)
					return OpStatus::ERR_NO_MEMORY;
			}
			stat = ImportString(const_cast<uni_char **>(&is->string), &is->length, ev->u.s.string, ev->u.s.len, temp);
			if (OpStatus::IsError(stat))
			{
				if (!temp) OP_DELETE(is);
			}
			else
			{
				iv->value.string_with_length = is;
			}
			return stat;
		case JSP_TYPE_EXPRESSION:
			return ImportExpression(iv, ev);
		case JSP_TYPE_NATIVE_OBJECT:
			{
				iv->type = VALUE_OBJECT;
				JS_Plugin_Native *o = GET_JS_PLUGIN_NATIVE_OBJECT(ev->u.object);
				iv->value.object = o->GetShadow();
			}
			break;
		case JSP_TYPE_OBJECT:
			{
				iv->type = VALUE_OBJECT;
				JS_Plugin_Object *o = GET_JS_PLUGIN_OBJECT(ev->u.object);
				if (o->IsObject())
				{
					HTML_Element *elm = static_cast<JS_Plugin_HTMLObjectElement_Object *>(o)->GetElement();
					iv->value.object = DOM_Utils::GetES_Object(elm->GetESElement());
				}
				else
					iv->value.object = *o;
			}
			break;
		}

	return OpStatus::OK;
}

/* static */ void
JS_Plugin_Object::FreeValue(jsplugin_value *ev)
{
	if (ev->type == JSP_TYPE_STRING || ev->type == JSP_TYPE_STRING_WITH_LENGTH)
		OP_DELETEA(const_cast<char *>(ev->u.string));
}

/* static */ void
JS_Plugin_Object::FreeValue(ES_Value *ev)
{
	// Free value allocated by JS_Plugin_Object::Import(..., ..., FALSE)
	if (ev->type == VALUE_STRING)
		OP_DELETEA(const_cast<uni_char *>(ev->value.string));
	else if (ev->type == VALUE_STRING_WITH_LENGTH)
	{
		OP_DELETEA(const_cast<uni_char *>(ev->value.string_with_length->string));
		OP_DELETE(ev->value.string_with_length);
	}
}

ES_GetState
JS_Plugin_Object::GetName(const uni_char* property_name, ES_Value* value, BOOL *cacheable, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (!getname)
		return GET_FAILED;

	int r;
	char *pn;
	jsplugin_value result;
	result.type = JSP_TYPE_UNDEFINED;
	result.u.number = 0.0;

	JS_Plugin_Context* origining_context = GetOriginingContext(origining_runtime);

	if (origining_context->GetSuspensionOrigin() == JS_Plugin_Context::SUSPEND_GET)
	{
		origining_context->ResetSuspensionOrigin();
		if (restart_object)
		{
			ExportObject(GetContext(), result.u.object, restart_object);
			result.type = JSP_TYPE_OBJECT;
		}
		else
			result.type = JSP_TYPE_NULL;
	}

	GET_FAILED_IF_ERROR(ExportString(&pn, NULL, property_name, uni_strlen(property_name)));
	r = getname( &self, pn, &result );
	OP_DELETEA(pn);

	switch (r)
	{
	case JSP_GET_VALUE_CACHE:
		{
			ES_Value backup, *value_ptr = value ? value : &backup;
			GET_FAILED_IF_ERROR(Import(value_ptr, &result));
			if (result.type == JSP_TYPE_EXPRESSION)
				return GET_SUSPEND;
			GET_FAILED_IF_ERROR(Put(property_name, *value_ptr));
			*cacheable = TRUE;
		}
		return GET_SUCCESS;

	case JSP_GET_VALUE:
		GET_FAILED_IF_ERROR(Import(value, &result));
		if (result.type == JSP_TYPE_EXPRESSION)
			return GET_SUSPEND;
		else
			return GET_SUCCESS;

	case JSP_GET_DELAYED:
		OP_ASSERT(result.type == JSP_TYPE_OBJECT || result.type == JSP_TYPE_NULL);
		// we must return a NULL or an OBJECT when it is suspended
		if (result.type == JSP_TYPE_OBJECT)
			GET_FAILED_IF_ERROR(Import(value, &result));
		else
			value->type = VALUE_NULL;
		origining_context->Suspend(JS_Plugin_Context::SUSPEND_GET);
		return GET_SUSPEND;

	case JSP_GET_EXCEPTION:
		GET_FAILED_IF_ERROR(Import(value, &result));
		return GET_EXCEPTION;

	case JSP_GET_NOTFOUND:
		return GET_FAILED;

	case JSP_GET_NOMEM:
		return GET_NO_MEMORY;

	case JSP_GET_ERROR:
	default:
		return GET_SECURITY_VIOLATION;  // Not correct, but triggers an exception at least
	}
}

/* virtual */
ES_GetState
JS_Plugin_Object::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	/* NOTE!  origining_runtime will be NULL if call comes through GetIndex */

	BOOL dummy;
	return GetName(property_name, value, &dummy, origining_runtime);
}

/* virtual */ ES_GetState
JS_Plugin_Object::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (GetOriginingContext(origining_runtime)->GetSuspensionOrigin() == JS_Plugin_Context::SUSPEND_GET)
	{
		BOOL dummy;
		return GetName(property_name, value, &dummy, origining_runtime, restart_object);
	}
	else if (restart_object)
	{
		JS_Eval_Elm *elm = static_cast<JS_Eval_Elm *>(GetRuntime()->GetHostObject(restart_object));
		if (elm)
		{
			elm->Import(value);
			return GET_SUCCESS;
		}
	}

	return GET_FAILED;
}

/* virtual */
ES_PutState
JS_Plugin_Object::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	/* NOTE!  origining_runtime will be NULL if call comes through PutIndex */

	if (!putname)
		return PUT_FAILED;

	jsplugin_value val;
	jsplugin_value tmp_val;
	int r;
	char *pn;

	PUT_FAILED_IF_ERROR(Export(GetContext(), &val, value));

	if (OpStatus::IsError(ExportString(&pn, NULL, property_name, uni_strlen(property_name))))
	{
		FreeValue(&val);
		return PUT_NO_MEMORY;
	}
	tmp_val = val;

	r = putname( &self, pn, &val );
	OP_DELETEA(pn);

	// On put exceptions, the exception is passed back from the plugin in val
	if (r == JSP_PUT_EXCEPTION)
	{
		PUT_FAILED_IF_ERROR(Import(value, &val));
		OP_ASSERT(val.type != JSP_TYPE_EXPRESSION);	// not yet supported
	}

	FreeValue(&tmp_val);

	switch (r)
	{
	case JSP_PUT_OK:
		return PUT_SUCCESS;
	case JSP_PUT_NOTFOUND:
		return PUT_FAILED;
	case JSP_PUT_NOMEM:
		return PUT_NO_MEMORY;
	case JSP_PUT_EXCEPTION:
		return PUT_EXCEPTION;

	case JSP_PUT_READONLY:
	case JSP_PUT_ERROR:
	default:
		return PUT_SECURITY_VIOLATION;  // Wrong for ERROR but triggers an exception, at least.
	}
}

/* virtual */ ES_GetState
JS_Plugin_Object::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	uni_char buf[32]; /* ARRAY OK 2010-11-18 lasse */

	uni_snprintf( buf, 32, UNI_L("%d"), property_index );
	return GetName( buf, 0, value, origining_runtime );
}


/* virtual */ ES_PutState
JS_Plugin_Object::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	uni_char buf[32]; /* ARRAY OK 2010-11-18 lasse */

	uni_snprintf( buf, 32, UNI_L("%d"), property_index );
	return PutName( buf, 0, value, origining_runtime );
}

int
JS_Plugin_Object::CallOrConstruct(jsplugin_function *f, ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	jsplugin_obj *this_obj;
	ExportObject(GetContext(), this_obj, this_object);

	JS_Plugin_Context* origining_context = GetOriginingContext(origining_runtime);

	// Restart-handler
	if (argc < 0 && origining_context->GetSuspensionOrigin() != JS_Plugin_Context::SUSPEND_CALL && return_value->type == VALUE_OBJECT)
	{
		JS_Eval_Elm *elm = static_cast<JS_Eval_Elm *>(GetRuntime()->GetHostObject(return_value->value.object));
		if (elm)
		{
			elm->Import(return_value);
			return ES_VALUE;
		}
		else
			return ES_FAILED;
	}

	// Normal code path (not a restart)
	jsplugin_value local_argv_base[32], result;
	jsplugin_value *local_argv = local_argv_base;
	int ret = ES_FAILED;
	int index = 0;
	int r;
	if (argc >= 0)
	{
		if (argc > 32)
			local_argv = OP_NEWA(jsplugin_value, argc);

		if (!local_argv)
			return ES_NO_MEMORY;

		for (index = 0; index < argc; ++index)
			if (OpStatus::IsError(Export(GetContext(), &local_argv[index], &argv[index])))
			{
				ret = ES_NO_MEMORY;
				goto cleanup;
			}
	}
	result.type = JSP_TYPE_UNDEFINED;
	if (argc < 0 && origining_context->GetSuspensionOrigin() == JS_Plugin_Context::SUSPEND_CALL)
	{
		origining_context->ResetSuspensionOrigin();
		if (return_value->type == VALUE_OBJECT)
			CALL_FAILED_IF_ERROR(Export(GetContext(), &result, return_value));
		else
			result.type = JSP_TYPE_NULL;
	}

	r = f(this_obj, &self, argc, argc >= 0 ? local_argv : NULL, &result);

	switch (r)
	{
	case JSP_CALL_VALUE:
		if (OpStatus::IsSuccess(Import(return_value, &result)))
		{
			if (result.type == JSP_TYPE_EXPRESSION)
				ret = ES_SUSPEND | ES_RESTART;
			else
				ret = ES_VALUE;
		}
		else
			ret = ES_NO_MEMORY;
		break;

	case JSP_CALL_NO_VALUE:
		ret = ES_FAILED;
		break;

	case JSP_CALL_NOMEM:
		ret = ES_NO_MEMORY;
		break;

	case JSP_CALL_EXCEPTION:
		if (OpStatus::IsSuccess(Import(return_value, &result)))
		{
			OP_ASSERT(result.type != JSP_TYPE_EXPRESSION);	// Not yet supported
			ret = ES_EXCEPTION;
		}
		else
			ret = ES_NO_MEMORY;
		break;

	case JSP_CALL_DELAYED:
		origining_context->Suspend(JS_Plugin_Context::SUSPEND_CALL);

		OP_ASSERT(result.type == JSP_TYPE_OBJECT || result.type == JSP_TYPE_NULL || result.type == JSP_TYPE_UNDEFINED);
		// We must return a NULL or an OBJECT when it is suspended
		if (result.type == JSP_TYPE_OBJECT)
			CALL_FAILED_IF_ERROR(Import(return_value, &result));
		else
			return_value->type = VALUE_NULL;
		ret = ES_SUSPEND | ES_RESTART;
		break;

	case JSP_CALL_ERROR:
	default:
		/* FIXME: need to throw exception here. */
		OP_ASSERT(FALSE);
	}

cleanup:
	while (--index >= 0)
		FreeValue(&local_argv[index]);

	if (local_argv != local_argv_base)
		OP_DELETEA(local_argv);

	return ret;
}

/* virtual */ int
JS_Plugin_Object::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	jsplugin_function *f = call;

	/* NOTE feature: Constructor is called if call is not implemented but constructor is. */
	if (!call && construct)
		f = construct;

	return CallOrConstruct(f, this_object, argv, argc, return_value, origining_runtime);
}

BOOL
JS_Plugin_Object::SecurityCheck(ES_Runtime* origining_runtime)
{
	BOOL allowed = FALSE;
	OpStatus::Ignore(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, OpSecurityContext(DOM_Utils::GetDOM_Runtime(GetRuntime())), OpSecurityContext(DOM_Utils::GetDOM_Runtime(origining_runtime)), allowed));
	return allowed;
}

/* virtual */ int
JS_Plugin_Object::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	if (!construct)
		return ES_FAILED;

	return CallOrConstruct(construct, *this, argv, argc, return_value, origining_runtime);
}

#ifdef JS_PLUGIN_ATVEF_VISUAL
OP_STATUS
JS_Plugin_Object::RegisterAsTvListener(const uni_char *url)
{
	if (IsAtvefVisualPlugin())
	{
		// Record the URL that will be used in subsequent callbacks
		// from the layout engine and register us as a listener.
		RETURN_IF_ERROR(tv_url.Set(url));
		return g_tvManager->RegisterTvListener(this);
	}
	else
	{
		// Invalid call (callbacks are not available in the plugin).
		return OpStatus::ERR_NULL_POINTER;
	}
}

void
JS_Plugin_Object::SetDisplayRect(const uni_char *url, const OpRect &rect)
{
	// If this is ours, we call the plugin with the details.
	if (tv_url.Compare(url) == 0)
	{
		tv_position(&self, rect.x, rect.y, rect.width, rect.height);
	}
}

void
JS_Plugin_Object::OnTvWindowAvailable(const uni_char *url, BOOL available)
{
	// If this is ours, we call the plugin with the details.
	if (tv_url.Compare(url) == 0)
	{
		ES_Runtime* runtime;
		FramesDocument* doc;
		BOOL is_doc_undisplaying =
				context
				&& (runtime = context->GetRuntime())
				&& (doc = runtime->GetFramesDocument())
				&& doc->IsUndisplaying();
		tv_become_visible(&self, available, is_doc_undisplaying);
	}
}
#endif

/* static */ OP_STATUS
JS_Plugin_Native::Make(JS_Plugin_Native *&new_obj, JS_Plugin_Context *context, ES_Object* obj)
{
	new_obj = OP_NEW(JS_Plugin_Native, (obj));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_obj->SetObjectRuntime(context->GetRuntime(), context->GetRuntime()->GetObjectPrototype(), "JS_Plugin_Native"));

	return OpStatus::OK;
}

void
JS_Plugin_HTMLObjectElement_Object::SetCallbacks(jsplugin_getter *g, jsplugin_setter *s, jsplugin_function *c,
												 jsplugin_function *ctor, jsplugin_destructor *dtor, jsplugin_gc_trace *t,
												 jsplugin_notify *i, jsplugin_notify *r)
{
	JS_Plugin_Object::SetCallbacks(g, s, c, ctor, dtor, t);
	inserted = i;
	removed = r;
}

void
JS_Plugin_HTMLObjectElement_Object::Inserted()
{
	if (inserted && !is_inserted)
	{
		inserted(&self);
		is_inserted = TRUE;
	}
}

void
JS_Plugin_HTMLObjectElement_Object::Removed()
{
	if (removed && is_inserted)
	{
		removed(&self);
		is_inserted = FALSE;
	}
}

/* static */ OP_STATUS
JS_Eval_Elm::Make(JS_Eval_Elm *&new_obj, JS_Plugin_Context *context, const uni_char* expression)
{
	new_obj = OP_NEW(JS_Eval_Elm, (context, expression));
	if (!new_obj)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_obj->SetObjectRuntime(context->GetRuntime(), context->GetRuntime()->GetObjectPrototype(), "JS_Eval_Elm"));

	RETURN_IF_ERROR(new_obj->Init());

	return OpStatus::OK;
}

JS_Eval_Elm::JS_Eval_Elm(JS_Plugin_Context *context, const uni_char *expression)
	: context(context)
	, expression(expression)
{
}

OP_STATUS
JS_Eval_Elm::Init()
{
	ES_ThreadScheduler *scheduler = context->GetRuntime()->GetESScheduler();

	ES_AsyncInterface *asyncif = context->GetRuntime()->GetFramesDocument()->GetESAsyncInterface();
	RETURN_IF_ERROR(asyncif->Eval(expression, this, scheduler->GetCurrentThread()));

	return OpStatus::OK;
}

OP_STATUS
JS_Eval_Elm::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	if (status == ES_ASYNC_SUCCESS)
	{
		value.type = result.type;
		if (value.type == VALUE_STRING)
		{
			value.value.string = uni_strdup(result.value.string);
			if (value.value.string == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
		else
			value.value = result.value;
	}

	// Remove ourselves, and prepare for GC
	Out();

	return OpStatus::OK;
}

void
JS_Eval_Elm::Import(ES_Value *iv)
{
	// Just pass ownership over to caller
	iv->type = value.type;
	iv->value = value.value;

	value.type = VALUE_UNDEFINED;
}

#endif // JS_PLUGIN_SUPPORT
