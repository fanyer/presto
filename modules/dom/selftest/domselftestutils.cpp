/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/dom/selftest/domselftestutils.h"
#include "modules/dom/src/domobj.h"

#include "modules/selftest/testutils.h"

#include "modules/doc/frm_doc.h"



DOM_SelftestUtils::SelftestAsyncCalback::SelftestAsyncCalback(ES_AsyncOperation expected_operation, ES_AsyncStatus expected_status, const ES_Value &expected_result)
	: m_expected_operation(expected_operation)
	, m_expected_status(expected_status)
	, m_expected_result(expected_result)
{
}

// TODO better fail messages
/* virtual */ OP_STATUS
DOM_SelftestUtils::SelftestAsyncCalback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	if (operation != m_expected_operation)
	{
		ST_failed("Unexpected operation in callback");
		return OpStatus::OK;
	}
	if (status != m_expected_status)
	{
		ST_failed("Unexpected status in  callback");
		return OpStatus::OK;
	}
	if (status == ES_ASYNC_SUCCESS)
	{
		if (m_expected_result.type != result.type)
		{
			ST_failed("Unexpected result in callback");
			return OpStatus::OK;
		}
		BOOL results_are_the_same = TRUE;
		switch (result.type)
		{
			case VALUE_STRING:
				if (uni_strcmp(m_expected_result.value.string, result.value.string))
					results_are_the_same = FALSE;
				break;
			case VALUE_NUMBER:
				if (m_expected_result.value.number != result.value.number)
					results_are_the_same = FALSE;
				break;
			case VALUE_OBJECT:
				if (m_expected_result.value.object != result.value.object)
					results_are_the_same = FALSE;
				break;
			case VALUE_UNDEFINED:
			case VALUE_NULL:
			default:
				results_are_the_same = TRUE;
		}
		if (!results_are_the_same)
		{
			ST_failed("Unexpected result in callback");
			return OpStatus::OK;
		}
	}
	ST_passed();
	return OpStatus::OK;
}

/* static */ void
DOM_SelftestUtils::TestAsyncFunction(ES_Object* function, ES_Object* this_obj, int argc, ES_Value* argv, ES_AsyncStatus expected_status, const ES_Value &expected_result, int line)
{
	OP_ASSERT(function);
	OP_ASSERT(argc == 0 || (argc >= 0 && argv));

	ES_AsyncInterface* async_if = g_selftest.utils->doc->GetESAsyncInterface();
	if (!async_if)
	{
		ST_failed_with_status("Couldn't aquire AsyncInterface for current doc", OpStatus::ERR_NO_MEMORY, line);
		return;
	}
	async_if->SetWantExceptions();

	if (!this_obj)
	{
		EcmaScript_Object* ecma_obj = ES_Runtime::GetHostObject(function);
		OP_ASSERT(ecma_obj);
		ES_Runtime* rt = ecma_obj->GetRuntime();
		if (!rt)
		{
			ST_failed_with_status("Function not added to runtime", OpStatus::ERR, line);
			return;
		}
		this_obj = rt->GetGlobalObjectAsPlainObject();
	}
	SelftestAsyncCalback* async_verification_callback = OP_NEW(SelftestAsyncCalback, (ES_ASYNC_CALL_FUNCTION, expected_status, expected_result));
	if (!async_verification_callback)
	{
		ST_failed_with_status("Failed to allocate async_verification_callback", OpStatus::ERR_NO_MEMORY, line);
		return;
	}
	OP_STATUS status = g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(async_verification_callback);
	if (OpStatus::IsError(status))
	{
		ST_failed_with_status("Failed to add async_verification_callback to cleanup list", status, line);
		return;
	}
	status = async_if->CallFunction(function, this_obj, argc, argv, async_verification_callback, NULL);
	if (OpStatus::IsError(status))
		ST_failed_with_status("Async calling of function failed", status, line);
}

/* static */ void
DOM_SelftestUtils::TestAsyncEval(const uni_char* program, ES_AsyncStatus expected_status, const ES_Value &expected_result, int line)
{
	OP_ASSERT(program);
	ES_AsyncInterface* async_if = g_selftest.utils->doc->GetESAsyncInterface();
	if (!async_if)
	{
		ST_failed_with_status("Couldn't aquire AsyncInterface for current doc", OpStatus::ERR_NO_MEMORY, line);
		return;
	}
	async_if->SetWantExceptions();

	SelftestAsyncCalback* async_verification_callback = OP_NEW(SelftestAsyncCalback, (ES_ASYNC_EVAL, expected_status, expected_result));
	if (!async_verification_callback)
	{
		ST_failed_with_status("Failed to allocate async_verification_callback", OpStatus::ERR_NO_MEMORY, line);
		return;
	}

	OP_STATUS status = g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(async_verification_callback);
	if (OpStatus::IsError(status))
	{
		ST_failed_with_status("Failed to add async_verification_callback to cleanup list", status, line);
		return;
	}

	status = async_if->Eval(program, async_verification_callback);
	if (OpStatus::IsError(status))
	{
		ST_failed_with_status("Async eval failed", status, line);
		return;
	}
}

#endif // SELFTEST
