// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
//

#include "core/pch.h"

#include "platforms/crashlog/gpu_info.h"
#include "modules/libvega/vega3ddevice.h"
#include "platforms/vega_backends/vega_blocklist_file.h"
#include "platforms/vega_backends/vega_blocklist_device.h"

GpuInfo* g_gpu_info;

void FillGpuInfo(VEGA3dDevice* device)
{
	// If we have already filled it out, don't do it again.
	if (g_gpu_info->initialized == GpuInfo::INITIALIZED)
		return;

	OpAutoPtr<VEGABlocklistDevice::DataProvider> provider(device->CreateDataProvider());
	if (!provider.get())
		return;

	OpString info;
	OpString value;

	// Back-end
	RETURN_VOID_IF_ERROR(info.Set(UNI_L("[back-end]") UNI_L(NEWLINE)));
	RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), device->getDescription()));

	// Vendor ID
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("vendor id"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[vendor id]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	// Device ID
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("device id"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[device id]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	// Driver Version
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("driver version"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[driver version]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	// OpenGL vendor
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-vendor"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[OpenGL-vendor]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	// OpenGL Renderer
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-renderer"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[OpenGL-renderer]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	// OpenGL Version
	if (OpStatus::IsSuccess(provider->GetValueForKey(UNI_L("OpenGL-version"), value)) && value.HasContent())
	{
		RETURN_VOID_IF_ERROR(info.Append(UNI_L("[OpenGL-version]") UNI_L(NEWLINE)));
		RETURN_VOID_IF_ERROR(info.AppendFormat(UNI_L("%s") UNI_L(NEWLINE) UNI_L(NEWLINE), value.CStr()));
	}

	OpString8 info8;
	RETURN_VOID_IF_ERROR(info8.SetUTF8FromUTF16(info));
	g_gpu_info->device_info_size = MIN(info8.Length(), GpuInfo::MAX_INFO_LENGTH);
	op_memcpy(g_gpu_info->device_info, info8.CStr(), g_gpu_info->device_info_size);
 	g_gpu_info->initialized = GpuInfo::INITIALIZED;

	g_gpu_info->initialized_device = device;

}


void DeviceDeleted(VEGA3dDevice* device)
{
	if (g_gpu_info->initialized_device == device)
		g_gpu_info->initialized = GpuInfo::NOT_INITIALIZED;
}