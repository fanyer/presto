/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_COOKIE_MODULE_H
#define MODULES_COOKIE_MODULE_H

#include "modules/hardcore/opera/module.h"

class CookiesModule : public OperaModule
{
public:
	CookiesModule() : cookie_header_generation_buffer(NULL) {}
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	char* cookie_header_generation_buffer;
};

#define g_cookie_header_buffer (g_opera->cookies_module.cookie_header_generation_buffer)

#define COOKIES_MODULE_REQUIRED

#endif // !MODULES_COOKIE_MODULE_H
