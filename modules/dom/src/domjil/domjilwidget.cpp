/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilwidget.h"

#include "modules/database/opstorage.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domjil/domjilpim.h"
#include "modules/dom/src/domjil/domjildevice.h"
#include "modules/dom/src/domjil/domjilmessaging.h"
#include "modules/dom/src/domjil/domjilmultimedia.h"
#include "modules/dom/src/domjil/domjilexceptions.h"
#include "modules/dom/src/domjil/domjiltelephony.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/storage/storageutils.h"
#include "modules/ecmascript_utils/esasyncif.h"


#include "modules/gadgets/OpGadget.h"

/* static */ void
DOM_JILWidget::MakeL(DOM_JILWidget*& new_object, DOM_Runtime* runtime)
{
	new_object = OP_NEW(DOM_JILWidget, ());
	LEAVE_IF_ERROR(DOMSetObjectRuntime(new_object, runtime, runtime->GetPrototype(DOM_Runtime::JIL_WIDGET_PROTOTYPE), "Widget"));
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_PIM, runtime))
	{
		DOM_JILPIM* pim;
		LEAVE_IF_ERROR(DOM_JILPIM::Make(pim, runtime));
		ES_Value pim_val;
		DOMSetObject(&pim_val, pim);
		new_object->PutL("PIM", pim_val, PROP_READ_ONLY | PROP_DONT_DELETE);
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_DEVICE, runtime))
	{
		DOM_JILDevice* device;
		LEAVE_IF_ERROR(DOM_JILDevice::Make(device, runtime));
		ES_Value device_val;
		DOMSetObject(&device_val, device);
		new_object->PutL("Device", device_val, PROP_READ_ONLY | PROP_DONT_DELETE);
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_MESSAGING, runtime))
	{
		DOM_JILMessaging* messaging;
		LEAVE_IF_ERROR(DOM_JILMessaging::Make(messaging, runtime));
		ES_Value messaging_val;
		DOMSetObject(&messaging_val, messaging);
		new_object->PutL("Messaging", messaging_val, PROP_READ_ONLY | PROP_DONT_DELETE);
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_MULTIMEDIA, runtime))
		new_object->m_lazy_multimedia_init = TRUE;

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_EXCEPTION, runtime))
		new_object->PutFunctionL("Exception", OP_NEW(DOM_JILException_Constructor, ()), "Exception", NULL);

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_EXCEPTIONTYPES, runtime))
	{
		DOM_Object* exception_types;
		DOM_JILWidget::CreateExceptionTypesL(exception_types, runtime);
		ES_Value exception_types_value;
		DOMSetObject(&exception_types_value, exception_types);
		new_object->PutL("ExceptionTypes", exception_types_value, PROP_READ_ONLY | PROP_DONT_DELETE);
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_TELEPHONY, runtime))
	{
		DOM_JILTelephony* telephony;
		LEAVE_IF_ERROR(DOM_JILTelephony::Make(telephony, runtime));
		ES_Value telephony_val;
		DOMSetObject(&telephony_val, telephony);
		new_object->PutL("Telephony", telephony_val, PROP_READ_ONLY | PROP_DONT_DELETE);
	}
#ifdef PI_POWER_STATUS
	LEAVE_IF_ERROR(g_op_power_status_monitor->AddListener(new_object));
	runtime->GetEnvironment()->SetJILWidget(new_object);
#endif
}

/* static */ void
DOM_JILWidget::CreateExceptionTypesL(DOM_Object*& new_exception_types_object, DOM_Runtime* runtime)
{
	new_exception_types_object = OP_NEW(DOM_Object, ());
	LEAVE_IF_ERROR(DOMSetObjectRuntime(new_exception_types_object, runtime));
	LEAVE_IF_ERROR(new_exception_types_object->PutStringConstant(UNI_L("INVALID_PARAMETER"), UNI_L("invalid_parameter")));
	LEAVE_IF_ERROR(new_exception_types_object->PutStringConstant(UNI_L("SECURITY"), UNI_L("security")));
	LEAVE_IF_ERROR(new_exception_types_object->PutStringConstant(UNI_L("UNKNOWN"), UNI_L("unknown")));
	LEAVE_IF_ERROR(new_exception_types_object->PutStringConstant(UNI_L("UNSUPPORTED"), UNI_L("unsupported")));
}

