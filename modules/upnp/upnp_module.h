/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef UPNP_MODULE_H
#define UPNP_MODULE_H

#ifdef UPNP_SUPPORT

#include "modules/hardcore/opera/module.h"

class UPnP;

#define UPNP_MODULE_REQUIRED

class UpnpModule : public OperaModule
{
public:
    UpnpModule();
	void InitL(const OperaInitInfo& info);
	void Destroy();
	
	UPnP *upnp;
};

#define g_upnp g_opera->upnp_module.upnp
#define g_upnp_dom g_upnp

#endif // UPNP_SUPPORT
#endif // !UPNP_MODULE_H
