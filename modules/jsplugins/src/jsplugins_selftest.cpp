/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (c) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT)

#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_manager.h"
#include "modules/jsplugins/src/js_plugin_context.h"
#include "modules/jsplugins/src/js_plugin_object.h"
#include "modules/jsplugins/src/js_plugin_macros.h"
#include "modules/jsplugins/src/jsplugins_selftest.h"
#include "modules/jsplugins/jsplugins_module.h"

void
JspluginsModule::ResetDynamicSelftestBits()
{
	// Rest the dynamic bits (i.e., keep the ones that are only set during
	// the set-up phase).
	jsp_selftest_bits &= JSP_SELFTEST_STATIC_BITS;
}

static int create_method(const jsplugin_obj *this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result);
static jsplugin_cap cap;
static jsplugin_callbacks *opera_callbacks;
static jsplugin_value obj_held;
static Head delayelms;

CONST_ARRAY(jsp_selftests_global_names, char*)
	CONST_ENTRY("JSP_Selftests"),
	CONST_ENTRY(NULL)
CONST_END(jsp_selftests_global_names)

CONST_ARRAY(jsp_selftests_object_types, char*)
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest"),
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest2"),
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest3"),
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest4"),
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest.param.listener"),
	CONST_ENTRY("image/vnd.opera.jsplugins.selftest.unreg.param.listener"),
	CONST_ENTRY("application/vnd.opera.jsplugins.selftest"),
	CONST_ENTRY(NULL)
CONST_END(jsp_selftests_object_types)

jsp::data::data()
	: delayed_value(0)
	, get_restart(FALSE)
{
	suspended_ref = NULL;
	timer.SetTimerListener(this);
}

class jspGcDelayTest
	: public OpTimerListener
	, public Link
{
public:
	jspGcDelayTest(OpTimer *timer, const jsplugin_obj *this_obj, const jsplugin_obj *func)
		: timer(timer)
		, this_obj(this_obj)
		, func(func)
		, destroyed(FALSE)
	{
	}

	virtual
	~jspGcDelayTest()
	{
		OP_DELETE(timer);
		Out();
	}

	virtual void
	OnTimeOut(OpTimer* timer)
	{
		if (!destroyed)
		{
			// SELFTEST CHEAT. THE FOLLOWING IS NOT POSSIBLE IN A REGULAR JSPLUGIN
			EcmaScript_Object *eso = GET_ECMASCRIPT_OBJECT(this_obj);
			eso->GetRuntime()->DebugForceGC();
			//////////////////////////////////////////////////////////////////////
		}

		jsplugin_value value;
		value.type = JSP_TYPE_BOOLEAN;
		value.u.boolean = destroyed;
		opera_callbacks->call_function(this_obj, NULL, func, 1, &value, 0, NULL);

		OP_DELETE(this);
	}

	void
	SetDestroyed() { destroyed = TRUE; }

	void
	gctrace()
	{
		opera_callbacks->gcmark(this_obj);
		opera_callbacks->gcmark(func);
	}

private:
	OpTimer *timer;
	const jsplugin_obj *this_obj;
	const jsplugin_obj *func;
	BOOL destroyed;
};

/* static */ int
jsp::allow_access(const char *p, const char *h, int port)
{
	return 1; // Allow everything
}

/* static */ void
jsp::gc_trace(jsplugin_obj *ctx)
{
	if (obj_held.type == JSP_TYPE_OBJECT || obj_held.type == JSP_TYPE_NATIVE_OBJECT)
		opera_callbacks->gcmark(obj_held.u.object);

	jspGcDelayTest *o = static_cast<jspGcDelayTest *>(delayelms.First());
	while (o)
	{
		o->gctrace();
		o = static_cast<jspGcDelayTest *>(o->Suc());
	}
}

