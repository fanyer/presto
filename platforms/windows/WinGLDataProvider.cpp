/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef VEGA_BACKEND_OPENGL

#include "WinGLDataProvider.h"
#include "platforms/vega_backends/opengl/vegaglapi.h"
#include "platforms/vega_backends/opengl/vegagldevice.h"
#include "platforms/windows/Windows3dDevice.h"

#include <D3D9.h>

typedef IDirect3D9* (WINAPI * LPDIRECT3DCREATE9PROC)(UINT SDKVersion);


OP_STATUS WinGLDataProvider::GetValueForKey(const uni_char* key, OpString& val)
{
	if (!uni_stricmp(key, UNI_L("vendor id")))
	{
		GetHWInfo();
		val.Empty();
		return val.AppendFormat(UNI_L("0x%.4x"), vendor_id);
	}
	if (!uni_stricmp(key, UNI_L("device id")))
	{
		GetHWInfo();
		val.Empty();
		return val.AppendFormat(UNI_L("0x%.4x"), device_id);
	}

	if (!uni_stricmp(key, UNI_L("driver version")))
	{
		RETURN_IF_ERROR(GetRegInfo());
		return val.Set(driver_version);
	}

	if (!uni_stricmp(key, "OS-kernel-version"))
	{
		OSVERSIONINFO version_info = {0};
		version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		if (!GetVersionEx(&version_info))
			return OpStatus::ERR;
		return val.AppendFormat(UNI_L("%d.%d"), version_info.dwMajorVersion, version_info.dwMinorVersion);
	}

	// Have to initialize opengl now to use glGetString
	OP_ASSERT(g_vegaGlobals.vega3dDevice);
	if (g_vegaGlobals.vega3dDevice)
		RETURN_IF_ERROR(((WindowsGlDevice*)g_vegaGlobals.vega3dDevice)->InitDevice());

	if (!uni_stricmp(key, "OpenGL-vendor"))
		return val.Set((const char*)glGetString(GL_VENDOR));
	if (!uni_stricmp(key, "OpenGL-renderer"))
		return val.Set((const char*)glGetString(GL_RENDERER));
	if (!uni_stricmp(key, "OpenGL-version"))
		return val.Set((const char*)glGetString(GL_VERSION));

	val.Empty();
	return OpStatus::OK;
}

void WinGLDataProvider::GetHWInfo()
{
	if (fetched_hwinfo)
		return;

	// Get the right vendor id and device id. Try first with DX9 enumeration. If that does not work,
	// use Win32 (but then we will get the wrong IDs for Nvidia optimus and ATI Hybrid Graphics cards
	if (!GetHWInfoDX9(vendor_id, device_id))
		GetHWInfoWin32(vendor_id, device_id);

	fetched_hwinfo = TRUE;
}

bool WinGLDataProvider::GetHWInfoDX9(UINT16& vendor_id, UINT16& device_id)
{
	vendor_id = 0;
	device_id = 0;

	HMODULE d3d9_dll = WindowsUtils::SafeLoadLibrary(UNI_L("d3d9.dll"));

	if (!d3d9_dll)
		return false;

	LPDIRECT3DCREATE9PROC pDirect3DCreate9 = (LPDIRECT3DCREATE9PROC)GetProcAddress(d3d9_dll, "Direct3DCreate9");

	if (!pDirect3DCreate9)
		return false;

	LPDIRECT3D9 d3d = pDirect3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	D3DADAPTER_IDENTIFIER9 id;
	if (d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id) != D3D_OK)
	{
		d3d->Release();
		return false;
	}

	vendor_id = (UINT16)id.VendorId;
	device_id = (UINT16)id.DeviceId;

	d3d->Release();

	return true;
}

// Get vendor id and device id from GDI.
void WinGLDataProvider::GetHWInfoWin32(UINT16& vendor_id, UINT16& device_id)
{
	vendor_id = device_id = 0;
	// Get vendor and device id.
	const uni_char* DeviceID = 0;

	DISPLAY_DEVICE dd = {0};
	dd.cb = sizeof(DISPLAY_DEVICE);
	for (DWORD i = 0; EnumDisplayDevices(0, i, &dd, 0); ++i)
	{
		if ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE))
		{
			DeviceID = (const uni_char*)dd.DeviceID;
			break;
		}
	}
	// Now DeviceID should have something like:
	// PCI\VEN_10DE&DEV_1040&SUBSYS_26301462&REV_A1
	// Do a sanity check that the string is long enough (more than 20), and if
	// it is, then get the numbers after VEN_ and DEV_ which is the vendor and device ID.
	if (uni_strlen(DeviceID) > 20)
	{
		long tmp;
		tmp = uni_strtol(DeviceID + 8, 0, 16);
		if (tmp > 0 && tmp <= USHRT_MAX)
			vendor_id = tmp;
		tmp = uni_strtol(DeviceID + 17, 0, 16);
		if (tmp > 0 && tmp <= USHRT_MAX)
			device_id = tmp;
	}
}

