/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (c) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef JSPLUGINS_OT_H
#define JSPLUGINS_OT_H

#if defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT)

/******************************************************************************

interface JSP_Selftests_Constructor	// Global constructor for JSP_Selftests
{
	// var obj = new JSP_Selftests();
	JSP_Selftests	JSP_Selftests()
}

interface JSP_Selftests
{
	readonly attribute DOMString			teststring;		// Returns "Opera Rules!"
	readonly attribute JSP_Selftests_child	childnode;		// Readonly instance of JSP_Selftests_child

	Object	ReturnValue();									// Returns value 3 through "return 1+1+1" and JSP_TYPE_EXPRESSION
	Object	Multiply(JSP_Selftests_child obj);				// Returns JSP_Selftests_child.value*2 through JSP_TYPE_EXPRESSION "return obj.value*2"
}

interface JSP_Selftests_child
{
	readonly attribute Number	value;					// Returns 2
}



interface something
{
	attribute DOMString		string;			// GET IMPORT STRING, PUT IMPORT STRING
	function  DOMString		getString();	// CALL IMPORT STRING
	function  Object		eval();			// Return via JSP_TYPE_EXPRESSION
	function  Number		eval(callback, ....)
}



******************************************************************************/

#include "modules/hardcore/timer/optimer.h"

class jsp
{
public:
	// used for private data 
	class data
		: public OpTimerListener
	{
	public:
		data();

		void* test;

		OpTimer       timer;
		unsigned      delayed_value;
		const char*	  delayed_string;
		const jsplugin_obj  *suspended_ref;
		BOOL          get_restart;

		static void EvalCallbackProxy(int status, jsplugin_value *return_value, void *user_data);

	private:
		virtual void OnTimeOut(OpTimer* timer);
		virtual void EvalCallback(int status, jsplugin_value *return_value);
	};

	static void
	gc_trace(jsplugin_obj *this_obj);

	static int
	allow_access(const char *p, const char *h, int port);

	static int
	capabilities(jsplugin_cap **result, jsplugin_callbacks *cbs);

	static int
	global_getname(jsplugin_obj *ctx, const char *name, jsplugin_value *result);

	static int
	instance_destructor(jsplugin_obj *this_obj);

	static int
	instance_getname(jsplugin_obj *this_obj, const char *name, jsplugin_value *result);

	static int
	instance_constructor(jsplugin_obj *global, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static void
	instance_attr_change(jsplugin_obj *this_obj, const char *name, const char *value);

	static void
	instance_param_set(jsplugin_obj *this_obj, const char *name, const char *value);

	static OpString&
	get_latest_param_set_name();

	static OpString&
	get_latest_param_set_value();

	static unsigned int
	get_param_set_called_count();

	static void
	reset_param_set_test_data();

	static int 
	eval(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int 
	return_as_expression(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	get_window_identifier(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	get_delayed_result(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	get_document_title(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);	

	static int
	get_object_host(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	hold(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	gctest(jsplugin_obj *this_obj, jsplugin_obj *function_obj, int argc, jsplugin_value *argv, jsplugin_value *result);

	static int
	handle_object(jsplugin_obj *global_context, jsplugin_obj *object_obj,
				  int attrs_count, jsplugin_attr *attrs,
				  jsplugin_getter **getter, jsplugin_setter **setter,
				  jsplugin_destructor **destructor, jsplugin_gc_trace **trace,
				  jsplugin_notify **inserted, jsplugin_notify **removed);

	static void
	tv_become_visible(jsplugin_obj *object_obj, int visibility, int unloading);

	static void
	tv_position(jsplugin_obj *object_obj, int x, int y, int w, int h);

	static int
	obj_getname(jsplugin_obj *obj, const char *name, jsplugin_value *result);

	static int
	obj_setname(jsplugin_obj *obj, const char *name, jsplugin_value *value);

private:
	static struct _set_param_test_data{
		OpString latest_name;
		OpString latest_value;
		unsigned int called_times;
	} set_param_test_data;
};

#define JSP_SELFTEST_TV_VISUAL_SUPPORTED 		0x01
#define JSP_SELFTEST_HANDLE_OBJECT_CALLED		0x02
#define JSP_SELFTEST_TV_BECOME_VISIBLE_CALLED		0x04
#define JSP_SELFTEST_TV_POSITION_CALLED			0x08
#define JSP_SELFTEST_HANDLED_MIME_TYPE			0x10
#define JSP_SELFTEST_TV_UNLOADED			0x20
#define JSP_SELFTEST_ATTR_CHANGE_SUPPORTED		0x40
#define	JSP_SELFTEST_ATTR_CHANGE_CALLED			0x80
#define	JSP_SELFTEST_ATTR_CHANGE_ARBITRARY_PARAM_OK	0x100
#define	JSP_SELFTEST_ATTR_CHANGE_STANDARD_PARAM_OK	0x200
#define JSP_SELFTEST_ATTR_CHANGE_EMPTY_VALUE_OK		0x400
#define JSP_SELFTEST_SET_STRING_WITH_NULL		0x800
#define JSP_SELFTEST_ATTR_CHANGE_NULL_VALUE_OK     0x1000
#define JSP_SELFTEST_PARAM_SET_SUPPORTED		   0x2000
#define	JSP_SELFTEST_PARAM_SET_CALLED			   0x4000
#define	JSP_SELFTEST_PARAM_SET_NAME_UPDATED 	   0x8000
#define	JSP_SELFTEST_PARAM_SET_VALUE_UPDATED	  0x10000
#define	JSP_SELFTEST_PARAM_SET_VALUE_NULL		  0x20000
#define JSP_SELFTEST_STATIC_BITS				(JSP_SELFTEST_TV_VISUAL_SUPPORTED)
#define g_jsplugins_selftest_bits (g_opera->jsplugins_module.jsp_selftest_bits)

#endif // defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT)
#endif // JSPLUGINS_OT_H
