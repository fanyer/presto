//
//	FILE		Registry.cpp
//	CREATED		18 Juni 1998 - DG[61]
//
//	DESCRIPTION	General purpose Registry releated functions.
//
//	PLEASE NOTE -- NOT FULLY CHECKED YET.


#include "core/pch.h"

#include "Registry.h"
#include "modules/hardcore/unicode/unicode.h"

#ifdef _DEBUG	
	//	DEBUG
	//	------------------------------------
	BOOL _precond_stack[1024];
	int  _precond_stack_index = 0;

	#define _precond_push(expr)		_precond_stack[_precond_stack_index++] = (expr) != 0
	#define _precond_pop()			_precond_stack[--_precond_stack_index]
	#define _precond_now()			_precond_stack[_precond_stack_index-1]

	#define	IF_NOT_PRECOND(expr)\
		_precond_push(expr);\
		if( !_precond_now())\
			OP_ASSERT( !#expr);\
		if( !_precond_pop())

	#define IF_PRECOND(expr)\
		_precond_push(expr);\
		if( !_precond_now())\
			OP_ASSERT( !#expr);\
		if( _precond_pop())
		
	#define FTELL(hfile)			DEBUG_WARN1("\nFileptr = %li", _llseek((hfile), 0, FILE_CURRENT));

#else
	//	RELEASE
	//	--------------------------------------
	#define IF_NOT_PRECOND(expr)	if(!(expr))	
	#define IF_PRECOND(expr)		if(expr)
	#define FTELL(hfile)			

#endif
	




static const DWORD _FILEVER = 1;
#define	_FILEID					"OPERA-REGISTRY-FILE\nDO-NOT-EDIT\n\n"	
#define	_MAX_KEY_LEN			1024		//	Max length for one sub key
#define	_MARKER					"\nKEY"		//	len = 5

#pragma pack(1)
struct FileHeader
{
	char	szID[100];			//	== _FILEID
	DWORD	dwVersion;			//	== _FILEVER
	LONG	cbMaxFullName;		//	Max length for full key name stored in file
	LONG	cbMaxValueName;		//	Max length for value name stored in file
	LONG	cbMaxValueData;		//	Max length for value data stored in file
};
#pragma pack()


#pragma pack(1)
struct SavedRegKey
{
	//	File stamp
	char	marker[5];		//	== _MARKER
	LONG	nValueCount;	//	# values including default

	//
	//	After this follows the key name
	//
	//			DWORD	cbKeyNameLen
	//			char	szKeyName[]
	//
	//
	//
	//	Then comes the all the values and its data
	//	Note that the default value has "" as its value name
	//
	//			BYTE	fValueOK
	//		
	//		if( fValueOK)
	//		{
	//			DWORD	dwValueType		
	//
	//			DWORD	cbValueNameLen
	//			char	szValueName[]
	//
	//			DWORD	cbValueDataLen
	//			BYTE	valueData[]
	//		}
	//
	//	Repeated 'nValueCount' times.
	//
};
#pragma pack()


static const struct
{
	HKEY	hKey;
	uni_char	*szName;
	BOOL	fAllowChildKeyDelete;	//	NOT USED YET -- Paranoid check. Modify this if delete access is required
}
ValidRoots[] =
{
	{ HKEY_CLASSES_ROOT,	UNI_L("HKEY_CLASSES_ROOT"),		TRUE  },
	{ HKEY_CURRENT_USER,	UNI_L("HKEY_CURRENT_USER"),		FALSE },
	{ HKEY_LOCAL_MACHINE,	UNI_L("HKEY_LOCAL_MACHINE"),	FALSE },
	{ HKEY_USERS,			UNI_L("HKEY_USERS"),			FALSE },
	{ HKEY_CURRENT_CONFIG,	UNI_L("HKEY_CURRENT_CONFIG"),	FALSE },
	{ HKEY_DYN_DATA,		UNI_L("HKEY_DYN_DATA"),			FALSE }	
};


//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	___________________________________________________________________________
//

#ifdef _REGISTRY_SAVE_AND_RESTORE_SUPPORT_
DWORD RegDeleteKeyNT
(
	HKEY hStartKey ,
	const uni_char* pKeyName
)
{

#ifdef _NO_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
	return 0;
#else

	const int MAX_KEY_LENGTH = 1024;
		
	DWORD   dwRtn = ERROR_BADKEY;
	DWORD	dwSubKeyLength;
	// LPTSTR  pSubKey = NULL;
	uni_char szSubKey[MAX_KEY_LENGTH];
	HKEY    hKey;

	// do not allow NULL or empty key name
	IF_NOT_PRECOND( IsStr(pKeyName))
		goto LABEL_DONE;

	dwRtn = RegOpenKeyEx( hStartKey,pKeyName, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &hKey );
	OP_ASSERT( dwRtn == ERROR_SUCCESS || dwRtn == ERROR_FILE_NOT_FOUND);
	if( !(dwRtn == ERROR_SUCCESS || dwRtn == ERROR_FILE_NOT_FOUND))
		goto LABEL_DONE;

	//	Paranoid check
	//	Do not allow delete of keys we are not using in Opera
	if(1)
	{
		for( int i=0; i<ARRAY_SIZE(ValidRoots); i++)
			if( ValidRoots[i].hKey == hKey)
			{
				//	Ooops -- trying to delete an root key
				#ifdef _DEBUG
					MessageBoxA( g_main_hwnd, "Let Dag know this happend\nPrevented root key delete\n", "_DEBUG ONLY", MB_OK);
				#endif
				return 0;
			}
	}
	
	while( dwRtn == ERROR_SUCCESS )
	{
		dwSubKeyLength = MAX_KEY_LENGTH;
		dwRtn=RegEnumKeyEx(
			hKey,
			0,		// always index zero
			szSubKey,
			&dwSubKeyLength,
			NULL,
			NULL,
			NULL,
			NULL);

		if(dwRtn == ERROR_NO_MORE_ITEMS)
		{
			dwRtn = RegDeleteKey(hStartKey, pKeyName);
			break;
		}

		else if(dwRtn == ERROR_SUCCESS)
			dwRtn=RegDeleteKeyNT(hKey, szSubKey);
	}
	if(hKey)
		RegCloseKey(hKey);
	
	//	dwRtn now has correct value.

LABEL_DONE:
	OP_ASSERT( dwRtn == ERROR_SUCCESS || dwRtn == ERROR_FILE_NOT_FOUND);
	return dwRtn;

#endif	// _REGISTRY_SAVE_AND_RESTORE_SUPPORT_
}
#endif // _REGISTRY_SAVE_AND_RESTORE_SUPPORT_


OP_STATUS OpSafeRegQueryValue(HKEY root_key,const uni_char* key_name, OpString &value, const uni_char* value_name, REGSAM view_flags)
{
	DWORD value_len;
	HKEY key;

	DWORD err = RegOpenKeyEx(root_key, key_name, 0, KEY_READ | view_flags, &key);
	if (err == ERROR_FILE_NOT_FOUND)
		return OpStatus::ERR_FILE_NOT_FOUND;
	if (err != ERROR_SUCCESS)
		return OpStatus::ERR;

	err = RegQueryValueEx(key, value_name, NULL, NULL, NULL, &value_len);
	if (err != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		if (err == ERROR_FILE_NOT_FOUND)
			return OpStatus::ERR_FILE_NOT_FOUND;
		return OpStatus::ERR;
	}
	value.Empty();
	if (value_len > 0)
	{
		// Reserve takes the length without the terminating NUL, while it's
		// included in value_len.
		if (!value.Reserve((value_len-1)/sizeof(uni_char)))
		{
			RegCloseKey(key);
			return OpStatus::ERR_NO_MEMORY;
		}
		if (RegQueryValueEx(key, value_name, NULL, NULL, (LPBYTE)value.CStr(), &value_len) != ERROR_SUCCESS)	
		{
			RegCloseKey(key);
			return OpStatus::ERR;
		}
	}
	RegCloseKey(key);
	return OpStatus::OK;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	RegCreateKeyWithValue
//	___________________________________________________________________________
//

BOOL RegCreateKeyWithValue
(
	HKEY		hkeyRoot,
	const uni_char*	szKeyName,
	const uni_char* szDefaultValue
)
{

#ifdef _NO_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
	return 0;
#else

	BOOL	bOK	= FALSE;
	HKEY	hkeyNew;
	LRESULT	result;

	//	Validate params
	if( ! (hkeyRoot && IsStr(szKeyName)))
		return FALSE;

	//	Open/Create key
	DWORD	dwDisposition;

	result = RegCreateKeyEx(
		hkeyRoot, szKeyName, 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
		&hkeyNew, &dwDisposition);

	if( result==ERROR_SUCCESS)
	{
		int sLen = uni_strlen(szDefaultValue);
		//	Set default value.
		bOK = RegSetValueEx(hkeyNew, NULL, 0, REG_SZ, (unsigned char*)szDefaultValue, (sLen*sizeof(uni_char))) == ERROR_SUCCESS;
		RegCloseKey( hkeyNew);
	}

	return bOK;

#endif	// _REGISTRY_SAVE_AND_RESTORE_SUPPORT_
}

LONG OpRegReadStrValue
(
	HKEY	hKeyRoot,		//	root key
	LPCTSTR	szKeyName,		//	key path below hKeyRoot
	LPCTSTR	szValueName,	//	if NULL or "" => read unnamed default value
	LPCTSTR	szValue,		//	result buffer
	DWORD	*pdwSize		//	sizeof(szValue)
)
{
	HKEY	hKey = NULL;
	LONG	dwRes = ERROR_SUCCESS;

	//
	//	Open the key containing the value
	//
	if (IsStr(szKeyName))
	{
		dwRes = RegOpenKeyEx( hKeyRoot, szKeyName, NULL, KEY_READ, &hKey);
	}
	else
	{
		hKey = hKeyRoot;
	}

	//
	//	Read key value
	//
	if (dwRes == ERROR_SUCCESS)
	{
		DWORD dwType;

		dwRes = RegQueryValueEx(hKey, szValueName, NULL, &dwType, (LPBYTE)szValue, pdwSize);

		if (dwRes == ERROR_SUCCESS)
		{
			if (dwType != REG_SZ && dwType != REG_EXPAND_SZ)
			{
				dwRes = E_FAIL;
			}
		}
	}

	//
	//	Close and clean up
	//
	if (hKey && (hKey != hKeyRoot))
	{
		RegCloseKey( hKey);
	}

	return dwRes;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	OpRegWriteStrValue
//	___________________________________________________________________________
//
//
DWORD OpRegWriteStrValue
(
	IN	HKEY	hkeyRoot,		//	root key
	IN	LPCTSTR	szKeyName,		//	key path below hKeyRoot
	IN	LPCTSTR	szValueName,	//	name of string value
	IN	LPCTSTR	szValue			//	string value to store
)
{
	BOOL	bOK	= FALSE;
	HKEY	hkeyNew;
	LRESULT	result;

	//	Validate params
	if( ! (hkeyRoot && IsStr(szKeyName)))
		return FALSE;

	//	Open/Create key
	DWORD	dwDisposition;

	result = RegCreateKeyEx(
		hkeyRoot, szKeyName, 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
		&hkeyNew, &dwDisposition);

	if( result==ERROR_SUCCESS)
	{
		//	Set string value.
		bOK = RegSetValueEx(
			hkeyNew, szValueName, 0, REG_SZ,
			(BYTE*)szValue, (uni_strlen(szValue)+1) * sizeof(szValue[0]))
			== ERROR_SUCCESS;
		RegCloseKey( hkeyNew);
	}

	return bOK;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	OpRegReadDWORDValue
//	___________________________________________________________________________
//
//

LONG OpRegReadDWORDValue
(
	IN	HKEY	hKeyRoot,
	IN	LPCTSTR	szKeyName,
	IN	LPCTSTR	szValueName,
	OUT LPDWORD	dwValue
)
{
	HKEY	hKey;
	LONG	dwRes = ERROR_SUCCESS;

	unsigned long size = sizeof(DWORD);
	//
	//	Open the key containing the value
	//
	if (IsStr(szKeyName))
		dwRes = RegOpenKeyEx( hKeyRoot, szKeyName, NULL, KEY_READ, &hKey);
	else
		hKey = hKeyRoot;

	//
	//	Read key value
	//
	if (dwRes == ERROR_SUCCESS)
	{
		DWORD dwType;
		if (RegQueryValueEx( hKey, (uni_char*) szValueName, NULL, &dwType, (LPBYTE)dwValue, &size) == ERROR_SUCCESS)
		{
			if (!(dwType == REG_DWORD))
				dwRes = E_FAIL;
		}
		else dwRes = E_FAIL;
	}

	//
	//	Close and clean up
	//
	if (hKey && (hKey != hKeyRoot))	
		RegCloseKey( hKey);
	return dwRes;
}


DWORD OpRegWriteDWORDValue
(
	IN	HKEY	hKeyRoot,
	IN	LPCTSTR	szKeyName,
	IN	LPCTSTR	szValueName,
	IN	DWORD	dwValue
)
{
	BOOL	bOK	= FALSE;
	HKEY	hkeyNew;
	LRESULT	result;

	//	Validate params
	if( ! (hKeyRoot && IsStr(szKeyName)))
		return FALSE;

	//	Open/Create key
	DWORD	dwDisposition;

	result = RegCreateKeyEx(
		hKeyRoot, szKeyName, 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
		&hkeyNew, &dwDisposition);

	if( result==ERROR_SUCCESS)
	{
		//	Set string value.
		bOK = RegSetValueEx(
			hkeyNew, szValueName, 0, REG_DWORD,
			(BYTE*)&dwValue, sizeof(DWORD))
			== ERROR_SUCCESS;
		RegCloseKey( hkeyNew);
	}

	return bOK;
}

