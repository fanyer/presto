/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjildevicestateinfo.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domjil/domjilaccelerometerinfo.h"
#include "modules/dom/src/domjil/domjilconfig.h"

#include "modules/util/language_codes.h"

#include "modules/pi/OpGeolocation.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/device_api/jil/JILNetworkResources.h"
#include "modules/console/opconsoleengine.h"

DOM_JILDeviceStateInfo::DOM_JILDeviceStateInfo()
: m_on_position_retrieved(NULL)
, m_on_screen_change_dimension(NULL)
{
	m_audio_path_strings[JIL_AUDIOPATH_SPEAKER]		= UNI_L("speaker");
	m_audio_path_strings[JIL_AUDIOPATH_RECEIVER]	= UNI_L("receiver");
	m_audio_path_strings[JIL_AUDIOPATH_EARPHONE]	= UNI_L("earphone");
}

/* static */ OP_STATUS
DOM_JILDeviceStateInfo::Make(DOM_JILDeviceStateInfo*& new_object, DOM_Runtime* runtime)
{
	new_object = OP_NEW(DOM_JILDeviceStateInfo, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_object, runtime, runtime->GetPrototype(DOM_Runtime::JIL_DEVICESTATEINFO_PROTOTYPE), "DeviceStateInfo"));

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_ACCELEROMETERINFO, runtime))
	{
		DOM_JILAccelerometerInfo* accelerometer_info;
		RETURN_IF_ERROR(DOM_JILAccelerometerInfo::Make(accelerometer_info, runtime));
		ES_Value accelerometer_info_val;
		DOMSetObject(&accelerometer_info_val, accelerometer_info);
		RETURN_IF_ERROR(runtime->PutName(new_object->GetNativeObject(), UNI_L("AccelerometerInfo"), accelerometer_info_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_CONFIG, runtime))
	{
		DOM_JILConfig* config;
		RETURN_IF_ERROR(DOM_JILConfig::Make(config, runtime));
		ES_Value config_val;
		DOMSetObject(&config_val, config);
		RETURN_IF_ERROR(runtime->PutName(new_object->GetNativeObject(), UNI_L("Config"), config_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_JILDeviceStateInfo::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_position_retrieved);
	GCMark(m_on_screen_change_dimension);
}

/* virtual */ ES_GetState
DOM_JILDeviceStateInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_audioPath:
		{
			if (value)
			{
				OpDeviceStateInfo::AudioOutputDevice path;
				GET_FAILED_IF_ERROR(g_op_device_state_info->GetAudioOutputDevice(&path));
				const uni_char *path_str = GetAudioPathString(path);
				if (path_str)
				{
					TempBuffer* buffer = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(buffer->Append(path_str));
					DOMSetString(value, buffer);
				}
			}
			return GET_SUCCESS;
		}

	case OP_ATOM_backLightOn:
		{
			if (value)
			{
				OP_BOOLEAN backlight = g_op_device_state_info->IsBacklightOn();
				GET_FAILED_IF_ERROR(backlight);
				DOMSetBoolean(value, backlight == OpBoolean::IS_TRUE);
			}
			return GET_SUCCESS;
		}

	case OP_ATOM_keypadLightOn:
		{
			if (value)
			{
				OP_BOOLEAN keypadlight = g_op_device_state_info->IsKeypadBacklightOn();
				GET_FAILED_IF_ERROR(keypadlight);
				DOMSetBoolean(value, keypadlight == OpBoolean::IS_TRUE);
			}
			return GET_SUCCESS;
		}

	case OP_ATOM_language:
		{
			if (value)
			{
				OpString user_languages;
				GET_FAILED_IF_ERROR(g_op_system_info->GetUserLanguages(&user_languages));
				if (user_languages.IsEmpty())
					DOMSetUndefined(value);
				else
				{
					TempBuffer* buffer = GetEmptyTempBuf();
					OpString jil_language;
					GET_FAILED_IF_ERROR(LanguageCodeUtils::ConvertTo3LetterCode(&jil_language, user_languages.CStr()));

					GET_FAILED_IF_ERROR(buffer->Append(jil_language.CStr()));
					DOMSetString(value, buffer);
				}
			}
			return GET_SUCCESS;
		}
	case OP_ATOM_onPositionRetrieved:
		{
			DOMSetObject(value, m_on_position_retrieved);
			return GET_SUCCESS;
		}

	case OP_ATOM_onScreenChangeDimensions:
		{
			DOMSetObject(value, m_on_screen_change_dimension);
			return GET_SUCCESS;
		}

	case OP_ATOM_availableMemory:
		{
			if (value)
			{
				UINT32 avail_mem;
				OP_STATUS status = g_op_system_info->GetAvailablePhysicalMemorySizeMB(&avail_mem);
				if (status == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				else
				{
					GET_FAILED_IF_ERROR(status);
					DOMSetNumber(value, avail_mem * 1024.0);
				}
			}
			return GET_SUCCESS;
		}
	case OP_ATOM_processorUtilizationPercent:
		{
			if (value)
			{
				double utilization_percent;
				OP_STATUS status = g_op_system_info->GetProcessorUtilizationPercent(&utilization_percent);
				if (status == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				else
				{
					GET_FAILED_IF_ERROR(status);
					TempBuffer* buf = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(buf->AppendFormat(UNI_L("%.1f"), utilization_percent * 100.0));
					DOMSetString(value, buf);
				}
			}
			return GET_SUCCESS;
		}
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILDeviceStateInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_availableMemory:
	case OP_ATOM_language:
	case OP_ATOM_audioPath:
	case OP_ATOM_backLightOn:
	case OP_ATOM_keypadLightOn:
	case OP_ATOM_processorUtilizationPercent:
		return PUT_SUCCESS;
	case OP_ATOM_onPositionRetrieved:
		{
			switch (value->type)
			{
			case VALUE_OBJECT:
				m_on_position_retrieved = value->value.object;
				return PUT_SUCCESS;
			case VALUE_NULL:
			case VALUE_UNDEFINED:
				m_on_position_retrieved = NULL;
				return PUT_SUCCESS;
			}

			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}

	case OP_ATOM_onScreenChangeDimensions:
		{
			switch(value->type)
			{
			case VALUE_OBJECT:
				m_on_screen_change_dimension = value->value.object;
				break;
			case VALUE_NULL:
			case VALUE_UNDEFINED:
				m_on_screen_change_dimension = NULL;
				break;
			default:
				return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			origining_runtime->GetFramesDocument()->GetWindow()->SetScreenPropsChangeListener(m_on_screen_change_dimension ? this : NULL);
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

class JILPositionInfoCallbackImpl : public OpGeolocationListener, public OpTimerListener, public DOM_AsyncCallback
{
public:
	JILPositionInfoCallbackImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback)
		: DOM_AsyncCallback(runtime, es_callback, this_object)
	{
		m_deletion_timer.SetTimerListener(this);
	}

	virtual ~JILPositionInfoCallbackImpl()
	{
		g_geolocation->StopReception(this); // if we are destroyed before geoloc request is processed
		Out();
	}

	OP_STATUS SetMethod(const uni_char* method)
	{
		return m_method.Set(method);
	}

	virtual void OnGeolocationUpdated(const OpGeolocation::Position& position)
	{
		GeolocationRequestFinished(&position);
	}

	/** @short Geographical location reception failed.
	 *
	 * This will cancel the ongoing continuous reception of geographical
	 * position previously requested by OpGeolocation::StartReception().
	 *
	 * @param error Type of error that occurred.
	 */
	virtual void OnGeolocationError(const OpGeolocation::Error& /* unused */)
	{
		GeolocationRequestFinished(NULL);
	}

	virtual void OnTimeOut(OpTimer* timer)
	{
		OP_ASSERT(timer == &m_deletion_timer);
		OP_DELETE(this);
	}

	// Clean up and delete the callback
	virtual void OnBeforeDestroy()
	{
		DOM_AsyncCallback::OnBeforeDestroy();
		// Can't defer destruction with timer because this takes place at the shutdown.
		OP_DELETE(this);
	}
private:
	void GeolocationRequestFinished(const OpGeolocation::Position* position)
	{
		ES_Value argv[2];
		ES_Object* position_info;
		OP_STATUS error = DOM_JILDeviceStateInfo::MakePositionInfoObject(&position_info, GetRuntime(), position);
		if (OpStatus::IsError(error)) // if we have OOM here then we will likely fail in scripts later on so no point trying to handle it
			DOM_Object::DOMSetNull(&argv[0]);
		else
			DOM_Object::DOMSetObject(&argv[0], position_info);

		DOM_Object::DOMSetString(&argv[1], m_method.CStr());
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		g_geolocation->StopReception(this);
		if (InList())
			Out();
		m_deletion_timer.Start(0); // hackish solution for deleting OpGeolocation object TODO:refactor
	}

	OpString m_method;
	OpTimer m_deletion_timer;
};

struct GetSelfLocationCallbackImpl: public JilNetworkResources::GetSelfLocationCallback, public DOM_AsyncCallback, public OpTimerListener
{
	int m_cellId;
	double m_latitude, m_longitude, m_accuracy;
	OpTimer m_deletion_timer;
	OpString m_method;

	GetSelfLocationCallbackImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* callback): DOM_AsyncCallback(runtime, callback, this_object)
	{
		m_deletion_timer.SetTimerListener(this);
	}

	virtual ~GetSelfLocationCallbackImpl()
	{
		Out();
	}

	virtual void SetCellId(int cellId)
	{
		m_cellId = cellId;
	}

	virtual void SetLatitude(const double &latitude)
	{
		m_latitude = latitude;
	}

	virtual void SetLongitude(const double &longitude)
	{
		m_longitude = longitude;
	}

	virtual void SetAccuracy(const double &accuracy)
	{
		m_accuracy = accuracy;
	}

	OP_STATUS SetMethod(const uni_char* method)
	{
		return m_method.Set(method);
	}

	virtual void Finished(OP_STATUS error)
	{
		ES_Value argv[2];
		if (OpStatus::IsError(error))
			DOM_Object::DOMSetNull(&argv[0]);
		else
		{
			ES_Object* position_info;
			OpGeolocation::Position position;
			position.capabilities = OpGeolocation::Position::STANDARD;
			unsigned long seconds, milliseconds;
			g_op_time_info->GetWallClock(seconds, milliseconds);
			position.timestamp = g_op_time_info->GetTimeUTC();
			position.latitude = m_latitude;
			position.longitude = m_longitude;
			position.horizontal_accuracy = m_accuracy;
			position.type = OpGeolocation::RADIO;
			position.type_info.radio.cell_id = m_cellId;
			OP_STATUS make_error = DOM_JILDeviceStateInfo::MakePositionInfoObject(&position_info, GetRuntime(), &position);
			if (OpStatus::IsError(make_error))
				DOM_Object::DOMSetNull(&argv[0]);
			else
				DOM_Object::DOMSetObject(&argv[0], position_info);
		}

		DOM_Object::DOMSetString(&argv[1], m_method.CStr());
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		if (InList())
			Out();
		m_deletion_timer.Start(0);
	}

	virtual OP_STATUS SetErrorCodeAndDescription(int code, const uni_char* description)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Message message(OpConsoleEngine::Gadget, OpConsoleEngine::Information);
			RETURN_IF_ERROR(message.message.AppendFormat(UNI_L("JIL Network Protocol: GetSelfLocation returned error: %d (%s)"), code, description));
			TRAP_AND_RETURN(res, g_console->PostMessageL(&message));
		}
		return OpStatus::OK;
#endif // OPERA_CONSOLE
	}

	virtual void OnTimeOut(OpTimer* timer)
	{
		OP_ASSERT(timer == &m_deletion_timer);
		OP_DELETE(this);
	}
};


/* static */ int
DOM_JILDeviceStateInfo::requestPositionInfo(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_DEVICESTATEINFO, DOM_JILDeviceStateInfo);
	DOM_CHECK_ARGUMENTS_JIL("s");
	const uni_char* method = argv[0].value.string;

	if (uni_stri_eq(method, "cellid"))
	{
		GetSelfLocationCallbackImpl* callback = OP_NEW(GetSelfLocationCallbackImpl, (origining_runtime, this_object->GetNativeObject(), this_->m_on_position_retrieved));
		if (!callback)
			return ES_NO_MEMORY;
		OpAutoPtr<GetSelfLocationCallbackImpl> callback_deleter(callback);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->Construct(), HandleJILError);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->SetMethod(method), HandleJILError);
		g_DAPI_network_resources->GetSelfLocation(callback);
		callback_deleter.release();
	}
	else
	{
		JILPositionInfoCallbackImpl* callback = OP_NEW(JILPositionInfoCallbackImpl, (origining_runtime, this_object->GetNativeObject(), this_->m_on_position_retrieved));
		if (!callback)
			return ES_NO_MEMORY;
		OpAutoPtr<JILPositionInfoCallbackImpl> callback_deleter(callback);

		int location_provider_type;
		if (uni_stri_eq(method, "gps") || uni_stri_eq(method, "agps"))
			location_provider_type = OpGeolocation::GPS;
		else
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Unknown method"));

		CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->Construct(), HandleJILError);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->SetMethod(method), HandleJILError);
		callback_deleter.release();

		OpGeolocation::Options geoloc_options;
		geoloc_options.one_shot = TRUE;
		geoloc_options.type = location_provider_type;
		OpStatus::Ignore(g_geolocation->StartReception(geoloc_options, callback)); // we can safely ignore the result as listener error callback will be called if it fails
	}
	return ES_FAILED;
}

