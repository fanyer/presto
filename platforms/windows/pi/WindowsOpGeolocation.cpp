/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DOM_GEOLOCATION_SUPPORT

#include <ntddndis.h>
#include "modules/pi/OpGeolocation.h"

#ifndef NDIS_STATUS_INVALID_LENGTH
# define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)	// Taken from ndis.h for WinCE.
#endif // NDIS_STATUS_INVALID_LENGTH
#ifndef NDIS_STATUS_BUFFER_TOO_SHORT
# define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)	// Taken from ndis.h for WinCE.
#endif // NDIS_STATUS_BUFFER_TOO_SHORT

static const int INITIAL_BUF_SIZE		= 8192;			// Good for about 50 APs.
static const int MAX_BUF_SIZE			= 2097152;

class Win32WifiDataProvider
	: public OpGeolocationWifiDataProvider
{
public:
	Win32WifiDataProvider(OpGeolocationDataListener *listener);
	virtual ~Win32WifiDataProvider();
	OP_STATUS Construct();

private:
	OP_STATUS GetAccessPointDataWLAN(OpWifiData *data);
	OP_STATUS GetAccessPointDataNDIS(OpWifiData *data);

	static HANDLE GetFileHandle(const uni_char *device_name);
	static OP_STATUS DefineDosDeviceIfNotExists(const uni_char *device_name);
	static OP_STATUS UndefineDosDevice(const uni_char *device_name);
	static DWORD PerformQuery(HANDLE adapter_handle, BYTE *buffer, DWORD buffer_size, DWORD *bytes_out);

	// From OpGeolocationWifiDataProvider
	OP_STATUS Poll();

	OpAutoVector<OpString> m_interface_service_names;
	DWORD m_oid_buffer_size;
	BOOL m_is_running_vista;
	OpGeolocationDataListener *m_listener;
};

