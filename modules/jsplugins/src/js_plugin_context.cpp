/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef JS_PLUGIN_SUPPORT
#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_context.h"
#include "modules/jsplugins/src/js_plugin_object.h"
#include "modules/jsplugins/src/js_plugin_manager.h"
#include "modules/jsplugins/src/js_plugin_macros.h"

#include "modules/dom/domutils.h"
#include "modules/dom/src/domobj.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/ecmascript_utils/esasyncif.h"

extern "C" {

int cb_set_poll_interval(const jsplugin_obj *global_context, unsigned interval, jsplugin_poll_callback *callback)
{
	if (!global_context || interval == 0 || !callback)
		return JSP_POLL_INVALID;

	JS_Plugin_Context *context = GET_JS_PLUGIN_OBJECT(global_context)->GetContext();

	return OpStatus::IsMemoryError(context->AddPoll(global_context, interval, callback)) ? JSP_POLL_NOMEM : JSP_POLL_OK;
}

int cb_call_function(const jsplugin_obj *global_context, const jsplugin_obj *this_obj, const jsplugin_obj *function_obj, int argc, jsplugin_value *argv, void *user_data, jsplugin_async_callback *callback)
{
	int i, ret = JSP_CALL_ERROR;
	ES_Value *local_argv, local_argv_base[32];

	if (!global_context)
		return ret;

	JS_Plugin_Object *object = GET_JS_PLUGIN_OBJECT(global_context);
	JS_Plugin_Context *context = object->GetContext();
	
	ES_Object *to = this_obj ? GET_ES_OBJECT(this_obj) : NULL;
	ES_Object *fo = function_obj ? GET_ES_OBJECT(function_obj) : NULL;

	if (argc > 32)
		local_argv = OP_NEWA(ES_Value, argc);
	else
		local_argv = local_argv_base;

	if (!local_argv)
		return JSP_CALL_NOMEM;

	for (i = 0; i < argc; i++)
	{
		OP_ASSERT(argv[i].type != JSP_TYPE_EXPRESSION);	// not yet supported
		if (OpStatus::IsError(object->Import(&local_argv[i], &argv[i], FALSE)))
		{
			ret = JSP_CALL_NOMEM;
			goto cleanup;
		}
	}

	switch (g_jsPluginManager->CallFunction(context, fo, to, argc, local_argv, user_data, callback))
	{
	case OpStatus::OK:
		ret = JSP_CALL_NO_VALUE;
		break;

	case OpStatus::ERR_NO_MEMORY:
		ret = JSP_CALL_NOMEM;
		break;

	default:
		ret = JSP_CALL_ERROR;
		break;
	}

cleanup:
	while (--argc >= 0)
		JS_Plugin_Object::FreeValue(&local_argv[argc]);

	if (local_argv != local_argv_base)
		OP_DELETEA(local_argv);

	return ret;
}

int cb_getter(const jsplugin_obj *global_context, const jsplugin_obj *obj, const char *name, void *user_data, jsplugin_async_callback *callback)
{
	JS_Plugin_Context *context = GET_JS_PLUGIN_OBJECT(global_context)->GetContext();
	if (context)
	{
		ES_Object *o = obj ? GET_ES_OBJECT(obj) : NULL;
		if (o)
		{
			if (OpStatus::IsSuccess(g_jsPluginManager->GetSlot(context, o, name, user_data, callback)))
				return JSP_GET_VALUE;
		}
	}
	return JSP_GET_ERROR;
}

int cb_setter(const jsplugin_obj *global_context, const jsplugin_obj *obj, const char *name, jsplugin_value *value, void *user_data, jsplugin_async_callback *callback)
{
	JS_Plugin_Object *object = GET_JS_PLUGIN_OBJECT(global_context);
	JS_Plugin_Context *context = object->GetContext();
	if (context)
	{
		ES_Object *o = obj ? GET_ES_OBJECT(obj) : NULL;

		ES_Value val;
		RETURN_VALUE_IF_ERROR(object->Import(&val, value), JSP_PUT_ERROR);
		OP_ASSERT(value->type != JSP_TYPE_EXPRESSION);	// Not yet supported

		if (o)
		{
			if (OpStatus::IsSuccess(g_jsPluginManager->SetSlot(context, o, name, &val, user_data, callback)))
				return JSP_PUT_OK;
		}
	}
	return JSP_PUT_ERROR;
}

} // extern "C"

void
JS_Plugin_Context::ThreadState::Suspend(ES_Thread *t, SuspensionOrigin o)
{
	OP_ASSERT(state == STATE_RUNNING);
	if (state != STATE_RUNNING)
		return;

	state = STATE_SUSPENDED;
	origin = o;
	OP_ASSERT(!thread);
	thread = t;
	thread->AddListener(this);
	thread->Block();
}

