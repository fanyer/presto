/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilpim.h"

#include "modules/device_api/jil/JILFSMgr.h"

#include "modules/dom/src/domjil/domjiladdressbookitem.h"
#include "modules/dom/src/domjil/domjilcalendaritem.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domcallstate.h"
#include "modules/dom/domenvironment.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/pi/device_api/OpCalendarListenable.h"
#include "modules/util/opfile/opfile.h"

DOM_JILPIM::DOM_JILPIM()
	: m_on_address_book_items_found(NULL)
	, m_on_calendar_items_found(NULL)
	, m_on_calendar_item_alert(NULL)
	, m_event_reccurence_types(NULL)
	, m_on_v_card_exporting_finish(NULL)
{
}

/* virtual */
DOM_JILPIM::~DOM_JILPIM()
{
	g_op_calendar_listeners->DetachListener(this);
}

#include "modules/util/opfile/opfile.h"
#include "modules/device_api/OpVCardEntry.h"

#include "modules/prefs/prefsmanager/collections/pc_jil.h"

/* virtual */ void
DOM_JILPIM::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_address_book_items_found);
	GCMark(m_event_reccurence_types);
	GCMark(m_on_calendar_items_found);
	GCMark(m_on_calendar_item_alert);
	GCMark(m_on_v_card_exporting_finish);
}

/* static */ OP_STATUS
DOM_JILPIM::Make(DOM_JILPIM*& pim, DOM_Runtime* runtime)
{
	pim = OP_NEW(DOM_JILPIM, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(pim, runtime, runtime->GetPrototype(DOM_Runtime::JIL_PIM_PROTOTYPE), "PIM"));
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_ADDRESSBOOKITEM, runtime))
		RETURN_IF_LEAVE(pim->PutFunctionL("AddressBookItem", OP_NEW(DOM_JILAddressBookItem_Constructor, ()), "AddressBookItem", NULL));

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_CALENDARITEM, runtime))
		RETURN_IF_LEAVE(pim->PutFunctionL("CalendarItem", OP_NEW(DOM_JILCalendarItem_Constructor, ()), "CalendarItem", NULL));

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_EVENTRECURRENCETYPES, runtime))
		RETURN_IF_ERROR(pim->CreateEventReccurenceTypes());

	RETURN_IF_ERROR(g_op_calendar_listeners->AttachListener(pim));
	return OpStatus::OK;
}

OP_STATUS DOM_JILPIM::CreateEventReccurenceTypes()
{
	OP_ASSERT(GetRuntime()); // this should only be called after PIM is added to runtime
	RETURN_IF_ERROR(GetRuntime()->CreateNativeObjectObject(&m_event_reccurence_types));

	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_DAILY_STR, EVENT_RECCURENCE_DAILY_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_EVERY_WEEKDAY_STR, EVENT_RECCURENCE_EVERY_WEEKDAY_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_MONTHLY_ON_DAY_STR, EVENT_RECCURENCE_MONTHLY_ON_DAY_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_MONTHLY_ON_DAY_COUNT_STR, EVENT_RECCURENCE_MONTHLY_ON_DAY_COUNT_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_NOT_REPEAT_STR, EVENT_RECCURENCE_NOT_REPEAT_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_WEEKLY_ON_DAY_STR, EVENT_RECCURENCE_WEEKLY_ON_DAY_STR, GetRuntime()));
	RETURN_IF_ERROR(PutStringConstant(m_event_reccurence_types, EVENT_RECCURENCE_YEARLY_STR, EVENT_RECCURENCE_YEARLY_STR, GetRuntime()));

	ES_Value val;
	DOMSetObject(&val, m_event_reccurence_types);
	return GetRuntime()->PutName(GetNativeObject(), EVENT_RECCURENCE_TYPES_STR, val, PROP_READ_ONLY | PROP_DONT_DELETE);
}

/* virtual */ ES_GetState
DOM_JILPIM::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onAddressBookItemsFound:
			DOMSetObject(value, m_on_address_book_items_found);
			return GET_SUCCESS;
		case OP_ATOM_onCalendarItemsFound:
			DOMSetObject(value, m_on_calendar_items_found);
			return GET_SUCCESS;
		case OP_ATOM_onCalendarItemAlert:
			DOMSetObject(value, m_on_calendar_item_alert);
			return GET_SUCCESS;
		case OP_ATOM_onVCardExportingFinish:
			DOMSetObject(value, m_on_v_card_exporting_finish);
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILPIM::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onAddressBookItemsFound:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_address_book_items_found = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			m_on_address_book_items_found = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	case OP_ATOM_onCalendarItemsFound:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_calendar_items_found = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
		case VALUE_UNDEFINED:
			m_on_calendar_items_found = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	case OP_ATOM_onCalendarItemAlert:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_calendar_item_alert = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
		case VALUE_UNDEFINED:
			m_on_calendar_item_alert = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	case OP_ATOM_onVCardExportingFinish:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_v_card_exporting_finish = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			m_on_v_card_exporting_finish = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	}
	return PUT_FAILED;
}

