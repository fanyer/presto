/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef X11API

#include "platforms/x11api/x11api_module.h"
#include "platforms/x11api/plugins/plugin_window_tracker.h"

void X11apiModule::InitL(const OperaInitInfo& info) { }

void X11apiModule::Destroy()
{
#ifdef NS4P_COMPONENT_PLUGINS
	PluginWindowTracker::DeleteInstance();
#endif
}

#endif  // X11API
