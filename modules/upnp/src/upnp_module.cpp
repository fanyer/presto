/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UPNP_SUPPORT
#include "modules/upnp/upnp_module.h"
#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/upnp_port_opening.h"

void UpnpModule::InitL(const OperaInitInfo& info)
{
	g_upnp=NULL;
	g_upnp=OP_NEW(UPnP, (TRUE));
	
	OP_ASSERT(g_upnp);
}

void UpnpModule::Destroy()
{
	// g_upnp for now is not intended to be deleted here
	OP_DELETE(g_upnp);
	g_upnp=NULL;
}

UpnpModule::UpnpModule()
{
	upnp=NULL;
}

#endif
