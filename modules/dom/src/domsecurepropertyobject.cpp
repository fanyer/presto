/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domsecurepropertyobject.h"

#ifdef DOM_ACCESS_SECURITY_RULES

#include "modules/dom/src/domcallstate.h"

/* virtual */ OP_STATUS
DOM_SecurePropertyObject::GetOperationName(const char* property_name, OpString8* operation_name) const
{
	OP_ASSERT(property_name);
	OP_ASSERT(operation_name);
	operation_name->Empty();
	return operation_name->AppendFormat("%s.%s", ES_Runtime::GetClass(GetNativeObject()), property_name);
}

int
DOM_SecurePropertyObject::PropertySecurityCheck(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	if (value != NULL)
	{	// Perform check only for Get and Put, not HasProperty
		OpString8 operation_name;
		CALL_FAILED_IF_ERROR(GetOperationName(DOM_AtomToString(property_atom), &operation_name));

		return PropertyAccessSecurityCheck(value, NULL, 0, origining_runtime, restart_value, DOM_Object::ACCESS_TYPE_PROPERTY_ACCESS, operation_name.CStr(), NULL, NULL);
	}
	else
		return ES_VALUE;
}

ES_GetState
DOM_SecurePropertyObject::CallGet(OpAtom property_atom, ES_Value* value,
							ES_Runtime* es_origining_runtime, ES_Object* restart_object)
{
	if (property_atom == OP_ATOM_UNASSIGNED)
		return GET_FAILED;

	OP_ASSERT(property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);
	ES_Value restart_value;
	DOMSetObject(&restart_value, restart_object);
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);

	int security_check_status = PropertySecurityCheck(property_atom, value, origining_runtime, &restart_value);
	if (security_check_status != ES_VALUE)
		return ConvertCallToGetName(security_check_status, value);

	DOM_CallState* call_state = DOM_CallState::FromReturnValue(&restart_value);
	BOOL internal_restart = call_state && call_state->GetPhase() > DOM_CallState::PHASE_SECURITY_CHECK;

	ES_Value empty_restart_value;
	ES_GetState get_state = InternalGetName(property_atom, value, origining_runtime, internal_restart ? &restart_value : &empty_restart_value);
	return (GET_FAILED == get_state)
		? DOM_Object::GetName(property_atom, value, origining_runtime)
		: get_state;
}

ES_PutState
DOM_SecurePropertyObject::CallPut(OpAtom property_atom, ES_Value* value,
							ES_Runtime* es_origining_runtime, ES_Object* restart_object)
{
	if (property_atom == OP_ATOM_UNASSIGNED)
		return PUT_FAILED;

	OP_ASSERT(property_atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);
	ES_Value restart_value;
	DOMSetObject(&restart_value, restart_object);
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);

	int security_check_status = PropertySecurityCheck(property_atom, value, origining_runtime, &restart_value);
	if (security_check_status != ES_VALUE)
		return ConvertCallToPutName(security_check_status, value);

	BOOL internal_restart = DOM_CallState::GetPhaseFromESValue(&restart_value) > DOM_CallState::PHASE_SECURITY_CHECK;

	ES_Value empty_restart_value;
	ES_PutState put_state = InternalPutName(property_atom, value, origining_runtime, internal_restart ? &restart_value : &empty_restart_value);
	if (put_state == PUT_FAILED)
		return DOM_Object::PutName(property_atom, value, origining_runtime);
	else if ((put_state == PUT_NEEDS_NUMBER
			 || put_state == PUT_NEEDS_STRING
			 || put_state == PUT_NEEDS_STRING_WITH_LENGTH
			 || put_state == PUT_NEEDS_BOOLEAN)
			&& DOM_CallState::GetPhaseFromESValue(&restart_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		// PUT_SUSPEND is not handled here, if InternalPutName decides to suspend, it is responsible
		// for setting the phase itself.
		DOM_CallState::FromReturnValue(&restart_value)->SetPhase(DOM_CallState::PHASE_EXECUTION_0);
	}

	return put_state;
}

#endif // DOM_ACCESS_SECURITY_RULES
