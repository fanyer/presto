/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_FORMATS_MODULE_H
#define MODULES_FORMATS_MODULE_H

#include "modules/url/tools/arrays_decl.h"

struct KeywordIndex;
struct Keyword_Index_Item;

class FormatsModule : public OperaModule
{
public:
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, HTTPHeaders);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, HTTP_General_Keywords);
	DECLARE_MODULE_CONST_ARRAY(Keyword_Index_Item, Keyword_Index_List);
	DECLARE_MODULE_CONST_ARRAY(const char*, paramsep);
#ifdef _ENABLE_AUTHENTICATE
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, HTTP_Authentication_Keywords);
#endif
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, HTTP_Cookie_Keywords);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, APC_keywords);
#endif

public:
	FormatsModule(){};
	virtual ~FormatsModule(){};

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
};

#ifndef HAS_COMPLEX_GLOBALS
# define g_HTTPHeaders	CONST_ARRAY_GLOBAL_NAME(formats, HTTPHeaders)
# define g_HTTP_General_Keywords	CONST_ARRAY_GLOBAL_NAME(formats, HTTP_General_Keywords)
# define g_Keyword_Index_List	CONST_ARRAY_GLOBAL_NAME(formats, Keyword_Index_List)
# define g_paramsep	CONST_ARRAY_GLOBAL_NAME(formats, paramsep)
# ifdef _ENABLE_AUTHENTICATE
#  define g_HTTP_Authentication_Keywords	CONST_ARRAY_GLOBAL_NAME(formats, HTTP_Authentication_Keywords)
# endif
# define g_HTTP_Cookie_Keywords	CONST_ARRAY_GLOBAL_NAME(formats, HTTP_Cookie_Keywords)
# ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
#  define g_APC_keywords	CONST_ARRAY_GLOBAL_NAME(formats, APC_keywords)
# endif
#endif

#define FORMATS_MODULE_REQUIRED

#endif // MODULES_FORMATS_MODULE_H