OP_STATUS
JS_Plugin_Context::ThreadState::Resume()
{
	OP_ASSERT(state == STATE_SUSPENDED);
	if (state != STATE_SUSPENDED)
		return OpStatus::ERR;
	OP_ASSERT(thread);
	if (!thread)
		return OpStatus::ERR_NULL_POINTER;
	state = STATE_RUNNING;
	Remove();
	ES_Thread *t = thread;
	thread = NULL;
	return t->Unblock();
}

/* virtual */ OP_STATUS
JS_Plugin_Context::ThreadState::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED)
	{
		state = STATE_TERMINATED;
		Remove();
	}
	return OpStatus::OK;
}

void
JS_Plugin_Context::Suspend(SuspensionOrigin origin)
{
	OP_ASSERT(origin != SUSPEND_NONE);
	thread_state.Suspend(DOM_Object::GetCurrentThread(GetRuntime()), origin);
}

OP_STATUS
JS_Plugin_Context::Resume()
{
	return thread_state.Resume();
}

BOOL
JS_Plugin_Context::IsSuspended()
{
	return thread_state.state == ThreadState::STATE_SUSPENDED;
}

JS_Plugin_Context::SuspensionOrigin
JS_Plugin_Context::GetSuspensionOrigin()
{
	return thread_state.origin;
}

void
JS_Plugin_Context::ResetSuspensionOrigin()
{
	thread_state.origin = SUSPEND_NONE;
}

/* virtual */ void
JS_Plugin_Context::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_JSPLUGIN_POLL);

	for (JS_Plugin_Poll *poll = static_cast<JS_Plugin_Poll *>(polls.First()); poll; poll = static_cast<JS_Plugin_Poll *>(poll->Suc()))
		if (poll->id == static_cast<unsigned>(par1))
		{
			if (poll->callback(poll->global_context) == 0)
			{
				poll->Out();
				OP_DELETE(poll);
			}
			else
				poll_mh->PostDelayedMessage(MSG_JSPLUGIN_POLL, poll->id, 0, poll->interval);
			return;
		}
}

JS_Plugin_Context::JS_Plugin_Context(JS_Plugin_Manager *manager, ES_Runtime *runtime)
	: manager(manager),
	  poll_mh(NULL),
	  poll_id_counter(0),
	  runtime(runtime)
{
}

JS_Plugin_Context::~JS_Plugin_Context()
{
	polls.Clear();
	if (poll_mh)
	{
		poll_mh->UnsetCallBacks(this);
		OP_DELETE(poll_mh);
	}

	pluginelms.Clear();
	pluginobjs.RemoveAll();
}

OP_STATUS
JS_Plugin_Context::AddPlugin(JS_Plugin_Object *obj, jsplugin_cap *cap
#ifdef JS_PLUGIN_ATVEF_VISUAL
							, jsplugin_cap_ext *cap_ext
#endif
							 )
{
	PluginElm *pe = OP_NEW(PluginElm, ());
	if (!pe)
		return OpStatus::ERR_NO_MEMORY;

	pe->obj = obj->GetRepresentation();
	pe->obj_opera = obj;
	pe->cap = cap;
#ifdef JS_PLUGIN_ATVEF_VISUAL
	pe->cap_ext = cap_ext;
#endif
	pe->Into(&pluginelms);

	return OpStatus::OK;
}

ES_GetState
JS_Plugin_Context::GetName(const uni_char *property_name, ES_Value *value, BOOL *cacheable)
{
	PluginElm *pe = static_cast<PluginElm *>(pluginelms.First());

	while (pe)
	{
		const char* const* names = pe->cap->global_names;

		if (names)
		{
			for (int index = 0; names[index]; ++index)
			{
				if (uni_str_eq(property_name, names[index]))
				{
					if (value)
						return pe->obj_opera->GetName(property_name, value, cacheable, NULL);
					else
						return GET_SUCCESS;
				}
			}
		}
		pe = static_cast<PluginElm *>(pe->Suc());
	}

	return GET_FAILED;
}

ES_PutState
JS_Plugin_Context::PutName(const uni_char* property_name, ES_Value *value)
{
	PluginElm *pe = static_cast<PluginElm *>(pluginelms.First());

	while (pe)
	{
		const char* const* names = pe->cap->global_names;

		if (names)
		{
			for (int index = 0; names[index]; ++index)
			{
				if (uni_str_eq(property_name, names[index]))
				{
					if (value)
						return pe->obj_opera->PutName(property_name, 0, value, GetRuntime());
					else
						return PUT_SUCCESS;
				}
			}
		}
		pe = static_cast<PluginElm *>(pe->Suc());
	}

	return PUT_FAILED;
}

JS_Plugin_Context::PluginElm *
JS_Plugin_Context::GetFirstPlugin()
{
	return static_cast<PluginElm *>(pluginelms.First());
}

BOOL
JS_Plugin_Context::HasObjectHandler(const uni_char *type, JS_Plugin_Object **obj_pp)
{
	PluginElm *pe = static_cast<PluginElm *>(pluginelms.First());

	while (pe)
	{
		const char* const* types = pe->cap->object_types;

		if (types)
		{
			for (int index = 0; types[index]; ++index)
			{
				if (uni_stri_eq(type, types[index]))
				{
					if (obj_pp)
						*obj_pp = pe->obj_opera;
					return TRUE;
				}
			}
		}
		pe = static_cast<PluginElm *>(pe->Suc());
	}

	return FALSE;
}

