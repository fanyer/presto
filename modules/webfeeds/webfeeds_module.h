/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_WEBFEEDS_WEBFEEDS_MODULE_H
#define MODULES_WEBFEEDS_WEBFEEDS_MODULE_H

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/hardcore/opera/module.h"

class WebFeedsAPI;

class WebfeedsModule : public OperaModule
{
public:
	WebfeedsModule() : webfeeds_api(NULL) {}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual BOOL FreeCachedData(BOOL toplevel_context);

	WebFeedsAPI* webfeeds_api;
};

#define g_webfeeds_api g_opera->webfeeds_module.webfeeds_api
#define WEBFEEDS_MODULE_REQUIRED

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif // !MODULES_WEBFEEDS_WEBFEEDS_MODULE_H
