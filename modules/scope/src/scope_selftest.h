/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef SCOPE_SELFTEST_H
#define SCOPE_SELFTEST_H

#ifdef SELFTEST

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_selftest_interface.h"

class OpScopeSelftest
	: public OpScopeSelftest_SI
{
public:
	OpScopeSelftest();

	OP_STATUS OnSelftestOutput(const uni_char *output);
	OP_STATUS OnSelftestFinished();

	// Request/Response functions
	virtual OP_STATUS DoRunModules(const RunModulesArg &in);
};

#endif // SELFTEST

#endif // SCOPE_SELFTEST_H
