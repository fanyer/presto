/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjildeviceinfo.h"

#include "modules/doc/frm_doc.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"

/* static */ OP_STATUS
DOM_JILDeviceInfo::Make(DOM_JILDeviceInfo*& new_object, DOM_Runtime* runtime)
{
	new_object = OP_NEW(DOM_JILDeviceInfo, ());
	return DOMSetObjectRuntime(new_object, runtime, runtime->GetPrototype(DOM_Runtime::JIL_DEVICEINFO_PROTOTYPE), "DeviceInfo");
}

/* virtual */ ES_GetState
DOM_JILDeviceInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_phoneColorDepthDefault:
		{
			if (value)
			{
				OpScreenProperties screen_properties;
				GET_FAILED_IF_ERROR(g_op_screen_info->GetProperties(&screen_properties, origining_runtime->GetFramesDocument()->GetWindow()->GetOpWindow()));
				DOMSetNumber(value, screen_properties.bits_per_pixel);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneScreenHeightDefault:
		{
			if (value)
			{
				OpScreenProperties screen_properties;
				GET_FAILED_IF_ERROR(g_op_screen_info->GetProperties(&screen_properties, origining_runtime->GetFramesDocument()->GetWindow()->GetOpWindow()));
				if (screen_properties.orientation == SCREEN_ORIENTATION_0 || screen_properties.orientation == SCREEN_ORIENTATION_180)
					DOMSetNumber(value, screen_properties.height);
				else
					DOMSetNumber(value, screen_properties.width);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneScreenWidthDefault:
		{
			if (value)
			{
				OpScreenProperties screen_properties;
				GET_FAILED_IF_ERROR(g_op_screen_info->GetProperties(&screen_properties, origining_runtime->GetFramesDocument()->GetWindow()->GetOpWindow()));
				if (screen_properties.orientation == SCREEN_ORIENTATION_0 || screen_properties.orientation == SCREEN_ORIENTATION_180)
					DOMSetNumber(value, screen_properties.width);
				else
					DOMSetNumber(value, screen_properties.height);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneFirmware:
		case OP_ATOM_phoneSoftware:
		{
			if (value)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				OpString string;
				GET_FAILED_IF_ERROR(g_op_system_info->GetDeviceFirmware(&string));
				GET_FAILED_IF_ERROR(buffer->Append(string.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneManufacturer:
		{
			if (value)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				OpString string;
				GET_FAILED_IF_ERROR(g_op_system_info->GetDeviceManufacturer(&string));
				GET_FAILED_IF_ERROR(buffer->Append(string.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneModel:
		{
			if (value)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				OpString string;
				GET_FAILED_IF_ERROR(g_op_system_info->GetDeviceModel(&string));
				GET_FAILED_IF_ERROR(buffer->Append(string.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneOS:
		{
			if (value)
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(g_op_system_info->GetPlatformStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_totalMemory:
		{
			if (value)
#ifdef OPSYSTEMINFO_GETPHYSMEM
				DOMSetNumber(value, static_cast<double>(g_op_system_info->GetPhysicalMemorySizeMB()) * 1024);
#else
				DOMSetUndefined(value);
#endif // OPSYSTEMINFO_GETAVAILMEM
			return GET_SUCCESS;
		}
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILDeviceInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_phoneColorDepthDefault:
		case OP_ATOM_phoneFirmware:
		case OP_ATOM_phoneManufacturer:
		case OP_ATOM_phoneModel:
		case OP_ATOM_phoneOS:
		case OP_ATOM_phoneScreenHeightDefault:
		case OP_ATOM_phoneScreenWidthDefault:
		case OP_ATOM_phoneSoftware:
		case OP_ATOM_totalMemory:
			return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

#endif // DOM_JIL_API_SUPPORT
