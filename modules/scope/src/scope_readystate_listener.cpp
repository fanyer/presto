/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/scope_readystate_listener.h"

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_document_manager.h"
#include "modules/scope/src/scope_ecmascript.h"
#include "modules/scope/src/scope_ecmascript_debugger.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/scope/src/scope_ecmascript.h"

/* static */ void
OpScopeReadyStateListener::OnReadyStateChanged(FramesDocument *doc, ReadyState state)
{
	if (!g_scope_manager)
		return;

#ifdef SCOPE_DOCUMENT_MANAGER
	if (g_scope_manager->document_manager)
		g_scope_manager->document_manager->ReadyStateChanged(doc, state);
#endif // SCOPE_DOCUMENT_MANAGER

#ifdef SCOPE_ECMASCRIPT
	if (g_scope_manager->ecmascript)
		g_scope_manager->ecmascript->ReadyStateChanged(doc, state);
#endif // SCOPE_ECMASCRIPT

#ifdef SCOPE_ECMASCRIPT_DEBUGGER
	if (g_scope_manager->ecmascript_debugger)
		g_scope_manager->ecmascript_debugger->ReadyStateChanged(doc, state);
#endif // SCOPE_ECMASCRIPT_DEBUGGER

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (g_scope_manager->window_manager)
		g_scope_manager->window_manager->ReadyStateChanged(doc, state);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
}

#endif // SCOPE_SUPPORT
