/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/windows/installer/CreateRegKeyOperation.h"
#include "platforms/windows/windows_ui/registry.h"


CreateRegKeyOperation::CreateRegKeyOperation(HKEY ancestor_handle, const OpStringC& key_name)
	: m_ancestor_handle(ancestor_handle)
	, m_restore_access_info(NULL)
{
	if (OpStatus::IsError(m_key_name.Set(key_name)))
		m_key_name.Empty();
}


OP_STATUS CreateRegKeyOperation::Do()
{
	if (m_key_name.IsEmpty())
		return OpStatus::ERR;

	HKEY key;

	//Fail if the key already exists
	DWORD err = RegOpenKeyEx(m_ancestor_handle, m_key_name.CStr(), 0, KEY_QUERY_VALUE, &key);
	if (err != ERROR_FILE_NOT_FOUND)
	{
		if (err == ERROR_SUCCESS)
			RegCloseKey(key);

		return OpStatus::ERR;
	}

	OpString object_name;

	if (OpStatus::IsSuccess(WindowsUtils::RegkeyToObjectname(m_ancestor_handle, m_key_name, object_name)))
	{
		OpString subkey_path;
		RETURN_IF_ERROR(subkey_path.Set(m_key_name));
		HKEY subkey;

		do
		{
			int pos = subkey_path.FindLastOf('\\');

			if (pos == KNotFound)
				return OpStatus::ERR;
			else
				subkey_path.Delete(pos);

			DWORD err = RegOpenKeyEx(m_ancestor_handle, subkey_path.CStr(), 0, KEY_CREATE_SUB_KEY, &subkey);

			if (err == ERROR_SUCCESS)
				RegCloseKey(subkey);

			if (err == ERROR_SUCCESS || err == ERROR_ACCESS_DENIED)
			{
				RETURN_IF_ERROR(WindowsUtils::RegkeyToObjectname(m_ancestor_handle, subkey_path, object_name));

				//Sometimes, opening the key succeeds even though we have no access
				if (err == ERROR_ACCESS_DENIED || !WindowsUtils::CheckObjectAccess(object_name, SE_REGISTRY_KEY, KEY_QUERY_VALUE | KEY_SET_VALUE))
				{
					if (m_restore_access_info || !WindowsUtils::GiveObjectAccess(object_name, SE_REGISTRY_KEY, KEY_CREATE_SUB_KEY, FALSE, m_restore_access_info))
						return OpStatus::ERR;
				}
				break;
			}

			if (err != ERROR_FILE_NOT_FOUND)
				return OpStatus::ERR;


		}
		while (TRUE);

	}
		
	if (RegCreateKeyEx(m_ancestor_handle, m_key_name, NULL, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL) == ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return OpStatus::OK;
	}

	return OpStatus::ERR;

}


void CreateRegKeyOperation::Undo()
{
	OP_ASSERT(m_key_name.HasContent());

	RegDeleteKeyNT(m_ancestor_handle, m_key_name);
}


HKEY CreateRegKeyOperation::GetAncestorHandle() const
{
	return m_ancestor_handle;
}


const OpStringC& CreateRegKeyOperation::GetKeyName() const
{
	return m_key_name;
}

CreateRegKeyOperation::~CreateRegKeyOperation()
{
	if (m_restore_access_info)
		WindowsUtils::RestoreAccessInfo(m_restore_access_info);
}