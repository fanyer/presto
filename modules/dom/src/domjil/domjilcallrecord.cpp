/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilcallrecord.h"
#include "modules/dom/src/domjil/domjilcallrecordtypes.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/pi/device_api/OpTelephony.h"

/* virtual */ void
DOM_JILCallRecord::GCTrace()
{
	GCMark(m_startTime);
}

/* static */ OP_STATUS
DOM_JILCallRecord::Make(DOM_JILCallRecord*& new_obj, DOM_Runtime *origining_runtime, const OpTelephony::CallRecord* call_record_contents)
{
	new_obj = OP_NEW(DOM_JILCallRecord, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::JIL_CALLRECORD_PROTOTYPE), "CallRecord"));
	new_obj->m_undefnull.address = IS_UNDEF;
	new_obj->m_undefnull.id = IS_UNDEF;
	new_obj->m_undefnull.name = IS_UNDEF;
	new_obj->m_undefnull.type = IS_UNDEF;
	new_obj->m_undefnull.durationSeconds = IS_UNDEF;
	if (call_record_contents)
		RETURN_IF_ERROR(new_obj->SetCallRecord(call_record_contents));
	return OpStatus::OK;
}

OP_STATUS
DOM_JILCallRecord::SetCallRecord(const OpTelephony::CallRecord* call_record_contents)
{
	m_durationSeconds = call_record_contents->m_duration;
	ES_Value val;
	RETURN_IF_ERROR(DOMSetDate(&val, GetRuntime(), static_cast<time_t>(call_record_contents->m_duration)));
	m_startTime = val.value.object;
	RETURN_IF_ERROR(m_address.Set(call_record_contents->m_phone_number));
	RETURN_IF_ERROR(m_id.Set(call_record_contents->m_id));
	RETURN_IF_ERROR(m_name.Set(call_record_contents->m_name));
	RETURN_IF_ERROR(m_type.Set(DOM_JILCallRecordTypes::ToString(call_record_contents->m_type)));
	m_undefnull.address = IS_VALUE;
	m_undefnull.id = IS_VALUE;
	m_undefnull.name = IS_VALUE;
	m_undefnull.type = IS_VALUE;
	m_undefnull.durationSeconds = IS_VALUE;
	return OpStatus::OK;
}

OP_STATUS
DOM_JILCallRecord::CopyTo(DOM_JILCallRecord*& call_record)
{
	RETURN_IF_ERROR(DOM_JILCallRecord::Make(call_record, GetRuntime()));
	call_record->m_durationSeconds = m_durationSeconds;
	if (m_startTime)
	{
		ES_Value val;
		RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(m_startTime, &val));
		OP_ASSERT(val.type == VALUE_NUMBER);
		RETURN_IF_ERROR(DOMSetDate(&val, GetRuntime(), val.value.number));
		m_startTime = val.value.object;
	}
	RETURN_IF_ERROR(call_record->m_address.Set(m_address));
	RETURN_IF_ERROR(call_record->m_id.Set(m_id));
	RETURN_IF_ERROR(call_record->m_name.Set(m_name));
	RETURN_IF_ERROR(call_record->m_type.Set(m_type));
	call_record->m_undefnull.address = m_undefnull.address;
	call_record->m_undefnull.id = m_undefnull.id;
	call_record->m_undefnull.name = m_undefnull.name;
	call_record->m_undefnull.type = m_undefnull.type;
	call_record->m_undefnull.durationSeconds = m_undefnull.durationSeconds;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILCallRecord::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_callRecordAddress:
		switch (m_undefnull.address)
		{
			case IS_UNDEF: DOMSetUndefined(value); break;
			case IS_NULL: DOMSetNull(value); break;
			case IS_VALUE: DOMSetString(value, m_address.CStr()); break;
		}
		return GET_SUCCESS;
	case OP_ATOM_callRecordId:
		switch (m_undefnull.id)
		{
			case IS_UNDEF: DOMSetUndefined(value); break;
			case IS_NULL: DOMSetNull(value); break;
			case IS_VALUE: DOMSetString(value, m_id.CStr()); break;
		}
		return GET_SUCCESS;
	case OP_ATOM_callRecordName:
		switch (m_undefnull.name)
		{
			case IS_UNDEF: DOMSetUndefined(value); break;
			case IS_NULL: DOMSetNull(value); break;
			case IS_VALUE: DOMSetString(value, m_name.CStr()); break;
		}
		return GET_SUCCESS;
	case OP_ATOM_callRecordType:
		switch (m_undefnull.type)
		{
			case IS_UNDEF: DOMSetUndefined(value); break;
			case IS_NULL: DOMSetNull(value); break;
			case IS_VALUE: DOMSetString(value, m_type.CStr()); break;
		}
		return GET_SUCCESS;
	case OP_ATOM_durationSeconds:
		switch (m_undefnull.durationSeconds)
		{
			case IS_UNDEF: DOMSetUndefined(value); break;
			case IS_NULL: DOMSetNull(value); break;
			case IS_VALUE: DOMSetNumber(value, m_durationSeconds); break;
		}
		return GET_SUCCESS;
	case OP_ATOM_startTime:
		DOMSetObject(value, m_startTime);
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILCallRecord::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_callRecordAddress:
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.address = IS_NULL; m_address.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.address = IS_UNDEF; m_address.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_address.Set(value->value.string));
					m_undefnull.address = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		case OP_ATOM_callRecordId:  // should be read-only?
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.id = IS_NULL; m_id.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.id = IS_UNDEF; m_id.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_id.Set(value->value.string));
					m_undefnull.id = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		case OP_ATOM_callRecordName:
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.name = IS_NULL; m_name.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.name = IS_UNDEF; m_name.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_name.Set(value->value.string));
					m_undefnull.name = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		case OP_ATOM_callRecordType:
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.type = IS_NULL; m_type.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.type = IS_UNDEF; m_type.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_type.Set(value->value.string));
					m_undefnull.type = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		case OP_ATOM_durationSeconds:
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.durationSeconds = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.durationSeconds = IS_UNDEF; break;
				case VALUE_NUMBER:
					if (!op_isfinite(value->value.number) || value->value.number < 0)
						return PUT_SUCCESS;
					m_durationSeconds = static_cast<int>(value->value.number);
					m_undefnull.durationSeconds = IS_VALUE;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		JIL_PUT_DATE(OP_ATOM_startTime, m_startTime);
	}
	return PUT_FAILED;
}

/* virtual */ int
DOM_JILCallRecord_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_JILCallRecord* dom_call_record = NULL;
	CALL_FAILED_IF_ERROR(DOM_JILCallRecord::Make(dom_call_record, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_call_record);
	return ES_VALUE;
}

#endif // DOM_JIL_API_SUPPORT

