/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/scope/src/scope_manager.h"

#include "modules/scope/scope_jsconsole_listener.h"
#include "modules/scope/src/scope_ecmascript_debugger.h"

/* static */ void
OpScopeJSConsoleListener::OnConsoleLog(ES_Runtime *runtime, LogType type, ES_Value* argv, int argc)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleLog(runtime, type, argv, argc);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleTrace(ES_Runtime *runtime)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleTrace(runtime);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleTime(ES_Runtime *runtime, const uni_char *title)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleTime(runtime, title);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleTimeEnd(ES_Runtime *runtime, const uni_char *title)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleTimeEnd(runtime, title);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleProfile(ES_Runtime *runtime, const uni_char *title)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleProfile(runtime, title);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleProfileEnd(ES_Runtime *runtime)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleProfileEnd(runtime);
}

/* static */ void
OpScopeJSConsoleListener::OnConsoleException(ES_Runtime *runtime, ES_Value* argv, int argc)
{
	if (g_scope_manager && g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ConsoleException(runtime, argv, argc);
}

#endif // SCOPE_ECMASCRIPT_DEBUGGER