/* static */ int
jsp::capabilities(jsplugin_cap **result, jsplugin_callbacks *cbs)
{
	opera_callbacks = cbs;

	cap.global_names	= jsp_selftests_global_names;
	cap.object_types	= jsp_selftests_object_types;
	cap.global_getter	= jsp::global_getname;
	cap.global_setter	= NULL;
	cap.init			= NULL;
	cap.destroy			= NULL;
	cap.handle_object	= jsp::handle_object;
	cap.allow_access	= jsp::allow_access;
	cap.gc_trace		= jsp::gc_trace;
	*result = &cap;

	/* Try registering TV callbacks (FEATURE_JS_PLUGINS_ATVEF_VISUAL) */
	if (cbs->add_tv_visual &&
		cbs->add_tv_visual(jsp::tv_become_visible, jsp::tv_position) == 0)
	{
		g_jsplugins_selftest_bits |= JSP_SELFTEST_TV_VISUAL_SUPPORTED;
	}

	return 0;
}

/* static */ int
jsp::global_getname(jsplugin_obj *ctx, const char *name, jsplugin_value *result)
{
	if (op_strcmp(name, "JSP_Selftests") == 0)
	{
		jsplugin_obj *thevp;
		int r = opera_callbacks->create_function(
			ctx,						// reference object, used for environment etc 
			NULL, NULL,					// getter/setter, not needed for constructor
			NULL,						// not a regular function call...
			jsp::instance_constructor,	// ... but a constructor
			"",							// signature is empty
			NULL,						// no destructor necessary
			NULL,						// no gc handler needed
			&thevp);					// here we get the function object

		if (r < 0)
		{
			return JSP_GET_ERROR; /* FIXME, too generic */
		}
		else
		{
			result->type = JSP_TYPE_OBJECT;
			result->u.object = thevp;
			return JSP_GET_VALUE_CACHE;
		}
	}
	else
	{
		return JSP_GET_NOTFOUND;
	}
}

/* static */ int
jsp::instance_destructor(jsplugin_obj *this_obj)
{
	data *priv_data = static_cast<data *>(this_obj->plugin_private);
	OP_DELETE(priv_data);
	return JSP_DESTROY_OK;
}

/* static */ int
jsp::instance_getname(jsplugin_obj *this_obj, const char *name, jsplugin_value *result)
{
	if (op_strcmp(name, "eval") == 0)
	{
		return create_method(this_obj, eval, "", result);
	}
	else 
	if (op_strcmp(name, "return_as_expression") == 0)
	{
		return create_method(this_obj, return_as_expression, "", result);
	}
	else
	if (op_strcmp(name, "get_window_identifier") == 0)
	{
		return create_method(this_obj, get_window_identifier, "", result);
	}
	else 
	if (op_strcmp(name, "get_delayed_result") == 0)
	{
		return create_method(this_obj, get_delayed_result, "", result);
	}
	else
	if (op_strcmp(name, "get_document_title") == 0)
	{
		return create_method(this_obj, get_document_title, "", result);
	}
	else
	if (op_strcmp(name, "get_object_host") == 0)
	{
		return create_method(this_obj, get_object_host, "", result);
	}
	else
	if (op_strcmp(name, "hold") == 0)
	{
		return create_method(this_obj, hold, "", result);
	}
	else
	if (op_strcmp(name, "gctest") == 0)
	{
		return create_method(this_obj, gctest, "", result);
	}
	else 
	if (op_strcmp(name, "delayed_property") == 0)
	{
		data *priv_data = static_cast<data *>(this_obj->plugin_private);
		// result->type is the object we previosuly returned when a restart is triggered
		// If an object was not returned with JSP_GET_DELAYED result->type would be
		// JSP_TYPE_NULL on the restart and JSP_TYPE_UNDEFINED otherwise.
		if (result->type == JSP_TYPE_OBJECT)
		{
			priv_data->suspended_ref = NULL;
			result->type = JSP_TYPE_NUMBER;
			result->u.number = priv_data->delayed_value;
			return JSP_GET_VALUE;
		}
		priv_data->suspended_ref = this_obj;
		priv_data->get_restart = TRUE;
		priv_data->timer.Start(100);

		// JSP_GET_DELAYED allows for an object to be returned, it will be sent in the 'result' parameter when restarting the get
		if (opera_callbacks->create_object(this_obj, NULL, NULL, NULL, NULL, &result->u.object) != JSP_CREATE_OK)
			return JSP_GET_ERROR;
		result->type = JSP_TYPE_OBJECT;
		return JSP_GET_DELAYED;
}

	return JSP_GET_NOTFOUND;
}