/* virtual */ void
DOM_JILWidget::GCTrace()
{
#ifdef PI_POWER_STATUS
	GCMark(m_on_focus);
	GCMark(m_on_maximize);
	GCMark(m_on_restore);
	GCMark(m_on_wakeup);
#endif
}

class PreferenceOperationCallback : public DOM_SuspendCallback<WebStorageOperationCallback>
{
public:
	PreferenceOperationCallback(DOM_Runtime* runtime)
		: m_runtime(runtime)
#ifdef _DEBUG
		, m_is_discarded(FALSE)
#endif // _DEBUG
	{}
	virtual ~PreferenceOperationCallback()
	{
#ifdef _DEBUG
		OP_ASSERT(m_is_discarded);
#endif // _DEBUG
	}

	virtual OP_STATUS HandleOperation(const WebStorageOperation* result)
	{
		if (result->IsError())
		{
			// TODO: could output error to the console
			OnFailed(result->m_error_data.m_error);
		}
		else
			OnSuccess();

		return OpStatus::OK;
	}

	void OnFailure(OP_STATUS error)
	{
		OnFailed(error);
	}

	virtual Window* GetWindow()
	{
		FramesDocument* frm_doc = m_runtime->GetEnvironment()->GetFramesDocument();
		return frm_doc ? frm_doc->GetWindow() : NULL;
	}

	virtual void Discard()
	{
#ifdef _DEBUG
		m_is_discarded = TRUE;
#endif // _DEBUG
	}

private:
	DOM_Runtime* m_runtime;
#ifdef _DEBUG
	BOOL m_is_discarded; // Used for debugging only, see destructor and Discard.
#endif // _DEBUG
};

class GetPreferenceOperationCallback : public PreferenceOperationCallback
{
public:

	GetPreferenceOperationCallback(DOM_Runtime* runtime) : PreferenceOperationCallback(runtime) { }

	virtual OP_STATUS HandleOperation(const WebStorageOperation* result)
	{
		if (!result->IsError())
		{
			if (result->m_data.m_storage.m_value)
			{
				OP_STATUS copy_status = m_value.Copy(result->m_data.m_storage.m_value);
				if (OpStatus::IsSuccess(copy_status))
					OnSuccess();
				else
					OnFailed(copy_status);
			}
			else
				OnSuccess();

			return OpStatus::OK;
		}
		else
			return PreferenceOperationCallback::HandleOperation(result);
	}

	WebStorageValueTemp GetValue()
	{
		return WebStorageValueTemp(m_value);
	}

private:
	WebStorageValue m_value;
};

void
DOM_JILWidget::GetStorageItem(const WebStorageValue *key, GetPreferenceOperationCallback* callback)
{
	OP_STATUS result = m_storage->GetItem(key, callback);
	if (OpStatus::IsError(result))
		callback->OnFailure(result);
}

/* static */ int
DOM_JILWidget::preferenceForKey(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jil_widget, DOM_TYPE_JIL_WIDGET, DOM_JILWidget);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("z");

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	ES_ValueString* vs = argv[0].value.string_with_length;
	WebStorageValueTemp key(vs->string, vs->length);

	CALL_FAILED_IF_ERROR(jil_widget->EnsureStorageObject());

	NEW_SUSPENDING_CALLBACK(GetPreferenceOperationCallback, callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject2<DOM_JILWidget, const WebStorageValue*, GetPreferenceOperationCallback*>
		function(jil_widget, &DOM_JILWidget::GetStorageItem, &key, callback);

	DOM_SUSPENDING_CALL(call, function, GetPreferenceOperationCallback, callback);
	OP_ASSERT(callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandlePreferenceError);

	WebStorageValueTemp value = callback->GetValue();

	if (value.m_value)
	{
		OP_ASSERT(value.m_value);
		TempBuffer* buffer = GetEmptyTempBuf();
		buffer->Append(value.m_value, value.m_value_length);
		DOMSetStringWithLength(return_value, &(g_DOM_globalData->string_with_length_holder), buffer->GetStorage(), value.m_value_length);
	}
	else
		DOMSetUndefined(return_value);

	return ES_VALUE;
}

