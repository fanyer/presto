/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

#include "modules/autoproxy/autoproxy_module.h"
#include "modules/autoproxy/autoproxy_public.h"
#include "modules/autoproxy/autoproxy.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/autoproxy/autoproxy.h"

AutoproxyModule::AutoproxyModule()
	: apc_globals(NULL)
{
}

void
AutoproxyModule::InitL(const OperaInitInfo& info)
{
#ifdef SELFTEST
	if (g_proxycfg == NULL)
	{
		g_proxycfg = OP_NEW(AutoProxyGlobals, ());
	}
#endif
}

void
AutoproxyModule::Destroy()
{
	ReleaseAutoProxyResources();
	OP_DELETE(g_proxycfg);
}

#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