/* static */ int
jsp::instance_constructor(jsplugin_obj *global, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	jsplugin_obj *theobj;
	int r;
	jsp::data* priv_data;

	if (argc != 0)
	{
		return JSP_CALL_ERROR;
	}

	r = opera_callbacks->create_object(function_obj, jsp::instance_getname, NULL, jsp::instance_destructor, NULL, &theobj);
	if (r < 0)
	{
		return JSP_CALL_NOMEM;
	}
	else
	{
		result->type = JSP_TYPE_OBJECT;
		result->u.object = theobj;
		theobj->plugin_private = NULL;

		// Allocate private data struct 
		priv_data = OP_NEW(data, ());
		if (priv_data == NULL)
		{
			return JSP_CALL_NOMEM;
		}
		theobj->plugin_private = priv_data;

		return JSP_CALL_VALUE;
	}
}

/*static */ void
jsp::instance_attr_change(jsplugin_obj *this_obj, const char *name, const char *value)
{
	g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_CALLED;

	if (0 == op_strcmp(name, "test-name") && 0 == op_strcmp(value,"test-value"))
			g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_ARBITRARY_PARAM_OK;

	if (0 == op_strcmp(name, "data"))
	{
		if (!value)
			g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_NULL_VALUE_OK;
		else if (0 == op_strcmp(value,"http://www.example.com/video.mpg" ))
			g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_STANDARD_PARAM_OK;
		else if (0 == op_strcmp(value,"" ))
			g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_EMPTY_VALUE_OK;
	}
}

/*static*/ struct jsp::_set_param_test_data jsp::set_param_test_data;

/*static */ void
jsp::instance_param_set(jsplugin_obj *this_obj, const char *name, const char *value)
{
	g_jsplugins_selftest_bits |= JSP_SELFTEST_PARAM_SET_CALLED;
	set_param_test_data.called_times++;

	if (set_param_test_data.latest_name.Compare(name) != 0)
		g_jsplugins_selftest_bits |= JSP_SELFTEST_PARAM_SET_NAME_UPDATED;

	if (value && set_param_test_data.latest_value.Compare(value) != 0)
		g_jsplugins_selftest_bits |= JSP_SELFTEST_PARAM_SET_VALUE_UPDATED;

	if (!value)
		g_jsplugins_selftest_bits |= JSP_SELFTEST_PARAM_SET_VALUE_NULL;

	RETURN_VOID_IF_ERROR(set_param_test_data.latest_name.Set(name));
	RETURN_VOID_IF_ERROR(set_param_test_data.latest_value.Set(value));
}

/* static */ OpString&
jsp::get_latest_param_set_name()
{
	return set_param_test_data.latest_name;
}

/* static */ OpString&
jsp::get_latest_param_set_value()
{
	return set_param_test_data.latest_value;
}

/* static */ unsigned int
jsp::get_param_set_called_count()
{
	return set_param_test_data.called_times;
}

/* static */ void
jsp::reset_param_set_test_data()
{
	set_param_test_data.called_times = 0;
	set_param_test_data.latest_name.Empty();
	set_param_test_data.latest_value.Empty();
}

