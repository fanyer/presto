/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjildatanetworkinfo.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/pi/device_api/OpTelephonyNetworkInfo.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

/* static */ OP_STATUS
DOM_JILDataNetworkInfo::Make(DOM_JILDataNetworkInfo*& data_network_info, DOM_Runtime* runtime)
{
	data_network_info = OP_NEW(DOM_JILDataNetworkInfo, ());

	RETURN_IF_ERROR(data_network_info->DOMSetObjectRuntime(data_network_info, runtime, runtime->GetPrototype(DOM_Runtime::JIL_DATANETWORKINFO_PROTOTYPE), "DataNetworkInfo"));

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_DATANETWORKCONNECTIONTYPES, runtime))
	{
		ES_Value data_network_connection_types_value;
		data_network_connection_types_value.type = VALUE_OBJECT;
		RETURN_IF_ERROR(MakeDataNetworkConnectionTypes(data_network_connection_types_value.value.object, runtime));
		RETURN_IF_ERROR(runtime->PutName(data_network_info->GetNativeObject(), UNI_L("DataNetworkConnectionTypes"), data_network_connection_types_value, PROP_READ_ONLY | PROP_DONT_DELETE));
	}
	return OpNetworkInterfaceManager::Create(&data_network_info->m_network_interface_manager, data_network_info);
}

DOM_JILDataNetworkInfo::DOM_JILDataNetworkInfo()
	: m_on_network_connection_changed(NULL)
	, m_network_interface_manager(NULL)
	, m_http_connection(NULL)
{
}

DOM_JILDataNetworkInfo::~DOM_JILDataNetworkInfo()
{
	OP_DELETE(m_network_interface_manager);
}

/* virtual */ void
DOM_JILDataNetworkInfo::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_network_connection_changed);
}

OpNetworkInterface* DOM_JILDataNetworkInfo::GetActiveNetworkInterface()
{
	RETURN_VALUE_IF_ERROR(m_network_interface_manager->BeginEnumeration(), NULL);
	OpNetworkInterface* current_interface = NULL;

	OpNetworkInterface* retval = NULL;
	while (current_interface = m_network_interface_manager->GetNextInterface())
	{
		if (current_interface->GetStatus() == NETWORK_LINK_UP)
		{
			retval = current_interface;
			break;
		}
	}
	m_network_interface_manager->EndEnumeration();
	return retval;
}
BOOL DOM_JILDataNetworkInfo::IsDataNetworkConnected()
{
	return GetActiveNetworkInterface() ? TRUE : FALSE;
}

const uni_char* DOM_JILDataNetworkInfo::GetConnectionTypeString(OpNetworkInterface* network_interface)
{
	OpNetworkInterface::NetworkType type = network_interface->GetNetworkType();

	switch (type)
	{
		case OpNetworkInterface::ETHERNET:
			return UNI_L("ethernet");
		case OpNetworkInterface::CELLULAR_RADIO:
		{
			OpTelephonyNetworkInfo::RadioDataBearer bearer;
			if (OpStatus::IsError(g_op_telephony_network_info->GetRadioDataBearer(&bearer)))
				break;

			switch (bearer)
			{
			case OpTelephonyNetworkInfo::RADIO_DATA_CSD:
				return UNI_L("csd");
			case OpTelephonyNetworkInfo::RADIO_DATA_GPRS:
				return UNI_L("gprs");
			case OpTelephonyNetworkInfo::RADIO_DATA_EDGE:
				return UNI_L("edge");
			case OpTelephonyNetworkInfo::RADIO_DATA_HSCSD:
				return UNI_L("hscsd");
			case OpTelephonyNetworkInfo::RADIO_DATA_EVDO:
				return UNI_L("evdo");
			case OpTelephonyNetworkInfo::RADIO_DATA_LTE:
				return UNI_L("lte");
			case OpTelephonyNetworkInfo::RADIO_DATA_UMTS:
				return UNI_L("umts");
			case OpTelephonyNetworkInfo::RADIO_DATA_HSPA:
				return UNI_L("hspa");
			case OpTelephonyNetworkInfo::RADIO_DATA_ONEXRTT:
				return UNI_L("onexrtt");
			}
			break;
		}
		case OpNetworkInterface::IRDA:
			return UNI_L("irda");
		case OpNetworkInterface::WIFI:
			return UNI_L("wifi");
		case OpNetworkInterface::DIRECT_PC_LINK:
			return UNI_L("cable");
		case OpNetworkInterface::BLUETOOTH:
			return UNI_L("bluetooth");
	}
	return UNI_L("unknown");
}