template<class ElementType, class IterationCallbackType>
class GetCollectionElementCallbackSuspendingImpl : public DOM_SuspendCallback<IterationCallbackType>
{
public:
	GetCollectionElementCallbackSuspendingImpl():
		m_result(NULL)
	{}

	virtual ~GetCollectionElementCallbackSuspendingImpl()
	{
		OP_DELETE(m_result);
	}

	virtual BOOL OnItem(ElementType* result)
	{
		m_result = result;
		DOM_SuspendCallbackBase::OnSuccess();
		return FALSE;
	}

	virtual void OnFinished()
	{
		DOM_SuspendCallbackBase::OnSuccess(); // not found
	}

	virtual void OnError(OP_STATUS error)
	{
		DOM_SuspendCallbackBase::OnFailed(error);
	}

	// This function clears stored result - it hands over the ownership
	ElementType* GetFindResult()
	{
		ElementType* retval = m_result;
		m_result = NULL;
		return retval;
	}
private:
	ElementType* m_result;
};

int
DOM_JILPIM::getAddressBookItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	const uni_char* id = argv[0].value.string;
	typedef GetCollectionElementCallbackSuspendingImpl<OpAddressBookItem, OpAddressBookIterationCallback> GetAddressBookItemCallbackSuspendingImpl;
	NEW_SUSPENDING_CALLBACK(GetAddressBookItemCallbackSuspendingImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpAddressBook, OpAddressBookIterationCallback*, const uni_char*>
		function(g_op_address_book, &OpAddressBook::GetItem, callback, id);

	DOM_SUSPENDING_CALL(call, function, GetAddressBookItemCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleAddressBookError);

	OpAutoPtr<OpAddressBookItem> ap_addr_item(callback->GetFindResult());
	if (ap_addr_item.get())
	{
		DOM_JILAddressBookItem* address_book_item;
		CALL_FAILED_IF_ERROR_WITH_HANDLER(DOM_JILAddressBookItem::Make(address_book_item, ap_addr_item.release(), origining_runtime), HandleAddressBookError);
		DOMSetObject(return_value, address_book_item);
	}
	else
		CALL_FAILED_IF_ERROR_WITH_HANDLER(OpAddressBookStatus::ERR_ITEM_NOT_FOUND, HandleAddressBookError);
	return ES_VALUE;
}

int
DOM_JILPIM::addAddressBookItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("o");
	DOM_ARGUMENT_JIL_OBJECT(dom_addr_item, 0, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);

	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		OpString id;
		CALL_FAILED_IF_ERROR(dom_addr_item->GetAddressBookItem()->GetId(&id));
		if (!id.IsEmpty())
			return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("AddressBookItem already added"));
	}


	int check_result = dom_addr_item->CheckCanBeSaved(return_value, origining_runtime);
	if (check_result != ES_VALUE)
		return check_result;

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(AddressBookModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpAddressBook, OpAddressBookItemModificationCallback*, OpAddressBookItem*>
		function(g_op_address_book, &OpAddressBook::AddItem, callback, dom_addr_item->GetAddressBookItem());
	DOM_SUSPENDING_CALL(call, function, AddressBookModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleAddressBookError);
#ifdef SELFTEST
	TempBuffer* buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(callback->GetElementId()));
	DOMSetString(return_value, buffer);
	return ES_VALUE;
#else
	return ES_FAILED;
#endif
}

/* static */ int
DOM_JILPIM::createAddressBookItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	return DOM_JILAddressBookItem_Constructor::CreateAddressBookItem(return_value, origining_runtime);
}

/* static */ int
DOM_JILPIM::deleteAddressBookItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	const uni_char* item_id = argv[0].value.string;
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(AddressBookModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpAddressBook, OpAddressBookItemModificationCallback*, const uni_char*>
		function(g_op_address_book, &OpAddressBook::RemoveItem, callback, item_id);
	DOM_SUSPENDING_CALL(call, function, AddressBookModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleAddressBookError);
	return ES_FAILED; // success
}