void
DOM_JILWidget::SetStorageItem(const WebStorageValue *key, const WebStorageValue* value, PreferenceOperationCallback* callback)
{
	OP_STATUS result = m_storage->SetItem(key, value, callback);
	if (OpStatus::IsError(result))
		callback->OnFailure(result);
}

/* static */ int
DOM_JILWidget::setPreferenceForKey(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(jil_widget, DOM_TYPE_JIL_WIDGET, DOM_JILWidget);

	WebStorageValueTemp key;
	WebStorageValueTemp value;
	WebStorageValueTemp* value_pointer = NULL;
	// The arguments are in different order than usual in JIL API versions prior to 1.2.0
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_REVERSE_SET_PREFERENCE_FOR_KEY_ARGS, origining_runtime))
	{
		DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("zZ");
		key.Set(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);
		if (argv[1].type == VALUE_STRING_WITH_LENGTH)
		{
			value.Set(argv[1].value.string_with_length->string, argv[1].value.string_with_length->length);
			value_pointer = &value;
		}
	}
	else
	{
		DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("Zz");

		key.Set(argv[1].value.string_with_length->string, argv[1].value.string_with_length->length);
		if (argv[0].type == VALUE_STRING_WITH_LENGTH)
		{
			value.Set(argv[0].value.string_with_length->string, argv[0].value.string_with_length->length);
			value_pointer = &value;
		}
	}
	DOM_CHECK_OR_RESTORE_PERFORMED;

	CALL_FAILED_IF_ERROR(jil_widget->EnsureStorageObject());

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(PreferenceOperationCallback, callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject3<DOM_JILWidget, const WebStorageValue*, const WebStorageValue*, PreferenceOperationCallback*>
		function(jil_widget, &DOM_JILWidget::SetStorageItem, &key, value_pointer, callback);

	DOM_SUSPENDING_CALL(call, function, PreferenceOperationCallback, callback);
	OP_ASSERT(callback);

	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandlePreferenceError);

	return ES_FAILED;
}

/* static */ int
DOM_JILWidget::openURL(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_WIDGET);
	DOM_CHECK_ARGUMENTS_JIL("s");
	const uni_char* url = argv[0].value.string;

	OpGadget* gadget = g_DOM_jilUtils->GetGadgetFromRuntime(origining_runtime);
	OP_ASSERT(gadget);
	gadget->OpenURLInNewWindow(url);
	return ES_FAILED; // No return value.
}

/* virtual */ ES_GetState
DOM_JILWidget::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	if (property_atom == OP_ATOM_Multimedia)
	{
		if (value && m_lazy_multimedia_init)
		{
			m_lazy_multimedia_init = FALSE;
			DOM_JILMultimedia* multimedia;
			GET_FAILED_IF_ERROR(DOM_JILMultimedia::Make(multimedia, GetRuntime()));
			ES_Value multimedia_val;
			DOMSetObject(&multimedia_val, multimedia);
			TRAPD(error, PutL("Multimedia", multimedia_val, PROP_READ_ONLY | PROP_DONT_DELETE));
			GET_FAILED_IF_ERROR(error);
			DOMSetObject(value, multimedia);
		}
		return GET_SUCCESS;
	}
	else
	{
		ES_Object** event_handler = GetEventHandler(property_atom);
		if (event_handler)
		{
			if (value)
				DOMSetObject(value, *event_handler);
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILWidget::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
#ifdef PI_POWER_STATUS
	ES_Object** event_handler = GetEventHandler(property_atom);

	if (event_handler)
	{
		switch (value->type)
		{
			case VALUE_NULL:
			case VALUE_UNDEFINED:
				*event_handler = NULL;
				break;
			case VALUE_OBJECT:
				*event_handler = value->value.object;
				break;
			default:
				return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
		return PUT_SUCCESS;
	}
#endif // PI_POWER_STATUS
	return PUT_FAILED;
}

ES_Object**
DOM_JILWidget::GetEventHandler(OpAtom property_atom)
{
	switch (property_atom)
	{
		case OP_ATOM_onFocus:
			return &m_on_focus;
		case OP_ATOM_onMaximize:
			return &m_on_maximize;
		case OP_ATOM_onRestore:
			return &m_on_restore;
		case OP_ATOM_onWakeup:
			return &m_on_wakeup;
	}

	return NULL;
}

#ifdef PI_POWER_STATUS
/* virtual */ void
DOM_JILWidget::OnWakeUp(OpPowerStatus* power_status)
{
	if (m_on_wakeup)
	{
		ES_AsyncInterface* es_asyncif = GetEnvironment()->GetAsyncInterface();
		OP_ASSERT(es_asyncif);
		OpStatus::Ignore(es_asyncif->CallFunction(m_on_wakeup, GetNativeObject(), 0, NULL));
	}
}

/* virtual */ void
DOM_JILWidget::OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level)
{
	// TODO: Gold
}

/* virtual */ void
DOM_JILWidget::OnPowerSourceChange(OpPowerStatus* power_status, OpPowerStatusListener::PowerSupplyType power_source)
{
	// TODO: Gold
}

/* virtual */ void
DOM_JILWidget::OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low)
{
	// TODO: Gold
}