OP_STATUS
JS_Plugin_Context::HandleObject(const uni_char *type, HTML_Element *element, ES_Runtime *runtime, EcmaScript_Object *global_object)
{
	PluginElm *pe = static_cast<PluginElm *>(pluginelms.First());

	DOM_Object *node = element->GetESElement();

	JS_Plugin_HTMLObjectElement_Object *obj_opera = OP_NEW(JS_Plugin_HTMLObjectElement_Object, (this, element));
	if (!obj_opera)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsMemoryError(obj_opera->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "PluginObject", FALSE)))
	{
		OP_DELETE(obj_opera);
		return OpStatus::ERR_NO_MEMORY;
	}

	HTML_AttrIterator iter(element);

	int attrs_count = iter.Count();
	jsplugin_attr *attrs = OP_NEWA(jsplugin_attr, attrs_count);
	if (!attrs)
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(jsplugin_attr, attrs);

	int index = 0;
	const uni_char *name, *value;

	while (iter.GetNext(name, value))
	{
		if (name)
		{
			attrs[index].name = uni_down_strdup(name);
			attrs[index].value = value ? uni_down_strdup(value) : op_strdup("");

			if (!attrs[index].name || !attrs[index].value)
			{
				while (index-- >= 0)
				{
					op_free(const_cast<char *>(attrs[index].name));
					op_free(const_cast<char *>(attrs[index].value));
				}
				return OpStatus::ERR_NO_MEMORY;
			}

			++index;
		}
	}

	while (pe)
	{
		const char* const* types = pe->cap->object_types;

		if (types)
		{
			for (int index = 0; types[index]; ++index)
			{
				if (uni_stri_eq(type, types[index]))
				{
					jsplugin_getter *g = 0;
					jsplugin_setter *s = 0;
					jsplugin_destructor *d = 0;
					jsplugin_notify *i = 0;
					jsplugin_notify *r = 0;
					jsplugin_gc_trace *t = 0;

					int result = pe->cap->handle_object(pe->obj, obj_opera->GetRepresentation(), attrs_count, attrs, &g, &s, &d, &t, &i, &r);

					for (int idx = 0; idx < attrs_count; ++idx)
					{
						op_free(const_cast<char *>(attrs[idx].name));
						op_free(const_cast<char *>(attrs[idx].value));
					}

					switch (result)
					{
					case JSP_OBJECT_VISUAL:
#ifdef JS_PLUGIN_ATVEF_VISUAL
						obj_opera->SetTvCallbacks(pe->cap_ext->cb_tv_become_visible, pe->cap_ext->cb_tv_position);
#endif
						/* fall through */

					case JSP_OBJECT_OK:
						obj_opera->SetCallbacks(g, s, NULL, NULL, d, t, i, r);
						DOM_Utils::SetJSPluginObject(node, *obj_opera);
						/* fall through */

					case JSP_OBJECT_ERROR:
						return OpStatus::OK;

					case JSP_OBJECT_NOMEM:
						return OpStatus::ERR_NO_MEMORY;

					default:
						/* This means the plugin is broken. */
						OP_ASSERT(FALSE);
						return OpStatus::ERR;
					}
				}
			}
		}
		pe = static_cast<PluginElm *>(pe->Suc());
	}

	/* This function shouldn't be called if no plugin handles the object type. */
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}

OP_STATUS
JS_Plugin_Context::AddPoll(const jsplugin_obj *global_context, unsigned interval, jsplugin_poll_callback *callback)
{
	if (!poll_mh)
	{
		poll_mh = OP_NEW(MessageHandler, (NULL));
		if (!poll_mh || OpStatus::IsMemoryError(poll_mh->SetCallBack(this, MSG_JSPLUGIN_POLL, 0)))
		{
			OP_DELETE(poll_mh);
			poll_mh = NULL;

			return OpStatus::ERR_NO_MEMORY;
		}
	}

	JS_Plugin_Poll *poll = OP_NEW(JS_Plugin_Poll, (global_context));
	if (!poll)
		return OpStatus::ERR_NO_MEMORY;

	poll->id = ++poll_id_counter;
	poll->interval = interval;
	poll->callback = callback;
	poll->Into(&polls);

	poll_mh->PostDelayedMessage(MSG_JSPLUGIN_POLL, poll->id, 0, poll->interval);
	return OpStatus::OK;
}

void
JS_Plugin_Context::BeforeUnload()
{
	for (JS_Plugin_Object *obj = static_cast<JS_Plugin_Object *>(pluginobjs.First()); obj; obj = static_cast<JS_Plugin_Object *>(obj->Suc()))
	{
		obj->BeforeUnload();
	}
}

#endif // JS_PLUGIN_SUPPORT
