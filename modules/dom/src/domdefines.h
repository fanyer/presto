/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMDEFINES_H
#define DOM_DOMDEFINES_H

#include "modules/dom/src/dominternaltypes.h"


/* Macros for handling the this_object. */

/** Retrieves the DOM part of the this object and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it throws an InternalException (WRONG_THIS_ERR). */
#define DOM_THIS_OBJECT_EXISTING(VARIABLE, TYPE, CLASS) \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::WRONG_THIS_ERR); \
		if (this_object_result != ES_VALUE) \
			return this_object_result; \
		VARIABLE = (CLASS *) this_object; \
	} while (0)

/** Retrieves the DOM part of the this object and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it sets VARIABLE to NULL. */
#define DOM_THIS_OBJECT_EXISTING_OPTIONAL(VARIABLE, TYPE, CLASS) \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::INTERNALEXCEPTION_NO_ERR); \
		if (this_object_result != ES_VALUE) \
			VARIABLE = NULL; \
		else \
			VARIABLE = (CLASS *) this_object; \
	} while (0)

/** If VARIABLE is NULL, retrieves the DOM part of the this
    object and checks that if it is the right type of DOM object.
	If it is, set VARIABLE to it; otherwise leave VARIABLE as NULL.
	Used when chaining together a sequence object type tests */
#define DOM_THIS_OBJECT_EXISTING_OR(VARIABLE, TYPE, CLASS) \
	if (VARIABLE == NULL) \
	{ \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::INTERNALEXCEPTION_NO_ERR); \
		if (this_object_result == ES_VALUE) \
			VARIABLE = this_object; \
	}

/** Retrieves the DOM part of the this object and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it throws an InternalException (WRONG_THIS_ERR). */
#define DOM_THIS_OBJECT(VARIABLE, TYPE, CLASS) \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::WRONG_THIS_ERR); \
		if (this_object_result != ES_VALUE) \
			return this_object_result; \
	} while (0); \
	CLASS *VARIABLE = (CLASS *) this_object

/** Retrieves the DOM part of the this object and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it sets VARIABLE to NULL. */
#define DOM_THIS_OBJECT_OPTIONAL(VARIABLE, TYPE, CLASS) \
	CLASS *VARIABLE = NULL; \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::INTERNALEXCEPTION_NO_ERR); \
		if (this_object_result == ES_VALUE) \
			VARIABLE = (CLASS *) this_object; \
	} while (0) \

#define DOM_THIS_OBJECT_CHECK(VALID) \
	do { \
		if (!(VALID)) \
			return DOM_CheckType((DOM_Runtime *) origining_runtime, NULL, DOM_TYPE_OBJECT, return_value, DOM_Object::WRONG_THIS_ERR); \
	} while (0)

/** Retrieves the DOM part of the this object and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED. */
#define DOM_THIS_OBJECT_UNUSED(TYPE) \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE, return_value, DOM_Object::WRONG_THIS_ERR); \
		if (this_object_result != ES_VALUE) \
			return this_object_result; \
	} while (0)
#define DOM_THIS_OBJECT_UNUSED_2(TYPE1, TYPE2) \
	do { \
		int this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE1, return_value, DOM_Object::WRONG_THIS_ERR); \
		if (this_object_result != ES_VALUE) \
		{ \
			this_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, this_object, TYPE2, return_value, DOM_Object::WRONG_THIS_ERR); \
			if (this_object_result != ES_VALUE) \
				return this_object_result; \
		} \
	} while (0)

#define DOM_ALLOCATE(var, cls, ctorargs, proto, name, rt) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) return OpStatus::ERR_NO_MEMORY; \
		RETURN_IF_ERROR(rt->AllocateHostObject(obj, mem, sizeof(cls), proto, name)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)

#define DOM_ALLOCATE_L(var, cls, ctorargs, proto, name, rt) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) LEAVE(OpStatus::ERR_NO_MEMORY); \
		LEAVE_IF_ERROR(rt->AllocateHostObject(obj, mem, sizeof(cls), proto, name)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)


#define DOM_ALLOCATE_FLAGS(var, cls, ctorargs, proto, name, rt, flags)	\
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) return OpStatus::ERR_NO_MEMORY; \
		RETURN_IF_ERROR(rt->AllocateHostObject(obj, mem, sizeof(cls), proto, name, flags)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)

#define DOM_ALLOCATE_FLAGS_L(var, cls, ctorargs, proto, name, rt, flags) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) LEAVE(OpStatus::ERR_NO_MEMORY); \
		LEAVE_IF_ERROR(rt->AllocateHostObject(obj, mem, sizeof(cls), proto, name, flags)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)


