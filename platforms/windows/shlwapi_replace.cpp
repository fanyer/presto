/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

// The following functions duplicate the (reverse engineered) behaviour of those in
// SHLWAPI.DLL version 6.0 (newest version as of Nov 2008) that comes with IE 6.0 and later.
// We don't want to depend on an IE component, now do we?

BOOL PathFileExistsA(LPCSTR pszPath)
{
	return GetFileAttributesA(pszPath) != 0xFFFFFFFF;
}

BOOL PathFileExists(LPCWSTR pszPath)
{
	return GetFileAttributes(pszPath) != 0xFFFFFFFF;
}

LPCWSTR UNCGetServer(LPCWSTR pszPath)
{
	if (pszPath[0] != '\\' || pszPath[1] != '\\')
		return NULL;
		
	if (pszPath[2] != '?')
		return pszPath+2;

	if (uni_strni_eq(pszPath+3, UNI_L("\\UNC\\"), 5))
		return pszPath+8;

	return NULL;
}

BOOL PathIsUNCServer(LPCWSTR pszPath)
{
	LPCWSTR server = UNCGetServer(pszPath);

	if (!server)
		return FALSE;

	return !uni_strchr(server,'\\');
}

BOOL PathIsUNCServerShare(LPCWSTR pszPath)
{
	LPCWSTR server = UNCGetServer(pszPath);

	if (!server)
		return FALSE;

	LPCWSTR server_end = uni_strchr(server,'\\');

	if (!server_end)
		return FALSE;	

	return !uni_strchr(server_end+1,'\\');
}

BOOL PathIsDirectory(LPCWSTR pszPath)
{
	if (PathIsUNCServer(pszPath))
		return FALSE;

	/*
	if (PathIsUNCServerShare(pszPath))
	{
		// the original implementation uses WNetGetResourceInformation to determine if
		// it's really a disk "share", but that's far beyond what we need this function
		// to check, so whatever
	}
	*/

	DWORD attrib = GetFileAttributes(pszPath);

	if (attrib == 0xFFFFFFFF)
		return FALSE;

	return attrib & FILE_ATTRIBUTE_DIRECTORY;
}

int PathGetDriveNumber(LPCWSTR pszPath)
{
	if (pszPath[0] == '\\' && pszPath[1] == '\\' && pszPath[2] == '?' && pszPath[3] == '\\')
		pszPath += 4;	// Long UNC path

	if (!pszPath[0] || pszPath[1] != ':')
		return -1;

	uni_char first_char = pszPath[0] | 0x20;

	if (first_char < 'a' || first_char > 'z')
		return -1;

	return first_char - 'a';
}

BOOL PathIsRelative(LPCWSTR pszPath)
{
	if (pszPath[0] == '\\')
		return FALSE;

	return PathGetDriveNumber(pszPath) < 0;
}

BOOL PathSearchAndQualify(LPCWSTR pszPath, LPWSTR pszBuf, UINT cchBuf)
{
	DWORD result_len;

	if (uni_strpbrk(pszPath, UNI_L(":/\\"))
		|| (result_len = SearchPath(0, pszPath, 0, cchBuf, pszBuf, 0)) == 0
		|| result_len >= cchBuf)
	{
		result_len = GetFullPathName(pszPath, cchBuf, pszBuf, 0);
		if (result_len == 0 || result_len >= cchBuf)
		{
			*pszBuf = 0;
			return FALSE;
		}
	}
	return TRUE;
}

LPWSTR PathFindExtension(LPCWSTR pszPath)
{
	const uni_char *extension = NULL;
	while (*pszPath)
	{
		if (*pszPath == ' ' || *pszPath == '\\')
			extension = NULL;
		else if (*pszPath == '.')
			extension = pszPath;

		pszPath++;
	}
	
	return (LPWSTR)(extension ? extension : pszPath);
}