OP_STATUS WinGLDataProvider::GetRegInfo()
{
	if (fetched_reginfo)
		return OpStatus::OK;

	GetHWInfo();

	if (!vendor_id || !device_id)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	OpVector<void> cards;
	int i = -1;
	if (CollectDevices(UNI_L("VIDEO"), UNI_L("\\Device\\Video"), cards))
		i = FindMatchingCard(vendor_id, device_id, cards);

	if (i >= 0)
	{
		RegKeyHandle handle((HKEY)cards.Get(i));
		if (handle.QueryInfoKey() == ERROR_SUCCESS)
		{
			if (handle.FindValue(UNI_L("DriverVersion")))
			{
				if (handle.valueType == REG_SZ)
					status = driver_version.Set((const uni_char*)handle.valueBuf);
			}
		}
	}

	for (UINT32 i = 0; i < cards.GetCount(); ++i)
		RegCloseKey((HKEY)cards.Get(i));

	fetched_reginfo = TRUE;

	return status;
}

/**
	Collect registry entries corresponding to devices of type. if mask is provided, only collect entries whose name starts with mask.
	@param the type of devices to collect - should be a subkey in HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP
	@param mask if provided, only collect entries whose value name starts with mask
	@return TRUE if registry was successfully opened (does not mean entries were found), FALSE if registry could not be opened
*/
BOOL WinGLDataProvider::CollectDevices(const uni_char* type, const uni_char* mask, OpVector<void>& keys)
{
	RegKeyHandle maps;
	if (maps.Open(HKEY_LOCAL_MACHINE, UNI_L("HARDWARE\\DEVICEMAP")) != ERROR_SUCCESS)
		return FALSE;
	RegKeyHandle devices;
	if (devices.Open(maps, type) != ERROR_SUCCESS || devices.QueryInfoKey() != ERROR_SUCCESS)
		return FALSE;

	const DWORD mask_len = (DWORD)uni_strlen(mask);

	for (DWORD i = 0; i < devices.cValues; ++i)
	{
		if (devices.EnumValue(i) == ERROR_SUCCESS &&
			(!mask ||
			(devices.valueType == REG_SZ && devices.cchValue >= mask_len && !uni_strnicmp(mask, devices.achValue, mask_len))))
		{
			const size_t offset = op_strlen("\\Registry\\Machine\\");
			if (2*offset < devices.valueLen)
			{
				// value points to registry entry for device
				LPCTSTR key = ((LPCTSTR)devices.valueBuf) + offset;
				RegKeyHandle device;
				if (device.Open(HKEY_LOCAL_MACHINE, key) == ERROR_SUCCESS && OpStatus::IsSuccess(keys.Add((void*)device.Get())))
					device.Release();
			}
		}
	}

	return TRUE;
}

// Returns TRUE if str starts with sub.
static inline BOOL StartsWith(const uni_char* str, const uni_char* sub)
{
	return uni_stristr(str, sub) == str;
}

// Find card matching vendor id and device id from a set of registry keys by looking at the MatchingDeviceId value.
int WinGLDataProvider::FindMatchingCard(UINT16 vendor_id, UINT16 device_id, OpVector<void>& keys)
{
	OpString s;
	if (!s.Reserve(22))
		return -1;

	// Match against vendor and device ids, stored in value named MatchingDeviceId.
	if (OpStatus::IsError(s.AppendFormat("pci\\ven_%.4x&dev_%.4x", vendor_id, device_id)))
		return -1;

	for (UINT32 i = 0; i < keys.GetCount(); ++i)
	{
		HKEY key = (HKEY)keys.Get(i);
		OP_ASSERT(key);
		RegKeyHandle handle(key);

		if (handle.QueryInfoKey() != ERROR_SUCCESS)
			continue;

		if (handle.FindValue(UNI_L("MatchingDeviceId")) &&
			handle.valueType == REG_SZ && StartsWith((const uni_char*)handle.valueBuf, s.CStr()))
			return i;
	}

	return -1;
}

#endif // VEGA_BACKEND_OPENGL