/* static */ int
jsp::eval(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	if (argc < 1)
		return JSP_CALL_ERROR;

	if (argv[0].type != JSP_TYPE_OBJECT)
		return JSP_CALL_ERROR;

	// Change argument type from string to expression to get them evaluated.
	for (int i=1; i < argc; i++)
	{
		OP_ASSERT(argv[i].type == JSP_TYPE_STRING);
		argv[i].type = JSP_TYPE_EXPRESSION;
	}

	opera_callbacks->call_function(this_obj, NULL, argv[0].u.object, argc-1, &argv[1], 0, NULL);

	return JSP_CALL_NO_VALUE;
}

/* static */ int 
jsp::return_as_expression(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	if (argc < 1)
		return JSP_CALL_ERROR;

	if (argv[0].type != JSP_TYPE_STRING)
		return JSP_CALL_ERROR;

	*result = argv[0];
	result->type = JSP_TYPE_EXPRESSION;

	return JSP_CALL_VALUE;
}

/* static */ int
jsp::get_window_identifier(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	long int window_identifier;

	if (opera_callbacks->get_window_identifier(this_obj, &window_identifier) == JSP_CALLBACK_OK)
	{
		result->type = JSP_TYPE_NUMBER;
		result->u.number = window_identifier;
		return JSP_CALL_VALUE;
	}

	return JSP_CALL_ERROR;
}

/* static */ int
jsp::get_delayed_result(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	data *priv_data = static_cast<data *>(this_obj->plugin_private);
	if (argc < 0)
	{
		// Check that the object we returned in JSP_CALL_DELAYED is sent in 'result'
		OP_ASSERT(result->type == JSP_TYPE_OBJECT);
		result->type = JSP_TYPE_NUMBER;
		result->u.number = priv_data->delayed_value;
		return JSP_CALL_VALUE;
	}

	OP_ASSERT(priv_data->suspended_ref == NULL);
	priv_data->suspended_ref = this_obj;
	priv_data->timer.Start(100);

	// JSP_CALL_DELAYED allows for an object to be returned, it will be sent in the 'result' parameter when restarting the call
	if (opera_callbacks->create_object(this_obj, NULL, NULL, NULL, NULL, &result->u.object) != JSP_CREATE_OK)
		return JSP_GET_ERROR;
	result->type = JSP_TYPE_OBJECT;
	return JSP_CALL_DELAYED;
}

/* static */ int
jsp::get_document_title(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	// this function waits for jsplugin_callbacks::eval inside JSP_CALL_DELAYED
   // therefore TWEAK_JSPLUGINS_SEPARATED_SCHEDULER is required for this proper functioning
	// otherwise, the ecma script thread will hung and never return

	data *priv_data = static_cast<data *>(this_obj->plugin_private);
	if (argc < 0)
	{
		// Check that the object we returned in JSP_CALL_DELAYED is sent in 'result'
		result->type = JSP_TYPE_STRING;
		result->u.string = priv_data->delayed_string;
		return JSP_CALL_VALUE;
	}

	OP_ASSERT(priv_data->suspended_ref == NULL);
	priv_data->suspended_ref = this_obj;

	opera_callbacks->eval(this_obj, "document.title", priv_data, data::EvalCallbackProxy);

	return JSP_CALL_DELAYED;
}

/* static */ int
jsp::get_object_host(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	char *output = NULL;
	if (opera_callbacks->get_object_host(this_obj, &output) == JSP_CALLBACK_OK)
	{
		result->type = JSP_TYPE_STRING;
		result->u.string = output;
		return JSP_CALL_VALUE;
	}

	return JSP_CALL_ERROR;
}

/* static */ int
jsp::hold(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	if (argc == 1)
		obj_held = argv[0];
	else
		obj_held.type = VALUE_UNDEFINED;

	return JSP_CALL_NO_VALUE;
}

