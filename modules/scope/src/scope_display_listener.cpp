/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_EXEC_SUPPORT

#include "modules/scope/scope_display_listener.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_exec.h"

/* static */ void 
OpScopeDisplayListener::OnWindowPainted(Window* window, OpRect rect)
{
	if (g_scope_manager && g_scope_manager->exec)
		g_scope_manager->exec->WindowPainted(window, rect);
}

#endif // SCOPE_EXEC_SUPPORT
