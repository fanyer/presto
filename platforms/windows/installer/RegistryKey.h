// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//


#include "platforms/windows/Installer/OperaInstaller.h"

/*
    <key path="Software\Classes\Opera.Protocol" clean="True">
      <value name="EditFlags" type="REG_DWORD">
        2
      </value>
    </key>
*/

class RegistryKey
{

public:

	//
	// Static declarations
	//

	// Array of the string names of the predefined registry value types in Windows.
	static const uni_char* TYPE[];

	// The count of predefinded value types in Windows.
	static const DWORD	REG_TYPE_COUNT;

	// The value of an invalid registry key type.
	static const DWORD	REG_INVALID_TYPE = ULONG_MAX;

	//
	// Static helper functions
	//

	// Return TRUE if type has a value equal to any of Windows predefined value types (ie. REG_NONE, REG_SZ).
	static BOOL				ValidType(DWORD type) 
								{ return (type < REG_TYPE_COUNT) ? TRUE : FALSE; }

	// Returns TRUE if type is one of the predefined value types that we have written an implementation for.
	static BOOL				KnownType (DWORD type) 
								{ return (type == REG_NONE || type == REG_SZ || type == REG_EXPAND_SZ || type == REG_DWORD || type == REG_BINARY); }

	// Returns the value number if it finds a match in the array, otherwise -1 (which should not be a valid type)
	static DWORD			ValueStr2ValueNum(const uni_char* str);

	// Return the string representing the registry value type (ie UNI_L("REG_SZ")).
	static const uni_char*	ValueNum2ValueStr(DWORD value) 
								{ if (!ValidType(value)) return NULL; return TYPE[value]; }

	
	//
	//	Public member functions
	//

	RegistryKey();
	~RegistryKey() {};

	OP_STATUS		SetKeyPath(const uni_char* key_path);
	OP_STATUS		SetValueType(DWORD type);
	OP_STATUS		SetValueType(const uni_char* type);
	OP_STATUS		SetValueName(const uni_char* name);
	OP_STATUS		SetValueData(const uni_char* data);
	OP_STATUS		SetCleanKey(UINT clean);
	OP_STATUS		SetCleanKey(const uni_char* clean);

	const uni_char*	GetKeyPath()	const	{ return m_key_path.CStr(); }
	const DWORD		GetValueType()	const	{ return m_type; }
	const uni_char* GetValueName()	const	{ return m_name.CStr(); }
	const uni_char* GetValueData()	const	{ return m_data.CStr(); }
	const UINT		GetCleanKey()	const	{ return m_clean; }
	OP_STATUS		GetValueType(OpString& value_data) const;
	
	OP_STATUS		Copy(const RegistryKey&);
	OP_STATUS		Copy(OpStringC &key_name, OperaInstaller::RegistryOperation reg_op);
	BOOL			HasValue() const { return m_data.HasContent() || m_name.HasContent(); }


private:

	// Make the copy constructors private, so you have to explicitly call copy to get a copy.
	RegistryKey(const RegistryKey& other);

	UINT			m_clean;			// Tells us if the value should be cleaned on uninstall or not (ie. 0, 1, 2 or 3 and so on)
	OpString		m_key_path;			// Registry key path (ie. "Software\Classes\Opera.Protocol")
	DWORD			m_type;				// Value - type (ie. "REG_SZ")
	OpString		m_name;				// Value - name (ie. "EditFlags")
	OpString		m_data;				// Value - data (ie. "1")

	/*
	  m_clean:
		0 means: delete only the value.
		1 means: delete the key containing that value.
		2 means: delete the parent of the key containing the value
		3 means: delete the parent of the parent of the key containing the value
		and so on
	*/
};
