/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/scope/src/scope_selftest.h"
#include "modules/dom/domenvironment.h"
#include "modules/selftest/operaselftestdispatcher.h"
#include "modules/selftest/selftest_module.h"

OpScopeSelftest::OpScopeSelftest()
	: OpScopeSelftest_SI()
{
}

OP_STATUS
OpScopeSelftest::OnSelftestOutput(const uni_char *output)
{
	if (!IsEnabled())
		return OpStatus::OK;

	SelftestOutput msg;
	RETURN_IF_ERROR(msg.SetOutput(output));
	return SendOnSelftestOutput(msg);
}

OP_STATUS
OpScopeSelftest::OnSelftestFinished()
{
	if (!IsEnabled())
		return OpStatus::OK;

	return SendOnSelftestFinished();
}

OP_STATUS
OpScopeSelftest::DoRunModules(const RunModulesArg &in)
{
	OpString moduleListString;

	unsigned count = in.GetModuleList().GetCount();

	for (unsigned i = 0; i < count; ++i)
	{
		OpString *str = in.GetModuleList().Get(i);

		//append string to list
		RETURN_IF_ERROR(moduleListString.Append(*str));

		//append a , if not at end
		if(i < count - 1)
			RETURN_IF_ERROR(moduleListString.Append(","));
	}

	// Convert to char*
	char* moduleListStringCharArray = uni_down_strdup(moduleListString.CStr());

	BOOL spartan_readable = (in.GetOutputType() == RunModulesArg::OUTPUTTYPE_MACHINE_READABLE);

	// Call selftest API.
	if (moduleListStringCharArray != 0 && moduleListStringCharArray[0] != 0)
		g_selftest.selftestDispatcher->selftestRunFromScope(moduleListStringCharArray, spartan_readable);

	// Release
	if (moduleListStringCharArray != 0)
		op_free((void*)moduleListStringCharArray);

	// Happily ever after
	return OpStatus::OK;
}

#endif // SELFTEST
