// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

#include "platforms/windows/installer/DeleteRegKeyOperation.h"
#include "modules/util/opfile/opfile.h"
#include "platforms/windows/windows_ui/registry.h"

DeleteRegKeyOperation::DeleteRegKeyOperation(const HKEY root_key, const OpStringC &key_path)
	: m_root_key(root_key)
{
	m_key_path.Set(key_path);
}

OP_STATUS DeleteRegKeyOperation::Do()
{
	if (m_key_path.IsEmpty())
		return OpStatus::ERR;

	HKEY key;
	DWORD err = RegOpenKeyEx(m_root_key, m_key_path, 0, KEY_QUERY_VALUE, &key);
	if (err == ERROR_FILE_NOT_FOUND)
		return OpStatus::OK;
	else if (err != ERROR_SUCCESS && err != ERROR_ACCESS_DENIED)
		return OpStatus::ERR;

	RegCloseKey(key);

	OpString full_key_path;
	RETURN_IF_ERROR(WindowsUtils::RegkeyToObjectname(m_root_key, m_key_path, full_key_path));

	OP_STATUS status = DeleteAndSaveRecursive(m_root_key, m_key_path, full_key_path);

	if (OpStatus::IsError(status))
		Undo();

	return status;
}

OP_STATUS DeleteRegKeyOperation::DeleteAndSaveRecursive(const HKEY parent_key, const OpStringC &key_path, OpString &full_key_path)
{
	OpAutoHKEY key;

	DWORD err;
	DWORD sec_desc_size = 0;

	err = RegOpenKeyEx(parent_key, key_path, 0, READ_CONTROL, &key);

	//Sometimes, opening the key succeeds even though we have no access
	if (err == ERROR_ACCESS_DENIED ||
		(err == ERROR_SUCCESS && !WindowsUtils::CheckObjectAccess(full_key_path, SE_REGISTRY_KEY, READ_CONTROL)))
	{
		if (err == ERROR_SUCCESS)
			RegCloseKey(key);

		WindowsUtils::RestoreAccessInfo* t;
		if (!WindowsUtils::GiveObjectAccess(full_key_path, SE_REGISTRY_KEY, KEY_READ | DELETE, TRUE, t))
			return OpStatus::ERR;
		err = RegOpenKeyEx(parent_key, key_path, 0, KEY_READ | DELETE, &key);
	}

	if (err != ERROR_SUCCESS)
		return OpStatus::ERR;

	err = RegGetKeySecurity(key, GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, NULL, &sec_desc_size);
	if (err != ERROR_SUCCESS && err != ERROR_INSUFFICIENT_BUFFER)
		return OpStatus::ERR;

	KeyInfo* ki = OP_NEW(KeyInfo, ());
	OpAutoPtr<KeyInfo> key_info(ki);
	RETURN_IF_ERROR(key_info->key_path.Set(key_path));
	RETURN_OOM_IF_NULL(key_info->sec_desc = (PSECURITY_DESCRIPTOR)(OP_NEWA(char, sec_desc_size)));

	//The SACL won't be backed up here (and will thus get lost) because doing so requires administrative privileges.
	err = RegGetKeySecurity(key, GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, key_info->sec_desc, &sec_desc_size);

	if (err != ERROR_SUCCESS)
		return OpStatus::ERR;

	key.reset();
	err = RegOpenKeyEx(parent_key, key_path, 0, KEY_READ | DELETE, &key);

	//Sometimes, opening the key succeeds even though we have no access
	if (err == ERROR_ACCESS_DENIED ||
		(err == ERROR_SUCCESS && !WindowsUtils::CheckObjectAccess(full_key_path, SE_REGISTRY_KEY, KEY_QUERY_VALUE | KEY_SET_VALUE)))
	{
		if (err == ERROR_SUCCESS)
			RegCloseKey(key);

		WindowsUtils::RestoreAccessInfo* t;
		if (!WindowsUtils::GiveObjectAccess(full_key_path, SE_REGISTRY_KEY, KEY_READ | DELETE, TRUE, t))
			return OpStatus::ERR;
		err = RegOpenKeyEx(parent_key, key_path, 0, KEY_READ | DELETE, &key);
	}

	if (err != ERROR_SUCCESS)
		return OpStatus::ERR;
		

	DWORD max_subkey_len;
	DWORD num_values;
	DWORD max_value_name_len;
	DWORD max_value_len;

	//Ignoring the key class here because it doesn't seem possible to read and or write it. (and it practically not used anyway)
	if (RegQueryInfoKey(key, NULL, NULL, NULL, NULL, &max_subkey_len, NULL, &num_values, &max_value_name_len, &max_value_len, NULL, NULL) != ERROR_SUCCESS)
		return OpStatus::ERR;

	max_subkey_len++;
	max_value_name_len++;

	OpAutoArray<uni_char> value_name(OP_NEWA(uni_char, max_value_name_len));
	RETURN_OOM_IF_NULL(value_name.get());

	OpAutoArray<BYTE> value_data(OP_NEWA(BYTE, max_value_len));
	RETURN_OOM_IF_NULL(value_data.get());

	for (UINT i = 0; i < num_values; i++)
	{
		DWORD value_name_len = max_value_name_len;
		DWORD value_len = max_value_len;
		DWORD value_type;

		if (RegEnumValue(key, i, value_name.get(), &value_name_len, NULL, &value_type, value_data.get(), &value_len) != ERROR_SUCCESS)
			return OpStatus::ERR;

		ValueInfo *value_info = OP_NEW(ValueInfo, ());
		value_info->value_type = value_type;
		RETURN_IF_ERROR(value_info->value_name.Set(value_name.get()));
		RETURN_OOM_IF_NULL(value_info->value_data = OP_NEWA(BYTE, value_len));
		value_info->value_data_len = value_len;
		op_memcpy(value_info->value_data, value_data.get(), value_len);

		RETURN_IF_ERROR(key_info->values.Add(value_info));
	}


	RETURN_IF_ERROR(m_keys.Add(key_info.get()));

	key_info.release();

	OpAutoArray<uni_char> subkey_name(OP_NEWA(uni_char, max_subkey_len));
	RETURN_OOM_IF_NULL(subkey_name.get());

	do
	{
		DWORD subkey_len = max_subkey_len;
		DWORD ret = RegEnumKeyEx(key, 0, subkey_name.get(), &subkey_len, NULL, NULL, NULL, NULL);
		if (ret == ERROR_NO_MORE_ITEMS)
			break;
		if (ret != ERROR_SUCCESS)
			return OpStatus::ERR;

		OpString subkey_path;
		RETURN_IF_ERROR(subkey_path.Set(key_path));
		RETURN_IF_ERROR(subkey_path.Append(PATHSEP));
		RETURN_IF_ERROR(subkey_path.Append(subkey_name.get()));

		OpString full_subkey_path;
		RETURN_IF_ERROR(full_subkey_path.Set(full_key_path));
		RETURN_IF_ERROR(full_subkey_path.Append("\\"));
		RETURN_IF_ERROR(full_subkey_path.Append(subkey_name.get()));

		RETURN_IF_ERROR(DeleteAndSaveRecursive(parent_key, subkey_path, full_subkey_path));
	}
	while(TRUE);

	//Need to close the key here, sowe can delete it.
	key.reset();
	RegDeleteKey(parent_key, key_path);

	return OpStatus::OK;
}

void DeleteRegKeyOperation::Undo()
{
	UINT n_keys = m_keys.GetCount();
	for (UINT i = 0; i < n_keys; i++)
	{
		KeyInfo* key_info = m_keys.Get(i);

		SECURITY_ATTRIBUTES sec_attr;
		sec_attr.bInheritHandle = FALSE;
		sec_attr.nLength = sizeof(sec_attr);
		sec_attr.lpSecurityDescriptor = key_info->sec_desc;

		HKEY key;
		DWORD disposition;

		if (RegCreateKeyEx(m_root_key, key_info->key_path.CStr(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, &sec_attr, &key, &disposition) != ERROR_SUCCESS || disposition != REG_CREATED_NEW_KEY)
			continue;

		UINT n_values = key_info->values.GetCount();
		for (UINT j = 0; j < n_values; j++)
		{
			ValueInfo* value_info = key_info->values.Get(j);

			RegSetValueEx(key, value_info->value_name.CStr(), 0, value_info->value_type, value_info->value_data, value_info->value_data_len);
		}

		RegCloseKey(key);
	}

	m_keys.DeleteAll();
}
