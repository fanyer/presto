/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMDEBUGGER_H
#define DOM_DOMDEBUGGER_H

#ifdef DOM_DEBUGGER_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/util/tempbuf.h"

class ES_Debugger;

class DOM_Debugger
	: public DOM_Object,
	  public ES_DebugFrontend
{
protected:
	DOM_Debugger();
	~DOM_Debugger();

public:
	static OP_STATUS Make(DOM_Debugger *&debugger, DOM_EnvironmentImpl *environment);

	/* From DOM_Object. */
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DEBUGGER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	/* From ES_Debugger. */
	virtual OP_STATUS NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data);
	virtual OP_STATUS StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugPosition &position, ES_DebugReturnValue *dbg_return_value);
	virtual OP_STATUS BacktraceReply(unsigned dbg_runtime_id, unsigned length, ES_DebugStackFrame *stack, BOOL function_names, BOOL argument_values);
	virtual OP_STATUS EvalReply(unsigned dbg_runtime_id, unsigned tag, BOOL success, const ES_DebugValue &result);

	DOM_DECLARE_FUNCTION(attachTo);
	DOM_DECLARE_FUNCTION(continueScript);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };
};

#endif // DOM_DEBUGGER_SUPPORT
#endif // DOM_DOMDEBUGGER_H