class FindAddressBookItemsCallbackAsyncImpl : public OpAddressBookIterationCallback, public DOM_AsyncCallback
{
public:
	// Can't use SearchParams with indexes 0 .. -1 to return an empty search.
	struct SearchParams
	{
		JIL_AutoAddressBookItemData reference_item;
		unsigned int start_index;
		unsigned int end_index;
		SearchParams() : start_index(0) , end_index(0){}
	};
	FindAddressBookItemsCallbackAsyncImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, OpAutoPtr<SearchParams> params) :
		DOM_AsyncCallback(runtime, es_callback, this_object),
		m_runtime(runtime),
		m_results_array_protector(NULL, runtime),
		m_current_index(0),
		m_search_params(params),
		m_match_count(0)
	{
	}

	OP_STATUS Construct()
	{
		RETURN_IF_ERROR(DOM_AsyncCallback::Construct());
		ES_Object* results_array;
		RETURN_IF_ERROR(GetRuntime()->CreateNativeArrayObject(&results_array, 0));
		m_results_array_protector.Reset(results_array);
		RETURN_IF_ERROR(m_results_array_protector.Protect());
		return OpStatus::OK;
	}

	virtual BOOL OnItem(OpAddressBookItem* result)
	{
		OP_STATUS error = OpStatus::OK;
		JIL_AutoAddressBookItemData result_data(result);
		OP_BOOLEAN match_result = m_search_params->reference_item.IsMatch(&result_data);
		if (OpBoolean::IS_TRUE != match_result)
		{
			if (OpBoolean::IS_FALSE == match_result)
				return TRUE; // not a match, give me next
			else
				return FALSE; // ERROR - stop iteration
		}
		if (m_match_count < m_search_params->start_index)
		{
			// m_match_count is lower than specified start_index - skip it
			++m_match_count;
			return TRUE;
		}
		DOM_JILAddressBookItem* dom_addressbookitem;
		result_data.SetAddressBookItem(NULL);
		error = DOM_JILAddressBookItem::Make(dom_addressbookitem, result, GetRuntime());
		if (OpStatus::IsError(error))
		{
			OnError(error);
			return FALSE;
		}
		ES_Value val;
		DOM_Object::DOMSetObject(&val, dom_addressbookitem);
		error = m_runtime->PutIndex(m_results_array_protector.Get(), m_current_index, val);
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

	virtual void OnError(OP_STATUS error)
	{
		ES_Value argv[1];
		DOM_Object::DOMSetNull(&(argv[0]));
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

private:
	DOM_Runtime* m_runtime;
	DOM_AutoProtectPtr m_results_array_protector;
	unsigned int m_current_index;
	OpAutoPtr<SearchParams> m_search_params;
	unsigned int m_match_count;
};

/* static */ int
DOM_JILPIM::findAddressBookItems(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_PIM, DOM_JILPIM);
	DOM_CHECK_ARGUMENTS_JIL("onN");
	DOM_ARGUMENT_JIL_OBJECT(dom_addr_item, 0, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);

	// We allow infinity but disallow nulls.
	if (argv[2].type != VALUE_NUMBER || op_isnan(argv[2].value.number))
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("End index invalid"));

	/* JIL HAPI 1.2.0, findAddressBookItems, section "Throws" */
	if (argv[2].value.number < 0 || argv[1].value.number < 0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Negative index value"));

	if (argv[1].value.number > argv[2].value.number)
	{
		if (this_->m_on_address_book_items_found)
		{
			// Don't make a query to the platform if not necessary
			ES_AsyncInterface* async_iface = origining_runtime->GetEnvironment()->GetAsyncInterface();
			OP_ASSERT(async_iface);
			ES_Object* empty_array;
			CALL_FAILED_IF_ERROR(this_->GetRuntime()->CreateNativeArrayObject(&empty_array));

			ES_Value async_argv[1];
			DOMSetObject(&async_argv[0], empty_array);
			CALL_FAILED_IF_ERROR_WITH_HANDLER(async_iface->CallFunction(this_->m_on_address_book_items_found, this_->GetNativeObject(), ARRAY_SIZE(async_argv), async_argv, NULL, NULL), HandleAddressBookError);
		}
		return ES_FAILED;
	}

	OP_ASSERT(dom_addr_item->GetAddressBookItem());
	OpAutoPtr<FindAddressBookItemsCallbackAsyncImpl::SearchParams> params(OP_NEW(FindAddressBookItemsCallbackAsyncImpl::SearchParams, ()));
	if (!params.get())
		return ES_NO_MEMORY;
	params->start_index = static_cast<unsigned int>(argv[1].value.number);
	params->end_index = argv[2].value.number < UINT_MAX ? static_cast<unsigned int>(argv[2].value.number) : UINT_MAX;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(params->reference_item.CloneFrom(dom_addr_item->GetAddressBookItemData()), HandleAddressBookError);

	FindAddressBookItemsCallbackAsyncImpl* callback = OP_NEW(FindAddressBookItemsCallbackAsyncImpl
														, (origining_runtime, this_object->GetNativeObject()
														, this_->m_on_address_book_items_found, params));
	if (!callback)
		return ES_NO_MEMORY;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->Construct(), HandleAddressBookError);
	g_op_address_book->Iterate(callback);
	return ES_FAILED;
}