int
gctest_destructor(jsplugin_obj *obj)
{
	jspGcDelayTest *l = static_cast<jspGcDelayTest*>(obj->plugin_private);
	jspGcDelayTest *elm = static_cast<jspGcDelayTest*>(delayelms.First());
	while (elm)
	{
		if (elm == l)
			l->SetDestroyed();	// It was still alive, mark as dead

		elm = static_cast<jspGcDelayTest*>(elm->Suc());
	}
	return JSP_CALL_NO_VALUE;
}


/* static */ int
jsp::gctest(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result)
{
	OP_ASSERT(argc == 1);
	jsplugin_obj *func = argv[0].u.object;

	if (opera_callbacks->create_object(this_obj, NULL, NULL, gctest_destructor, NULL, &result->u.object) != JSP_CREATE_OK)
		return JSP_GET_ERROR;

	OpTimer *timer = OP_NEW(OpTimer, ());
	jspGcDelayTest *listener = OP_NEW(jspGcDelayTest, (timer, this_obj, func));
	if (timer)
	{
		timer->SetTimerListener(listener);
		timer->Start(100);
	}
	if (!listener)
	{
		OP_DELETE(timer);
		return JSP_CALL_NOMEM;
	}

	listener->Into(&delayelms);

	result->type = JSP_TYPE_OBJECT;
	result->u.object->plugin_private = listener;

	return JSP_GET_VALUE;
}

