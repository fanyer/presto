/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilradioinfo.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
/* virtual */
DOM_JILRadioInfo::~DOM_JILRadioInfo()
{
	g_op_telephony_network_info->RemoveRadioSourceListener(this);
}

/* static */
OP_STATUS DOM_JILRadioInfo::Make(DOM_JILRadioInfo*& new_jil_radio_info, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_jil_radio_info = OP_NEW(DOM_JILRadioInfo, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_jil_radio_info, runtime, runtime->GetPrototype(DOM_Runtime::JIL_RADIOINFO_PROTOTYPE), "RadioInfo"));
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_RADIOSIGNALSOURCETYPES, runtime))
	{
		RETURN_IF_ERROR(new_jil_radio_info->CreateRadioSignalSourceTypes());
	}
	RETURN_IF_ERROR(g_op_telephony_network_info->AddRadioSourceListener(new_jil_radio_info));
	return OpStatus::OK;
}

const uni_char* DOM_JILRadioInfo::RadioSignalSourceValueToString(OpTelephonyNetworkInfo::RadioSignalSource radio_src)
{
	switch (radio_src)
	{
		case OpTelephonyNetworkInfo::RADIO_SIGNAL_CDMA:
			return UNI_L("cdma");
		case OpTelephonyNetworkInfo::RADIO_SIGNAL_LTE:
			return UNI_L("lte");
		case OpTelephonyNetworkInfo::RADIO_SIGNAL_WCDMA:
			return UNI_L("wcdma");
		case OpTelephonyNetworkInfo::RADIO_SIGNAL_TDSCDMA:
			return UNI_L("tdscdma");
		default:
			OP_ASSERT(!"UNHANDLED RADIO SOURCE"); // intentional fall through
		case OpTelephonyNetworkInfo::RADIO_SIGNAL_GSM:
			return UNI_L("gsm");
	}
}

/* virtual */
ES_GetState DOM_JILRadioInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_isRadioEnabled:
		{
			if (value)
			{
				OP_BOOLEAN is_radio_enabled = g_op_telephony_network_info->IsRadioEnabled();
				if (!OpStatus::IsError(is_radio_enabled))
					DOMSetBoolean(value, is_radio_enabled == OpBoolean::IS_TRUE);
				else if (is_radio_enabled == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				else
					return ConvertCallToGetName(HandleJILError(is_radio_enabled, value, origining_runtime), value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_isRoaming:
		{
			if (value)
			{
				OP_BOOLEAN is_roaming = g_op_telephony_network_info->IsRoaming();
				if (!OpStatus::IsError(is_roaming))
					DOMSetBoolean(value, is_roaming == OpBoolean::IS_TRUE);
				else if (is_roaming == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				else
					return ConvertCallToGetName(HandleJILError(is_roaming, value, origining_runtime), value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_radioSignalSource:
		{
			if (value)
			{
				OpTelephonyNetworkInfo::RadioSignalSource radio_source;
				OP_STATUS ret_val = g_op_telephony_network_info->GetRadioSignalSource(&radio_source);
				if (!OpStatus::IsError(ret_val))
				{
					TempBuffer* buffer = GetEmptyTempBuf();
					ret_val = buffer->Append(RadioSignalSourceValueToString(radio_source));
					if (OpStatus::IsSuccess(ret_val))
						DOMSetString(value, buffer);
				}
				else if (ret_val == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				if (OpStatus::IsError(ret_val))
					return ConvertCallToGetName(HandleJILError(ret_val, value, origining_runtime), value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_radioSignalStrengthPercent:
		{
			if (value)
			{
				double signal_strength;
				OP_STATUS ret_val = g_op_telephony_network_info->GetRadioSignalStrength(&signal_strength);
				if (!OpStatus::IsError(ret_val))
					DOMSetNumber(value, signal_strength);
				else if (ret_val == OpStatus::ERR_NOT_SUPPORTED)
					DOMSetUndefined(value);
				else
					return ConvertCallToGetName(HandleJILError(ret_val, value, origining_runtime), value);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_onSignalSourceChange:
			DOMSetObject(value, m_on_radio_source_changed);
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */
ES_PutState DOM_JILRadioInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_isRadioEnabled:
		case OP_ATOM_isRoaming:
		case OP_ATOM_radioSignalSource:
		case OP_ATOM_radioSignalStrengthPercent:
			return PUT_SUCCESS;
		case OP_ATOM_onSignalSourceChange:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_radio_source_changed = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			m_on_radio_source_changed = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	}
	return PUT_FAILED;
}

/* virtual */
void DOM_JILRadioInfo::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_radio_source_changed);
}

/* virtual */
void DOM_JILRadioInfo::OnRadioSourceChanged(OpTelephonyNetworkInfo::RadioSignalSource radio_source, BOOL is_roaming)
{
	if (m_on_radio_source_changed)
	{
		ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
		ES_Value argv[2];
		DOMSetString(&(argv[0]), RadioSignalSourceValueToString(radio_source));
		DOMSetBoolean(&(argv[1]), is_roaming);
		async_iface->CallFunction(m_on_radio_source_changed, GetNativeObject(), sizeof(argv) / sizeof(argv[0]), argv, NULL, NULL);
	}
}

OP_STATUS
DOM_JILRadioInfo::CreateRadioSignalSourceTypes()
{
	ES_Object* radio_signal_sources;
	RETURN_IF_ERROR(GetRuntime()->CreateNativeObjectObject(&radio_signal_sources));
	ES_Value value;
	DOMSetString(&value, UNI_L("cdma"));
	RETURN_IF_ERROR(GetRuntime()->PutName(radio_signal_sources, UNI_L("CDMA"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("gsm"));
	RETURN_IF_ERROR(GetRuntime()->PutName(radio_signal_sources, UNI_L("GSM"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("lte"));
	RETURN_IF_ERROR(GetRuntime()->PutName(radio_signal_sources, UNI_L("LTE"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("tdscdma"));
	RETURN_IF_ERROR(GetRuntime()->PutName(radio_signal_sources, UNI_L("TDSCDMA"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("wcdma"));
	RETURN_IF_ERROR(GetRuntime()->PutName(radio_signal_sources, UNI_L("WCDMA"), value, PROP_READ_ONLY | PROP_DONT_DELETE));

	DOMSetObject(&value, radio_signal_sources);
	RETURN_IF_ERROR(GetRuntime()->PutName(GetNativeObject(), UNI_L("RadioSignalSourceTypes"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	return OpStatus::OK;
}

#endif // DOM_JIL_API_SUPPORT