template <class CallbackInterface>
class GetItemCountSuspendingCallbackImpl : public DOM_SuspendCallback<CallbackInterface>
{
public:
	GetItemCountSuspendingCallbackImpl()
		: m_count(0)
	{
	}

	virtual void OnSuccess(unsigned int count)
	{
		m_count = count;
		DOM_SuspendCallbackBase::OnSuccess();
	}

	virtual void OnFailed(OP_STATUS error)
	{
		DOM_SuspendCallbackBase::OnFailed(error);
	}
	unsigned int GetCount() { return m_count; }
private:
	unsigned int m_count;
};

int
DOM_JILPIM::getAddressBookItemsCount(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS("");

	typedef GetItemCountSuspendingCallbackImpl<OpGetAddressBookItemCountCallback> GetAddressBookItemCountCallbackImpl;
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(GetAddressBookItemCountCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject1<OpAddressBook, OpGetAddressBookItemCountCallback*>
		function(g_op_address_book, &OpAddressBook::GetItemCount, callback);
	DOM_SUSPENDING_CALL(call, function, GetAddressBookItemCountCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleAddressBookError);
	DOMSetNumber(return_value, callback->GetCount());
	return ES_VALUE;
}

class ExportAsVCardAsyncTask : public MessageObject, public DOM_AsyncCallback
{
public:
	ExportAsVCardAsyncTask(DOM_Runtime* runtime, ES_Object* callback, ES_Object* this_object, ES_Object* address_book_items)
		: DOM_AsyncCallback(runtime, callback, this_object)
		, m_es_address_book_items(address_book_items)
	{

	}

	virtual ~ExportAsVCardAsyncTask()
	{
		g_main_message_handler->UnsetCallBacks(this);
	}

	virtual OP_STATUS Construct()
	{
		ES_Value length_val;
		OP_BOOLEAN length_found = GetRuntime()->GetName(m_es_address_book_items, UNI_L("length"), &length_val);
		if (length_found != OpBoolean::IS_TRUE || length_val.type != VALUE_NUMBER || length_val.value.number < 0)
			return OpStatus::ERR; // bad argument
		int length = static_cast<int>(length_val.value.number);

		for (int i = 0; i < length; ++i)
		{
			ES_Value item_val;
			OP_BOOLEAN item_found = GetRuntime()->GetIndex(m_es_address_book_items, i, &item_val);
			if (item_found != OpBoolean::IS_TRUE || item_val.type != VALUE_OBJECT)
				return OpStatus::ERR; //Array element is not an addressbookitem
			DOM_HOSTOBJECT_SAFE(addressbook_item, item_val.value.object, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);
			if (!addressbook_item)
				return OpStatus::ERR; //Array element is not an addressbookitem
			OpAddressBookItem* item_copy;
			RETURN_IF_ERROR(OpAddressBookItem::Make(&item_copy, addressbook_item->GetAddressBookItem()));
			OP_ASSERT(item_copy);
			OP_STATUS error = m_address_book_items.Add(item_copy);
			if (error)
			{
				OP_DELETE(item_copy);
				return error;
			}
		}

		OpString destination_path;
		RETURN_IF_LEAVE(g_pcjil->GetStringPrefL(PrefsCollectionJIL::ExportAsVCardDestinationDirectory, destination_path));
		if (!destination_path.IsEmpty())
		{
			RETURN_IF_ERROR(destination_path.AppendFormat(UNI_L("%c%s"), PATHSEPCHAR, UNI_L("vcard.vcf")));
			RETURN_IF_ERROR(m_output_file.Construct(destination_path));
		}
		else
			RETURN_IF_ERROR(m_output_file.Construct(UNI_L("vcard.vcf"), OPFILE_TEMP_FOLDER));
		RETURN_IF_ERROR(m_output_file.Open(OPFILE_WRITE));
		g_main_message_handler->SetCallBack(this, MSG_DOM_JIL_ASYNC_EXPORT_AS_VCARD, reinterpret_cast<MH_PARAM_1>(this));
		return OpStatus::OK;
	}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		OP_ASSERT(msg == MSG_DOM_JIL_ASYNC_EXPORT_AS_VCARD);
		OP_ASSERT(reinterpret_cast<ExportAsVCardAsyncTask*>(par1) == this);
		UINT32 index = static_cast<UINT32>(par2);
		OpAddressBookItem* item = m_address_book_items.Get(index);
		OP_ASSERT(item);
		OpVCardEntry entry;
		OP_STATUS error = entry.ImportFromOpAddressBookItem(item);
		if (OpStatus::IsError(error))
		{
			Finish(error);
			return;
		}
		error = entry.Print(&m_output_file);
		if (OpStatus::IsError(error))
		{
			Finish(error);
			return;
		}
		Continue(index + 1);	
	}

	void Start()
	{
		Continue(0);
	}
private:
	void Finish(OP_STATUS error)
	{
		m_output_file.Close();
		ES_Value callback_argv[1];

		OpString jil_path;
		if (OpStatus::IsSuccess(error))
		{
			if (OpStatus::IsSuccess(g_DAPI_jil_fs_mgr->SystemToJILPath(m_output_file.GetFullPath(), jil_path)))
				DOM_Object::DOMSetString(&callback_argv[0], jil_path.CStr());
			else
				DOM_Object::DOMSetNull(&callback_argv[0]);
		}
		else
		{
			m_output_file.Delete();
			DOM_Object::DOMSetNull(&callback_argv[0]);
		}
		CallCallback(callback_argv, ARRAY_SIZE(callback_argv));
		
		OP_DELETE(this);
	}

	void Continue(UINT32 next_index)
	{
		if (next_index >= m_address_book_items.GetCount())
		{
			Finish(OpStatus::OK);
			return;
		}
		BOOL post_success = g_main_message_handler->PostMessage(MSG_DOM_JIL_ASYNC_EXPORT_AS_VCARD, reinterpret_cast<MH_PARAM_1>(this), next_index); 
		if (!post_success)
			Finish(OpStatus::ERR);
	}
	OpFile m_output_file;
	ES_Object* m_es_address_book_items; // this value may be invalid after Construct - do not use it then
	OpAutoVector<OpAddressBookItem> m_address_book_items;
};

