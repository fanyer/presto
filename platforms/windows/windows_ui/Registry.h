//
//	FILE		Registry.h
//	CREATED		18 Juni 1998 - DG[61] 
//
//	DESCRIPTION	General purpose Registry releated functions.
//
//

#ifndef	_REGISTRY_H_INCLUDED_
#define _REGISTRY_H_INCLUDED_

#ifdef WIN32
	#define	_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
#else
	//#define	_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
#endif


//	Do not change 
#ifdef _REGISTRY_SAVE_AND_RESTORE_SUPPORT_
	#undef	_NO_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
#else
	#define	_NO_REGISTRY_SAVE_AND_RESTORE_SUPPORT_
#endif

BOOL	RegCreateKeyWithValue	( HKEY	hkeyRoot, const uni_char* szKeyName, const uni_char* szDefaultValue);
DWORD	RegDeleteKeyNT			( HKEY hStartKey, const uni_char* );

OP_STATUS OpSafeRegQueryValue	(HKEY root_key,const uni_char* key_name, OpString &value, const uni_char* value_name = NULL, REGSAM viewflags = 0);

LONG OpRegReadStrValue
(
	HKEY	hKeyRoot,		//	root key
	LPCTSTR	szKeyName,		//	key path below hKeyRoot
	LPCTSTR	szValueName,	//	if NULL or "" => read unnamed default value
	LPCTSTR	szValue,		//	result buffer
	DWORD	*pdwSize		//	sizeof(szValue)
);

inline LONG OpRegReadStrValue
( 
	IN  HKEY	hKeyRoot,		//	root key
	IN  LPCTSTR	szKeyName,		//	key path below hKeyRoot
	IN  LPCTSTR	szValueName,	//	if NULL or "" => read unnamed default value
	OUT LPCTSTR	szValue,		//	result buffer
	DWORD		dwSize			//	sizeof(szValue)
)
{
	return OpRegReadStrValue( hKeyRoot, szKeyName, szValueName, szValue, &dwSize);
};

DWORD OpRegWriteStrValue
(
	IN	HKEY	hkeyRoot,		//	root key
	IN	LPCTSTR	szKeyName,		//	key path below hKeyRoot
	IN	LPCTSTR	szValueName,	//	name of string value
	IN	LPCTSTR	szValue			//	string value to store
);

LONG OpRegReadDWORDValue
(
	IN	HKEY	hKeyRoot,
	IN	LPCTSTR	szKeyName,
	IN	LPCTSTR	szValueName,
	OUT LPDWORD	dwValue
);

DWORD OpRegWriteDWORDValue
(
	IN	HKEY	hKeyRoot,
	IN	LPCTSTR	szKeyName,
	IN	LPCTSTR	szValueName,
	IN	DWORD	dwValue
);
#endif	//	_REGISTRY_H_INCLUDED_	#
