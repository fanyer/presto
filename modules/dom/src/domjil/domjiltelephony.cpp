/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjiltelephony.h"
#include "modules/dom/src/domjil/domjilcallrecord.h"
#include "modules/pi/device_api/OpTelephony.h"
#include "modules/dom/src/domjil/domjilcallrecordtypes.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/device_api/WildcardCmp.h"

#include "modules/doc/frm_doc.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommander.h"

/* static */ OP_STATUS
DOM_JILTelephony::Make(DOM_JILTelephony*& new_obj, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_obj = OP_NEW(DOM_JILTelephony, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_TELEPHONY_PROTOTYPE), "Telephony"));
	RETURN_IF_ERROR(OpTelephony::Create(&new_obj->m_telephony, new_obj));

	DOM_JILCallRecordTypes* call_record_types;
	RETURN_IF_ERROR(DOM_JILCallRecordTypes::Make(call_record_types, runtime));
	ES_Value call_record_types_val;
	DOMSetObject(&call_record_types_val, call_record_types);
	RETURN_IF_LEAVE(new_obj->PutL("CallRecordTypes", call_record_types_val, PROP_READ_ONLY | PROP_DONT_DELETE));

	RETURN_IF_LEAVE(new_obj->PutFunctionL("CallRecord", OP_NEW(DOM_JILCallRecord_Constructor, ()), "CallRecord", NULL));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILTelephony::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onCallEvent:
			DOMSetObject(value, m_onCallEventCallback);
			return GET_SUCCESS;
		case OP_ATOM_onCallRecordsFound:
			DOMSetObject(value, m_onCallRecordsFoundCallback);
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILTelephony::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onCallEvent:
			switch (value->type)
			{
				case VALUE_NULL:
					m_onCallEventCallback = NULL;
					break;
				case VALUE_OBJECT:
					m_onCallEventCallback = value->value.object;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		case OP_ATOM_onCallRecordsFound:
			switch (value->type)
			{
				case VALUE_NULL:
					m_onCallRecordsFoundCallback = NULL;
					break;
				case VALUE_OBJECT:
					m_onCallRecordsFoundCallback = value->value.object;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* virtual */ void
DOM_JILTelephony::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_onCallEventCallback);
	GCMark(m_onCallRecordsFoundCallback);
}

/* virtual */ void
DOM_JILTelephony::OnCall(OpTelephony::CallRecord::Type type, const OpString& phone_number)
{
	if (!m_onCallEventCallback)
		return;

	ES_AsyncInterface* async = GetRuntime()->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async);
	ES_Value argv[2];
	DOM_Object::DOMSetString(&(argv[0]), DOM_JILCallRecordTypes::ToString(type));
	DOM_Object::DOMSetString(&(argv[1]), phone_number.CStr());
	OpStatus::Ignore(async->CallFunction(m_onCallEventCallback, GetNativeObject(), ARRAY_SIZE(argv), argv, NULL, NULL));
}

class DOM_JILTelephonySimpleCallback : public DOM_SuspendCallback<OpTelephony::SimpleCallback>
{
public:
	virtual void OnFailed(OP_STATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
	virtual void OnFinished() { OnSuccess(); }
};

int
DOM_JILTelephony::deleteAllCallRecords(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_TELEPHONY, DOM_JILTelephony);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* call_record_type_str = argv[0].value.string;
	OpTelephony::CallRecord::Type call_record_type = DOM_JILCallRecordTypes::FromString(call_record_type_str);
	if (call_record_type == OpTelephony::CallRecord::TypeUnknown)
		return HandleError(OpTelephony::Status::ERR_WRONG_CALL_RECORD_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);

