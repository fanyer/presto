/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/windows/user_fun.h"
#include "platforms/windows/windows_ui/OpShObjIdl.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/util/path.h"

#include "platforms/windows/win_handy.h"
#include "platforms/windows/utils/ntdll.h"
#undef PostMessage
#define PostMessage PostMessageU

#include <process.h>
#include <Psapi.h>
#include <Tlhelp32.h>

#if !defined (WS_EX_LAYERED)
// Imported these from vc8 winuser.h:
#define WS_EX_LAYERED           0x00080000
#endif


OP_STATUS ParseMenuItemName(const OpStringC& name, OpString& parsed_name, int& amp_pos)
{
	amp_pos = -1;
	if (name.IsEmpty())
	{
		parsed_name.Empty();
		return OpStatus::OK;
	}

	RETURN_OOM_IF_NULL(parsed_name.Reserve(name.Length()));
	uni_char* out = parsed_name.DataPtr();

	for (const uni_char* in = name.DataPtr(); *in != '\0'; ++in)
	{
		if (*in == '&')
		{
			++in;
			if (*in != '&')
			{
				amp_pos = out - parsed_name.DataPtr();
				if (*in == '\0')
					break;
			}
		}
		*(out++) = *in;
	}
	*out = '\0';

	return OpStatus::OK;
}

OP_STATUS DrawMenuItemText(HDC hDC, const OpStringC& text, LPCRECT rect, int underline_pos)
{
	const int length = text.Length();

	SIZE size;
	if (!GetTextExtentPoint32(hDC, (LPCTSTR)text.CStr(), length, &size))
		return OpStatus::ERR;

	int height = rect->bottom - rect->top;

	SetTextAlign(hDC, TA_TOP | TA_NOUPDATECP | TA_LEFT);

	int v_offset = 0;
	int h_offset = 0;

	// Center vertically.
	v_offset = (height - size.cy) / 2;

	if (underline_pos != -1)
	{
		int prefix_x = 0;
		int prefix_width;

		if (underline_pos)
		{
			GetTextExtentPoint32(hDC, text.CStr(), underline_pos, &size);
			prefix_x = size.cx;
		}

		GetTextExtentPoint32(hDC, text.CStr() + underline_pos, 1, &size);
		prefix_width = size.cx;

		HBRUSH brush = CreateSolidBrush(GetTextColor(hDC));

		RECT prefix_rect;
		
		prefix_rect.left = rect->left + prefix_x + h_offset;
		prefix_rect.top = rect->bottom - v_offset - 1;
		prefix_rect.right = prefix_rect.left + prefix_width;
		prefix_rect.bottom = prefix_rect.top + 1;

		FillRect(hDC, &prefix_rect, brush);

		DeleteObject(brush);
	}

	BOOL success = ExtTextOut(hDC, rect->left + h_offset, rect->top + v_offset, ETO_CLIPPED, rect, (LPTSTR)text.CStr(), length, NULL);
	return success ? OpStatus::OK : OpStatus::ERR;
}

typedef struct _ENUMINFO {
    HINSTANCE hInstance;        // Supplied on input.
    HWND hWnd;                  // Filled in by enum function.
} ENUMINFO, FAR *LPENUMINFO;


extern "C" BOOL CALLBACK EnumWndProc(HWND hWnd, LPARAM lParam)
{
    HINSTANCE hInstance;
    LPENUMINFO lpInfo;

    lpInfo = (LPENUMINFO) lParam;

    hInstance = (HMODULE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

    if (hInstance == lpInfo->hInstance) {

        lpInfo->hWnd = hWnd;

        return FALSE;
    }

    return TRUE;
}

#ifdef _PRINT_SUPPORT_
void CancelPrintPreview(Window* window)
{
	if(window && window->GetPreviewMode())
	{
		window->TogglePrintMode(TRUE);
	}
}
#endif // _PRINT_SUPPORT_

uni_char* ReplaceCharsWithStr(const uni_char* source, uni_char* dest, uni_char* strReplace, uni_char chrReplace)
{
	if (!source)
	{
		dest[0] = 0;
		return dest;
	}

	int j=0;
	for(int i=0; *(source+i); i++)
	{
		if(*(source+i) == chrReplace)
		{
			uni_strcpy((dest+j), strReplace); 	
			j+=uni_strlen(strReplace);
		}
		else 
		{
			*(dest+j) = *(source+i);
			j++;
		}
	}
	*(dest+j) = 0;
	return dest;
}

OP_STATUS GetUniqueFileName(OpString& inName, OpString& outName)
{
	OpString osExtension;
	OpString osTemp;
	OpString osFilename; 
	outName.Set(inName);	// FIXME: OOM
	if(inName.Length())
	{
		osExtension.Set( inName.SubString(inName.FindLastOf('.')) );		// FIXME: OOM (twice)
		if(inName.FindLastOf('(') != -1 )
		{
			osFilename.Set( inName.SubString(0, inName.FindLastOf('(')) );	// FIXME: OOM (twice)
		}
		else
		{
			osFilename.Set( inName.SubString(0, inName.FindLastOf('.')) );	// FIXME: OOM (twices)
		}

		struct stat buf;

		int res;

		res =_wstat(inName.CStr(), &buf);

		if (res==0)													//trust the file status 
		{
			char* szDigit = new char[64];
			if (errno==EEXIST || errno == ENOENT || errno == EACCES)
			{
				for(int i=1; (errno==EEXIST || errno == ENOENT || errno == EACCES) && res==0 && i<50; i++)
				{
					if(osFilename.Length() && osExtension.Length())
					{
						itoa(i, szDigit, 10);
						osTemp.Set( osFilename );
						osTemp.Append( " (" );
						osTemp.Append( szDigit );
						osTemp.Append( ")" );
						osTemp.Append( osExtension );
					}
					else
					{ 
						break;
					}
					res =_wstat(osTemp.CStr(), &buf);
				}
				outName.Set( osTemp );
			}
			delete [] szDigit;
		}
	}
	return OpStatus::OK;
}

const char* GetSystemCharsetName()
{
	static char codePageName[32] = "";
	UINT codePage = GetACP();
	switch (codePage)
	{
		case 936:
			strcpy(codePageName, "euc-cn");
			break;
		case 950:
			strcpy(codePageName, "big5");
			break;
		case 932:
			strcpy(codePageName, "shift_jis");
			break;
		case 949:
			strcpy(codePageName, "euc-kr");
			break;
		case 1200:
		case 1250:
		case 1251:
		case 1252:
		case 1253:
		case 1254:
		case 1255:
		case 1256:
		case 1257:
		case 1258:
		case 1361:
			sprintf(codePageName, "windows-%i", codePage);
			break;
		default:
			strcpy(codePageName, "windows-1252");
	}
	return codePageName;
}


BOOL CharsetSupportedBySystem(const uni_char* str)
{
	if (!(str && *str))
		return TRUE;

	BOOL conversion_failed = FALSE;
	/*int res =*/ WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, str, -1, NULL, 0, "?", &conversion_failed);

	return (!conversion_failed);
}