ES_GetState
DOM_JILDataNetworkInfo::GetNetworkConnectonType(ES_Value* value, DOM_Object* this_object, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	ES_Object* es_result_array = NULL;
	GET_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&es_result_array));
	GET_FAILED_IF_ERROR(m_network_interface_manager->BeginEnumeration());
	OpNetworkInterface* current_interface = NULL;
	int index = 0;
	OP_STATUS error = OpStatus::OK;
	OpINT32Set interfaces;
	while (current_interface = m_network_interface_manager->GetNextInterface())
	{
		if (interfaces.Contains(current_interface->GetNetworkType())) // only one interface of a specific type allowed
			continue;
		ES_Value type_val;
		DOMSetString(&type_val, GetConnectionTypeString(current_interface));
		OP_STATUS error = origining_runtime->PutIndex(es_result_array, index, type_val);
		if (OpStatus::IsError(error))
			break;
		error = interfaces.Add(current_interface->GetNetworkType());
		if (OpStatus::IsError(error))
			break;

		++index;
	}
	m_network_interface_manager->EndEnumeration();
	GET_FAILED_IF_ERROR(error);
	DOMSetObject(value, es_result_array);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_JILDataNetworkInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onNetworkConnectionChanged:
		{
			DOMSetObject(value, m_on_network_connection_changed);
			return GET_SUCCESS;
		}
		case OP_ATOM_isDataNetworkConnected:
		{
			if (value)
				DOMSetBoolean(value, IsDataNetworkConnected());
			return GET_SUCCESS;
		}
		case OP_ATOM_networkConnectionType:
		{
			if (value)
				return GetNetworkConnectonType(value, this, origining_runtime, restart_value);
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILDataNetworkInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onNetworkConnectionChanged:
		{
			OP_ASSERT(restart_value == NULL);
			switch (value->type)
			{
			case VALUE_OBJECT:
				m_on_network_connection_changed = value->value.object;
				m_http_connection = GetActiveNetworkInterface(); //
				return PUT_SUCCESS;
			case VALUE_NULL:
				m_on_network_connection_changed = NULL;
				return PUT_SUCCESS;
			}
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	case OP_ATOM_isDataNetworkConnected:
	case OP_ATOM_networkConnectionType:
		return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* static */ int
DOM_JILDataNetworkInfo::getNetworkConnectionName(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_DATANETWORKINFO, DOM_JILDataNetworkInfo);
	DOM_CHECK_ARGUMENTS_JIL("s");
	const uni_char* connection_type = argv[0].value.string;

	CALL_FAILED_IF_ERROR_WITH_HANDLER(this_->m_network_interface_manager->BeginEnumeration(), HandleJILError);
	OpNetworkInterface* current_interface = NULL;
	while (current_interface = this_->m_network_interface_manager->GetNextInterface())
		if (uni_str_eq(connection_type, GetConnectionTypeString(current_interface))) // should it be case sensitive?
			break;
	this_->m_network_interface_manager->EndEnumeration();
	if (!current_interface)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Unknown interface type"));
	OpString interface_name;
	switch (current_interface->GetNetworkType())
	{
		case OpNetworkInterface::CELLULAR_RADIO:
			CALL_FAILED_IF_ERROR_WITH_HANDLER(current_interface->GetAPN(&interface_name), HandleJILError);
			break;
		case OpNetworkInterface::WIFI:
			CALL_FAILED_IF_ERROR_WITH_HANDLER(current_interface->GetSSID(&interface_name), HandleJILError);
			break;
		default:
			CALL_FAILED_IF_ERROR_WITH_HANDLER(current_interface->GetId(&interface_name), HandleJILError);
			break;
	}
	TempBuffer* buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(interface_name.CStr()));
	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

void DOM_JILDataNetworkInfo::CheckHTTPConnection()
{
	if (!m_on_network_connection_changed)
		return; // no need to do anything if there is no handler set

	OpNetworkInterface *http_connection = GetActiveNetworkInterface();
	if (http_connection == m_http_connection)
		return; // nothing is changed - no need to report anything

	m_http_connection = http_connection;

	if (http_connection == NULL)
		return; // only a new connection should cause a function call to onNetworkConnectionChanged

	ES_AsyncInterface* async_if = GetFramesDocument()->GetESAsyncInterface();
	if (async_if == NULL)
		return; // no way of reporting error specified in JIL

	OpString interface_name;
	RETURN_VOID_IF_ERROR(http_connection->GetId(&interface_name));
	ES_Value argv[1];
	DOMSetString(&(argv[0]), interface_name.CStr());
	RETURN_VOID_IF_ERROR(async_if->CallFunction(m_on_network_connection_changed, GetNativeObject(), 1, argv));
}

/* virtual */
void DOM_JILDataNetworkInfo::OnInterfaceAdded(OpNetworkInterface* network_interface)
{
	CheckHTTPConnection();
}

/* virtual */
void DOM_JILDataNetworkInfo::OnInterfaceRemoved(OpNetworkInterface* network_interface)
{
	CheckHTTPConnection();
}

/* virtual */
void DOM_JILDataNetworkInfo::OnInterfaceChanged(OpNetworkInterface* network_interface)
{
	CheckHTTPConnection();
}

/* static */
OP_STATUS DOM_JILDataNetworkInfo::MakeDataNetworkConnectionTypes(ES_Object*& data_network_connection_types, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(runtime->CreateNativeObjectObject(&data_network_connection_types));

	ES_Value value;
	DOMSetString(&value, UNI_L("bluetooth"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("BLUETOOTH"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("edge"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("EDGE"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("evdo"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("EVDO"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("gprs"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("GPRS"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("irda"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("IRDA"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("lte"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("LTE"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("onexrtt"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("ONEXRTT"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("umts"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("UMTS"), value, PROP_READ_ONLY | PROP_DONT_DELETE));
	DOMSetString(&value, UNI_L("wifi"));
	RETURN_IF_ERROR(runtime->PutName(data_network_connection_types, UNI_L("WIFI"), value, PROP_READ_ONLY | PROP_DONT_DELETE));

	return OpStatus::OK;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILDataNetworkInfo)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILDataNetworkInfo, DOM_JILDataNetworkInfo::getNetworkConnectionName, "getNetworkConnectionName", "s-", "DataNetworkInfo.getNetworkConnectionName")
DOM_FUNCTIONS_END(DOM_JILDataNetworkInfo)

#endif // DOM_JIL_API_SUPPORT