/* static */ OP_STATUS
OpGeolocationWifiDataProvider::Create(OpGeolocationWifiDataProvider *& new_obj, OpGeolocationDataListener *listener)
{
	Win32WifiDataProvider *obj = OP_NEW(Win32WifiDataProvider, (listener));
	if (!obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err;
	if (OpStatus::IsError(err = obj->Construct()))
	{
		OP_DELETE(obj);
		return err;
	}

	new_obj = obj;
	return OpStatus::OK;
}

Win32WifiDataProvider::Win32WifiDataProvider(OpGeolocationDataListener *listener)
	: m_oid_buffer_size(INITIAL_BUF_SIZE)
	, m_is_running_vista(FALSE)
	, m_listener(listener)
{
}

Win32WifiDataProvider::~Win32WifiDataProvider()
{
}

OP_STATUS
Win32WifiDataProvider::Construct()
{
	OSVERSIONINFO version_info = {0};
	version_info.dwOSVersionInfoSize = sizeof(version_info);
	::GetVersionEx(&version_info);

	m_is_running_vista = version_info.dwMajorVersion >= 6 && version_info.dwPlatformId == VER_PLATFORM_WIN32_NT;

	if (m_is_running_vista)
	{
		if (!(HASWlanOpenHandle() && HASWlanCloseHandle() && HASWlanEnumInterfaces() && HASWlanGetNetworkBssList() && HASWlanFreeMemory()))
			return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	else
	{
		HKEY network_cards_key = NULL;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, UNI_L("Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards"), 0, KEY_READ, &network_cards_key) != ERROR_SUCCESS)
			return OpStatus::ERR_NO_ACCESS;

		int i = 0;
		do 
		{
			TCHAR name[512];
			DWORD name_size = 512;
			if (RegEnumKeyEx(network_cards_key, i, name, &name_size, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
				break;

			HKEY hardware_key = NULL;
			if (RegOpenKeyEx(network_cards_key, name, 0, KEY_READ, &hardware_key) != ERROR_SUCCESS)
				break;

			TCHAR service_name[512];
			DWORD service_name_size = 512;
			DWORD type = 0;
			if (RegQueryValueEx(hardware_key, UNI_L("ServiceName"), NULL, &type, reinterpret_cast<LPBYTE>(service_name), &service_name_size) == ERROR_SUCCESS)
			{
				OpString *interface_service_name = OP_NEW(OpString, ());
				if (!interface_service_name || OpStatus::IsError(interface_service_name->Set(service_name)) || OpStatus::IsError(m_interface_service_names.Add(interface_service_name)))
				{
					OP_DELETE(interface_service_name);
					break;
				}
			}
			RegCloseKey(hardware_key);

			i++;
		} while (1);

		RegCloseKey(network_cards_key);
	}

	return OpStatus::OK;
}

OP_STATUS
Win32WifiDataProvider::Poll()
{
	OpWifiData data;
	OP_STATUS err;

#ifdef _DEBUG
	double now = g_op_time_info->GetRuntimeMS();
#endif // _DEBUG

	if (m_is_running_vista)
		err = GetAccessPointDataWLAN(&data);
	else
		err = GetAccessPointDataNDIS(&data);

	if (OpStatus::IsSuccess(err))
		m_listener->OnNewDataAvailable(&data);

#ifdef _DEBUG
	double later = g_op_time_info->GetRuntimeMS();

	double diff = later-now;
	uni_char debug_buf[128];
	uni_snprintf(debug_buf, 127, UNI_L("GEOLOCATION: POLL+NOTIFY TOOK %ldms\r\n"), (unsigned long) diff);
	::OutputDebugString(debug_buf);
#endif // _DEBUG

	return err;
}

OP_STATUS
Win32WifiDataProvider::GetAccessPointDataWLAN(OpWifiData *data)
{
	// Get the handle to the WLAN API.
	DWORD negotiated_version;
	HANDLE wlan_handle = NULL;

	if (OPWlanOpenHandle(1 /* Client version for Windows XP with SP3 and Wireless LAN API for Windows XP with SP2 */, NULL, &negotiated_version, &wlan_handle) != ERROR_SUCCESS)
		return OpStatus::OK;	// not an error to not have wifi installed

	// Get the list of interfaces. WlanEnumInterfaces allocates interface_list.
	WLAN_INTERFACE_INFO_LIST *interface_list = NULL;
	if (OPWlanEnumInterfaces(wlan_handle, NULL, &interface_list) != ERROR_SUCCESS)
		return OpStatus::ERR;

	// Go through the list of interfaces and get the data for each.
	for (DWORD i = 0; i < interface_list->dwNumberOfItems; i++) 
	{
		WLAN_BSS_LIST *bss_list = NULL;
		if (OPWlanGetNetworkBssList(wlan_handle, &interface_list->InterfaceInfo[i].InterfaceGuid, NULL, dot11_BSS_type_any, FALSE, NULL, &bss_list) == ERROR_SUCCESS)
		{
			for (DWORD i = 0; i < bss_list->dwNumberOfItems; i++)
			{
				OpWifiData::AccessPointData *access_point_data;
				access_point_data = OP_NEW(OpWifiData::AccessPointData, ());
				if (access_point_data)
				{
					access_point_data->mac_address.AppendFormat(UNI_L("%02X-%02X-%02X-%02X-%02X-%02X"),
						bss_list->wlanBssEntries[i].dot11Bssid[0],
						bss_list->wlanBssEntries[i].dot11Bssid[1],
						bss_list->wlanBssEntries[i].dot11Bssid[2],
						bss_list->wlanBssEntries[i].dot11Bssid[3],
						bss_list->wlanBssEntries[i].dot11Bssid[4],
						bss_list->wlanBssEntries[i].dot11Bssid[5]);
					access_point_data->signal_strength = bss_list->wlanBssEntries[i].lRssi;
					access_point_data->ssid.SetFromUTF8(reinterpret_cast<const char*>(bss_list->wlanBssEntries[i].dot11Ssid.ucSSID), bss_list->wlanBssEntries[i].dot11Ssid.uSSIDLength);

					data->wifi_towers.Add(access_point_data);
				}
			}

			OPWlanFreeMemory(bss_list);
		}
	}

	OPWlanFreeMemory(interface_list);

	if (OPWlanCloseHandle(wlan_handle, NULL) != ERROR_SUCCESS)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS
Win32WifiDataProvider::GetAccessPointDataNDIS(OpWifiData *data)
{
	for (UINT32 i = 0; i < m_interface_service_names.GetCount(); i++)
	{
		// First, check that we have a DOS device for this adapter.
		if (OpStatus::IsError(DefineDosDeviceIfNotExists(m_interface_service_names.Get(i)->CStr())))
			continue;

		// Get the handle to the device. This will fail if the named device is not
		// valid.
		HANDLE adapter_handle = GetFileHandle(m_interface_service_names.Get(i)->CStr());
		if (adapter_handle == INVALID_HANDLE_VALUE)
			continue;

		// Get the data.
		BYTE *buffer = reinterpret_cast<BYTE*>(op_malloc(m_oid_buffer_size));
		if (!buffer)
		{
			::CloseHandle(adapter_handle);
			return OpStatus::ERR;
		}

		DWORD result;
		do
		{
			DWORD bytes_out = 0;
			result = PerformQuery(adapter_handle, buffer, m_oid_buffer_size, &bytes_out);
			if (result == ERROR_GEN_FAILURE || // Returned by some Intel cards.
				result == ERROR_INSUFFICIENT_BUFFER ||
				result == ERROR_MORE_DATA ||
				result == NDIS_STATUS_INVALID_LENGTH ||
				result == NDIS_STATUS_BUFFER_TOO_SHORT)
			{
				// The buffer we supplied is too small, so increase it. bytes_out should
				// provide the required buffer size, but this is not always the case.
				if (bytes_out > m_oid_buffer_size)
					m_oid_buffer_size = bytes_out;
				else
					m_oid_buffer_size <<= 1;

				// Re-allocate buffer
				if (m_oid_buffer_size > MAX_BUF_SIZE)
				{
					m_oid_buffer_size = INITIAL_BUF_SIZE;  // Reset for next time.
					op_free(buffer);
					buffer = NULL;
					break;
				}

				buffer = reinterpret_cast<BYTE*>(op_realloc(buffer, m_oid_buffer_size));
				if (buffer == NULL)
				{
					m_oid_buffer_size = INITIAL_BUF_SIZE;  // Reset for next time.
					break;
				}
			}
			else
				break; // The buffer is large enough
		} while (1);

		if (result == ERROR_SUCCESS && buffer)
		{
			NDIS_802_11_BSSID_LIST* bssid_list = reinterpret_cast<NDIS_802_11_BSSID_LIST*>(buffer);

			// Walk through the BSS IDs.
			const uint8 *iterator = reinterpret_cast<const uint8*>(&bssid_list->Bssid[0]);
			const uint8 *end_of_buffer = reinterpret_cast<const uint8*>(bssid_list) + m_oid_buffer_size;
			for (int i = 0; i < static_cast<int>(bssid_list->NumberOfItems); i++)
			{
				const NDIS_WLAN_BSSID *bss_id = reinterpret_cast<const NDIS_WLAN_BSSID*>(iterator);
				// Check that the length of this BSS ID is reasonable.
				if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) || iterator + bss_id->Length > end_of_buffer)
					break;

				OpWifiData::AccessPointData *access_point_data;
				access_point_data = OP_NEW(OpWifiData::AccessPointData, ());

				if (access_point_data)
				{
					access_point_data->mac_address.AppendFormat(UNI_L("%02X-%02X-%02X-%02X-%02X-%02X"),
						bss_id->MacAddress[0],
						bss_id->MacAddress[1],
						bss_id->MacAddress[2],
						bss_id->MacAddress[3],
						bss_id->MacAddress[4],
						bss_id->MacAddress[5]);
					access_point_data->signal_strength = bss_id->Rssi;
					// Note that _NDIS_802_11_SSID::Ssid::Ssid is not null-terminated.
					access_point_data->ssid.SetFromUTF8(reinterpret_cast<const char*>(bss_id->Ssid.Ssid), bss_id->Ssid.SsidLength);
					data->wifi_towers.Add(access_point_data);
				}

				// Move to the next BSS ID.
				iterator += bss_id->Length;
			}
		}

		op_free(buffer);

		// Clean up.
		::CloseHandle(adapter_handle);
		UndefineDosDevice(m_interface_service_names.Get(i)->CStr());
	}

	return OpStatus::OK;
}

OP_STATUS
Win32WifiDataProvider::UndefineDosDevice(const uni_char *device_name)
{
	// We remove only the mapping we use, that is \Device\<device_name>.
	OpString target_path;
	RETURN_IF_ERROR(target_path.SetConcat(UNI_L("\\Device\\"), device_name));

	return DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, device_name, target_path.CStr()) ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS
Win32WifiDataProvider::DefineDosDeviceIfNotExists(const uni_char *device_name)
{
	// We create a DOS device name for the device at \Device\<device_name>.
	OpString target_path;
	RETURN_IF_ERROR(target_path.SetConcat(UNI_L("\\Device\\"), device_name));

	TCHAR target[512];
	if (QueryDosDevice(device_name, target, 512) > 0 && target_path.Compare(target) == 0)
		return OpStatus::OK;

	if (GetLastError() != ERROR_FILE_NOT_FOUND)
		return OpStatus::ERR_FILE_NOT_FOUND;

	if (!DefineDosDevice(DDD_RAW_TARGET_PATH, device_name, target_path.CStr()))
		return OpStatus::ERR;

	// Check that the device is really there.
	return QueryDosDevice(device_name, target, 512) > 0 && target_path.Compare(target) == 0 ? OpStatus::OK : OpStatus::ERR;
}

HANDLE
Win32WifiDataProvider::GetFileHandle(const uni_char *device_name)
{
	// We access a device with DOS path \Device\<device_name> at
	// \\.\<device_name>.
	OpString formatted_device_name;
	formatted_device_name.SetConcat(UNI_L("\\\\.\\"), device_name);

	return CreateFile(formatted_device_name.CStr(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, INVALID_HANDLE_VALUE);
}

DWORD
Win32WifiDataProvider::PerformQuery(HANDLE adapter_handle, BYTE *buffer, DWORD buffer_size, DWORD *bytes_out)
{
	DWORD oid = OID_802_11_BSSID_LIST;
	if (!DeviceIoControl(adapter_handle, IOCTL_NDIS_QUERY_GLOBAL_STATS, &oid, sizeof(oid), buffer, buffer_size, bytes_out, NULL))
	{
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

// Empty declarations for unsupported stuff

/* static */ OP_STATUS
OpGeolocationRadioDataProvider::Create(OpGeolocationRadioDataProvider *& new_obj, OpGeolocationDataListener *)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* static */ OP_STATUS
OpGeolocationGpsDataProvider::Create(OpGeolocationGpsDataProvider *& new_obj, OpGeolocationDataListener *)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

#endif // DOM_GEOLOCATION_SUPPORT
