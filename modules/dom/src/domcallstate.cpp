/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domcallstate.h"

/* static */
OP_STATUS DOM_CallState::Make(DOM_CallState*& new_obj, DOM_Runtime* origining_runtime, ES_Value* argv, int argc)
{
	new_obj = OP_NEW(DOM_CallState, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime));
	RETURN_IF_ERROR(new_obj->SetArguments(argv, argc)); // no need to free if error - it will be garbage collected
	return OpStatus::OK;
}

DOM_CallState::DOM_CallState()
	: m_argv(NULL)
	, m_argc(0)
	, m_user_data(NULL)
	, m_phase(PHASE_NONE)
	, m_ready_for_suspend(FALSE)
	{}

/* virtual*/
DOM_CallState::~DOM_CallState()
{
	// freeing strings in args if we copied them
	if (m_ready_for_suspend)
	{
		for (int i = 0; i < m_argc; ++i)
		{
			switch (m_argv[i].type)
			{
			case VALUE_STRING:
				OP_DELETEA(m_argv[i].value.string);
				break;
			case VALUE_STRING_WITH_LENGTH:
				OP_ASSERT(m_argv[i].value.string_with_length);
				OP_DELETEA(m_argv[i].value.string_with_length->string);
				OP_DELETE(m_argv[i].value.string_with_length);
				break;
			}
		}
	}
	OP_DELETEA(m_argv);
}

/* static */
DOM_CallState* DOM_CallState::FromReturnValue(ES_Value* return_value)
{
	if (return_value && return_value->type == VALUE_OBJECT)
		return FromES_Object(return_value->value.object);
	else
		return NULL;
}

/* static */
DOM_CallState* DOM_CallState::FromES_Object(ES_Object* es_object)
{
	if (es_object)
	{
		EcmaScript_Object* ecmascript_object = ES_Runtime::GetHostObject(es_object);
		if (ecmascript_object && ecmascript_object->IsA(DOM_TYPE_CALLSTATE))
			return static_cast<DOM_CallState*>(ecmascript_object);
	}
	return NULL;
}

OP_STATUS DOM_CallState::SetArguments(ES_Value* argv, int argc)
{
	OP_ASSERT(!m_argv);
	m_argv = OP_NEWA(ES_Value, (argc));
	if (!m_argv)
		return OpStatus::ERR_NO_MEMORY;

	m_argc = argc;
	for (int i = 0; i < m_argc; ++i)
		m_argv[i] = argv[i];

	return OpStatus::OK;
}

/* virtual */
void DOM_CallState::GCTrace()
{
	for (int i = 0; i < m_argc; ++i)
		GCMark(m_argv[i]);
}

/* static */
DOM_CallState::CallPhase DOM_CallState::GetPhaseFromESValue(ES_Value* restart_value)
{
	DOM_CallState* call_state = FromReturnValue(restart_value);
	if (!call_state)
		return PHASE_NONE;
	else
		return call_state->GetPhase();
}

/* static */
OP_STATUS DOM_CallState::RestoreArgumentsFromRestartValue(ES_Value* restart_value, ES_Value*& argv, int& argc)
{
	if (restart_value)
	{
		DOM_CallState* call_state = FromReturnValue(restart_value);
		if (call_state)
		{
			call_state->RestoreArguments(argv, argc);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

OP_STATUS DOM_CallState::PrepareForSuspend()
{
	if (!m_ready_for_suspend)
	{
		int i; // needed outside of the loop
		OP_STATUS error =OpStatus::OK;
		for (i = 0; i < m_argc; ++i)
		{
			switch (m_argv[i].type)
			{
			case VALUE_STRING:
				{
					m_argv[i].value.string = UniSetNewStr(m_argv[i].value.string);
					if (!m_argv[i].value.string)
						error = OpStatus::ERR_NO_MEMORY;
					break;
				}
			case VALUE_STRING_WITH_LENGTH:
				{
					ES_ValueString* copy = OP_NEW(ES_ValueString, ());
					int len = m_argv[i].value.string_with_length->length;
					uni_char* str_copy = OP_NEWA(uni_char, len);
					if (copy && str_copy)
					{
						op_memcpy(str_copy, m_argv[i].value.string_with_length->string, len * sizeof(uni_char));
						copy->string = str_copy;
						copy->length = len;
						m_argv[i].value.string_with_length = copy;
					}
					else
					{
						OP_DELETE(copy);
						OP_DELETEA(str_copy);
						error = OpStatus::ERR_NO_MEMORY;
					}
					break;
				}
			}

			if (OpStatus::IsError(error))
				break;
		}

		if (OpStatus::IsError(error))
		{
			for (i = i - 1;  i >= 0; --i)
			{
				switch (m_argv[i].type)
				{
				case VALUE_STRING:
					OP_DELETEA(m_argv[i].value.string);
					break;
				case VALUE_STRING_WITH_LENGTH:
					OP_ASSERT(m_argv[i].value.string_with_length);
					OP_DELETEA(m_argv[i].value.string_with_length->string);
					OP_DELETE(m_argv[i].value.string_with_length);
					break;
				}
			}
			return error;
		}
		m_ready_for_suspend = TRUE;
	}
	return OpStatus::OK;
};