	NEW_SUSPENDING_CALLBACK(DOM_JILTelephonySimpleCallback, delete_callback, return_value, call, ());
	OpMemberFunctionObject2<OpTelephony, OpTelephony::CallRecord::Type, OpTelephony::SimpleCallback*>
		function_delete_all(self->m_telephony, &OpTelephony::DeleteAllCallRecords, call_record_type, delete_callback);
	DOM_SUSPENDING_CALL(call, function_delete_all, DOM_JILTelephonySimpleCallback, delete_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(delete_callback->GetErrorCode(), HandleError);
	return ES_FAILED;
}

int
DOM_JILTelephony::deleteCallRecord(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_TELEPHONY, DOM_JILTelephony);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* call_record_type_str = argv[0].value.string;
	const uni_char* id = argv[1].value.string;
	OpTelephony::CallRecord::Type call_record_type = DOM_JILCallRecordTypes::FromString(call_record_type_str);
	if (call_record_type == OpTelephony::CallRecord::TypeUnknown)
		return HandleError(OpTelephony::Status::ERR_WRONG_CALL_RECORD_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILTelephonySimpleCallback, delete_callback, return_value, call, ());
	OpMemberFunctionObject3<OpTelephony, OpTelephony::CallRecord::Type, const uni_char*, OpTelephony::SimpleCallback*>
		function_delete(self->m_telephony, &OpTelephony::DeleteCallRecord, call_record_type, id, delete_callback);
	DOM_SUSPENDING_CALL(call, function_delete, DOM_JILTelephonySimpleCallback, delete_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(delete_callback->GetErrorCode(), HandleError);
	return ES_FAILED;
}

// TODO: Refactor to have a template class SearchCallbackAsyncImpl<OpTelephony::CallRecord>.
// TODO: Refactor DOM_AsyncCallback to have static Make() method.
class FindCallRecordsCallbackAsyncImpl : public OpTelephony::CallRecordsSearchIterationCallback, public DOM_AsyncCallback
{
public:
	struct SearchParams
	{
		DOM_JILCallRecord* reference_item;
		int start_index;
		int end_index;
		SearchParams() : reference_item(NULL), start_index(0), end_index(-1){}
		~SearchParams() {}
	};

	~FindCallRecordsCallbackAsyncImpl()
	{
		if (m_search_params && m_search_params->reference_item)
			GetRuntime()->Unprotect(*m_search_params->reference_item);
		OP_DELETE(m_search_params);
	}

	static OP_STATUS Make(FindCallRecordsCallbackAsyncImpl*& new_obj, DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, SearchParams* params)
	{
		FindCallRecordsCallbackAsyncImpl* callback = OP_NEW(FindCallRecordsCallbackAsyncImpl,
			(runtime, this_object, es_callback, params));
		if (!callback)
			return OpStatus::ERR_NO_MEMORY;
		OpAutoPtr<FindCallRecordsCallbackAsyncImpl> ap_callback(callback);
		OP_STATUS error = runtime->Protect(*callback->m_search_params->reference_item);
		if (OpStatus::IsError(error))
		{
			callback->m_search_params->reference_item = NULL;
			return error;
		}
		RETURN_IF_ERROR(callback->Construct());

		ES_Object* results_array;
		RETURN_IF_ERROR(callback->GetRuntime()->CreateNativeArrayObject(&results_array, 0));
		callback->m_results_array_protector.Reset(results_array);
		RETURN_IF_ERROR(callback->m_results_array_protector.Protect());
		new_obj = ap_callback.release();
		return OpStatus::OK;
	}

	virtual BOOL OnItem(OpTelephony::CallRecord* result)
	{
		OP_STATUS error;
		OpAutoPtr<OpTelephony::CallRecord> ap_result(result);
		OP_BOOLEAN matches = IsMatch(result);
		if (matches == OpBoolean::IS_FALSE)
			return TRUE; // not a match, give me next
		else if (matches != OpBoolean::IS_TRUE)
			return FALSE; // ERROR - stop iteration
		if (m_match_count < m_search_params->start_index)
		{
			// m_match_count is lower than specified start_index - skip it
			++m_match_count;
			return TRUE;
		}

		DOM_JILCallRecord* dom_call_record;
		error = DOM_JILCallRecord::Make(dom_call_record, GetRuntime(), result);
		if (OpStatus::IsError(error))
		{
			OnError(error);
			return FALSE;
		}
		ES_Value val;
		DOM_Object::DOMSetObject(&val, dom_call_record);
		error = GetRuntime()->PutIndex(m_results_array_protector.Get(), m_current_index, val);
		if (OpStatus::IsError(error))
		{
			OnError(OpStatus::ERR_NO_MEMORY);
			return FALSE;
		}
		++m_current_index;
		++m_match_count;
		if (m_match_count > m_search_params->end_index)
		{
			// max result reached
			OnFinished();
			return FALSE;
		}
		return TRUE;
	}