//	___________________________________________________________________________
//	PPTBlt
//  Like BitBlt, but for per-pixel transparency (and optional blanket transparency)
//	___________________________________________________________________________
//  
void PPTBlt (HWND hWnd, HDC dst, HDC src, BYTE transparency = 0xff)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);

	SIZE size    = {rect.right - rect.left, rect.bottom - rect.top};
	POINT pos    = {rect.left             , rect.top              };
	POINT origin = {0                     , 0                     };

	BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, transparency, AC_SRC_ALPHA};

	UpdateLayeredWindow(hWnd, dst, &pos, &size, src, &origin, NULL, &blend_function, ULW_ALPHA);
}

//	___________________________________________________________________________
//	AlphaBlt
//	___________________________________________________________________________
//  

// UNDOCUMENTED BY MICROSOFT.. SHOULD PROBABLY NOT BE USED
#ifndef AC_SRC_NO_PREMULTI_ALPHA
#define AC_SRC_NO_PREMULTI_ALPHA 0x01
#endif
#ifndef AC_SRC_NO_ALPHA
#define AC_SRC_NO_ALPHA 0x02
#endif 

void AlphaBlt(HDC dst, INT32 dstx, INT32 dsty, INT32 dstwidth, INT32 dstheight, HDC src, INT32 srcx, INT32 srcy, INT32 srcwidth, INT32 srcheight)
{
	BYTE transparency = 0xff;  //  No blanket transparency as implemented.
	BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, transparency, AC_SRC_ALPHA};
	AlphaBlend(dst, dstx, dsty, dstwidth, dstheight, src, srcx, srcy, srcwidth, srcheight, blend_function);
}

void OpacityBlt(HDC dst, INT32 dstx, INT32 dsty, INT32 dstwidth, INT32 dstheight, HDC src, INT32 srcx, INT32 srcy, INT32 srcwidth, INT32 srcheight, BYTE opacity)
{
	BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, opacity, 0};
	AlphaBlend(dst, dstx, dsty, dstwidth, dstheight, src, srcx, srcy, srcwidth, srcheight, blend_function);
}

extern OpString g_spawnPath;

// Returns the path for the Opera executable.
// E.g. "C:\Program Files\Opera\opera.exe"
OP_STATUS GetExePath(OpString &path)
{
	if (g_spawnPath.HasContent())
	{
		return path.Set(g_spawnPath);
	}

	DWORD n_size=0, ret_len=0;
	uni_char* buffer;
	for ( UINT i = 1 ; ret_len == n_size ; i++)
	{
		n_size = MAX_PATH * i;
		buffer = path.Reserve(n_size);
		if(!buffer)
			return OpStatus::ERR_NO_MEMORY;
		ret_len = GetModuleFileName(NULL, buffer, n_size);
	}
	return ret_len ? OpStatus::OK:OpStatus::ERR; 
}

BOOL GetLocation(uni_char result[3])
{
	// check location first
	GEOID geoid = GetUserGeoID(GEOCLASS_NATION);
	if (geoid != GEOID_NOT_AVAILABLE && GetGeoInfo(geoid, GEO_ISO2, result, 3, 0) == 3)
	{
		return TRUE;
	}

	// fallback to locale
	if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, result, 3) == 3)
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Terminates all other processes using the same opera module as current.
 */ 
void TerminateCurrentHandles()
{
	char processname[MAX_PATH];
	DWORD current_process = GetCurrentProcessId();

	if(!GetModuleFileNameA(NULL, processname, sizeof(processname)))
		return;

	OpAutoHANDLE process_snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

	if (process_snapshot.get() == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(process_snapshot, &pe32))
		return;

	static const DWORD PATH_LEN = 0x1000;
	char process_path[PATH_LEN];

	do
	{
		if (pe32.th32ProcessID==current_process)
			//suicide blocker ;) 
			continue;

		HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);

		if (!process)
			continue;

		ZeroMemory(process_path, PATH_LEN);

		if (GetModuleFileNameExA(process, NULL, process_path, PATH_LEN) != 0)
		{
			if(!op_stricmp(processname, process_path))
			{
				UINT x = 0;
				TerminateProcess(process,x);
			}
		}

		CloseHandle(process);
	}
	while(Process32Next(process_snapshot, &pe32));

	return;
}

