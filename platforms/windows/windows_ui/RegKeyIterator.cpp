/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/windows/windows_ui/RegKeyIterator.h"

const HKEY RegKeyIterator::NO_HANDLE = 0;


RegKeyIterator::RegKeyIterator()
	: m_ancestor_handle(NO_HANDLE),
	  m_key_handle(NO_HANDLE),
	  m_subkey_index(NO_INDEX)
{
}


RegKeyIterator::~RegKeyIterator()
{
	CleanUp();
}


void RegKeyIterator::CleanUp()
{
	if (NO_HANDLE != m_key_handle && m_ancestor_handle != m_key_handle)
	{
		(void) RegCloseKey(m_key_handle);
	}
}


OP_STATUS RegKeyIterator::Init(HKEY ancestor_handle, const OpStringC& key_name,
		HKEY* key_handle)
{
	CleanUp();

	m_ancestor_handle = ancestor_handle;
	RETURN_IF_ERROR(m_key_name.Set(key_name));
	m_key_handle = NO_HANDLE;
	m_subkey_index = NO_INDEX;

	if (key_name.IsEmpty())
	{
		m_key_handle = m_ancestor_handle;
	}
	else
	{
		const INT32 result = RegOpenKeyEx(
				m_ancestor_handle, key_name, 0, KEY_READ, &m_key_handle);
		if (ERROR_SUCCESS != result)
		{
			return OpStatus::ERR;
		}
		if (NULL != key_handle)
		{
			*key_handle = m_key_handle;
		}

		Next();
	}

	return OpStatus::OK;
}


BOOL RegKeyIterator::AtEnd() const
{
	return END_INDEX == m_subkey_index;
}


BOOL RegKeyIterator::AtSubkey() const
{
	return END_INDEX != m_subkey_index && NO_INDEX != m_subkey_index;
}


OP_STATUS RegKeyIterator::GetCurrent(OpString& subkey_name) const
{
	return AtSubkey() ? subkey_name.Set(m_subkey_name) : OpStatus::ERR;
}


void RegKeyIterator::Next()
{
	if (NO_INDEX == m_subkey_index)
	{
		m_subkey_index = 0;
	}
	else if (END_INDEX == m_subkey_index)
	{
		return;
	}
	else
	{
		++m_subkey_index;
	}

	uni_char name_buffer[MAX_KEY_NAME_LENGTH + 1];
	DWORD name_buffer_size = MAX_KEY_NAME_LENGTH + 1;

	const INT32 result = RegEnumKeyEx(m_key_handle, m_subkey_index,
			name_buffer, &name_buffer_size, NULL, NULL, NULL, NULL);
	if (ERROR_SUCCESS == result)
	{
		const OP_STATUS status = m_subkey_name.SetConcat(
				m_key_name, UNI_L("\\"), name_buffer);
		OP_ASSERT(OpStatus::IsSuccess(status));
	}
	else
	{
		m_subkey_index = END_INDEX;
	}
}

