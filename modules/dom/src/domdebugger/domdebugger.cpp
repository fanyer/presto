/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM_DEBUGGER_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domdebugger/domdebugger.h"
#include "modules/dom/src/js/window.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"

DOM_Debugger::DOM_Debugger()
{
}

DOM_Debugger::~DOM_Debugger()
{
}

/* static */ OP_STATUS
DOM_Debugger::Make(DOM_Debugger *&debugger, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(debugger = OP_NEW(DOM_Debugger, ()), runtime, runtime->GetPrototype(DOM_Runtime::DEBUGGER_PROTOTYPE), "Debugger"));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_Debugger::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Debugger::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_FAILED;
}

/* virtual */ void
DOM_Debugger::GCTrace()
{
}

/* virtual */ OP_STATUS
DOM_Debugger::StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugPosition &position, ES_DebugReturnValue *dbg_return_value)
{
	ES_AsyncInterface *asyncif = GetEnvironment()->GetAsyncInterface();

	ES_Value argv[4];
	DOMSetNumber(&argv[0], dbg_runtime_id);
	DOMSetNumber(&argv[1], dbg_thread_id);
	DOMSetNumber(&argv[2], position.scriptid);
	DOMSetNumber(&argv[3], position.linenr);

	return asyncif->CallMethod(*this, UNI_L("stoppedAt"), 2, argv);
}

/* virtual */ OP_STATUS
DOM_Debugger::NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data)
{
	ES_AsyncInterface *asyncif = GetEnvironment()->GetAsyncInterface();

	ES_Value argv[5];
	DOMSetNumber(&argv[0], dbg_runtime_id);
	DOMSetNumber(&argv[1], data->script_no);
	DOMSetString(&argv[2], data->type);
	DOMSetString(&argv[3], data->url);
	DOMSetString(&argv[4], data->source);

	return asyncif->CallMethod(*this, UNI_L("newScript"), 5, argv);
}

/* virtual */ OP_STATUS
DOM_Debugger::BacktraceReply(unsigned dbg_runtime_id, unsigned length, ES_DebugStackFrame *stack, BOOL function_names, BOOL argument_values)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_Debugger::EvalReply(unsigned dbg_runtime_id, unsigned tag, BOOL success, const ES_DebugValue &result)
{
	return OpStatus::OK;
}

/* static */ int
DOM_Debugger::attachTo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(debugger, DOM_TYPE_DEBUGGER, DOM_Debugger);
	DOM_CHECK_ARGUMENTS("ob");
	DOM_ARGUMENT_OBJECT(window, 0, DOM_TYPE_WINDOW, JS_Window);

	CALL_FAILED_IF_ERROR(debugger->ConstructEngineBackend());
	CALL_FAILED_IF_ERROR(debugger->AttachTo(window->GetRuntime(), argv[1].value.boolean));

	return ES_FAILED;
}

/* static */ int
DOM_Debugger::continueScript(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(debugger, DOM_TYPE_DEBUGGER, DOM_Debugger);
	DOM_CHECK_ARGUMENTS("n");

	CALL_FAILED_IF_ERROR(debugger->Continue((unsigned) argv[0].value.number, RUN));

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Debugger)
	DOM_FUNCTIONS_FUNCTION(DOM_Debugger, DOM_Debugger::attachTo, "attachTo", "-b-")
	DOM_FUNCTIONS_FUNCTION(DOM_Debugger, DOM_Debugger::continueScript, "continueScript", "n-")
DOM_FUNCTIONS_END(DOM_Debugger)

#endif // DOM_DEBUGGER_SUPPORT
