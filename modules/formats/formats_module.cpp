/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/url/tools/arrays.h"

void
FormatsModule::InitL(const OperaInitInfo& info)
{
#if defined(_ENABLE_AUTHENTICATE)
	CONST_ARRAY_INIT_L(HTTP_Authentication_Keywords);
#endif

	CONST_ARRAY_INIT_L(HTTPHeaders);
	CONST_ARRAY_INIT_L(HTTP_General_Keywords);
	CONST_ARRAY_INIT_L(paramsep);

	CONST_ARRAY_INIT_L(HTTP_Cookie_Keywords);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	CONST_ARRAY_INIT_L(APC_keywords);
#endif
	// Add all other array initializations above
	CONST_ARRAY_INIT_L(Keyword_Index_List);
}

void
FormatsModule::Destroy()
{
	CONST_ARRAY_SHUTDOWN(Keyword_Index_List);
	// Add all array shutowns below

#if defined(_ENABLE_AUTHENTICATE)
	CONST_ARRAY_SHUTDOWN(HTTP_Authentication_Keywords);
#endif
	CONST_ARRAY_SHUTDOWN(HTTPHeaders);
	CONST_ARRAY_SHUTDOWN(HTTP_General_Keywords);
	CONST_ARRAY_SHUTDOWN(paramsep);

	CONST_ARRAY_SHUTDOWN(HTTP_Cookie_Keywords);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	CONST_ARRAY_SHUTDOWN(APC_keywords);
#endif
}
