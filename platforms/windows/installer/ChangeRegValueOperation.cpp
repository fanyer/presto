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

#include "platforms/windows/installer/ChangeRegValueOperation.h"
#include "platforms/windows/installer/CreateRegKeyOperation.h"

ChangeRegValueOperation::ChangeRegValueOperation(HKEY root_key, const OpStringC &key_path, const OpStringC &value_name, const BYTE* value_data, const DWORD value_data_type, const DWORD value_data_size)
	: m_root_key(root_key)
	, m_new_data(value_data)
	, m_new_type(value_data_type)
	, m_new_data_size(value_data_size)
	, m_create_key_op(NULL)
	, m_old_data(NULL)
	, m_restore_access_info(NULL)
{
	if (OpStatus::IsError(m_key_path.Set(key_path)))
		m_key_path.Empty();
	if (OpStatus::IsError(m_value_name.Set(value_name)))
		m_key_path.Empty();
}

OP_STATUS ChangeRegValueOperation::Do()
{
	if (m_key_path.IsEmpty())
		return OpStatus::ERR;

	HKEY key;

	OpString full_key_name;
	RETURN_IF_ERROR(WindowsUtils::RegkeyToObjectname(m_root_key, m_key_path, full_key_name));

	LONG result = RegOpenKeyEx(m_root_key, m_key_path.CStr(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
	
	if (result == ERROR_FILE_NOT_FOUND)
	{
		if (m_new_type == REG_DELETE)
			return OpStatus::OK;
		RETURN_OOM_IF_NULL(m_create_key_op = OP_NEW(CreateRegKeyOperation, (m_root_key, m_key_path)));
		RETURN_IF_ERROR(m_create_key_op->Do());
		result = RegOpenKeyEx(m_root_key, m_key_path.CStr(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
	}
	//Sometimes, opening the key succeeds even though we have no access
	if (result == ERROR_ACCESS_DENIED ||
		(result == ERROR_SUCCESS && !WindowsUtils::CheckObjectAccess(full_key_name, SE_REGISTRY_KEY, KEY_QUERY_VALUE | KEY_SET_VALUE)))
	{
		if (result == ERROR_SUCCESS)
			RegCloseKey(key);
		if (m_restore_access_info || !WindowsUtils::GiveObjectAccess(full_key_name, SE_REGISTRY_KEY, KEY_QUERY_VALUE | KEY_SET_VALUE, FALSE, m_restore_access_info))
			return OpStatus::ERR;
		result = RegOpenKeyEx(m_root_key, m_key_path.CStr(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
	}
	if (result != ERROR_SUCCESS)
		return OpStatus::ERR;

	OP_STATUS err = OpStatus::ERR;

	do
	{
		LONG result = RegQueryValueEx(key, m_value_name.CStr(), NULL, NULL, NULL, &m_old_data_size);
		if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
			break;

		if (result == ERROR_SUCCESS)
		{
			if ((m_old_data = OP_NEWA(BYTE, m_old_data_size)) == NULL)
			{
				err = OpStatus::ERR_NO_MEMORY;
				break;
			}

			if (RegQueryValueEx(key, m_value_name.CStr(), NULL, &m_old_type, m_old_data, &m_old_data_size) != ERROR_SUCCESS)
				break;
		}
		else
			m_old_type = REG_DELETE;

		if (m_new_type == REG_DELETE)
		{
			if (result != ERROR_FILE_NOT_FOUND && RegDeleteValue(key, m_value_name.CStr()) != ERROR_SUCCESS)
				break;
		}
		else
		{
			if (RegSetValueEx(key, m_value_name.CStr(), NULL, m_new_type, m_new_data, m_new_data_size) != ERROR_SUCCESS)
				break;
		}

		err = OpStatus::OK;
	}
	while(false);

	RegCloseKey(key);

	return err;
}

void ChangeRegValueOperation::Undo()
{
	if (m_create_key_op)
	{
		m_create_key_op->Undo();
		return;
	}

	HKEY key;

	if (RegOpenKeyEx(m_root_key, m_key_path.CStr(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
		return;

	if (m_old_type == REG_DELETE)
	{
		RegDeleteValue(key, m_value_name.CStr());
	}
	else
	{
		RegSetValueEx(key, m_value_name.CStr(), NULL, m_old_type, m_old_data, m_old_data_size);
	}

	RegCloseKey(key);
}

ChangeRegValueOperation::~ChangeRegValueOperation()
{
	OP_DELETEA(m_old_data);
	if (m_restore_access_info)
		WindowsUtils::RestoreObjectAccess(m_restore_access_info);
}