/* virtual */ void
DOM_JILWidget::OnBeforeEnvironmentDestroy()
{
	g_op_power_status_monitor->RemoveListener(this);
}
#endif // PI_POWER_STATUS

OP_STATUS
DOM_JILWidget::HandleEvent(DOM_Environment::GadgetEvent event, DOM_Environment::GadgetEventData *data)
{
	ES_Object* handler = NULL;
	switch (event)
	{
		case DOM_Environment::GADGET_EVENT_ONFOCUS:
			handler = m_on_focus;
			break;
		case DOM_Environment::GADGET_EVENT_ONMODECHANGE:
			if (uni_str_eq(data->mode, "fullscreen") || uni_str_eq(data->mode, "maximized"))
				handler = m_on_maximize;
			else if (uni_str_eq(data->mode, "application") || uni_str_eq(data->mode, "widget") || uni_str_eq(data->mode, "floating"))
				handler = m_on_restore;
			break;
	}

	if (handler)
	{
		ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
		OP_ASSERT(async_iface);
		RETURN_IF_ERROR(async_iface->CallFunction(handler, GetNativeObject(), 0, NULL));
	}
	return OpStatus::OK;
}


/* static */ int
DOM_JILWidget::HandlePreferenceError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case PS_Status::ERR_READ_ONLY:
		return CallException(DOM_Object::JIL_UNSUPPORTED_ERR, return_value, origining_runtime, UNI_L("Cannot modify read-only preference"));
	case PS_Status::ERR_QUOTA_EXCEEDED:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Quota exceeded"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

OP_STATUS
DOM_JILWidget::EnsureStorageObject()
{
	if (m_storage != NULL)
		return OpStatus::OK;

	FramesDocument* frames_doc = GetFramesDocument();
	RETURN_VALUE_IF_NULL(frames_doc, OpStatus::ERR);

	OpStorageManager *storage_manager = frames_doc->GetWindow()->DocManager()->GetStorageManager(TRUE);
	RETURN_OOM_IF_NULL(storage_manager);

	OpGadget* gadget = frames_doc->GetWindow()->GetGadget();
	OP_ASSERT(gadget);

	DOM_PSUtils::PS_OriginInfo oi;
	RETURN_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(GetRuntime(), oi));

	OpStorage* storage;
	RETURN_IF_ERROR(storage_manager->GetStorage(WEB_STORAGE_WGT_PREFS, oi.m_context_id, oi.m_origin, oi.m_is_persistent, &storage));

	m_storage = storage;

	return gadget->InitializePreferences();
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILWidget)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILWidget, DOM_JILWidget::preferenceForKey,    "preferenceForKey",    "z-",  "Widget.preferenceForKey")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILWidget, DOM_JILWidget::setPreferenceForKey, "setPreferenceForKey", "ZZ-", "Widget.setPreferenceForKey")
	// openURL isn't protected by security checks so that it behaves exactly the same
	// as DOM_Widget::openURL. If any security checks are to be introduced, both
	// objects should be modified accordingly.
	DOM_FUNCTIONS_FUNCTION(DOM_JILWidget, DOM_JILWidget::openURL,                       "openURL",             "s-")
DOM_FUNCTIONS_END(DOM_JILWidget)

#endif // DOM_JIL_API_SUPPORT
