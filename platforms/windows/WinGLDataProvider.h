/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINGLDATAPROVIDER_H
#define WINGLDATAPROVIDER_H

#ifdef VEGA_BACKEND_OPENGL

#include "platforms/vega_backends/vega_blocklist_file.h"

class WinGLDataProvider : public VEGABlocklistDevice::DataProvider
{
public:

	WinGLDataProvider() : fetched_hwinfo(FALSE), fetched_reginfo(FALSE) {}

	OP_STATUS GetValueForKey(const uni_char* key, OpString& val);

private:

	void GetHWInfo();
	OP_STATUS GetRegInfo();
	bool GetHWInfoDX9(UINT16& vendor_id, UINT16& device_id);
	void GetHWInfoWin32(UINT16& vendor_id, UINT16& device_id);

	int FindMatchingCard(UINT16 vendor_id, UINT16 device_id, OpVector<void>& keys);
	BOOL CollectDevices(const uni_char* type, const uni_char* mask, OpVector<void>& keys);

	BOOL fetched_hwinfo;
	UINT16 vendor_id, device_id;
	BOOL fetched_reginfo;
	OpString driver_version;

};

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 32767 // Registry value names are limited to 32,767 bytes.

// Helper class for reading info from windows registry.
// See http://msdn.microsoft.com/en-us/library/ms724875(v=VS.85).aspx
struct RegKeyHandle
{
private:
	HKEY key;
	const BOOL close; ///< if TRUE, key will be closed from destructor

public:
	// Will not close key.
	RegKeyHandle(HKEY key) : key(key), close(FALSE) {}

	// Will close key.
	RegKeyHandle() : key(0), close(TRUE) {}
	DWORD Open(HKEY parent, LPCTSTR subKeyName) { Close(); return RegOpenKeyEx(parent, subKeyName, 0, KEY_READ, &key); }
	DWORD Open(RegKeyHandle& handle, LPCTSTR subKeyName) { return Open(handle.key, subKeyName); }
	HKEY Release() { HKEY tmp = key; key = 0; return tmp; }
	HKEY Get() { return key; }
	void Close()
	{
		if (!key)
			return;
		RegCloseKey(key);
		Release();
	}

	~RegKeyHandle() { if (close) Close(); }


    TCHAR    achClass[MAX_PATH];     // buffer for class name
    DWORD    cchClassName;           // size of class string
    DWORD    cSubKeys;               // number of subkeys
    DWORD    cbMaxSubKey;            // longest subkey size
    DWORD    cchMaxClass;            // longest class string
    DWORD    cValues;                // number of values for key
    DWORD    cchMaxValue;            // longest value name
    DWORD    cbMaxValueData;         // longest value data
    DWORD    cbSecurityDescriptor;   // size of security descriptor
    FILETIME ftLastWriteTime;        // last write time

	// Get info about key - must be called before any of the below functions are used.
	DWORD QueryInfoKey()
	{
		cchClassName = MAX_PATH;
		cSubKeys = 0;
		cValues = 0;
		*achClass = 0;
		return RegQueryInfoKey(
			key,                     // key handle
			achClass,                // buffer for class name
			&cchClassName,           // size of class string
			NULL,                    // reserved
			&cSubKeys,               // number of subkeys
			&cbMaxSubKey,            // longest subkey size
			&cchMaxClass,            // longest class string
			&cValues,                // number of values for this key
			&cchMaxValue,            // longest value name
			&cbMaxValueData,         // longest value data
			&cbSecurityDescriptor,   // security descriptor
			&ftLastWriteTime);       // last write time
	}

    TCHAR    achKey[MAX_KEY_LENGTH]; // buffer for subkey name
    DWORD    cbName;                 // size of name string

	// Eumerate subkey idx
	// NOTE: QueryInfoKey must have been successfully called before using!
	DWORD EnumKeyEx(DWORD idx)
	{
        cbName = MAX_KEY_LENGTH;
		*achKey = 0;
        return RegEnumKeyEx(key, idx,
            achKey,                  // buffer for subkey name
            &cbName,                 // length of subkey name
            NULL,
            NULL,
            NULL,
            &ftLastWriteTime);
	}

	WCHAR achValue[MAX_VALUE_NAME / 2];
	DWORD cchValue;

	DWORD valueType;
	BYTE valueBuf[MAX_VALUE_NAME];
	DWORD valueLen;

	// Enumerate value idx
	// NOTE: QueryInfoKey must have been successfully called before using!
	DWORD EnumValue(DWORD idx)
	{
		cchValue = MAX_VALUE_NAME / 2;
		*achValue = 0;
		valueLen = MAX_VALUE_NAME;
		*valueBuf = 0;
        return RegEnumValue(key, idx,
            achValue,  // buffer for value name
            &cchValue, // length of value name
            NULL,
            &valueType,      // type of data
            valueBuf,      // data
            &valueLen);  // size of data, in bytes
	}

	// Find value with name 'name' - returns TRUE if value exists, FALSE otherwise.
	// if type, data and dataLen are passed, value can be accessed from data on success.
	// dataLen should correspond to size of data (in bytes), and will be set to the size of the value on success.
	// NOTE: QueryInfoKey must have been successfully called before using!
	BOOL FindValue(const uni_char* name)
	{
        for (DWORD i = 0; i < cValues; ++i)
		{
            if (EnumValue(i) == ERROR_SUCCESS && !uni_stricmp(achValue, name))
				return TRUE;
		}
		return FALSE;
	}
};

#endif
#endif
