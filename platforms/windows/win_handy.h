/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2001-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _WIN_HANDY_H_INCLUDED__849607
#define _WIN_HANDY_H_INCLUDED__849607

uni_char*			StrGetTime				( CONST SYSTEMTIME *pSystemTime, uni_char* szResult, int sizeOfResult, bool fSec, bool fForce24);
uni_char*			StrGetDateTime			( CONST SYSTEMTIME *pSystemTime, uni_char* szResult, int maxlen, BOOL fSec);

inline int			StrGetMaxLen			( int size)
					{
						const int sizeOfChar = sizeof(uni_char);
						return MAX( ((size - sizeOfChar) / sizeOfChar), 0);
					}

#if defined(ARRAY_SIZE)
# undef ARRAY_SIZE
#endif
#define ARRAY_SIZE(arr)	(sizeof(arr)/sizeof(arr[0]))

BOOL IsOperaWindow( HWND hWnd);
BOOL WinGetVersion ( OUT OSVERSIONINFO *pVersion);

#ifndef ISWINDOW
#define ISWINDOW(h) ((h)?IsWindow(h):FALSE)
#endif

#define ISMENU(h)	((h)?IsMenu(h):FALSE)

#define SWP_BASIC	(SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER)
#define WS_MDICHILDDEF	( WS_OVERLAPPEDWINDOW | WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPED)	//	*** DG-080799 - default used by windows when MDIS_ALLCHILDSTYLES is used for parent

inline BOOL IsOfWndClass( HWND hWnd, ATOM atom)
{
	return 	ISWINDOW(hWnd)
		&&	atom
		&&  (atom == (ATOM)GetClassLong( hWnd, GCW_ATOM));
};
DWORD GetSystemErrorText( DWORD err, uni_char* pszBuf, int nMaxStrLen);

/**
  * Converts a LogFont-structure to a FontAtt-structure.
  * @lf LogFont. Contains font info.
  * @font FontAtt. Will contain result.
  */
class FontAtt;
void	LogFontToFontAtt			(const LOGFONT &lf, FontAtt &font);

/**
  * Converts a FontAtt-structure to a LogFont-structure.
  * @font FontAtt. Contains font info.
  * @lf LogFont. Will contain result.
  */
void	FontAttToLogFont			(const FontAtt &font, LOGFONT &lf);

BOOL EnsureWindowVisible
(
	HWND hWndChild,		//	Child to move
	BOOL fEntire		//	if TRUE - try to make the entire window visble inside parent
);

HBITMAP CreateScreenCompatibleBitmap(int cx, int cy, HBRUSH brush = NULL);	

HANDLE CreateDIBFromDDB( HBITMAP hBitmap, DWORD dwCompression, HPALETTE hPal );

BOOL IsNetworkDrive( const char *szDir);
BOOL IsNetworkDrive( const uni_char *szDir);

BOOL FileExist( const char * szPath);
BOOL FileExist( const uni_char * szPath);

inline BOOL DirExist( const uni_char * szPath) { return GetFileAttributes(szPath) != 0xFFFFFFFF; }

uni_char * GetFavoritesFolder( uni_char *buf, DWORD bufSize);

//	ClientToScreen for a RECT
inline void ClientToScreenRect( HWND hwnd, RECT *rc)
{
	ClientToScreen( hwnd, (POINT*)&rc->left);
	ClientToScreen( hwnd, (POINT*)&rc->right);
}

//	ScreenToClient for a RECT
inline void ScreenToClientRect( HWND hwnd, RECT *rc)
{
	ScreenToClient( hwnd, (POINT*)&rc->left);
	ScreenToClient( hwnd, (POINT*)&rc->right);
}

#define RECTSIZE(rc)	rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top
inline int RectCX(const RECT &rc)	{ return rc.right-rc.left;};
inline int RectCY(const RECT &rc)	{ return rc.bottom-rc.top;};

void *MallocResourceData
(	
	HINSTANCE	hInst,				//	App. instance
	LPCTSTR		lpName,				//	Resouce name
	LPCTSTR		lpType,				//	Resource Type
	DWORD		*dwResSize = NULL	//	Ret.result size (opt)
);

LPSTR GetWinVersionStr(	LPSTR szVersion, BOOL productname=FALSE);
	// szVersion:   pointer to storage for string, at least 15 characters long.
	// productname: if TRUE, generate a human-comprehensible name ("Windows XP") 
	//              rather than one for scripts ("Windows NT 5.1")


class Window;

const char* GetSystemCharsetName();
// returns TRUE if a conversion of the wide string can be done using the system's codepage
BOOL CharsetSupportedBySystem(const uni_char* str);

DWORD	UpdateCrc32		( const void* pBuffer, DWORD size, DWORD seed);

uni_char * GetLastErrorMessageAlloc();			//	*** DG[29] added

int					OpPathGetDriveType					(IN const uni_char* pszPath);

uni_char* StringFileName( const uni_char* fName);
char* StringFileName(const char* fName);

char *StrAppendSubDir( char *szParentDir, int nMaxLen, const char *szSubDir);
OP_STATUS StrAppendSubDir(const OpStringC &parentDir, const OpStringC &subDir, OpString &result);

OP_STATUS MakeOperaIniFileCheckSum(const uni_char* iniFile, OpString8* signature);
BOOL IsSystemWin2000orXP();
BOOL IsSystemWinXP();
BOOL IsSystemWinVista();
BOOL IsSystemWin7();
BOOL IsSystemWin8();

OP_STATUS GetOSVersionStr(OpString& version);

enum WinType
{
	FAILED = 0,
	OLDER_THAN_WIN2K,
	WIN2K,
	WINXP,
	WINVISTA,
	WIN7,
	WIN8
};

WinType GetWinType();

// Get major number of service pack version, 2 for sp2.1 for instance
unsigned int GetServicePackMajor();

/* Will tell you if more than one normal user is logged on.
 * It does not count logged on services like ANANYMOUS LOGON,
 * LOCAL SERVICE, and other users with session id == 0.
 *
 * NB: This function will only work for elevated admins on XP and above. 
 *
 *	@param others_logged_on Number of other users logged on.
 *
 *	@return Will return OpStatus::OK if we actually manage to 
 *			enumerate all the values. The others_logged_on value
 *			will not make sense if this value is an error.
 *
 *			This function will only work on XP and higher. So
 *			will return ERR_NOT_SUPPORTED if you are not.
 *
 *			This function will only work if you are an admin. So
 *			will return ERR_NO_ACCESS if you are not.
 */
OP_STATUS IsOtherUsersLoggedOn(UINT& others_logged_on);

#endif	//	_WIN_HANDY_H_INCLUDED__849607 undefined
