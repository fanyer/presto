/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/scope_window_listener.h"
#include "modules/scope/src/scope_window_manager.h"

/* static */ void
OpScopeWindowListener::OnNewWindow(Window *window)
{
	if (g_scope_manager && g_scope_manager->window_manager)
		g_scope_manager->window_manager->NewWindow(window);
}

/* static */ void
OpScopeWindowListener::OnWindowRemoved(Window *window)
{
	if (g_scope_manager && g_scope_manager->window_manager)
		g_scope_manager->window_manager->WindowRemoved(window);
}

/* static */ void
OpScopeWindowListener::OnWindowTitleChanged(Window *window)
{
	if (g_scope_manager && g_scope_manager->window_manager)
		g_scope_manager->window_manager->WindowTitleChanged(window);
}

/* static */ void
OpScopeWindowListener::OnActiveWindowChanged(Window *window)
{
	if (g_scope_manager && g_scope_manager->window_manager)
		g_scope_manager->window_manager->ActiveWindowChanged(window);
}

#endif // SCOPE_WINDOW_MANAGER_SUPPORT
