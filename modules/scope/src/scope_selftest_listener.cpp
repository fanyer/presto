/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/scope/scope_selftest_listener.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_selftest.h"

/* static */ OP_STATUS
OpScopeSelftestListener::OnSelftestOutput(const uni_char *output)
{
	if (g_scope_manager && g_scope_manager->selftest)
		return g_scope_manager->selftest->OnSelftestOutput(output);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeSelftestListener::OnSelftestFinished()
{
	if (g_scope_manager && g_scope_manager->selftest)
		return g_scope_manager->selftest->OnSelftestFinished();
	return OpStatus::ERR;
}

#endif // SELFTEST
