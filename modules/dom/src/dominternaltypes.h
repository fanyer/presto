/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMINTERNALTYPES_H
#define DOM_DOMINTERNALTYPES_H

enum DOM_RestartObjectTag
{
	DOM_RESTART_JSWCCALLBACK = 1,
	DOM_RESTART_STORAGE_OPERATION,
	DOM_RESTART_OBJECT_FETCHPROPERTIES
};

class DOM_Function;
class DOM_Object;
class DOM_Runtime;
class ES_Value;

#define DOM_DECLARE_FUNCTION(name) \
	static int name(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
#define DOM_DECLARE_FUNCTION_WITH_DATA(name) \
	static int name(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)

typedef int DOM_FunctionImpl(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
typedef int DOM_FunctionWithDataImpl(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data);

class DOM_FunctionSecurityRuleDesc
{
public:
	const char *rule_name;
	int arg_1_index;
	int arg_2_index;
};

/* Enables support for security checks on access to particular properties
 * and function calls.
 * Enables DOM_FUNCTIONS_WITH_SECURITY_RULE* macros. */
#ifdef DOM_JIL_API_SUPPORT
# define DOM_ACCESS_SECURITY_RULES
#endif

class DOM_FunctionDesc
{
public:
	DOM_FunctionImpl *impl;
	const char *name, *arguments;
#ifdef DOM_ACCESS_SECURITY_RULES
	DOM_FunctionSecurityRuleDesc security_rule;
#endif // DOM_ACCESS_SECURITY_RULES
};

class DOM_FunctionWithDataDesc
{
public:
	DOM_FunctionWithDataImpl *impl;
	int data;
	const char *name, *arguments;
};

#ifdef DOM_LIBRARY_FUNCTIONS

class DOM_LibraryFunction;

class DOM_LibraryFunctionDesc
{
public:
	const DOM_LibraryFunction *impl;
	const char *name;
};

#endif // DOM_LIBRARY_FUNCTIONS

class DOM_PrototypeDesc
{
public:
	const DOM_FunctionDesc *functions;
	const DOM_FunctionWithDataDesc *functions_with_data;
#ifdef DOM_LIBRARY_FUNCTIONS
	const DOM_LibraryFunctionDesc *library_functions;
#endif // DOM_LIBRARY_FUNCTIONS
	int prototype;
};


#ifndef HAS_COMPLEX_GLOBALS
/* No constant "complex" global data allowed.  "Complex" data include at least
   all types of pointers. */
# define DOM_NO_COMPLEX_GLOBALS
#endif // HAS_COMPLEX_GLOBALS

#ifdef DOM3_LOAD
/* Can fetch entities from the XMLDoctype object, so there is some point in
   supporting the Entity interface.  LSParser from DOM 3 Load & Save is used
   to parse the contents of entities. */
# define DOM_SUPPORT_ENTITY
#endif // DOM3_LOAD

#include "modules/xmlparser/xmlconfig.h"
#ifdef XML_STORE_NOTATIONS
/* Can fetch notations from the XMLDoctype object, so there is some point in
   supporting the Notation interface. */
# define DOM_SUPPORT_NOTATION
#endif // XML_STORE_NOTATIONS

#ifndef HAS_NOTEXTSELECTION
/* Can support selectionStart and selectionEnd properties on input and textarea
   objects and TextRange. */
# define DOM_SELECTION_SUPPORT
#endif // !HAS_NOTEXTSELECTION

#ifdef XSLT_SUPPORT
# define DOM_XSLT_SUPPORT
#endif // XSLT_SUPPORT

#define USER_JAVASCRIPT_ADVANCED

#if defined OPERACONFIG_URL || defined USER_JAVASCRIPT && defined PREFS_HAVE_STRING_API || defined DATABASE_STORAGE_SUPPORT || defined CLIENTSIDE_STORAGE_SUPPORT || defined ABOUT_PRIVATE_BROWSING
# define DOM_PREFERENCES_ACCESS
#endif // OPERACONFIG_URL || USER_JAVASCRIPT && PREFS_HAVE_STRING_API || DATABASE_STORAGE_SUPPORT || CLIENTSIDE_STORAGE_SUPPORT || ABOUT_PRIVATE_BROWSING

#ifdef _DEBUG
//#define DOM_EXPENSIVE_DEBUG_CODE
#endif // _DEBUG

#ifdef CORE_GOGI
# define DOM_BENCHMARKXML_SUPPORT
#endif // CORE_GOGI

#ifdef DOM2_TRAVERSAL_AND_RANGE
# undef DOM2_TRAVERSAL
# define DOM2_TRAVERSAL
# undef DOM2_RANGE
# define DOM2_RANGE
#endif // DOM2_TRAVERSAL_AND_RANGE

/* DOM Selectors: always on, but ifdef:ed in case we ever want to
   disable it to save footprint. */
#define DOM_SELECTORS_API

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
# define DOM_INTEGRATED_DEVTOOLS_SUPPORT
#endif // INTEGRATED_DEVTOOLS_SUPPORT

#ifdef ABOUT_HTML_DIALOGS
/* This code has some resource management crash bugs that need to be
   resolved before it is activated. */
//# define DOM_SHOWMODALDIALOG_SUPPORT
#endif // ABOUT_HTML_DIALOGS

#endif // DOM_DOMINTERNALTYPES_H