#define DOM_ALLOCATE_FUNCTION(var, cls, ctorargs, proto, fnname, clsname, params, rt) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) return OpStatus::ERR_NO_MEMORY; \
		RETURN_IF_ERROR(rt->AllocateHostFunction(obj, mem, sizeof(cls), proto, fnname, clsname, params)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)

#define DOM_ALLOCATE_FUNCTION_L(var, cls, ctorargs, proto, fnname, clsname, params, rt) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) LEAVE(OpStatus::ERR_NO_MEMORY); \
		LEAVE_IF_ERROR(rt->AllocateHostFunction(obj, mem, sizeof(cls), proto, fnname, clsname, params)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)


#define DOM_ALLOCATE_FUNCTION_FLAGS(var, cls, ctorargs, proto, fnname, clsname, params, rt, flags) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) return OpStatus::ERR_NO_MEMORY; \
		RETURN_IF_ERROR(rt->AllocateHostFunction(obj, mem, sizeof(cls), proto, fnname, clsname, params, flags)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)

#define DOM_ALLOCATE_FUNCTION_FLAGS_L(var, cls, ctorargs, proto, fnname, clsname, params, rt, flags) \
	do { \
		ES_Object *obj; \
		void *mem; \
		if (!proto) LEAVE(OpStatus::ERR_NO_MEMORY); \
		LEAVE_IF_ERROR(rt->AllocateHostFunction(obj, mem, sizeof(cls), proto, fnname, clsname, params, flags)); \
		var = new (mem) cls ctorargs; \
		var->ConnectObject(rt, obj); \
	} while (0)


/* Macros for handling arguments. */

/** Checks that argc >= strlen(FORMAT) and that the argument types match
    'FORMAT'.  The format is like the 'arguments' parameter to
    SetFunctionRuntime except for some minor differences.  If there are
    too few arguments or any argument doesn't pass the type check, an
    exception is thrown.

    The supported format characters and acceptable argument types are:

      b: VALUE_BOOLEAN
      n: VALUE_NUMBER (any number except NaN and {+,-}Inf)
      N: VALUE_NUMBER (any number)
      s: VALUE_STRING
      S: VALUE_STRING, VALUE_NULL and VALUE_UNDEFINED
      z: VALUE_STRING_WITH_LENGTH
      Z: VALUE_STRING_WITH_LENGTH, VALUE_NULL and VALUE_UNDEFINED
      o: VALUE_OBJECT
      O: VALUE_OBJECT, VALUE_NULL and VALUE_UNDEFINED
      f: VALUE_OBJECT and of type 'Function'
      F: VALUE_OBJECT and of type 'Function', VALUE_NULL and VALUE_UNDEFINED
      -: any
      |: any arguments after a vertical bar are optional
  */
#define DOM_CHECK_ARGUMENTS(FORMAT) \
	do { \
		int check_format_result = DOM_CheckFormat((DOM_Runtime *) origining_runtime, FORMAT, argc, argv, return_value); \
		if (check_format_result != ES_VALUE) \
			return check_format_result; \
	} while (0)

#ifdef DOM_JIL_API_SUPPORT
#define DOM_CHECK_ARGUMENTS_JIL(FORMAT) \
	do { \
		int check_format_result = JIL_CheckFormat((DOM_Runtime *) origining_runtime, FORMAT, argc, argv, return_value); \
		if (check_format_result != ES_VALUE) \
			return check_format_result; \
	} while (0)
#endif

/** Checks that argc >= strlen(FORMAT) and that the argument types match
    'FORMAT'.  The format is like the 'arguments' parameter to
    SetFunctionRuntime except for some minor differences.  If there are
    too few arguments or any argument doesn't pass the type check, the
    function returns silently.

    The supported format characters and acceptable argument types are:

      b: VALUE_BOOLEAN
      n: VALUE_NUMBER (any number except NaN and {+,-}Inf)
      N: VALUE_NUMBER (any number)
      s: VALUE_STRING
      S: VALUE_STRING, VALUE_NULL and VALUE_UNDEFINED
      z: VALUE_STRING_WITH_LENGTH
      Z: VALUE_STRING_WITH_LENGTH, VALUE_NULL and VALUE_UNDEFINED
      o: VALUE_OBJECT
      O: VALUE_OBJECT, VALUE_NULL and VALUE_UNDEFINED
      f: VALUE_OBJECT and of type 'Function'
      F: VALUE_OBJECT and of type 'Function', VALUE_NULL and VALUE_UNDEFINED
      -: any
      |: any arguments after a vertical bar are optional
  */