/* static */ int
DOM_JILPIM::exportAsVCard(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(pim, DOM_TYPE_JIL_PIM, DOM_JILPIM);
	DOM_CHECK_ARGUMENTS_JIL("o");
	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(origining_runtime))
		return HandleAddressBookError(OpStatus::ERR_NO_ACCESS, return_value, origining_runtime);
	ExportAsVCardAsyncTask* task = OP_NEW(ExportAsVCardAsyncTask, (origining_runtime, pim->m_on_v_card_exporting_finish, pim->GetNativeObject(), argv[0].value.object));
	if (!task)
		return ES_NO_MEMORY;
	OpAutoPtr<ExportAsVCardAsyncTask> task_deleter(task);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(task->Construct(), HandleAddressBookError);
	task_deleter.release(); // from this on task manages its own lifetime
	task->Start();
	return ES_FAILED;
}

/* static */ int
DOM_JILPIM::HandleAddressBookError(OP_ADDRESSBOOKSTATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpAddressBookStatus::ERR_ITEM_NOT_FOUND:
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Item not found"));
	case OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No such attribute"));
	case OpAddressBookStatus::ERR_NOT_YET_REGISTERED:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Item has not been registered with the address book yet"));
	case OpAddressBookStatus::ERR_NO_SPACE_LEFT:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Not addressbook space left"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

/* static */ int
DOM_JILPIM::HandleCalendarError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* runtime)
{
	switch (error)
	{
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("Item not found"));
	default:
		return HandleJILError(error, return_value, runtime);
	}
}

/* static */ int
DOM_JILPIM::HandleExportCalendarEventError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* runtime)
{
	switch (error)
	{
	case OpStatus::ERR:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, UNI_L("CalendarItem is missing a required value"));
	default:
		return HandleCalendarError(error, return_value, runtime);
	}
}

/* static */ int
DOM_JILPIM::getCalendarItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	const uni_char* id = argv[0].value.string;
	typedef GetCollectionElementCallbackSuspendingImpl<OpCalendarEvent, OpCalendarIterationCallback> GetCalendarItemCallbackSuspendingImpl;
	NEW_SUSPENDING_CALLBACK(GetCalendarItemCallbackSuspendingImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpCalendarService, OpCalendarIterationCallback*, const uni_char*>
		function(g_op_calendar, &OpCalendarService::GetEvent, callback, id);

	DOM_SUSPENDING_CALL(call, function, GetCalendarItemCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleCalendarError);

	OpAutoPtr<OpCalendarEvent> ap_calendar_event(callback->GetFindResult());
	if (ap_calendar_event.get())
	{
		DOM_JILCalendarItem* calendar_item;
		CALL_FAILED_IF_ERROR_WITH_HANDLER(DOM_JILCalendarItem::Make(calendar_item, ap_calendar_event.get(), origining_runtime), HandleAddressBookError);
		DOMSetObject(return_value, calendar_item);
	}
	else
		DOMSetNull(return_value); // JIL's Jira VFOP-9: getCalendarItem should return null for incorrect ID.
	return ES_VALUE;
}

/* static */ int
DOM_JILPIM::deleteCalendarItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");

	const uni_char* item_id = argv[0].value.string;
	typedef JILUtils::ModificationSuspendingCallbackImpl<OpCalendarEventModificationCallback> ModificationCallbackImpl;

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(ModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpCalendarService, OpCalendarEventModificationCallback*, const uni_char*>
		function(g_op_calendar, &OpCalendarService::RemoveEvent, callback, item_id);
	DOM_SUSPENDING_CALL(call, function, ModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleCalendarError);
	return ES_FAILED; // success
}