/* static */ int
jsp::handle_object(jsplugin_obj *global_context, jsplugin_obj *object_obj,
				   int attrs_count, jsplugin_attr *attrs,
				   jsplugin_getter **getter, jsplugin_setter **setter,
				   jsplugin_destructor **destructor, jsplugin_gc_trace **trace,
				   jsplugin_notify **inserted, jsplugin_notify **removed)
{
	g_jsplugins_selftest_bits |= JSP_SELFTEST_HANDLE_OBJECT_CALLED;

	// Check type attribute
	for (int i = 0; i < attrs_count; ++ i)
	{
		if (attrs[i].name && 0 == op_stricmp(attrs[i].name, "type"))
		{
			if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest"))
			{
				g_jsplugins_selftest_bits |= JSP_SELFTEST_HANDLED_MIME_TYPE;
				return JSP_OBJECT_VISUAL;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest2"))
			{
				g_jsplugins_selftest_bits |= JSP_SELFTEST_HANDLED_MIME_TYPE;
				return JSP_OBJECT_OK;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest3"))
			{
				if (opera_callbacks->set_attr_change_listener && JSP_CALLBACK_OK == opera_callbacks->set_attr_change_listener(object_obj, instance_attr_change))
					g_jsplugins_selftest_bits |= JSP_SELFTEST_ATTR_CHANGE_SUPPORTED;
				return JSP_OBJECT_OK;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest4"))
			{
				opera_callbacks->set_attr_change_listener(object_obj, instance_attr_change);
				opera_callbacks->set_attr_change_listener(object_obj, NULL);
				return JSP_OBJECT_OK;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest.param.listener"))
			{
				if (opera_callbacks->set_param_set_listener &&
					JSP_CALLBACK_OK == opera_callbacks->set_param_set_listener(object_obj, instance_param_set))
					g_jsplugins_selftest_bits |= JSP_SELFTEST_PARAM_SET_SUPPORTED;
				return JSP_OBJECT_OK;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "image/vnd.opera.jsplugins.selftest.unreg.param.listener"))
			{
				opera_callbacks->set_param_set_listener(object_obj, instance_param_set);
				opera_callbacks->set_param_set_listener(object_obj, NULL);
				return JSP_OBJECT_OK;
			}
			else if (attrs[i].value && 0 == op_stricmp(attrs[i].value, "application/vnd.opera.jsplugins.selftest"))
			{
				g_jsplugins_selftest_bits |= JSP_SELFTEST_HANDLED_MIME_TYPE;
				*getter = obj_getname;
				*setter = obj_setname;
				return JSP_OBJECT_OK;
			}
		}
	}

	// MIME type not passed
	return JSP_OBJECT_OK;
}

/* static */ void
jsp::tv_become_visible(jsplugin_obj *object_obj, int visibility, int unloading)
{
	if (visibility)
	{
		g_jsplugins_selftest_bits |= JSP_SELFTEST_TV_BECOME_VISIBLE_CALLED;
	}
	if (unloading)
	{
		g_jsplugins_selftest_bits |= JSP_SELFTEST_TV_UNLOADED;
	}
}

/* static */ void
jsp::tv_position(jsplugin_obj *object_obj, int x, int y, int w, int h)
{
	if (w && h)
	{
		g_jsplugins_selftest_bits |= JSP_SELFTEST_TV_POSITION_CALLED;
	}
}

static int 
create_method(const jsplugin_obj *this_obj, jsplugin_function *method, const char *sign, jsplugin_value *result)
{
	int r;

	jsplugin_obj *thefunction;
	r = opera_callbacks->create_function(this_obj, 
										 jsp::instance_getname, NULL,
										 method, method, sign,
										 NULL,
										 NULL,
										 &thefunction);
	if (r < 0)
	{
		return JSP_GET_ERROR;  /* FIXME; too generic */
	}
	else
	{
		result->type = JSP_TYPE_OBJECT;
		result->u.object = thefunction;
		return JSP_GET_VALUE_CACHE;
	}
}

/* static */ int
jsp::obj_getname(jsplugin_obj *obj, const char *name, jsplugin_value *result)
{
	if (op_strcmp(name, "xml") == 0)
	{
		result->u.string = "new DOMParser().parseFromString(\" <test /> \", 'text/xml')";
		result->type = JSP_TYPE_EXPRESSION;
		return JSP_GET_VALUE;
	}
	else if(op_strcmp(name, "string_with_null") == 0)
	{
		result->u.s.string = "foo\0bar";
		result->u.s.len = 7;
		result->type = JSP_TYPE_STRING_WITH_LENGTH;
		return JSP_GET_VALUE;
	}

	return JSP_GET_NOTFOUND;
}

/* static */ int
jsp::obj_setname(jsplugin_obj *obj, const char *name, jsplugin_value *value)
{
	if(op_strcmp(name, "string_with_null") == 0)
	{
		if (value->type == JSP_TYPE_STRING &&
		    value->u.s.len == 3 &&
		    value->u.s.string[0] == 'x' &&
		    value->u.s.string[1] == 0 &&
		    value->u.s.string[2] == 'y' &&
		    value->u.s.string[3] == 0 )
		g_jsplugins_selftest_bits |= JSP_SELFTEST_SET_STRING_WITH_NULL;
		return JSP_PUT_OK;
	}
	return JSP_PUT_ERROR;
}

/*virtual*/ void
jsp::data::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(suspended_ref != NULL);
	if (suspended_ref == NULL)
		return;
	++delayed_value;
	const jsplugin_obj *ref = suspended_ref;
	suspended_ref = NULL;
	opera_callbacks->resume(ref);
}

/*static*/ void
jsp::data::EvalCallbackProxy(int status, jsplugin_value *return_value, void *user_data)
{
	if (user_data)
	  (reinterpret_cast<jsp::data*>(user_data))->EvalCallback(status, return_value);
}

/*virtual*/ void
jsp::data::EvalCallback(int status, jsplugin_value *return_value)
{
	OP_ASSERT(suspended_ref != NULL);
	if (suspended_ref == NULL)
		return;

	OP_ASSERT(status == JSP_CALLBACK_OK);
	OP_ASSERT(return_value->type == JSP_TYPE_STRING);

	if (status != JSP_CALLBACK_OK || return_value->type != JSP_TYPE_STRING)
		return;

	delayed_string = op_strdup(return_value->u.string);
	const jsplugin_obj *ref = suspended_ref;
	suspended_ref = NULL;
	opera_callbacks->resume(ref);
}

#endif // defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT)