OP_STATUS DOM_JILDeviceStateInfo::MakePositionInfoObject(ES_Object** new_position_obj, DOM_Runtime* runtime, const OpGeolocation::Position* position)
{
	RETURN_IF_ERROR(runtime->CreateNativeObject(new_position_obj, runtime->GetPrototype(DOM_Runtime::JIL_POSITIONINFO_PROTOTYPE), "PositionInfo"));

	ES_Value accuracy_val, altitude_val, altitude_accuracy_val, cell_id_val, latitude_val, longitude_val, timestamp_val;
	if (position)
	{
		DOMSetNumber(&accuracy_val, position->horizontal_accuracy);

		if (position->capabilities& OpGeolocation::Position::HAS_ALTITUDE)
			DOMSetNumber(&altitude_val, position->altitude);
		else
			DOMSetNull(&altitude_val);

		if (position->capabilities& OpGeolocation::Position::HAS_ALTITUDEACCURACY)
			DOMSetNumber(&altitude_accuracy_val, position->vertical_accuracy);
		else
			DOMSetNull(&altitude_accuracy_val);

		if (position->type & OpGeolocation::RADIO)
			DOMSetNumber(&cell_id_val, position->type_info.radio.cell_id);
		DOMSetNumber(&latitude_val, position->latitude);
		DOMSetNumber(&longitude_val, position->longitude);
		RETURN_IF_ERROR(DOM_Object::DOMSetDate(&timestamp_val, runtime, position->timestamp));
	}

	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("accuracy"), accuracy_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("altitude"), altitude_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("altitudeAccuracy"), altitude_accuracy_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("cellID"), cell_id_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("latitude"), latitude_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("longitude"), longitude_val, PROP_DONT_DELETE | PROP_READ_ONLY));
	RETURN_IF_ERROR(runtime->PutName(*new_position_obj, UNI_L("timeStamp"), timestamp_val, PROP_DONT_DELETE | PROP_READ_ONLY));

	return OpStatus::OK;
}

