/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef _SHLWAPI_REPLACE_H_INCLUDED_
#define _SHLWAPI_REPLACE_H_INCLUDED_

BOOL PathFileExistsA(LPCSTR pszPath);
BOOL PathFileExists(LPCWSTR pszPath);
BOOL PathIsUNCServer(LPCWSTR pszPath);
BOOL PathIsUNCServerShare(LPCWSTR pszPath);
BOOL PathIsDirectory(LPCWSTR pszPath);
int PathGetDriveNumber(LPCWSTR pszPath);
BOOL PathIsRelative(LPCWSTR pszPath);
BOOL PathSearchAndQualify(LPCWSTR pszPath, LPWSTR pszBuf, UINT cchBuf);
LPWSTR PathFindExtension(LPCWSTR pszPath);

#endif // _SHLWAPI_REPLACE_H_INCLUDED_