	virtual void OnFinished()
	{
		ES_Value argv[1];
		DOM_Object::DOMSetObject(&argv[0], m_results_array_protector.Get());
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

	virtual void OnError(OP_TELEPHONYSTATUS error)
	{
		ES_Value argv[1];
		DOM_Object::DOMSetNull(&(argv[0]));
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

	OP_BOOLEAN IsMatch(OpTelephony::CallRecord* item)
	{
		OP_ASSERT(item);
		OP_BOOLEAN matches = OpBoolean::IS_TRUE;

		time_t reference_start_time = 0;
		if (m_search_params->reference_item->m_startTime)
		{
			ES_Value startTime_val;
			if (OpStatus::IsSuccess(GetRuntime()->GetNativeValueOf(m_search_params->reference_item->m_startTime, &startTime_val)))
			{
				OP_ASSERT(startTime_val.type == VALUE_NUMBER);
				reference_start_time = static_cast<time_t>(startTime_val.value.number / 1000);
			}
			else
				reference_start_time = 0;
		}

		if (m_search_params->reference_item->m_undefnull.address == IS_VALUE)
			matches = JILWildcardCmp(m_search_params->reference_item->m_address.CStr(), item->m_phone_number.IsEmpty() ? NULL : item->m_phone_number.CStr());
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.id == IS_VALUE)
			matches = JILWildcardCmp(m_search_params->reference_item->m_id.CStr(), item->m_id.CStr());
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.name == IS_VALUE)
			matches = JILWildcardCmp(m_search_params->reference_item->m_name.CStr(), item->m_name.IsEmpty() ? NULL : item->m_name.CStr());
		// Doesn't have to check for type because it is already in the initiating Iterate(type).

		// startTime and durationSeconds have different meaning in find comparison than in regular CallRecord.
		// startTime together with durationSeconds creates time interval [startTime, startTime + durationSeconds]
		// in which the startTimes of searched-for CallRecords have to reside.
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_startTime)
			matches = reference_start_time <= item->m_start ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
		if (matches == OpBoolean::IS_TRUE
			&& m_search_params->reference_item->m_undefnull.durationSeconds == IS_VALUE
			&& m_search_params->reference_item->m_startTime)
			matches = static_cast<time_t>(reference_start_time + m_search_params->reference_item->m_durationSeconds) >= item->m_start
				? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;

		return matches;
	}
private:
	FindCallRecordsCallbackAsyncImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, SearchParams* params)
		: DOM_AsyncCallback(runtime, es_callback, this_object)
		, m_results_array_protector(NULL, runtime)
		, m_current_index(0)
		, m_search_params(params)
		, m_match_count(0)
	{}

	DOM_AutoProtectPtr m_results_array_protector;
	int m_current_index;
	SearchParams* m_search_params;
	int m_match_count;
};

int
DOM_JILTelephony::findCallRecords(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_TELEPHONY, DOM_JILTelephony);
	DOM_CHECK_ARGUMENTS_JIL("onn");
	DOM_ARGUMENT_JIL_OBJECT(to_compare, 0, DOM_TYPE_JIL_CALL_RECORD, DOM_JILCallRecord);

	OpAutoPtr<FindCallRecordsCallbackAsyncImpl::SearchParams> search_params(OP_NEW(FindCallRecordsCallbackAsyncImpl::SearchParams, ()));
	if (!search_params.get())
		return ES_NO_MEMORY;
	search_params->start_index = static_cast<int>(argv[1].value.number);
	search_params->end_index = static_cast<int>(argv[2].value.number);
	if (search_params->start_index < 0 || search_params->end_index < 0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Negative index value"));
	if (to_compare->m_undefnull.durationSeconds == IS_VALUE && !to_compare->m_startTime)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("durationSeconds set, but startTime not set."));
	CALL_FAILED_IF_ERROR_WITH_HANDLER(to_compare->CopyTo(search_params->reference_item), HandleError);

	FindCallRecordsCallbackAsyncImpl* callback;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(FindCallRecordsCallbackAsyncImpl::Make(callback, origining_runtime, this_object->GetNativeObject(), self->m_onCallRecordsFoundCallback, search_params.get()), HandleError);
	search_params.release();

	self->m_telephony->Iterate(DOM_JILCallRecordTypes::FromString(to_compare->m_type), callback);
	return ES_FAILED;
}