const uni_char *DOM_JILDeviceStateInfo::GetAudioPathString(OpDeviceStateInfo::AudioOutputDevice device) const
{
	switch (device)
	{
	case OpDeviceStateInfo::AUDIO_DEVICE_SPEAKER:		return m_audio_path_strings[JIL_AUDIOPATH_SPEAKER];
	case OpDeviceStateInfo::AUDIO_DEVICE_HANDSET:		return m_audio_path_strings[JIL_AUDIOPATH_RECEIVER];
	case OpDeviceStateInfo::AUDIO_DEVICE_HEADPHONES:	return m_audio_path_strings[JIL_AUDIOPATH_EARPHONE];
	default:											OP_ASSERT(!"Unsupported audio device"); // intentional fall through
	}

	return NULL;
}

void DOM_JILDeviceStateInfo::OnScreenPropsChanged()
{
	ES_Value argv[2];
	DOM_Runtime *runtimne = GetRuntime();

	OpScreenProperties screen_properties;
	if (OpStatus::IsError(g_op_screen_info->GetProperties(&screen_properties, runtimne->GetFramesDocument()->GetWindow()->GetOpWindow())))
		return;

	DOM_Object::DOMSetNumber(&argv[0],screen_properties.width);
	DOM_Object::DOMSetNumber(&argv[1],screen_properties.height);

	ES_AsyncInterface* async_iface = runtimne->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async_iface);

	async_iface->CallFunction(m_on_screen_change_dimension, GetNativeObject(), 2, argv, NULL, NULL);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILDeviceStateInfo)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILDeviceStateInfo, DOM_JILDeviceStateInfo::requestPositionInfo, "requestPositionInfo", "s-", "DeviceStateInfo.requestPositionInfo", 0)
DOM_FUNCTIONS_END(DOM_JILDeviceStateInfo)

#endif // DOM_JIL_API_SUPPORT