#define DOM_CHECK_ARGUMENTS_SILENT(FORMAT) \
	do { \
		int check_format_result = DOM_CheckFormatNoException(FORMAT, argc, argv); \
		if (check_format_result != ES_VALUE) \
			return ES_FAILED; \
	} while (0)


/** Retrieves the DOM part of an object argument and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED. */
#define DOM_ARGUMENT_OBJECT_EXISTING(VARIABLE, INDEX, TYPE, CLASS) \
	do { \
		OP_ASSERT(argc > INDEX); \
		OP_ASSERT(argv[INDEX].type == VALUE_OBJECT || argv[INDEX].type == VALUE_NULL || argv[INDEX].type == VALUE_UNDEFINED); \
		if (argv[INDEX].type == VALUE_OBJECT) { \
			VARIABLE = DOM_VALUE2OBJECT(argv[INDEX], CLASS); \
			int argument_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, VARIABLE, TYPE, return_value, DOM_Object::WRONG_ARGUMENTS_ERR); \
			if (argument_object_result != ES_VALUE) \
				return argument_object_result; \
		} else \
			VARIABLE = NULL; \
	} while (0)


/** Retrieves the DOM part of an object argument and checks if it is
    the right type of DOM object.  If it is, sets VARIABLE to that DOM
	object. In all other cases, VARIABLE is set to NULL. */
#define DOM_ARGUMENT_OBJECT_EXISTING_OPTIONAL(VARIABLE, INDEX, TYPE, CLASS) \
	do { \
		OP_ASSERT(argc > INDEX); \
		OP_ASSERT(argv[INDEX].type == VALUE_OBJECT || argv[INDEX].type == VALUE_NULL || argv[INDEX].type == VALUE_UNDEFINED); \
		if (argv[INDEX].type == VALUE_OBJECT) { \
			VARIABLE = DOM_VALUE2OBJECT(argv[INDEX], CLASS); \
			int argument_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, VARIABLE, TYPE, return_value, DOM_Object::INTERNALEXCEPTION_NO_ERR); \
		    OP_ASSERT(argument_object_result == ES_VALUE || argument_object_result == ES_FAILED); \
			if (argument_object_result == ES_FAILED) \
				VARIABLE = NULL; \
		} else \
			VARIABLE = NULL; \
	} while (0)


/** Retrieves the DOM part of an object argument and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED. */
#define DOM_ARGUMENT_OBJECT(VARIABLE, INDEX, TYPE, CLASS) \
	CLASS *VARIABLE; \
	OP_ASSERT(argc > INDEX); \
	OP_ASSERT(argv[INDEX].type == VALUE_OBJECT || argv[INDEX].type == VALUE_NULL || argv[INDEX].type == VALUE_UNDEFINED); \
	if (argv[INDEX].type == VALUE_OBJECT) { \
		VARIABLE = DOM_VALUE2OBJECT(argv[INDEX], CLASS); \
		int argument_object_result = DOM_CheckType((DOM_Runtime *) origining_runtime, VARIABLE, TYPE, return_value, DOM_Object::WRONG_ARGUMENTS_ERR); \
		if (argument_object_result != ES_VALUE) \
			return argument_object_result; \
	} else \
		VARIABLE = NULL; \


/** Retrieves the DOM part of an object argument and checks that it is
    the right type of DOM object.  If it isn't, or if the object is a
    native object, it returns ES_FAILED.
	Throws a JIL exception in case the type is wrong.
	*/
#define DOM_ARGUMENT_JIL_OBJECT(VARIABLE, INDEX, TYPE, CLASS) \
	CLASS *VARIABLE; \
	OP_ASSERT(argc > INDEX); \
	OP_ASSERT(argv[INDEX].type == VALUE_OBJECT || argv[INDEX].type == VALUE_NULL || argv[INDEX].type == VALUE_UNDEFINED); \
	if (argv[INDEX].type == VALUE_OBJECT) \
	{ \
		VARIABLE = DOM_VALUE2OBJECT(argv[INDEX], CLASS); \
		int argument_object_result = JIL_CheckType((DOM_Runtime *) origining_runtime, VARIABLE, TYPE, return_value); \
		if (argument_object_result != ES_VALUE) \
			return argument_object_result; \
	} else \
		VARIABLE = NULL; \


