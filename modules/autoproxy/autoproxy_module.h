/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_AUTOPROXY_AUTOPROXY_MODULE_H
#define MODULES_AUTOPROXY_AUTOPROXY_MODULE_H

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

#include "modules/hardcore/opera/module.h"

struct AutoProxyGlobals;

class AutoproxyModule : public OperaModule
{
public:
	AutoProxyGlobals *apc_globals;

public:
	AutoproxyModule();

	void InitL(const OperaInitInfo& info);

	void Destroy();
};

#define g_proxycfg	g_opera->autoproxy_module.apc_globals

#define AUTOPROXY_MODULE_REQUIRED

#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

#endif // !MODULES_AUTOPROXY_AUTOPROXY_MODULE_H