class DOM_JILTelephonyGetCallRecordCallback : public DOM_SuspendCallback<OpTelephony::GetCallRecordCallback>
{
public:
	DOM_JILTelephonyGetCallRecordCallback(DOM_Runtime* runtime) : dom_result(NULL), m_runtime(runtime) {}
	virtual void OnFailed(OP_STATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
	virtual void OnFinished(const OpTelephony::CallRecord* op_result)
	{
		OP_STATUS error = DOM_JILCallRecord::Make(dom_result, m_runtime, op_result);
		if (OpStatus::IsError(error))
		{
			OnFailed(error);
			return;
		}
		OnSuccess();
	}
	DOM_JILCallRecord* dom_result;
	DOM_Runtime* m_runtime;
};

int
DOM_JILTelephony::getCallRecord(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_TELEPHONY, DOM_JILTelephony);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* call_record_type_str = argv[0].value.string;
	const uni_char* id = argv[1].value.string;
	OpTelephony::CallRecord::Type call_record_type = DOM_JILCallRecordTypes::FromString(call_record_type_str);
	if (call_record_type == OpTelephony::CallRecord::TypeUnknown)
		return HandleError(OpTelephony::Status::ERR_WRONG_CALL_RECORD_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILTelephonyGetCallRecordCallback, callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject3<OpTelephony, OpTelephony::CallRecord::Type, const uni_char*, OpTelephony::GetCallRecordCallback*>
		function(self->m_telephony, &OpTelephony::GetCallRecord, call_record_type, id, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILTelephonyGetCallRecordCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);
	DOMSetObject(return_value, callback->dom_result);

	return ES_VALUE;
}

class DOM_JILTelephonyGetCallRecordCountCallback : public DOM_SuspendCallback<OpTelephony::GetCallRecordCountCallback>
{
public:
	virtual void OnFailed(OP_STATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
	virtual void OnFinished(unsigned int result)
	{
		m_result = result;
		OnSuccess();
	}
	unsigned int m_result;
};

int
DOM_JILTelephony::getCallRecordCnt(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_TELEPHONY, DOM_JILTelephony);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* call_record_type_str = argv[0].value.string;
	OpTelephony::CallRecord::Type call_record_type = DOM_JILCallRecordTypes::FromString(call_record_type_str);
	if (call_record_type == OpTelephony::CallRecord::TypeUnknown)
		return HandleError(OpTelephony::Status::ERR_WRONG_CALL_RECORD_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILTelephonyGetCallRecordCountCallback, callback, return_value, call, ());
	OpMemberFunctionObject2<OpTelephony, OpTelephony::CallRecord::Type, OpTelephony::GetCallRecordCountCallback*>
		function(self->m_telephony, &OpTelephony::GetCallRecordCount, call_record_type, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILTelephonyGetCallRecordCountCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);
	DOMSetNumber(return_value, callback->m_result);

	return ES_VALUE;
}

int
DOM_JILTelephony::initiateVoiceCall(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_TELEPHONY);
	DOM_CHECK_ARGUMENTS_JIL("s");
	const uni_char* phone_number = argv[0].value.string;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DOM_jilUtils->ValidatePhoneNumber(phone_number), HandleError);

	OpApplicationListener* app_listener = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander()->GetApplicationListener();
	CALL_FAILED_IF_ERROR_WITH_HANDLER(app_listener->ExecuteApplication(OpApplicationListener::APPLICATION_PHONE, 1, &phone_number), HandleError);
	return ES_FAILED;
}

/* static */ int
DOM_JILTelephony::HandleError(OP_TELEPHONYSTATUS error, ES_Value* return_value, DOM_Runtime* runtime)
{
	switch (error)
	{
	case OpTelephony::Status::ERR_WRONG_CALL_RECORD_TYPE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("Wrong call record type"));
	case OpTelephony::Status::ERR_CALL_RECORD_PARAMETER:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("Invalid call record parameter"));
	case OpTelephony::Status::ERR_ID_NOT_FOUND:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("Invalid call record id"));
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_UNSUPPORTED_ERR, return_value, runtime, UNI_L("Phone application not available"));
	default:
		return HandleJILError(error, return_value, runtime);
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILTelephony)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::deleteAllCallRecords, "deleteAllCallRecords", "s-", "Telephony.deleteAllCallRecords")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::deleteCallRecord, "deleteCallRecord", "ss-", "Telephony.deleteCallRecord")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::findCallRecords, "findCallRecords", "onn-", "Telephony.findCallRecords")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::getCallRecord, "getCallRecord", "ss-", "Telephony.getCallRecord")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::getCallRecordCnt, "getCallRecordCnt", "s-", "Telephony.getCallRecordCnt")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILTelephony, DOM_JILTelephony::initiateVoiceCall, "initiateVoiceCall", "s-", "Telephony.initiateVoiceCall")
DOM_FUNCTIONS_END(DOM_JILTelephony)

#endif // DOM_JIL_API_SUPPORT