/* Warning: this is for convenience only; it does not verify that
   the conversions it does are safe or sane. */
#define DOM_VALUE2OBJECT(v, t) (DOM_HOSTOBJECT((v).value.object, t))

#define DOM_HOSTOBJECT_SAFE(name, obj, type, cls) \
	cls *name = NULL; \
	do { \
		if (obj) \
		{ \
			DOM_Object *obj0 = DOM_HOSTOBJECT(obj, DOM_Object); \
			if (obj0 && obj0->IsA(type)) \
				name = (cls *) obj0; \
			else \
				name = NULL; \
		} \
	} while (0)

/* Macros for throwing exceptions from DOM functions (Call/Construct, GetName
   and PutName, respectively).  The result of these macros should be returned
   from the function. */
#define DOM_CALL_DOMEXCEPTION(code) (this_object->CallDOMException(DOM_Object::code, return_value))
#define DOM_CALL_EVENTEXCEPTION(code) (this_object->CallEventException(DOM_Object::code, return_value))
#define DOM_CALL_XPATHEXCEPTION(code) (this_object->CallXPathException(DOM_Object::code, return_value))
#define DOM_CALL_INTERNALEXCEPTION(code) (this_object->CallInternalException(DOM_Object::code, return_value))
#define DOM_CALL_SVGEXCEPTION(code) (this_object->CallSVGException(DOM_Object::code, return_value))
#define DOM_CALL_FILEAPIEXCEPTION(code) (this_object->CallFileAPIException(DOM_Object::FILEAPI_##code, return_value))
#define DOM_CALL_WEBSERVEREXCEPTION(code) (this_object->CallWebServerException(DOM_Object::WEBSERVER_##code, return_value))
#define DOM_CALL_NATIVEEXCEPTION(type, msg) (this_object->CallNativeException(type, msg, return_value))
#define DOM_GETNAME_DOMEXCEPTION(code) (GetNameDOMException(DOM_Object::code, value))
#define DOM_GETNAME_XPATHEXCEPTION(code) (GetNameXPathException(DOM_Object::code, value))
#define DOM_PUTNAME_DOMEXCEPTION(code) (PutNameDOMException(DOM_Object::code, value))

#define DOM_HOSTOBJECT(o, T) (static_cast<T*>(DOM_GetHostObject(o)))


/* Macros for dealing with platforms that can have no "complex" constant
   global data. */
#ifdef DOM_NO_COMPLEX_GLOBALS
# ifndef DOM_ACCESS_SECURITY_RULES
#  define DOM_FUNCTIONS_START(cls) void cls##_InitFunctions(DOM_GlobalData *global_data) { DOM_FunctionDesc *descs = global_data->cls##_functions;
#  define DOM_FUNCTIONS_FUNCTION(cls, impl_, name_, args) descs->impl = impl_; descs->name = name_; descs->arguments = args; ++descs;
# else // DOM_ACCESS_SECURITY_RULES
#  define DOM_FUNCTIONS_START(cls) void cls##_InitFunctions(DOM_GlobalData *global_data) { DOM_FunctionDesc *descs = global_data->cls##_functions; DOM_FunctionSecurityRuleDesc secrule; (void)secrule;
#  define DOM_FUNCTIONS_FUNCTION(cls, impl_, name_, args) descs->impl = impl_; descs->name = name_; descs->arguments = args; secrule.rule_name = NULL; secrule.arg_1_index = -1; secrule.arg_2_index = -1; descs->security_rule = secrule; ++descs;
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE(cls, impl_, name_, args, security_rule_name) descs->impl = impl_; descs->name = name_; descs->arguments = args; secrule.rule_name = security_rule_name; secrule.arg_1_index = -1; secrule.arg_2_index = -1; descs->security_rule = secrule; ++descs;
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(cls, impl_, name_, args, security_rule_name, arg1idx) descs->impl = impl_; descs->name = name_; descs->arguments = args; secrule.rule_name = security_rule_name; secrule.arg_1_index = arg1idx; secrule.arg_2_index = -1; descs->security_rule = secrule; ++descs;
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE_A2(cls, impl_, name_, args, security_rule_name, arg1idx, arg2idx) descs->impl = impl_; descs->name = name_; descs->arguments = args; secrule.rule_name = security_rule_name; secrule.arg_1_index = arg1idx; secrule.arg_2_index = -1; descs->security_rule = secrule; ++descs;
# endif // DOM_ACCESS_SECURITY_RULES
# define DOM_FUNCTIONS_END(cls) descs->impl = 0; }
# define DOM_FUNCTIONS_WITH_DATA_START(cls) void cls##_InitFunctionsWithData(DOM_GlobalData *global_data) { DOM_FunctionWithDataDesc *descs = global_data->cls##_functions_with_data;
# define DOM_FUNCTIONS_WITH_DATA_FUNCTION(cls, impl_, data_, name_, args) descs->impl = impl_; descs->data = data_; descs->name = name_; descs->arguments = args; ++descs;
# define DOM_FUNCTIONS_WITH_DATA_END(cls) descs->impl = 0; }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_FUNCTIONS_START(cls) const DOM_FunctionDesc cls##_functions[cls::FUNCTIONS_ARRAY_SIZE] = {
# ifndef DOM_ACCESS_SECURITY_RULES
#  define DOM_FUNCTIONS_FUNCTION(cls, impl, name, args) { impl, name, args },
#  define DOM_FUNCTIONS_END(cls) { 0, 0, 0 } };
# else // DOM_ACCESS_SECURITY_RULES
#  define DOM_FUNCTIONS_FUNCTION(cls, impl, name, args) { impl, name, args, { NULL, -1, -1} },
/** Declares a function with a security rule attached.
 *
 * The security check is performed by security_manager with DOM_INVOKE_FUNCTION operation code.
 */
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE(cls, impl, name, args, security_rule) { impl, name, args, { security_rule, -1, -1} },
/** Declares a function with a security rule attached.
 *
 * Same as DOM_FUNCTIONS_WITH_SECURITY_RULE but with a string
 * argument (typically a function argument, like name of file to be copied)
 * that may be presented to the user in a security dialog.
 */
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(cls, impl, name, args, security_rule, arg1idx) { impl, name, args, { security_rule, arg1idx, -1} },
/** Declares a function with a security rule attached.
 *
 * Same as DOM_FUNCTIONS_WITH_SECURITY_RULE but with two string
 * argument that may be presented to the user in a security dialog.
 */
