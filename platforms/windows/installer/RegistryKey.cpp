// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "core/pch.h"
#include "RegistryKey.h"

//
// RegistryKey (statice definitions and  functions)
//

/* static */
const uni_char* RegistryKey::TYPE[] = 
			{	
				UNI_L("REG_NONE"),							// 0, No value type
				UNI_L("REG_SZ"),							// 1, Unicode nul terminated string
				UNI_L("REG_EXPAND_SZ"),						// 2, Unicode nul terminated string (with environment variable references)
				UNI_L("REG_BINARY"),						// 3, Free form binary
				UNI_L("REG_DWORD"),							// 4, 32-bit number
				// UNI_L("REG_DWORD_LITTLE_ENDIAN"),		// 4, 32-bit number (same as REG_DWORD)
				UNI_L("REG_DWORD_BIG_ENDIAN"),				// 5, 32-bit number
				UNI_L("REG_LINK"),							// 6, Symbolic Link (unicode)
				UNI_L("REG_MULTI_SZ"),						// 7, Multiple Unicode strings
				UNI_L("REG_RESOURCE_LIST"),					// 8, Resource list in the resource map
				UNI_L("REG_FULL_RESOURCE_DESCRIPTOR"),		// 9, Resource list in the hardware description
				UNI_L("REG_RESOURCE_REQUIREMENTS_LIST"),	//10, 
				UNI_L("REG_QWORD"),							//11, 64-bit number
				// UNI_L("REG_QWORD_LITTLE_ENDIAN")			//11, 64-bit number (same as REG_QWORD)
			};

/* static */
const DWORD RegistryKey::REG_TYPE_COUNT = sizeof(TYPE) / sizeof(TYPE[0]);

/* static */
DWORD RegistryKey::ValueStr2ValueNum(const uni_char* str)
{
	if (str)
	{
		DWORD i;

		for (i = 0; i < REG_TYPE_COUNT; i++)
		{
			if (uni_strcmp(str, TYPE[i]) == 0)
				return i; 
		}

		OP_ASSERT(!"This is an invalid type");
	}

	return REG_INVALID_TYPE;
}

//
// RegistryKey (member functions)
//

RegistryKey::RegistryKey():	m_clean(0)
						  , m_type(REG_SZ)
{}

RegistryKey::RegistryKey(const RegistryKey& other)
{
	OP_ASSERT(!"This is made private so you won't use it. Use the Copy() function instead.");
}

OP_STATUS RegistryKey::Copy(const RegistryKey& other)
{
	m_clean = other.GetCleanKey();
	m_type = other.GetValueType();
	RETURN_IF_ERROR(m_key_path.Set(other.GetKeyPath()));
	RETURN_IF_ERROR(m_name.Set(other.GetValueName()));
	RETURN_IF_ERROR(m_data.Set(other.GetValueData()));
	return OpStatus::OK;
}

OP_STATUS RegistryKey::Copy(OpStringC &key_name, OperaInstaller::RegistryOperation reg_op)
{
	RETURN_IF_ERROR(m_key_path.Set(key_name));
	RETURN_IF_ERROR(SetValueName(reg_op.value_name));
	RETURN_IF_ERROR(SetValueType(reg_op.value_type));
	RETURN_IF_ERROR(SetCleanKey(reg_op.clean_parents));

	m_data.Empty();
	switch (reg_op.value_type)
	{
		case REG_NONE:
			RETURN_IF_ERROR(m_data.Set(UNI_L("")));
			break;
		case REG_SZ: // Fallthrough
		case REG_EXPAND_SZ:
			if (reg_op.value_data == NULL)
				RETURN_IF_ERROR(m_data.Set(UNI_L("")));
			else
				RETURN_IF_ERROR(m_data.Set((uni_char*)reg_op.value_data));
			break;
		case REG_DWORD:
			RETURN_IF_ERROR(m_data.AppendFormat(UNI_L("%.8lx"), reg_op.value_data));
			break;
		case REG_BINARY:
		{
			char* data = (char*)reg_op.value_data;
			RETURN_IF_ERROR(m_data.AppendFormat(UNI_L("%.2x"), *data));

			while (*(++data))
				RETURN_IF_ERROR(m_data.AppendFormat(UNI_L(",%.2x"), *data));

			break;
		}
		default:
		{
			// Either the calling code has an error (sending in incorrect value for key_name),
			// or this is a type we have do not support yet. If so, please update the code in this function.
			OP_ASSERT(!"Code to convert this type is not implemented. Please do so");
		}
	}
	return OpStatus::OK;
}

OP_STATUS RegistryKey::GetValueType(OpString& value_type) const
{
	if (m_type >= REG_TYPE_COUNT)
	{
		// If there is a new value (ie. new Windows version with more standard Registry types defined), 
		// please update the REG_TYPE array, as well as all corresponding code.
		// If there is no new type, then the code that calls this function has an error.
		OP_ASSERT(!"This is a value outside det known range of registry key types.");
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	return value_type.Set(TYPE[m_type]);
}

OP_STATUS RegistryKey::SetKeyPath(const uni_char* key_path) 
{
	if (!key_path)
		return OpStatus::ERR;

	return m_key_path.Set(key_path); 
}

OP_STATUS RegistryKey::SetValueName(const uni_char* value_name) 
{ 
	// Special case. 
	if (!value_name)
	{
		RETURN_IF_ERROR(m_name.Set(""));
		return OpStatus::OK;
	}

	return m_name.Set(value_name); 
}

OP_STATUS RegistryKey::SetValueType(DWORD type) 
{ 
	// If this fires. The calling code is either doing something wrong.
	// Or maybe you have a new version of windows with more default value types,
	// than previous. If so, please update the class.
	OP_ASSERT(ValidType(type));

	if (!ValidType(type))
		return OpStatus::ERR;
	else
		m_type = type;

	return OpStatus::OK;
}

OP_STATUS RegistryKey::SetValueType(const uni_char* type)
{
	if (type)
	{
		DWORD value_type = ValueStr2ValueNum(type);
	
		if (value_type == REG_INVALID_TYPE)
			return OpStatus::ERR;

		m_type = value_type;
	}
	else
		m_type = REG_SZ;

	return OpStatus::OK;
}

OP_STATUS RegistryKey::SetValueData(const uni_char* data) 
{ 
	return m_data.Set(data); 
}

OP_STATUS RegistryKey::SetCleanKey(UINT clean) 
{ 
	if (clean > 32)
	{
		OP_ASSERT(!"This is not a valid value/string for clean");
		return OpStatus::ERR;
	}

	m_clean = clean; 
	return OpStatus::OK;
}

OP_STATUS RegistryKey::SetCleanKey(const uni_char* clean)
{	
	if (clean && *clean && uni_isdigit(clean[0]))
	{
		m_clean = uni_atoi(clean);
		return OpStatus::OK;
	}
	
	return OpStatus::ERR;
}