int
DOM_JILPIM::addCalendarItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("o");
	DOM_ARGUMENT_JIL_OBJECT(dom_calendar_item, 0, DOM_TYPE_JIL_CALENDARITEM, DOM_JILCalendarItem);

	OpCalendarEvent* calendar_event = 0;
	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		if (!dom_calendar_item->GetCalendarItem()->id.IsEmpty())
			return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("CalendarItem already added"));
		CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_calendar_item->PushDatesToCalendarItem(), DOM_JILPIM::HandleExportCalendarEventError);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_calendar_item->GetCalendarItem()->ExportToOpCalendarEvent(calendar_event), HandleExportCalendarEventError);
		OP_ASSERT(calendar_event);
	}
	OpAutoPtr<OpCalendarEvent> calendar_event_deleter(calendar_event);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(CalendarModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpCalendarService, OpCalendarEventModificationCallback*, OpCalendarEvent*>
		function(g_op_calendar, &OpCalendarService::AddEvent, callback, calendar_event);
	DOM_SUSPENDING_CALL(call, function, CalendarModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleCalendarError);
#ifdef SELFTEST
	TempBuffer* buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(callback->GetElementId()));
	DOMSetString(return_value, buffer);
	return ES_VALUE;
#else
	return ES_FAILED;
#endif
}

#ifdef SELFTEST
// Implementation of getCalendarItemsCount only enabled in selftest for now as it was 'ommited' in the spec
int
DOM_JILPIM::getCalendarItemsCount(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_PIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS("");

	typedef GetItemCountSuspendingCallbackImpl<OpGetCalendarEventCountCallback> GetCalendarItemCountSuspendingCallbackImpl;
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(GetCalendarItemCountSuspendingCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject1<OpCalendarService, OpGetCalendarEventCountCallback*>
		function(g_op_calendar, &OpCalendarService::GetEventCount, callback);
	DOM_SUSPENDING_CALL(call, function, GetCalendarItemCountSuspendingCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleCalendarError);
	DOMSetNumber(return_value, callback->GetCount());
	return ES_VALUE;
}
#endif // SELFTEST

class FindCalendarItemsCallbackHelper : public OpCalendarIterationCallback
{
public:
	struct SearchParams
	{
		JILCalendarItem* reference_event;
		int start_index;
		int end_index;
		SearchParams() : reference_event(NULL), start_index(0) , end_index(-1){}
		~SearchParams() { OP_DELETE(reference_event); }
	};
	FindCalendarItemsCallbackHelper(DOM_Runtime* runtime, OpAutoPtr<SearchParams> params) :
		m_results_array_protector(NULL, runtime),
		m_current_index(0),
		m_search_params(params),
		m_match_count(0)
		{
		}

protected:
	OP_STATUS Construct(ES_Runtime* runtime)
	{
		RETURN_OOM_IF_NULL(m_search_params->reference_event);
		ES_Object* results_array;
		RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&results_array, 0));
		m_results_array_protector.Reset(results_array);
		RETURN_IF_ERROR(m_results_array_protector.Protect());
		if (m_search_params->start_index < 0 // start index has to be non negative
			|| m_search_params->end_index < 0) // end index has to be non negative
			return OpStatus::ERR_OUT_OF_RANGE;// bad params
		return OpStatus::OK;
	}

	virtual BOOL OnItem(OpCalendarEvent* result, DOM_Runtime* runtime)
	{
		OP_STATUS error = OpStatus::OK;
		OpAutoPtr<OpCalendarEvent> ap_result(result);
		OP_BOOLEAN match_result = m_search_params->reference_event->IsMatch(result);
		if (match_result != OpBoolean::IS_TRUE)
		{
			if (match_result == OpBoolean::IS_FALSE)
				return TRUE; // not a match, give me next
			else
				return FALSE; // ERROR - stop iteration
		}
		if (m_match_count < m_search_params->start_index)
		{
			// m_match_count is lower than specified start_index - skip it
			++m_match_count;
			return TRUE;
		}
		DOM_JILCalendarItem* dom_calendaritem;
		error = DOM_JILCalendarItem::Make(dom_calendaritem, ap_result.get(), runtime);
		if (OpStatus::IsError(error))
		{
			OnError(error);
			return FALSE;
		}
		ES_Value val;
		DOM_Object::DOMSetObject(&val, dom_calendaritem);
		error = runtime->PutIndex(m_results_array_protector.Get(), m_current_index, val);
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
	DOM_AutoProtectPtr m_results_array_protector;
private:
	int m_current_index;
	OpAutoPtr<SearchParams> m_search_params;
	int m_match_count;
};