#  define DOM_FUNCTIONS_WITH_SECURITY_RULE_A2(cls, impl, name, args, security_rule, arg1idx, arg2idx) { impl, name, args, { security_rule, arg1idx, arg2idx} },

#  define DOM_FUNCTIONS_END(cls) { 0, 0, 0, { NULL, -1, -1} } };
# endif // DOM_ACCESS_SECURITY_RULES
# define DOM_FUNCTIONS_WITH_DATA_START(cls) const DOM_FunctionWithDataDesc cls##_functions_with_data[cls::FUNCTIONS_WITH_DATA_ARRAY_SIZE] = {
# define DOM_FUNCTIONS_WITH_DATA_FUNCTION(cls, impl, data, name, args) { impl, data, name, args },
# define DOM_FUNCTIONS_WITH_DATA_END(cls) { 0, 0, 0, 0 } };
#endif // DOM_NO_COMPLEX_GLOBALS


/* Macros for dealing with library functions (DOM functions implemented
   in ECMAScript.) */
#ifdef DOM_LIBRARY_FUNCTIONS
# define DOM_FUNCTIONS_FUNCTIONX(cls, impl_, name_, args)
# define DOM_FUNCTIONS_WITH_DATA_FUNCTIONX(cls, impl, data, name, args)
# define DOM_LIBRARY_FUNCTIONS_START(cls) const DOM_LibraryFunctionDesc cls##_library_functions[cls::LIBRARY_FUNCTIONS_ARRAY_SIZE] = {
# define DOM_LIBRARY_FUNCTIONS_FUNCTION(cls, impl, name) { &impl, name },
# define DOM_LIBRARY_FUNCTIONS_END(cls) { 0, 0 } };
#else // DOM_LIBRARY_FUNCTIONS
# define DOM_FUNCTIONS_FUNCTIONX(cls, impl, name, args) DOM_FUNCTIONS_FUNCTION(cls, impl, name, args)
# define DOM_FUNCTIONS_WITH_DATA_FUNCTIONX(cls, impl, data, name, args) DOM_FUNCTIONS_WITH_DATA_FUNCTION(cls, impl, data, name, args)
# define DOM_LIBRARY_FUNCTIONS_START(cls)
# define DOM_LIBRARY_FUNCTIONS_FUNCTION(cls, impl, name)
# define DOM_LIBRARY_FUNCTIONS_END(cls)
#endif // DOM_LIBRARY_FUNCTIONS

#endif // DOM_DOMDEFINES_H
