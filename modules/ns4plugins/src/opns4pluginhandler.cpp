/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/pluginmemoryhandler.h"
#include "modules/ns4plugins/opns4pluginhandler.h"

#include "modules/hardcore/mh/mh.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)      (sizeof(arr)/sizeof(arr[0]))
#endif

/* static */ OP_STATUS
OpNS4PluginHandler::Init()
{
	g_pluginhandler = OP_NEW(PluginHandler, ());
	RETURN_OOM_IF_NULL(g_pluginhandler);
	OP_STATUS status = g_pluginhandler->Init();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(g_pluginhandler);
		g_pluginhandler = NULL;
		return status;
	}

	const OpMessage msglist[] = {MSG_PLUGIN_CALL};
	RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(g_pluginhandler, 0, msglist, ARRAY_SIZE(msglist)));

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
    RETURN_IF_ERROR(g_pluginhandler->LoadStaticLib());
#endif // NS4P_SUPPORT_PROXY_PLUGIN

	return OpStatus::OK;
}

/* static */ void
OpNS4PluginHandler::Destroy()
{
	if (g_pluginhandler)
	{
		g_main_message_handler->UnsetCallBacks(g_pluginhandler);
		OP_DELETE(g_pluginhandler);
		g_pluginhandler = 0;
	}
}

#endif // _PLUGIN_SUPPORT_

