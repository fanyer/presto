/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "core/pch.h"

#ifdef M2_SUPPORT
# ifdef MSWIN

# include "adjunct/m2/src/import/MSWinUtil.h"


BOOL MSWinUtil::GetTextualSid(PSID pSid, LPTSTR TextualSid, LPDWORD lpdwBufferLen)
{
	if (!IsValidSid(pSid))
		return FALSE;

	// Get the identifier authority value from the SID
	PSID_IDENTIFIER_AUTHORITY psia = GetSidIdentifierAuthority(pSid);

	// Get the number of subauthorities in the SID
	DWORD dwSubAuthorities = *GetSidSubAuthorityCount(pSid);

	// Compute the buffer length.
	// S-SID_REVISION- + IdentifierAuthority- + subauthorities- + NULL
	DWORD dwSidSize = (15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);

	// Check input buffer length.
	// If too small, indicate the proper size and set last error.
	if (*lpdwBufferLen < dwSidSize)
	{
		*lpdwBufferLen = dwSidSize;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	// Add 'S' prefix and revision number to the string.
	DWORD dwSidRev = SID_REVISION;
	dwSidSize = wsprintf(TextualSid, TEXT("S-%lu-"), dwSidRev);

	// Add SID identifier authority to the string.
	if ( (psia->Value[0] != 0) || (psia->Value[1] != 0) )
	{
		dwSidSize+=wsprintf(TextualSid + lstrlen(TextualSid),
					TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
					(USHORT)psia->Value[0],
					(USHORT)psia->Value[1],
					(USHORT)psia->Value[2],
					(USHORT)psia->Value[3],
					(USHORT)psia->Value[4],
					(USHORT)psia->Value[5]);
	}
	else
	{
		dwSidSize+=wsprintf(TextualSid + lstrlen(TextualSid),
					TEXT("%lu"),
					(ULONG)(psia->Value[5]      )   +
					(ULONG)(psia->Value[4] <<  8)   +
					(ULONG)(psia->Value[3] << 16)   +
					(ULONG)(psia->Value[2] << 24)   );
	}

	// Add SID subauthorities to the string.
	for (DWORD dwCounter = 0; dwCounter < dwSubAuthorities; dwCounter++)
	{
		dwSidSize+=wsprintf(TextualSid + dwSidSize, TEXT("-%lu"),
					*GetSidSubAuthority(pSid, dwCounter) );
	}

	return TRUE;
}

#define MAX_NAME_STRING   1024
#define MAX_SID_SIZE	  1024 

BOOL MSWinUtil::LookupSid(LPCTSTR lpszMachineName, LPCTSTR lpszAccountName, LPTSTR lpszSidStringReceiver, LPDWORD lpdwBufferLen)
{
	BYTE bySidBuffer[MAX_SID_SIZE];
	TCHAR szDomainName[MAX_NAME_STRING];
	SID_NAME_USE sidType;   

	PSID pSid				= (PSID)bySidBuffer;
	DWORD dwSidSize			= sizeof(bySidBuffer);
	DWORD dwDomainNameSize	= sizeof(szDomainName);

	BOOL ret = LookupAccountName(lpszMachineName,
								 lpszAccountName,
								 pSid,
								 &dwSidSize,
								 szDomainName,
								 &dwDomainNameSize,
								 &sidType);

	if (!ret)
	{
		return FALSE;
	}

	return GetTextualSid(pSid, lpszSidStringReceiver, lpdwBufferLen);
}


BOOL MSWinUtil::IsWindowsNTFamily()
{
	OSVERSIONINFOA osvA;
	memset(&osvA, 0, sizeof(OSVERSIONINFOA));
	osvA.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

	GetVersionExA(&osvA);

	return (VER_PLATFORM_WIN32_NT == osvA.dwPlatformId);

}

# endif //MSWIN
#endif // M2_SUPPORT
