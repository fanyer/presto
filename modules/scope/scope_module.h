/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SCOPE_SCOPE_MODULE_H
#define MODULES_SCOPE_SCOPE_MODULE_H

#ifdef SCOPE_SUPPORT

#include "modules/hardcore/opera/module.h"

class OpScopeDefaultManager;

class ScopeModule : public OperaModule
{
public:
	ScopeModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual BOOL FreeCachedData(BOOL toplevel_context);

	OpScopeDefaultManager* manager; // Private to the module.
};

// g_scope_manager is private to the module.
#define g_scope_manager	g_opera->scope_module.manager

#define SCOPE_MODULE_REQUIRED

#endif // SCOPE_SUPPORT

#endif // !MODULES_SCOPE_SCOPE_MODULE_H
