/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_manager.h"

ScopeModule::ScopeModule()
	: manager(NULL)
{
}

void
ScopeModule::InitL(const OperaInitInfo&)
{
	manager = OP_NEW_L(OpScopeDefaultManager, ());
	LEAVE_IF_ERROR(manager->Construct());
}

void
ScopeModule::Destroy()
{
	OP_DELETE(manager);
	manager = NULL;
}

BOOL
ScopeModule::FreeCachedData(BOOL toplevel_context)
{
	(void)toplevel_context;
	return FALSE;
}

#endif // SCOPE_SUPPORT