class FindCalendarItemsCallbackAsyncImpl : public FindCalendarItemsCallbackHelper, public DOM_AsyncCallback
{
public:
	FindCalendarItemsCallbackAsyncImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, OpAutoPtr<SearchParams> params) :
		  FindCalendarItemsCallbackHelper(runtime, params)
		,  DOM_AsyncCallback(runtime, es_callback, this_object)

	{
	}

	OP_STATUS Construct()
	{
		RETURN_IF_ERROR(DOM_AsyncCallback::Construct());
		return FindCalendarItemsCallbackHelper::Construct(GetRuntime());
	}

	virtual BOOL OnItem(OpCalendarEvent* result)
	{
		return FindCalendarItemsCallbackHelper::OnItem(result, GetRuntime());
	}

	virtual void OnFinished()
	{
		ES_Value argv[1];
		DOM_Object::DOMSetObject(&argv[0], m_results_array_protector.Get());
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

	virtual void OnError(OP_STATUS error)
	{
		ES_Value argv[1];
		DOM_Object::DOMSetNull(&(argv[0]));
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}
};

class GetCalendarItemsCallbackSuspendingImpl : public DOM_SuspendCallbackBase, public FindCalendarItemsCallbackHelper
{
public:
	typedef OpCalendarIterationCallback CallbackInterface;
	GetCalendarItemsCallbackSuspendingImpl(DOM_Runtime* runtime, OpAutoPtr<SearchParams> params)
		: FindCalendarItemsCallbackHelper(runtime, params)
		, m_runtime(runtime)
	{
	}

	OP_STATUS Construct()
	{
		RETURN_IF_ERROR(DOM_SuspendCallbackBase::Construct());
		return FindCalendarItemsCallbackHelper::Construct(m_runtime);
	}

	virtual BOOL OnItem(OpCalendarEvent* result)
	{
		return FindCalendarItemsCallbackHelper::OnItem(result, m_runtime);
	}

	virtual void OnFinished()
	{
		DOM_SuspendCallbackBase::OnSuccess();
	}

	virtual void OnError(OP_STATUS error)
	{
		DOM_SuspendCallbackBase::OnFailed(error);
	}

	ES_Object* GetResults(){ return m_results_array_protector.Get(); }
private:
	DOM_Runtime* m_runtime;
};

OP_STATUS
DOM_JILPIM::GetDateFromDateObject(ES_Value& date_object, double& date)
{
	date = 0;
	if (date_object.type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(date_object.value.object), "Date") == 0)
	{
		ES_Value date_val;
		RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(date_object.value.object, &date_val));
		date = date_val.value.number;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

/* static */ int
DOM_JILPIM::getCalendarItems(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(pim, DOM_TYPE_JIL_PIM, DOM_JILPIM);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("oo");
	double start_time, end_time;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(pim->GetDateFromDateObject(argv[0], start_time), HandleCalendarError);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(pim->GetDateFromDateObject(argv[1], end_time), HandleCalendarError);
	OpAutoPtr<FindCalendarItemsCallbackHelper::SearchParams> params(NULL);
	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		params.reset(OP_NEW(FindCalendarItemsCallbackHelper::SearchParams, ()));
		if (!params.get())
			return ES_NO_MEMORY;
		params->start_index = 0;
		params->end_index = INT_MAX;
		RETURN_OOM_IF_NULL(params->reference_event = OP_NEW(JILCalendarItem, ()));
		params->reference_event->undefnull.start_date = IS_VALUE;
		params->reference_event->undefnull.end_date = IS_VALUE;
		params->reference_event->start_date = start_time;
		params->reference_event->end_date = end_time;
	}
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);

	NEW_SUSPENDING_CALLBACK(GetCalendarItemsCallbackSuspendingImpl, callback, return_value, call, (origining_runtime, params));
	OpMemberFunctionObject3<OpCalendarService, OpCalendarIterationCallback*, double, double>
		function(g_op_calendar, &OpCalendarService::Iterate, callback, start_time ,end_time);

	DOM_SUSPENDING_CALL(call, function, GetCalendarItemsCallbackSuspendingImpl, callback);
	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleCalendarError);

	DOMSetObject(return_value, callback->GetResults());
	return ES_VALUE;
}

