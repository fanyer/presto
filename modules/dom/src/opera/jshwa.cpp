/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKENDS_USE_BLOCKLIST)

#include "modules/dom/src/opera/jshwa.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/libvega/libvega_module.h"
#include "modules/util/tempbuf.h"

/* static */ OP_STATUS
JS_HWA::Make(JS_HWA *&hwa, JS_Opera *opera)
{
	RETURN_IF_LEAVE(opera->PutObjectL("hwa", hwa = OP_NEW(JS_HWA, ()), "HWA"));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
JS_HWA::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (uni_str_eq(property_name, "enabled"))
	{
		DOMSetNumber(value, g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration));
		return GET_SUCCESS;
	}
	else if (uni_str_eq(property_name, "backend"))
	{
		VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
		if (!device || g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D)
		{
			DOMSetString(value, UNI_L("Software"));
		}
		else
		{
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buffer->Append(device->getDescription()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;
	}
	else if (uni_str_eq(property_name, "details"))
	{
		TempBuffer *buffer = GetEmptyTempBuf();
		VEGA3dDevice* device = g_vegaGlobals.vega3dDevice;
		if (device && value && buffer)
		{
			OpString value;
			VEGABlocklistDevice::DataProvider* provider = device->CreateDataProvider();
			if (provider)
			{
				OP_STATUS retval;
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("vendor id"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("Vendor ID: %s,\n"), value.CStr()));

				value.Empty();
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("device id"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("Device ID: %s,\n"), value.CStr()));

				value.Empty();
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("driver version"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("Driver Version: %s,\n"), value.CStr()));

				value.Empty();
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("OpenGL-vendor"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("GL_VENDOR: %s,\n"), value.CStr()));

				value.Empty();
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("OpenGL-renderer"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("GL_RENDERER: %s,\n"), value.CStr()));

				value.Empty();
				GET_FAILED_IF_MEMORY_ERROR(retval = provider->GetValueForKey(UNI_L("OpenGL-version"), value));
				if (OpStatus::IsSuccess(retval))
					GET_FAILED_IF_ERROR(buffer->AppendFormat(UNI_L("GL_VERSION: %s\n"), value.CStr()));
			}
		}
		DOMSetString(value, buffer);
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
JS_HWA::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("enabled");
	enumerator->AddPropertyL("backend");
	enumerator->AddPropertyL("details");
	return GET_SUCCESS;
}

#endif // VEGA_3DDEVICE && VEGA_BACKENDS_USE_BLOCKLIST
