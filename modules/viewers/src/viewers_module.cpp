/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/viewers/viewers.h"

ViewersModule::ViewersModule(void) :
	viewer_list(0)
#if defined(_PLUGIN_SUPPORT_)
	, plugin_viewer_list(0)
#endif
{
}

ViewersModule::~ViewersModule(void)
{
}

void ViewersModule::InitL(const OperaInitInfo&)
{
	viewer_list = OP_NEW_L(Viewers, ());
	viewer_list->ConstructL();

#ifdef _PLUGIN_SUPPORT_
	plugin_viewer_list = OP_NEW_L(PluginViewers, ());
	plugin_viewer_list->ConstructL();
#endif
}

void ViewersModule::Destroy(void)
{
	OP_DELETE(viewer_list);
	viewer_list = 0;

#ifdef _PLUGIN_SUPPORT_
	OP_DELETE(plugin_viewer_list);
	plugin_viewer_list = 0;
#endif // _PLUGIN_SUPPORT_
}