/* static */ int
DOM_JILPIM::findCalendarItems(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(pim, DOM_TYPE_JIL_PIM, DOM_JILPIM);
	DOM_CHECK_ARGUMENTS_JIL("onn");
	DOM_ARGUMENT_JIL_OBJECT(dom_calendar_event, 0, DOM_TYPE_JIL_CALENDARITEM, DOM_JILCalendarItem);
	OP_ASSERT(dom_calendar_event->GetCalendarItem());

	CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_calendar_event->PushDatesToCalendarItem(), DOM_JILPIM::HandleExportCalendarEventError);
	double start_time = dom_calendar_event->GetCalendarItem()->undefnull.start_date == IS_VALUE ? dom_calendar_event->GetCalendarItem()->start_date : -g_stdlib_infinity;
	double end_time = dom_calendar_event->GetCalendarItem()->undefnull.end_date == IS_VALUE ? dom_calendar_event->GetCalendarItem()->end_date : g_stdlib_infinity;

	OpAutoPtr<FindCalendarItemsCallbackAsyncImpl::SearchParams> params(OP_NEW(FindCalendarItemsCallbackAsyncImpl::SearchParams, ()));
	if (!params.get())
		return ES_NO_MEMORY;

	params->start_index = static_cast<int>(argv[1].value.number);
	params->end_index = static_cast<int>(argv[2].value.number);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(JILCalendarItem::Make(params->reference_event, dom_calendar_event->GetCalendarItem()), HandleCalendarError);

	/* JIL HAPI 1.2.0, findCalendarItems, section "Throws" */
	if (params->start_index < 0 || params->end_index < 0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Negative index value"));

	if (params->start_index > params->end_index)
	{
		if (pim->m_on_calendar_items_found)
		{
			// Shortcircuit - dont make a query to the platform if we dont have to.
			ES_AsyncInterface* async_iface = origining_runtime->GetEnvironment()->GetAsyncInterface();
			OP_ASSERT(async_iface);
			ES_Object* empty_array;
			CALL_FAILED_IF_ERROR(pim->GetRuntime()->CreateNativeArrayObject(&empty_array));

			ES_Value async_argv[1];
			DOMSetObject(&async_argv[0], empty_array);
			CALL_FAILED_IF_ERROR_WITH_HANDLER(async_iface->CallFunction(pim->m_on_calendar_items_found, pim->GetNativeObject(), ARRAY_SIZE(async_argv), async_argv, NULL, NULL), HandleCalendarError);
		}
		return ES_FAILED;
	}

	FindCalendarItemsCallbackAsyncImpl* callback = OP_NEW(FindCalendarItemsCallbackAsyncImpl
														, (origining_runtime, this_object->GetNativeObject()
														, pim->m_on_calendar_items_found, params));
	if (!callback)
		return ES_NO_MEMORY;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->Construct(), HandleCalendarError);
	g_op_calendar->Iterate(callback, start_time ,end_time);
	return ES_FAILED;
}

/* virtual */ void
DOM_JILPIM::OnEvent(OpCalendarService* calendar, OpCalendarEvent* evt)
{
	OP_ASSERT(calendar);
	OP_ASSERT(evt);
	if (!m_on_calendar_item_alert)
		return;

	ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async_iface);

	DOM_JILCalendarItem* calendar_item = 0;
	RETURN_VOID_IF_ERROR(DOM_JILCalendarItem::Make(calendar_item, evt, GetRuntime()));
	OP_ASSERT(calendar_item);
	ES_Value argv[1];
	DOMSetObject(&argv[0], calendar_item);
	OpStatus::Ignore(async_iface->CallFunction(m_on_calendar_item_alert, GetNativeObject(), ARRAY_SIZE(argv), argv, NULL, NULL));
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILPIM)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::getAddressBookItem,       "getAddressBookItem",       "s-",   "PIM.getAddressBookItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::addAddressBookItem,       "addAddressBookItem",       "",     "PIM.addAddressBookItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::createAddressBookItem,    "createAddressBookItem",    "",     "PIM.createAddressBookItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::deleteAddressBookItem,    "deleteAddressBookItem",    "s-",   "PIM.deleteAddressBookItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::findAddressBookItems,     "findAddressBookItems",     "-nN-", "PIM.findAddressBookItems")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::getAddressBookItemsCount, "getAddressBookItemsCount", "",     "PIM.getAddressBookItemsCount")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::getCalendarItem,       "getCalendarItem",       "s-",   "PIM.getCalendarItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::addCalendarItem,       "addCalendarItem",       "",     "PIM.addCalendarItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::deleteCalendarItem,    "deleteCalendarItem",    "s-",   "PIM.deleteCalendarItem")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::getCalendarItems,       "getCalendarItems",       "oo-",   "PIM.getCalendarItems")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::findCalendarItems,       "findCalendarItems",       "-nn-",   "PIM.findCalendarItems")
#ifdef SELFTEST
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::getCalendarItemsCount,    "getCalendarItemsCount",       "",   "PIM.getCalendarItemsCount")
#endif // SELFTEST
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILPIM, DOM_JILPIM::exportAsVCard,	           "exportAsVCard",            "",     "PIM.exportAsVCard")
DOM_FUNCTIONS_END(DOM_JILPIM)

#endif // DOM_JIL_API_SUPPORT
