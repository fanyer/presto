/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JSPLUGINS_MODULE_H
#define JSPLUGINS_MODULE_H

#ifdef JS_PLUGIN_SUPPORT

#include "modules/hardcore/opera/module.h"

#define JSP_SELFTESTS_GLOBAL_NAMES_SIZE 2
#define JSP_SELFTESTS_OBJECT_TYPES_SIZE 8

class JS_Plugin_Manager;

class JspluginsModule
	: public OperaModule
{
public:
	JspluginsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	JS_Plugin_Manager *manager;
#ifdef _DEBUG
	int jspobj_balance;
#endif // _DEBUG
#if defined(SELFTEST) && !defined(HAS_COMPLEX_GLOBALS)
	const char *jsp_selftests_global_names[JSP_SELFTESTS_GLOBAL_NAMES_SIZE];
	const char *jsp_selftests_object_types[JSP_SELFTESTS_OBJECT_TYPES_SIZE];
#endif // SELFTEST
#ifdef SELFTEST
	unsigned int jsp_selftest_bits;
	void ResetDynamicSelftestBits();
#endif
};

#define JSPLUGINS_MODULE_REQUIRED

#define g_jsPluginManager (g_opera->jsplugins_module.manager)

#if defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT) && !defined(HAS_COMPLEX_GLOBALS)
# define jsp_selftests_global_names g_opera->jsplugins_module.jsp_selftests_global_names
# define jsp_selftests_object_types g_opera->jsplugins_module.jsp_selftests_object_types
#endif // defined(SELFTEST) && defined(JS_PLUGIN_SUPPORT) && defined(HAS_COMPLEX_GLOBALS)

#endif // JS_PLUGIN_SUPPORT
#endif // JSPLUGINS_MODULE_H
