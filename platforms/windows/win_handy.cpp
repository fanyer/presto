/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2001-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "win_handy.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/utils/authorization.h"
#include "platforms/windows/utils/ntstatus.h"

#include <Wtsapi32.h>
#include <Ntsecapi.h>

#undef PostMessage
#define PostMessage	PostMessageU

static char* current_defaults[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

void mswin_clear_locale_cache()
{
	for (int i = 0; i<6 ; i++)
		if (current_defaults[i])
			op_free((void*)current_defaults[i]);
}

char* mswin_setlocale(int category, const char *locale)
{
	BOOL reset_cache = category==LC_ALL && !strncmp("reset_cache", locale, 12);

	//caches the values obtained when calling setlocale with
	if (current_defaults[0] == NULL || reset_cache)
	{
		mswin_clear_locale_cache();

		char* old_default;
		old_default=setlocale(LC_COLLATE, "");
		current_defaults[0] = op_strdup(setlocale(LC_COLLATE, old_default));
		old_default=setlocale(LC_CTYPE, "");
		current_defaults[1] = op_strdup(setlocale(LC_CTYPE, old_default));
		old_default=setlocale(LC_MONETARY, "");
		current_defaults[2] = op_strdup(setlocale(LC_MONETARY, old_default));
		old_default=setlocale(LC_NUMERIC, "");
		current_defaults[3] = op_strdup(setlocale(LC_NUMERIC, old_default));
		old_default=setlocale(LC_TIME, "");
		current_defaults[4] = op_strdup(setlocale(LC_TIME, old_default));
		old_default=setlocale(LC_ALL, "");
		current_defaults[5] = op_strdup(setlocale(LC_ALL, old_default));
	}

	if (reset_cache)
		return NULL;

	if (locale && !(*locale))
	{
		switch (category)
		{
			case LC_COLLATE:
				setlocale(LC_COLLATE, current_defaults[0]);
				return current_defaults[0];
			case LC_CTYPE:
				setlocale(LC_COLLATE, current_defaults[1]);
				return current_defaults[1];
			case LC_MONETARY:
				setlocale(LC_COLLATE, current_defaults[2]);
				return current_defaults[2];
			case LC_NUMERIC:
				setlocale(LC_COLLATE, current_defaults[3]);
				return current_defaults[3];
			case LC_TIME:
				setlocale(LC_COLLATE, current_defaults[4]);
				return current_defaults[4];
			case LC_ALL:
				setlocale(LC_COLLATE, current_defaults[5]);
				return current_defaults[5];
		}
	}

	return setlocale(category, locale);
}


//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	StrGetTime
//	___________________________________________________________________________
//  
uni_char* StrGetTime ( CONST SYSTEMTIME *pSystemTime, uni_char* szResult, int sizeOfResult, bool fSec, bool fForce24)
{
	if (szResult == NULL || sizeOfResult == 0)
		return NULL;

	*szResult = NULL;

	DWORD dwFlags = (fSec ? 0 : TIME_NOSECONDS) | (fForce24 ? TIME_FORCE24HOURFORMAT : 0);
	int size = StrGetMaxLen(sizeOfResult);
	int len = GetTimeFormat( LOCALE_USER_DEFAULT, dwFlags, pSystemTime, NULL, szResult, size);
	if (len <= 0)
	{
		*szResult=0;
	}
	return szResult;
}

uni_char* StrGetDateTime ( CONST SYSTEMTIME *pSystemTime, uni_char* szResult, int maxlen, BOOL fSec)
{
	if( szResult && maxlen>=1)
	{
		*szResult = 0;
		if( pSystemTime)
		{
			int lenDate = GetDateFormat( LOCALE_USER_DEFAULT, DATE_SHORTDATE, pSystemTime, NULL, szResult, maxlen)-1;
			if( lenDate>0)
			{
				int lenRemaining = maxlen - lenDate;
				if( lenRemaining > 2)
				{
					szResult[lenDate] = ' ';
					szResult[lenDate+1] = 0;
					-- lenRemaining;
					++ lenDate;
#ifdef _DEBUG
					int lenTime =
#endif
					GetTimeFormat( LOCALE_USER_DEFAULT, (fSec ? 0 : TIME_NOSECONDS), pSystemTime, NULL, szResult+lenDate, lenRemaining);
					OP_ASSERT(lenTime>0);
				}
			}
		}	
	}
	return szResult;
}


//0110 0010 - 0110 0010 - 0110 0010 - 0110 0010

void TH (DWORD dw)
{
	char sz0[33] = "00000000000000000000000000000000";
	char sz1[33] = "";
	ltoa( dw, sz1, 2);
	strcpy( sz0+(32-strlen(sz1)), sz1);
}


//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	IsOperaWindow
//
//	Returns TRUE if the given window belongs to Opera
//	___________________________________________________________________________
//  
BOOL IsOperaWindow( HWND hWnd)
{
	if (hWnd)
	{
		DWORD dwProcessId = NULL;
		DWORD dwCurrentID = 0;

		GetWindowThreadProcessId( hWnd, &dwProcessId);
		
		dwCurrentID = GetCurrentProcessId();

		return dwProcessId == dwCurrentID;
	}

	return FALSE;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	GetSystemErrorText
//	RETURNS 0 on failure OR strlen(pszBuf) on success
//	___________________________________________________________________________
//  
DWORD GetSystemErrorText( DWORD error, uni_char* pszBuf, int nMaxStrLen)
{
	DWORD dwLen = 0;

	if( pszBuf && nMaxStrLen>0)
	{
		 dwLen = FormatMessage( 
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,												
			error,												
  			MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),			
			pszBuf,											
			nMaxStrLen,												
			NULL);
	}
	return dwLen;
}


//	DG-290199
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	EnsureWindowVisible
//
//	If window is outside the parent and unavailable for the user then
//	move it inside. NOTE - Do not use on a minimized window yet.
//	___________________________________________________________________________
//
//

BOOL MonitorRectFromWindow(RECT& hMonitorRect, HWND wnd, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);

BOOL EnsureWindowVisible
(
	HWND hWndChild,		//	Child to move
	BOOL fEntire		//	if TRUE - try to make the entire window visble inside parent
)
{	SUBROUTINE(504000)
	if( !(ISWINDOW( hWndChild)))
		return FALSE;

	BOOL fMoveIt = FALSE;	//	return value.

	int cxFudgeCheck = 4 * GetSystemMetrics( SM_CYCAPTION);
	int cyFudgeCheck = 3 * GetSystemMetrics( SM_CYCAPTION);
	int cxFudgeMove  = 2 * cxFudgeCheck;
	int cyFudgeMove  = 2 * cxFudgeCheck;
	
	//
	//	Get some info about the windows size
	//
	RECT rcChild, rcParent;
	GetWindowRect( hWndChild, &rcChild);

	HWND hWndParent = GetWindowLongPtr(hWndChild, GWL_STYLE) & WS_CHILD ? GetParent(hWndChild) : NULL;

	if (hWndParent)
	{
		GetWindowRect(hWndParent,  &rcParent);
	}
	else
	{
		MonitorRectFromWindow(rcParent, hWndChild, TRUE);
	}

	int  cxChild	= RectCX( rcChild);
	int  cyChild	= RectCY( rcChild);
	
	if( fEntire)
	{
		cxFudgeCheck = cxChild;
		cyFudgeCheck = cyChild;
		cxFudgeMove  = cxChild;
		cyFudgeMove  = cyChild;
	}

	//	
	//	Calc. how much the window should be
	//	moved or if it should be moved at all
	//

	if( rcChild.left > rcParent.right-cxFudgeCheck)			//	Horizontal, right
	{
		rcChild.left = rcParent.right-cxFudgeMove;
		fMoveIt = TRUE;
	}
	if( rcChild.right < rcParent.left+cxFudgeCheck)			//	Horizontal, left
	{
		rcChild.left = rcParent.left+cxFudgeMove-cxChild;
		fMoveIt = TRUE;
	}
	

	if( rcChild.top > rcParent.bottom-cyFudgeCheck)			//	Vertical, bottom
	{
		rcChild.top = rcParent.bottom-cyFudgeMove;
		fMoveIt = TRUE;
	}
	if( rcChild.bottom < rcParent.top+cyFudgeCheck)			//	Vertical, top
	{
		rcChild.top = rcParent.top+cyFudgeMove-cyChild;
		fMoveIt = TRUE;
	}
	
	POINT ptChild = { rcChild.left, rcChild.top};
	if(hWndParent)
		ScreenToClient( hWndParent, &ptChild);

	// Trond added: every window should at LEAST get the caption shown

	if (ptChild.y < rcParent.top)
	{
		ptChild.y = rcParent.top;
		fMoveIt = TRUE;
	}
	
	//
	//	Move the window
	//
	if( fMoveIt)
	{
		SetWindowPos( hWndChild, NULL, ptChild.x, ptChild.y ,0,0, SWP_BASIC | SWP_NOSIZE);
	}
	
	return (fMoveIt);
}

void LogFontToFontAtt(const LOGFONT &lf, FontAtt &font)
{	
	const short num = styleManager->GetFontNumber(lf.lfFaceName);
	if (num >= 0)
	{
		font.SetFaceName(lf.lfFaceName);
	}
	else
	{
		// On a Windows7 with English locale and different UI language, EnumFontFamily use
		// English names for the fonts while SystemParametersInfo(SPI_GETNONCLIENTMETRICS)
		// returns localized names.As our core only recognize one name for each font, try
		// to use the English name when this happens.
		extern const uni_char* GetEnglishNameForFont(const uni_char* font);
		font.SetFaceName(GetEnglishNameForFont(lf.lfFaceName));
	}

	font.SetHeight(-lf.lfHeight);		
	font.SetWeight(lf.lfWeight / 100);
	font.SetItalic(lf.lfItalic);
	font.SetUnderline(lf.lfUnderline);		
	font.SetStrikeOut(lf.lfStrikeOut);
	//font.SetCharSet(lf.lfCharSet);
}

void FontAttToLogFont(const FontAtt &font, LOGFONT &lf)
{
	if (NULL == font.GetFaceName())
		memset(lf.lfFaceName, 0, LF_FACESIZE);
	else
	{
		uni_strlcpy(lf.lfFaceName, font.GetFaceName(), LF_FACESIZE-1);	
	}
	//lf.lfCharSet = font.GetCharSet();
	lf.lfHeight = -font.GetHeight();
	lf.lfItalic = font.GetItalic();
	lf.lfStrikeOut = font.GetStrikeOut();
	lf.lfUnderline = font.GetUnderline();
	lf.lfWeight = font.GetWeight() * 100;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = DEFAULT_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH;
}

//	*** DG105-240998
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	CreateScreenCompatibleBitmap
//	___________________________________________________________________________
//
//
HBITMAP CreateScreenCompatibleBitmap(int width, int height, HBRUSH brush)
{
	HBITMAP bitmap = NULL;
	HWND hwnd = GetDesktopWindow();
	HDC	hdc	= GetDC(hwnd);

	if(hdc)
	{		
		bitmap = CreateCompatibleBitmap(hdc, width, height);

		if (!brush)
		{
			brush = GetSysColorBrush(COLOR_WINDOW);
		}

		if (bitmap && brush)
		{
			HBITMAP old_bitmap = (HBITMAP) SelectObject(hdc, bitmap);

			RECT rect = {0, 0, width, height};

			FillRect(hdc, &rect, brush);

			SelectObject(hdc, old_bitmap);
		}

		ReleaseDC(hwnd, hdc);
	}

	return bitmap;
}	



//	__ end defer stuff ________________________________________________________
//


BOOL IsNetworkDrive( const uni_char *szDir)
{	SUBROUTINE(504015)
	return (OpPathGetDriveType(szDir) == DRIVE_REMOTE);
}

BOOL FileExist( const uni_char *szFileName)
{
	OP_ASSERT( szFileName);
	return GetFileAttributes( szFileName) != 0xFFFFFFFF;
}

//	*** DG[HL32]
//	Lifted from MFC CWnd::CenterWindow()
void CenterWindow
(
	HWND	hwnd,
	HWND	hwndAlternateOwner,	//	= NULL
	int		cxOffset,				//	= 0
	int		cyOffset,				//	= 0
	BOOL	fUpperLeft				//	= FALSE
)
{	SUBROUTINE(504027)
    RECT MonitorRect;

	BOOL MonitorRectFromWindow(RECT& hMonitorRect, HWND wnd, BOOL onlyWorkArea, DWORD dwFlags = MONITOR_DEFAULTTOPRIMARY);
	MonitorRectFromWindow(MonitorRect, hwnd, TRUE);

	// hWndOwner is the window we should center ourself in
	HWND hWndOwner = hwndAlternateOwner ? hwndAlternateOwner : GetWindow( hwnd, GW_OWNER);

	// rcParent is the rectange we should center ourself in
	RECT rcParent;
	if (hWndOwner == NULL)
		CopyRect( &rcParent, &MonitorRect);
	else
    {
        RECT rcTemp;
		GetWindowRect(hWndOwner, &rcParent);
        if (!IntersectRect(&rcTemp, &MonitorRect, &rcParent)) //In case Opera is minimized or something...
    		CopyRect( &rcParent, &MonitorRect);
    }

	int	 xLeft;
	int	 yTop;
	RECT rcDlg;

	GetWindowRect( hwnd, &rcDlg);
	
	int	cxDlg = rcDlg.right - rcDlg.left;
	int	cyDlg = rcDlg.bottom - rcDlg.top;

	if( fUpperLeft)
	{
		//	Use parents upper,left pos
		xLeft = rcParent.left;
		yTop = rcParent.top;	
	}
	else
	{
		//	Use parents center
		int xMid = (rcParent.left + rcParent.right) / 2;
		int yMid = (rcParent.top + rcParent.bottom) / 2;

		// find dialog's upper left based on that
		xLeft = xMid - cxDlg / 2;
		yTop = yMid - cyDlg / 2;
	}

	//	Add offset
	xLeft += cxOffset;
	yTop += cyOffset;

	// if the dialog is outside the screen, move it inside
	if (xLeft < MonitorRect.left)
		xLeft = MonitorRect.left;
	else if (xLeft + cxDlg > MonitorRect.right)
		xLeft = MonitorRect.right - cxDlg;

	if (yTop < MonitorRect.top)
		yTop = MonitorRect.top;
	else if (yTop + cyDlg > MonitorRect.bottom)
		yTop = MonitorRect.bottom - cyDlg;

	SetWindowPos(hwnd, NULL, xLeft, yTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER);
}

//	---------------------------------------------------------------------------
//	FUNCITON MallocResourceData
//	
//	Returns a copy of the data for a named resource or NULL if failure.
//
//	The data returned is allocated with mal'loc().  The result size may be
//	larger than the actual size of the resource due to alignment.
//
//	HISTORY:	
//	22.nov.96	DG	Created	
//	---------------------------------------------------------------------------
void * MallocResourceData
(
	HINSTANCE	hInst,					//	App. instance
	LPCTSTR		lpName,					//	Resouce name
	LPCTSTR		lpType,					//	Resource Type
	DWORD		*dwResSize /*=NULL*/	//	Returns result size (optional)
)
{	SUBROUTINE(504032)
	LPVOID	data;				//	Ptr. to resource data
	HGLOBAL	handle;				//	Resource handle
	size_t	size	= 0;		//	Size of resource data
	LPVOID	result	= NULL;		//	The data we are returning

	HRSRC	hrsrcFound	= FindResource( hInst, lpName, lpType);

	if( hrsrcFound)
	{
		handle = LoadResource( hInst, hrsrcFound);
		if( handle)
		{
			data = LockResource( handle);
			if( data)
			{	
				//	This #ifdef is proably redundant but the old code did
				//	it this way.
				size = SizeofResource(hInst, hrsrcFound);

				if( size )
				{	//	Allocate memory for result and copy data
					result = op_malloc(size);
					if( result)
						memcpy( result, data, size);	
				}
				UnlockResource( handle);
				FreeResource( handle);
			}
			else
				FreeResource( handle);
		}
	}

	//	Did the caller ask for the size of the result ?
	if( dwResSize)
		*dwResSize = (DWORD)size;

	return result;
}

uni_char *GetFavoritesFolder( uni_char *pszBuf, DWORD bufSize)
{	SUBROUTINE(504029)
	HKEY hkeyShellFolders;
	
	if( !pszBuf)
		return NULL;

	long result = RegOpenKeyEx( HKEY_CURRENT_USER,
								UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"),
								0,KEY_READ, &hkeyShellFolders);
	if( result == ERROR_SUCCESS)
	{
		result = RegQueryValueEx( hkeyShellFolders, UNI_L("Favorites"), 0, NULL, (LPBYTE)pszBuf, &bufSize);
		RegCloseKey( hkeyShellFolders);
	}
	else
		*pszBuf = 0;

	return pszBuf;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	GetWinVersionStr
//	
//	Returns a string representing the version of Windows. E.q "Windows NT 4.0"
//	___________________________________________________________________________
//
//
LPSTR GetWinVersionStr(LPSTR szVersion, BOOL productname)	//	At least 15 characters long.
{
	if( !szVersion)
		return NULL;

	/*
	 * HENT SPRÅKVERSJON AV WINDOWS BASERT PÅ NOEN KERNEL FILER.
	 *
	char szSysFile[_MAX_PATH+50];
	GetSystemDirectory( szSysFile, sizeof(szSysFile));
	strcat( szSysFile, "\\user.exe");
	DWORD dwDummy;
	DWORD dwSize = GetFileVersionInfoSize( szSysFile, &dwDummy);
	if (dwSize > 0)
	{
		void *lpBuffer = new BYTE[dwSize];
		if (GetFileVersionInfo( szSysFile, NULL, dwSize, lpBuffer))
		{
			void *pszValue = NULL;
			UINT dwValueLen = 0;
			VerQueryValue( lpBuffer, "\\VarFileInfo\\Translation", &pszValue, &dwValueLen);

			WORD wLangId = LOWORD(*(WORD*)pszValue);
			DWORD lcid = MAKELCID( wLangId, SORT_DEFAULT);
			char szLanguage[128];
		>>>	GetLocaleInfo( lcid, LOCALE_SABBREVCTRYNAME, szLanguage, sizeof(szLanguage));
		}
	}
	*/

	OSVERSIONINFO osvi;
	
	//	Get version from OS
	memset(&osvi, 0, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	GetVersionEx (&osvi);

	//	Format string
	if (osvi.dwPlatformId == VER_PLATFORM_WIN32s)
		wsprintfA( szVersion, "Win32s %d.%d",
				osvi.dwMajorVersion,
				osvi.dwMinorVersion);

	else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
	{
		BOOL fWin95 = osvi.dwMajorVersion == 4 && osvi.dwMinorVersion < 10;
		BOOL fWin98 = osvi.dwMajorVersion == 4 && osvi.dwMinorVersion >= 10 &&  osvi.dwMinorVersion < 20;
		BOOL fWinME = ( osvi.dwMajorVersion > 4 || (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion >= 90) );

		OP_ASSERT(fWin95 || fWin98 || fWinME);	//	Let Dag know this happened.

		if( fWin95)
			wsprintfA( szVersion, "Windows 95");
		else if(fWin98)
			wsprintfA( szVersion, "Windows 98");
		else if(fWinME)
			wsprintfA( szVersion, "Windows ME");
		else 
			wsprintfA( szVersion, "Windows 9x based");
	}

	else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		/*  See bug #62690: Both MSIE and Mozilla use 5.x for all WIN32_NT platforms.
			Scripts depend on this, so be compatible when it matters.
			*/
		if(productname && osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
		{
			sprintf( szVersion, "Windows 2000");
		}
		else if(productname && osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
		{
			sprintf( szVersion, "Windows XP");
		}
		else
		{
			if(productname)
			{
				// these are not used for the UA string so safe to make more friendly
				if(osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0)
				{
					sprintf( szVersion, "Windows Vista");
				} 
				else if(osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1)
				{
					sprintf( szVersion, "Windows 7");
				}
				else if(osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2)
				{
					sprintf( szVersion, "Windows 8");
				}
			}
			else
			{
#ifdef _WIN64
				sprintf( szVersion, "Windows NT %d.%d x64",
					osvi.dwMajorVersion,
					osvi.dwMinorVersion);
#else
				sprintf( szVersion, "Windows NT %d.%d",
					osvi.dwMajorVersion,
					osvi.dwMinorVersion);
#endif
			}
		}
	}

	return szVersion;
}


//	***	LM
//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	CreateDIBFromDDB
//	___________________________________________________________________________
//
HANDLE CreateDIBFromDDB
(
	HBITMAP hBitmap,			//	Device dependent bitmap
	DWORD dwCompression,		//	Type of compression - see BITMAPINFOHEADER
	HPALETTE hPal				//	Logical palette
)
{	SUBROUTINE(504033)
	BITMAP				bm;
	BITMAPINFOHEADER	bi;
	LPBITMAPINFOHEADER 	lpbi;
	DWORD				dwLen;
	HANDLE				hDIB;
	HANDLE				handle;
	HDC 				hDC;


	OP_ASSERT( hBitmap );


	// The function has no arg for bitfields
	if( dwCompression == BI_BITFIELDS )
		return NULL;

	// If a palette has not been supplied use defaul palette
	if (hPal == NULL)
		hPal = (HPALETTE) GetStockObject(DEFAULT_PALETTE);

	// Get bitmap information
	GetObject(hBitmap, sizeof(bm), (LPSTR)&bm);

	// Initialize the bitmapinfoheader
	bi.biSize			= sizeof(BITMAPINFOHEADER);
	bi.biWidth			= bm.bmWidth;
	bi.biHeight 		= bm.bmHeight;
	bi.biPlanes 		= 1;
	// HACKKKKK BELOW 
	bi.biBitCount		= ((bm.bmPlanes * bm.bmBitsPixel) > 16) ? 16 : (bm.bmPlanes * bm.bmBitsPixel);
	bi.biCompression	= dwCompression;
	bi.biSizeImage		= 0;
	bi.biXPelsPerMeter	= 0;
	bi.biYPelsPerMeter	= 0;
	bi.biClrUsed		= 0;
	bi.biClrImportant	= 0;

	// Compute the size of the  infoheader and the color table
	int nColors = (1 << bi.biBitCount);
	if( nColors > 256 )
		nColors = 0;

	dwLen  = bi.biSize + nColors * sizeof(RGBQUAD);

	// We need a device context to get the DIB from
	hDC = GetDC(NULL);
	hPal = SelectPalette(hDC, hPal, FALSE);
	RealizePalette(hDC);

	// Allocate enough memory to hold bitmapinfoheader and color table
	hDIB = GlobalAlloc(GMEM_FIXED, dwLen);
	lpbi = (LPBITMAPINFOHEADER) GlobalLock(hDIB);

	if (!hDIB || !lpbi)
	{
		if (hDIB) GlobalFree(hDIB);
		SelectPalette(hDC, hPal, FALSE);
		ReleaseDC(NULL,hDC);
		return NULL;
	}

	*lpbi = bi;

	// Call GetDIBits with a NULL lpBits param, so the device driver
	// will calculate the biSizeImage field
	GetDIBits(hDC, 	hBitmap, 0, (UINT)bi.biHeight, NULL, (BITMAPINFO*)lpbi, DIB_RGB_COLORS);

	bi = *lpbi;

	// If the driver did not fill in the biSizeImage field, then compute it
	// Each scan line of the image is aligned on a DWORD (32bit) boundary
	if (bi.biSizeImage == 0)
	{
		bi.biSizeImage = ((((bi.biWidth * bi.biBitCount) + 31) & ~31) / 8) * bi.biHeight;

		// If a compression scheme is used the result may infact be larger
		// Increase the size to account for this.
		if (dwCompression != BI_RGB)
			bi.biSizeImage = (bi.biSizeImage * 3) / 2;
	}

	// Realloc the buffer so that it can hold all the bits
	dwLen += bi.biSizeImage;
	GlobalUnlock(hDIB);
	if ((handle = GlobalReAlloc(hDIB, dwLen, GMEM_MOVEABLE)) != NULL)
	{
		hDIB = handle;
	}
	else
	{
		GlobalFree(handle);

		// Reselect the original palette
		SelectPalette(hDC, hPal, FALSE);
		ReleaseDC(NULL, hDC);
		return NULL;
	}

	// Get the bitmap bits
	lpbi = (LPBITMAPINFOHEADER) GlobalLock(hDIB);

	// FINALLY get the DIB
	BOOL bGotBits = GetDIBits( hDC, hBitmap,
				0L,						// Start scan line
				(UINT)bi.biHeight,		// # of scan lines
				(LPBYTE)lpbi 			// address for bitmap bits
				+ (bi.biSize + nColors * sizeof(RGBQUAD)),
				(LPBITMAPINFO)lpbi,		// address of bitmapinfo
				DIB_RGB_COLORS);	// Use RGB for color table

	if( !bGotBits )
	{
		GlobalUnlock(hDIB);
		GlobalFree(hDIB);
		
		SelectPalette(hDC, hPal, FALSE);
		ReleaseDC(NULL,hDC);
		return NULL;
	}

	SelectPalette(hDC, hPal, FALSE);
	ReleaseDC(NULL,hDC);
	GlobalUnlock(hDIB);
	return hDIB;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	WinGetVersion
//	___________________________________________________________________________
//  
BOOL WinGetVersion ( OUT OSVERSIONINFO *pVersion)
{
	BOOL fOk = FALSE;
	if (pVersion)
	{
		int structSize = sizeof(OSVERSIONINFO);
		memset(pVersion, 0, structSize);
		pVersion->dwOSVersionInfoSize = structSize;
		fOk = GetVersionEx( pVersion);
	}
	return fOk;
}

OP_STATUS SearchForDefaultButtonDir(OpString* osRet)
{
	OpString pathName;

	osRet->Empty();
	RETURN_IF_ERROR(GetExePath(pathName));

	int pos = pathName.FindLastOf(UNI_L(PATHSEPCHAR));
	if(pos != KNotFound)
		pathName[pos]=0;	//strip the exename so we just have the path

	RETURN_IF_ERROR(pathName.Append("\\Buttons\\Standard.zip"));
	
	if (GetFileAttributes(pathName.CStr()) == 0xFFFFFFFF)
		return OpStatus::ERR;

	return osRet->Set(pathName);
}

OP_STATUS MakeDirFileCheckSum(OpString8* entityname, OpString* filename, OpString8* signature, BOOL global)
{
	OpString8 filename8;
	
	filename->MakeLower();
	RETURN_IF_ERROR(filename8.Set(filename->CStr()));

	unsigned int full_index;
	unsigned short hash = LinkObjectStore::GetHashIdx(filename8.CStr(), 0xffff, full_index);
	UINT32 hash2 = OpGenericStringHashTable::HashString(filename->CStr(), FALSE);

	OpString8 tmpsignature;
	RETURN_IF_ERROR(tmpsignature.AppendFormat("%s%d%d%d", entityname->CStr(), hash, full_index, hash2));

	if(global && IsSystemWin2000orXP())
	{
		RETURN_IF_ERROR(signature->Set("Global\\"));
		return signature->Append(tmpsignature);
	}
	return signature->TakeOver(tmpsignature);
}

OP_STATUS MakeOperaIniFileCheckSum(const uni_char* iniFile, OpString8* signature)
{
	OpString operainifile;
	RETURN_IF_ERROR(operainifile.Set(iniFile));

	OpString tmpBuffer;
	if (!tmpBuffer.Reserve(_MAX_PATH))
		return OpStatus::ERR_NO_MEMORY;

	GetPrivateProfileString(UNI_L("User Prefs"), UNI_L("Opera Directory"), UNI_L(""), tmpBuffer.CStr(), _MAX_PATH, iniFile);//FIXME:OOM

	OpString operadir;
	RETURN_IF_ERROR(operadir.Set(tmpBuffer));

	//if this is blank, a operadir/CACHE4 is used
	GetPrivateProfileString(UNI_L("User Prefs"), UNI_L("Cache Directory4"), UNI_L(""), tmpBuffer.CStr(), _MAX_PATH, iniFile);//FIXME:OOM

	OpString cachedir;
	RETURN_IF_ERROR(cachedir.Set(tmpBuffer));

#ifdef M2_SUPPORT
	//if this is blank a operadir/MAIL is used
	GetPrivateProfileString(UNI_L("Mail"), UNI_L("Mail Root Directory"), UNI_L(""), tmpBuffer.CStr(), _MAX_PATH, iniFile);//FIXME:OOM

	OpString maildir;
	RETURN_IF_ERROR(maildir.Set(tmpBuffer));
#endif //M2_SUPPORT

	GetPrivateProfileString(UNI_L("User Prefs"), UNI_L("Windows Storage File"), UNI_L(""), tmpBuffer.CStr(), _MAX_PATH, iniFile);//FIXME:OOM

	OpString winstorage;
	RETURN_IF_ERROR(winstorage.Set(tmpBuffer));

	operainifile.MakeLower();
	operadir.MakeLower();
	cachedir.MakeLower();
	maildir.MakeLower();
	winstorage.MakeLower();

	//first of all, check that all entries are not used by other Opera instances

	HANDLE mutex = 0;

	OpString8 filsig;
	OpString8 signame;

	OP_STATUS rc = signame.Set("opInifile");
	if(OpStatus::IsSuccess(rc))
	{
		MakeDirFileCheckSum(&signame, &operainifile, &filsig, TRUE); 
		mutex = CreateMutexA(NULL, TRUE, filsig);
		if(!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			rc = OpStatus::ERR;
		}
		if(mutex)
		{
			CloseHandle (mutex);
		}
	}	
/*
#ifdef _DEBUG
	OpString testinifile;
	OpString8 testsig1, testsig2;

	testinifile.Set("Program Files/Opera 9.5 (Alien 1)");
	MakeDirFileCheckSum(&signame, &testinifile, &testsig1, TRUE); 

	testinifile.Set("Program Files/Opera 9.5 (Alien 2)");
	MakeDirFileCheckSum(&signame, &testinifile, &testsig2, TRUE); 

	OP_ASSERT(testsig1.Compare(testsig2));
#endif // _DEBUG
*/
	if(OpStatus::IsSuccess(rc) && operadir.Length())
	{
		rc = signame.Set("opOpdirectory");
		MakeDirFileCheckSum(&signame, &operadir, &filsig, TRUE); 
		mutex = CreateMutexA(NULL, TRUE, filsig);
		if(!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			rc = OpStatus::ERR;
		}	
		if(mutex)
		{
			CloseHandle (mutex);
		}
	}
	if(OpStatus::IsSuccess(rc) && !maildir.Length())
	{
		rc = signame.Set("opMail");
		MakeDirFileCheckSum(&signame, &maildir, &filsig, TRUE); 
		mutex = CreateMutexA(NULL, TRUE, filsig);
		if(!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			rc = OpStatus::ERR;
		}
		if(mutex)
		{
			CloseHandle (mutex);
		}
	}
	if(OpStatus::IsSuccess(rc) && !cachedir.Length())
	{
		rc = signame.Set("opCache");
		MakeDirFileCheckSum(&signame, &cachedir, &filsig, TRUE); 
		mutex = CreateMutexA(NULL, TRUE, filsig);
		if(!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			rc = OpStatus::ERR;
		}	
		if(mutex)
		{
			CloseHandle (mutex);
		}
	}
	if(OpStatus::IsSuccess(rc) && !winstorage.Length())
	{
		rc = signame.Set("opWinstor");
		MakeDirFileCheckSum(&signame, &winstorage, &filsig, TRUE); 
		mutex = CreateMutexA(NULL, TRUE, filsig);
		if(!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			rc = OpStatus::ERR;
		}	
		if(mutex)
		{
			CloseHandle (mutex);
		}
	}


//	if(OpStatus::IsSuccess(rc))	//always make a signature
	{
		UINT32 hash;
		OpString string;
		OpString8 string8;

		RETURN_IF_ERROR(string.Append(operainifile));
		RETURN_IF_ERROR(string.Append(operadir));
		RETURN_IF_ERROR(string.Append(cachedir));
		RETURN_IF_ERROR(string.Append(maildir));
		RETURN_IF_ERROR(string.Append(winstorage));

		RETURN_IF_ERROR(string8.Set(string.CStr()));

		hash = CalculateHash(string8, UINT_MAX);

		if(IsSystemWin2000orXP())	//make the mutex global on 2000 and XP systems
		{
			rc = signature->Set("Global\\");
		}
		rc = signature->AppendFormat("OP_MUTEX%d", hash);
	}

	return rc;
}

WinType GetWinType()
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	//	Get OS version
	if ( !GetVersionEx(&osvi) )
	{
		return FAILED;
	}
	else if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 2))
	{
		return WIN8;
	}
	else if ((osvi.dwMajorVersion >= 6) && (osvi.dwMinorVersion >= 1))
	{
		return WIN7;
	}
	else if (osvi.dwMajorVersion >= 6) // Vista or newer, presumably compatible.
	{
		return WINVISTA;
	}
	else if ( (osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion >= 1) ) // Server is 5.2, but XP compatible.
	{
		return WINXP;
	}
	else if (osvi.dwMajorVersion == 5)
	{
		return WIN2K;
	}
	return OLDER_THAN_WIN2K;
}

unsigned int GetServicePackMajor()
{
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	if (!GetVersionEx((LPOSVERSIONINFO)&osvi))
		return 0;

	return osvi.wServicePackMajor;
}

BOOL IsSystemWin2000orXP()
{
	return (GetWinType() >= WIN2K);
}

BOOL IsSystemWinXP()
{
	return ( GetWinType() >= WINXP );
}

BOOL IsSystemWinVista()
{
	return ( GetWinType() >= WINVISTA );
}

BOOL IsSystemWin7()
{
	return ( GetWinType() >= WIN7 );
}

BOOL IsSystemWin8()
{
	return ( GetWinType() >= WIN8 );
}

OP_STATUS GetOSVersionStr(OpString& version)
{

	OP_STATUS found = OpStatus::ERR;
	WinType type = GetWinType();
	
	switch (type)
	{
		case WIN8:
			version.Set(UNI_L("8"));
			found = OpStatus::OK;
			break;
		case WIN7:
			version.Set(UNI_L("7"));
			found = OpStatus::OK;
			break;
		case WINVISTA:
			version.Set(UNI_L("Vista"));
			found = OpStatus::OK;
			break;
		case WINXP:
			version.Set(UNI_L("XP"));
			found = OpStatus::OK;
			break;
		case WIN2K:
			version.Set(UNI_L("2000"));
			found = OpStatus::OK;
			break;
		case OLDER_THAN_WIN2K:
			version.Set(UNI_L("9x"));
			found = OpStatus::OK;
			break;
		case FAILED:
			version.Set(UNI_L(""));
			found = OpStatus::ERR;
			break;
	}
	
	return found;

}


//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	UpdateCRC32
//	___________________________________________________________________________
//  
DWORD UpdateCrc32(const void* pBuffer, DWORD size, DWORD seed )
{
	if (!pBuffer)
		return 0;
	else
	{
		static const WORD wCRC16a[16]=
		{
			0000000,    0140301,    0140601,    0000500,
			0141401,    0001700,    0001200,    0141101,
			0143001,    0003300,    0003600,    0143501,
			0002400,    0142701,    0142201,    0002100,
		};
		static const WORD wCRC16b[16]=
		{
			0000000,    0146001,    0154001,    0012000,
			0170001,    0036000,    0024000,    0162001,
			0120001,    0066000,    0074000,    0132001,
			0050000,    0116001,    0104001,    0043000,
		};

		BYTE *pb = (BYTE *)pBuffer;

		for (DWORD i=0; i<size; i++)
	//	for (size; size--, pb++)
		{
			BYTE bTmp=(BYTE)(((WORD)*pb)^((WORD)seed));    // Xor CRC with new char
			seed=((seed)>>8) ^ wCRC16a[bTmp&0x0F] ^ wCRC16b[bTmp>>4];
			++ pb;
		}
		return seed;
	}
}

//	*** DG[29] ADDED
//	---------------------------------------------------------------------------
//	FUNCTION GetLastErrorMessageAlloc()
//
//	Returns a pointer to an allocated string describing the result from 
//	GetLastError().
//	
//	NOTE	The pointer should be freed with LocalFree
//			WIN32 ONLY
//	---------------------------------------------------------------------------

uni_char * GetLastErrorMessageAlloc()
{
	uni_char*	szMsg;
	DWORD		lastError;
	DWORD		result;

	lastError = GetLastError();
	result = FormatMessage( 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,							
		NULL,												
		lastError,												
  		MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),			
		(uni_char*)&szMsg,											
		0,												
		NULL												
	);
		
	//	Remove trailing CR/LF
	while( result >= 0 && (szMsg[result-1]=='\r' || szMsg[result-1]=='\n'))
		szMsg[--result] = 0;
	if( result == 0)
		LocalFree( szMsg);

	return (result ? szMsg : NULL);
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	OpPathGetDriveType
//	
//	#define DRIVE_UNKNOWN     0
//	#define DRIVE_NO_ROOT_DIR 1
//	#define DRIVE_REMOVABLE   2
//	#define DRIVE_FIXED       3
//	#define DRIVE_REMOTE      4
//	#define DRIVE_CDROM       5
//	#define DRIVE_RAMDISK     6
//	___________________________________________________________________________
//
//
int OpPathGetDriveType(const uni_char *szPath)
{
	int nDriveType = 0;
	
	if( IsStr( szPath))
	{
		if (uni_strstr( szPath, UNI_L("\\\\"))==szPath)	//	UNC
			nDriveType = DRIVE_REMOTE;
		else
		{
			uni_char szDrive[4];
		
			if (szPath[0] && szPath[1]==':')
			{
				szDrive[0]=szPath[0];
				szDrive[1]=szPath[1];
				szDrive[2]='\\';	// Win9X needs backslash at the end to work properly.
				szDrive[3]=0;

				nDriveType = GetDriveType( szDrive);
			}
		}
	}
	return nDriveType;
}

inline static size_t _strlen( char* s) {return strlen(s); };
inline static size_t _strlen( wchar_t* s) { return wcslen(s); };

/**
 * Returns a pointer into the argument string where the file name starts.
 */

uni_char* StringFileName(const uni_char* szPath)
{
	OP_ASSERT( szPath);
	if( !szPath)
		return NULL;

	uni_char *p = (uni_char*)szPath;
	uni_char *szFName = p;

	while( *p)
	{	
		if( (*p == '\\') || (*p == '/') || (*p == ':'))		//	Note: NT allows '/' to separate directories
			szFName = p+1;
		p++;
	}
	return szFName;
}

// Non-unicode version of the above
char* StringFileName(const char* szPath)
{
	OP_ASSERT( szPath);
	if( !szPath)
		return NULL;

	char *p = (char *) szPath;
	char *szFName = p;

	while( *p)
	{	
		if( (*p == '\\') || (*p == '/') || (*p == ':'))		//	Note: NT allows '/' to separate directories
			szFName = p+1;
		p++;
	}
	return szFName;
}

//	___________________________________________________________________________
//	¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//	StrAppendSubDir
//
//	Append a subdirectory to a parent directory (checking for ending slash
//	in the parent directory).
//	___________________________________________________________________________
//  
//
char *StrAppendSubDir( char *szParentDir, int nMaxLen, const char *szSubDir)
{
	if( !szParentDir)
		return NULL;
	if( !IsStr((const uni_char*) szSubDir))
		return szParentDir;

	int lenParentDir = strlen( szParentDir);
	if( lenParentDir >= nMaxLen)
		return NULL;

	//
	//	Calc size needed and wheter we should append a slash
	//
	char *pchLastChar = szParentDir+max(lenParentDir-1, 0);
	int lenNeeded = lenParentDir + strlen(szSubDir);
	BOOL fAppendSlash = FALSE;

	if( *szParentDir && (*pchLastChar != '\\' && *pchLastChar != '/'))
	{
		fAppendSlash = TRUE;
		++ lenNeeded;
	}
	
	//
	//	Check for overflow
	//
	if( lenNeeded > nMaxLen)
		return FALSE;			//	no space avail
	
	//
	//	Append subdir
	//
	if( fAppendSlash)
	{
		* ++pchLastChar = '\\';
		* (pchLastChar+1) = 0;
	}
	strcpy( *pchLastChar ? pchLastChar+1 : pchLastChar, szSubDir);
	return szParentDir;
}

OP_STATUS StrAppendSubDir(const OpStringC &parentDir, const OpStringC &subDir, OpString &result)
{
	if (parentDir.IsEmpty())
		return OpStatus::ERR;
	if (subDir.IsEmpty())
		return result.Set(parentDir);

	//
	//	Check wheter we should append a slash
	//
	uni_char lastChar = parentDir.CStr()[parentDir.Length() - 1];
	BOOL fAppendSlash = FALSE;

	if (lastChar != '\\' && lastChar != '/')
	{
		fAppendSlash = TRUE;
	}
	
	//
	//	Append subdir
	//
	OP_STATUS rc = result.Set(parentDir);
	if (OpStatus::OK != rc) return rc;

	if (fAppendSlash)
	{
		rc = result.Append(UNI_L("\\"), 1);
		if (OpStatus::OK != rc) return rc;
	}
	return result.Append(subDir);
}


OP_STATUS IsOtherUsersLoggedOn(UINT& others_logged_on)
{
	// We will start at -1, because if everything goes by the plan (we have new enough OS,
	// and the right privileges, we will count all logged in [interactive] users including our self.
	others_logged_on = (UINT)-1;

	ULONG count;
	LUID* sessions;
	
	// Enumerate the logged on sessions.
	if (LsaEnumerateLogonSessions(&count, &sessions) != NT_STATUS_SUCCESS)
		return OpStatus::ERR;

	// The logged on users on session 0 is not normal users, but services and the like.
	// So we will just be looking for users with session id > 0
	SECURITY_LOGON_SESSION_DATA* session_data = NULL;
	BOOL* sessions_in_use = OP_NEWA(BOOL, count);
	RETURN_OOM_IF_NULL(sessions_in_use);
	op_memset(sessions_in_use, 0, count*sizeof(BOOL));
	ULONG first_user_session = IsSystemWinVista() ? 1 : 0;

	for (ULONG i = 0; i < count; i++) 
	{
		// Get the session information.
		if (LsaGetLogonSessionData (&sessions[i], &session_data) == NT_STATUS_SUCCESS)
		{
			// If the session number is equal to the first session a user can connect to (first_user_session) or 
			// above  and not a session we already counted, then count it.
			if (session_data->Session >= first_user_session && sessions_in_use[session_data->Session] == FALSE)
				++others_logged_on;

			sessions_in_use[session_data->Session] = TRUE;
		}
	}

	// Free resources
	LsaFreeReturnBuffer(sessions);
	OP_DELETEA(sessions_in_use);

	return OpStatus::OK;;
  }
