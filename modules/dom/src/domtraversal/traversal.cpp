/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_TRAVERSAL

#include "modules/dom/src/domtraversal/traversal.h"
#include "modules/dom/src/domtraversal/nodefilter.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/node.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"

class DOM_TraversalObject_State
	: public DOM_Object,
	  public ES_AsyncCallback,
	  public ES_ThreadListener
{
public:
	/* From ES_AsyncCallback. */
	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	/* From ES_ThreadListener. */
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

	void HandleValue(const ES_Value &value);

	ES_Thread *thread;
	ES_Value exception;
	int result;
};

/* virtual */ OP_STATUS
DOM_TraversalObject_State::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &value)
{
	if (status == ES_ASYNC_SUCCESS)
		HandleValue(value);
	else if (status == ES_ASYNC_EXCEPTION)
	{
		exception = value;
		result = DOM_NodeFilter::FILTER_EXCEPTION;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_TraversalObject_State::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_FINISHED && thread->ReturnedValue() && !thread->IsFailed())
	{
		ES_Value value;
		RETURN_IF_ERROR(thread->GetReturnedValue(&value));
		HandleValue(value);
	}

	if (signal != ES_SIGNAL_SCHEDULER_TERMINATED)
		ES_ThreadListener::Remove();

	return OpStatus::OK;
}

void
DOM_TraversalObject_State::HandleValue(const ES_Value &value)
{
	// Would really prefer if the ecmascript engine had converted the object to a number for us
	if (value.type == VALUE_NUMBER)
	{
		double number = value.value.number;
		if (number == DOM_NodeFilter::FILTER_ACCEPT)
			result = DOM_NodeFilter::FILTER_ACCEPT;
		else if (number == DOM_NodeFilter::FILTER_REJECT)
			result = DOM_NodeFilter::FILTER_REJECT;
		else if (number == DOM_NodeFilter::FILTER_SKIP)
			result = DOM_NodeFilter::FILTER_SKIP;
	}
	else if (value.type == VALUE_BOOLEAN && value.value.boolean)
		result = DOM_NodeFilter::FILTER_ACCEPT;
}

DOM_TraversalObject::DOM_TraversalObject()
	: root(NULL),
	  what_to_show(0),
	  filter(NULL),
	  entity_reference_expansion(FALSE),
	  state(NULL)
{
}

/* virtual */ void
DOM_TraversalObject::GCTrace()
{
	if (root) root->GetRuntime()->GCMark(root);
	if (state) state->GetRuntime()->GCMark(state);
}

static const unsigned short g_DOM_nodetype_flag_table[] =
{
	0,
	DOM_NodeFilter::SHOW_ELEMENT,
	DOM_NodeFilter::SHOW_ATTRIBUTE,
	DOM_NodeFilter::SHOW_TEXT,
	DOM_NodeFilter::SHOW_CDATA_SECTION,
	DOM_NodeFilter::SHOW_ENTITY_REFERENCE,
	DOM_NodeFilter::SHOW_ENTITY,
	DOM_NodeFilter::SHOW_PROCESSING_INSTRUCTION,
	DOM_NodeFilter::SHOW_COMMENT,
	DOM_NodeFilter::SHOW_DOCUMENT,
	DOM_NodeFilter::SHOW_DOCUMENT_TYPE,
	DOM_NodeFilter::SHOW_DOCUMENT_FRAGMENT,
	DOM_NodeFilter::SHOW_NOTATION
};

int
DOM_TraversalObject::AcceptNode(DOM_Node *node, DOM_Runtime *origining_runtime, ES_Value &exception)
{
	if (state)
	{
		int result = state->result;

		if (result == DOM_NodeFilter::FILTER_EXCEPTION)
			exception = state->exception;

		state = NULL;

		return result;
	}

	unsigned flag = g_DOM_nodetype_flag_table[node->GetNodeType()];

	if ((what_to_show & flag) == 0)
		return DOM_NodeFilter::FILTER_SKIP;

	if (filter)
	{
		if (OpStatus::IsMemoryError(DOM_Object::DOMSetObjectRuntime(state = OP_NEW(DOM_TraversalObject_State, ()), origining_runtime)))
		{
			state = NULL;
			return DOM_NodeFilter::FILTER_OOM;
		}

		state->thread = DOM_Object::GetCurrentThread(origining_runtime);
		state->result = DOM_NodeFilter::FILTER_FAILED;

		ES_Value arguments[1];
		DOM_Object::DOMSetObject(&arguments[0], node);

		ES_AsyncInterface *asyncif = origining_runtime->GetESAsyncInterface();
		OP_STATUS status;

		asyncif->SetWantExceptions();

		if (op_strcmp(ES_Runtime::GetClass(filter), "Function") == 0)
			status = asyncif->CallFunction(filter, filter, 1, arguments, state, state->thread);
		else
			status = asyncif->CallMethod(filter, UNI_L("acceptNode"), 1, arguments, state, state->thread);

		if (OpStatus::IsError(status))
		{
			state = NULL;

			if (OpStatus::IsMemoryError(status))
				return DOM_NodeFilter::FILTER_OOM;
			else
				return DOM_NodeFilter::FILTER_FAILED;
		}
		else
			return DOM_NodeFilter::FILTER_DELAY;
	}

	return DOM_NodeFilter::FILTER_ACCEPT;
}

#endif // DOM2_TRAVERSAL
