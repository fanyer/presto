/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/scope/scope_element_inspector.h"

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_ecmascript_debugger.h"

/* static */ OP_STATUS 
OpScopeElementInspector::InspectElement(FramesDocument *doc, HTML_Element* html_elm)
{
	if (!g_scope_manager || !g_scope_manager->ecmascript_debugger)
		return OpStatus::ERR;

	return g_scope_manager->ecmascript_debugger->InspectElement(doc, html_elm);
}

#endif // SCOPE_ECMASCRIPT_DEBUGGER

