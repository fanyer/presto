/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/** @file user.cpp
  * Misc userinterface functions.
  * (interface for this file is in user.h)
  */

#include "core/pch.h"

#include "platforms/windows/user.h"

#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"

#include "modules/dochand/winman.h"
#include "modules/dochand/winman_constants.h"
#include "modules/history/direct_history.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/display/prn_info.h"
#include "modules/util/filefun.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#ifdef _DDE_SUPPORT_
# include "platforms/windows/userdde.h"
#endif //_DDE_SUPPORT_
#include "platforms/windows/windows_ui/printwin.h"
#include "platforms/windows/windows_ui/res/opera.h"
#include "platforms/windows/pi/WindowsOpMessageLoop.h"
#include "platforms/windows/pi/WindowsOpScreenInfo.h"
#include "platforms/windows/pi/WindowsOpTimeInfo.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/menubar.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/windows_ui/rasex.h"
#include "platforms/windows/windows_ui/msghook.h"

#include <locale.h>

class NullAppMenuListener;

#undef PostMessage
#define PostMessage PostMessageU

NullAppMenuListener* g_null_menu_listener = NULL;
HCURSOR hWaitCursor, hArrowCursor, hArrowWaitCursor, hLinkCursor, hSplitterCursor, hVSplitterCursor, hSizeAllCursor;
HCURSOR hDragCopyCursor, hDragMoveCursor, hDragBadCursor;
WindowsOpWindow* g_main_opwindow = NULL;
UINT main_timer_id = 0;

#if defined(_DDE_SUPPORT_) && !defined(_NO_GLOBALS_)
DDEHandler *ddeHandler = 0;
#endif // _DDE_SUPPORT_ && _NO_GLOBALS_

time_t LastUserInteraction = -1;

BOOL OnCreateMainWindow()
{
	OP_PROFILE_METHOD("Created main window");

	if ((g_main_opwindow = new WindowsOpWindow) == NULL)
		return FALSE;

	g_main_opwindow->m_hwnd = g_main_hwnd;

	main_timer_id = 1;
	main_timer_id = SetTimer(g_main_hwnd, main_timer_id, 100, NULL);

	{
		OP_PROFILE_METHOD("Bootstrap booted");

		RETURN_VALUE_IF_ERROR(g_desktop_bootstrap->Boot(), FALSE);
	}

#ifdef _PRINT_SUPPORT_
	{
		OP_PROFILE_METHOD("Initialized printer settings");

		InitPrinterSettings(g_main_hwnd);
	}
#endif // _PRINT_SUPPORT_

	hArrowCursor = LoadCursor(NULL, IDC_ARROW);
	hWaitCursor = LoadCursor(NULL, IDC_WAIT);
	hArrowWaitCursor = LoadCursor(NULL, IDC_APPSTARTING);

	hLinkCursor = LoadCursor(NULL, MAKEINTRESOURCE(32649));		// IDC_HAND

	if (hLinkCursor == NULL)
	{
		hLinkCursor = LoadCursorA(hInst, "SELECT");		// (was: hLinkCursor)
	}

    hDragCopyCursor  = LoadCursorA(hInst, "DRAGCOPY");
    hDragMoveCursor  = LoadCursorA(hInst, "DRAGMOVE");
    hDragBadCursor   = LoadCursorA(hInst, "NODROP");

	hSplitterCursor  = LoadCursor(NULL, IDC_SIZENS);
	hVSplitterCursor = LoadCursor(NULL, IDC_SIZEWE);
	hSizeAllCursor = LoadCursor(NULL, IDC_SIZEALL);

	return TRUE;
}

void OnDestroyMainWindow()
{
	// we are going down

	/**
	 * DSK-330450
	 *
	 * This is a workaround and the problem it fixes should be looked more closely at.
	 * julienp: The problem is that core gets destroyed as a result of the main window being destroyed while core still holds resources
	 * related to other windows that haven't been destroyed, if something happen to send WM_CLOSE to the main window before the other
	 * windows are closed.
	 */
	if (g_application && g_application->GetActiveDesktopWindow())
		g_application->GetActiveDesktopWindow()->Close(TRUE);

#ifndef NS4P_COMPONENT_PLUGINS
	g_windows_message_loop->SetIsExiting();
#endif // !NS4P_COMPONENT_PLUGINS

	g_desktop_bootstrap->ShutDown();

	g_main_opwindow->m_hwnd = NULL;
	delete g_main_opwindow;
	g_main_opwindow = NULL;

	if (main_timer_id)
	{
		KillTimer(g_main_hwnd, main_timer_id);
	}

	if(hWaitCursor)
	{
		DestroyCursor(hWaitCursor);
	}
	if(hArrowCursor)
	{
		DestroyCursor(hArrowCursor);
	}
	if(hArrowWaitCursor)
	{
		DestroyCursor(hArrowWaitCursor);
	}
	if(hLinkCursor)
	{
		DestroyCursor(hLinkCursor);
	}
	if(hSplitterCursor)
	{
		DestroyCursor(hSplitterCursor);
	}
	if(hVSplitterCursor)
	{
		DestroyCursor(hVSplitterCursor);
	}
	if(hSizeAllCursor)
	{
		DestroyCursor(hSizeAllCursor);
	}

	if (hLinkCursor)
	{
		DestroyCursor(hLinkCursor);
	}

	if (hDragCopyCursor)
	{
		DestroyCursor(hDragCopyCursor);
	}

	if (hDragMoveCursor)
	{
		DestroyCursor(hDragMoveCursor);
	}

	if (hDragBadCursor)
	{
		DestroyCursor(hDragBadCursor);
	}

#ifdef _PRINT_SUPPORT_
	ClearPrinterSettings();
#endif // _PRINT_SUPPORT_

#ifdef _DDE_SUPPORT_
	if (ddeHandler)
		ddeHandler->Stop(TRUE);
#endif

	PostQuitMessage(0);
}

BOOL IsPluginWnd( HWND hWnd, BOOL fTestParents)
{
	BOOL fMatch=FALSE;

#ifdef _PLUGIN_SUPPORT_
	extern ATOM atomPluginWindowClass;
	HWND hWndToCheck = hWnd;
	do
	{
		fMatch = IsOfWndClass( hWndToCheck, atomPluginWindowClass);
		if (fTestParents)
			hWndToCheck = GetParent(hWndToCheck);

	} while( fTestParents && !fMatch && hWndToCheck);
#endif

	if (fMatch)
	{
		return fMatch;
	}

	return fMatch;
}

BOOL SetFontDefault(PLOGFONT pLFont)
{
    pLFont->lfHeight           = 13;
    pLFont->lfWidth            = 0;
    pLFont->lfEscapement       = 0;
    pLFont->lfOrientation      = 0;
    pLFont->lfWeight           = FW_NORMAL;
    pLFont->lfItalic           = FALSE;
    pLFont->lfUnderline        = FALSE;
    pLFont->lfStrikeOut        = FALSE;
    pLFont->lfCharSet          = DEFAULT_CHARSET;
    pLFont->lfOutPrecision     = OUT_DEFAULT_PRECIS;
    pLFont->lfClipPrecision    = CLIP_DEFAULT_PRECIS;
    pLFont->lfQuality          = DEFAULT_QUALITY;
    pLFont->lfPitchAndFamily   = DEFAULT_PITCH|FF_DONTCARE;
    lstrcpy(pLFont->lfFaceName, UNI_L("System"));
    return TRUE;
}

// WINDOW MESSAGE
LRESULT ucClose(HWND hWnd, BOOL fAskUser)
{
	// we cannot exit if in SDI and main window is disabled.. that means dialogs are up and running!

	if (!IsWindowEnabled(g_main_hwnd))
		return FALSE;

	if (KioskManager::GetInstance()->GetNoExit())
	{
		BOOL correct_password = FALSE;

		if (!correct_password)
		{
			return FALSE;
		}
	}

	//	Close dial-up connections ?
#if defined(_RAS_SUPPORT_)
	OpRasEnableAskCloseConnections(g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::ShowCloseRasDialog));		//	prefsManager might be gone in few moments
	if ( OpRasAskCloseConnections( GetDesktopWindow(), TRUE) == IDCANCEL)
		return FALSE;
#endif

	OpStringC endsound = g_pcui->GetStringPref(PrefsCollectionUI::EndSound);
	OpStatus::Ignore(SoundUtils::SoundIt(endsound, FALSE, FALSE));//FIXME:OOM
	// else handle OOM condition

    return DefWindowProc(hWnd,WM_CLOSE,0,0);
}

//	___________________________________________________________________________
//	���������������������������������������������������������������������������
//	ucTimer4TimesASecond
//
//	Returns TRUE if a 1/4 sec (or more) has elapsed since the prev.
//	time it was called.
//
//	Timer resolution (using SetTimer) is 55 ms. This functions will return
//	TRUE for each 1/4 second at average. One single call might be off but
//	at average it will be correct.
//	___________________________________________________________________________
//
BOOL ucTimer4TimesASecond()
{
	BOOL f250msElapsed = FALSE;

	static DWORD dw250msCounter = 0;
	static DWORD dwPrev = GetTickCount();

	DWORD dwNow = GetTickCount();
	DWORD dwElap = dwNow - dwPrev;

	dw250msCounter += dwElap;
	dwPrev = dwNow;


	if (dw250msCounter>=250)
	{
		dw250msCounter %= 250;
		f250msElapsed = TRUE;
	}

	return f250msElapsed;
}

//	___________________________________________________________________________
//	���������������������������������������������������������������������������
//	ucTimer
//	Called for each 110 ms (SetTimer() does not give excact values)
//	___________________________________________________________________________
//
LRESULT ucTimer(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	static BYTE count = 0;
	static ST_MESSAGETYPE stringType = ST_ASTRING;
//	Window* window = g_application ? g_application->GetActiveWindow() : NULL;

	if (!g_windowManager || !g_timecache)
		return FALSE;

   	if (wParam == ID_TIMER_DELAYED_MSG_CONTROL)
		return FALSE;

	BOOL fQuarterOfASecondsHasElapsed = ucTimer4TimesASecond();

	if (fQuarterOfASecondsHasElapsed)
	{
		if (g_windowManager)
			g_windowManager->OnTimer();
	}

	if (!((count+1)%3))
		CheckForValidOperaWindowBelowCursor();

	count ++;

	if (count == 9)
	{
		count = 0;
#ifdef HAVE_TIMECACHE
		g_timecache->UpdateCurrentTime();
#elif defined(PREFSMAN_USE_TIMECACHE)
		prefsManager->UpdateCurrentTime();
#endif

		g_url_api->CheckTimeOuts();

#ifdef _TRANSFER_WINDOW_SUPPORT_
		if (transferWindow)
			transferWindow->UpdateProgress();
#endif
	}

    return DefWindowProc(hWnd,message,wParam,lParam);
}

BOOL ResetStation()
{
	if(KioskManager::GetInstance()->GetEnabled())       //be more agressive in kioskmode
	{
		if(KioskManager::GetInstance()->GetKioskResetStation() || KioskManager::GetInstance()->GetResetOnExit())
		{
			if(g_url_api)
			{
				g_url_api->CleanUp();
				g_url_api->PurgeData(TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE);
			}

			if(globalHistory)
			{
				g_globalHistory->DeleteAllItems();
				g_globalHistory->Save(TRUE);
			}

			if(directHistory)
			{
				g_directHistory->DeleteAllItems();
				g_directHistory->Save(TRUE);
			}
		}
	}
	return TRUE;
}

LRESULT ucWMDrawItem(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPDRAWITEMSTRUCT item = (LPDRAWITEMSTRUCT) lParam;

	if (item->CtlType == ODT_MENU && g_windows_menu_manager)
	{
		return g_windows_menu_manager->OnDrawItem(hWnd, item);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


//	WM_MEASUREITEM
LRESULT ucMeasureItem(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
	LPMEASUREITEMSTRUCT	item = (LPMEASUREITEMSTRUCT) lParam;

	if (item->CtlType == ODT_MENU && g_windows_menu_manager)
	{
		return g_windows_menu_manager->OnMeasureItem(hWnd, item);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}


//	___________________________________________________________________________
//	���������������������������������������������������������������������������
//	OnSystemSettingChanged
//	___________________________________________________________________________
//
void OnSettingsChanged()
{
	if (g_op_system_info)
	{
		((WindowsOpSystemInfo*)g_op_system_info)->OnSettingsChanged();
	}

	if (g_op_time_info)
	{
		((WindowsOpTimeInfo*)g_op_time_info)->OnSettingsChanged();
	}


	if (g_op_screen_info)
	{
		((WindowsOpScreenInfo*)g_op_screen_info)->OnSettingsChanged();
	}

	if (g_application)
	{
		g_application->SettingsChanged(SETTINGS_SYSTEM_SKIN);
	}
}

//-----------------------------------------------------------------------------------------

static BOOL WriteBitmapToFile(OpString* filename, OpBitmap* bitmap)
{
	if( !(bitmap && filename && filename->HasContent()) )
	{
		return FALSE;
	}

	HANDLE fh;
	DWORD dwWritten;

	UINT32 width = bitmap->Width();
	UINT32 height = bitmap->Height();
	UINT8 bpp = 32;  // always get bitmap data in 32 bit

	//First set up headers
	UINT32 header_size = sizeof(BITMAPINFOHEADER);
	BITMAPINFO bmpi;
	bmpi.bmiHeader.biSize = header_size;
	bmpi.bmiHeader.biWidth = width;
	bmpi.bmiHeader.biHeight = height;
	bmpi.bmiHeader.biPlanes = 1;
	bmpi.bmiHeader.biBitCount = bpp;
	bmpi.bmiHeader.biCompression = BI_RGB;
	bmpi.bmiHeader.biSizeImage = 0;
	bmpi.bmiHeader.biXPelsPerMeter = 0;
	bmpi.bmiHeader.biYPelsPerMeter = 0;
	bmpi.bmiHeader.biClrUsed = 0;
	bmpi.bmiHeader.biClrImportant = 0;

	UINT32 bytes_per_line = width * (bpp >> 3);
	UINT32 len = bytes_per_line * height + header_size;

	BITMAPFILEHEADER fileheader;
	fileheader.bfType = ((WORD) ('M' << 8) | 'B');			// is always "BM"
	fileheader.bfSize = len + sizeof(BITMAPFILEHEADER);
	fileheader.bfReserved1 = 0;
	fileheader.bfReserved2 = 0;
	fileheader.bfOffBits= (DWORD) (sizeof( fileheader ) + bmpi.bmiHeader.biSize ) + sizeof(RGBQUAD);

	HANDLE hGmem = GlobalAlloc(GHND | GMEM_DDESHARE, len);
	if (!hGmem)
	{
		return FALSE;
	}

	LPSTR buffer = (LPSTR) GlobalLock(hGmem);

	memcpy(buffer, &bmpi, header_size);

	// write all bitmap data into the clipboard bitmap
	for (UINT32 line = 0; line < height; line++)
	{
		bitmap->GetLineData(&buffer[line * bytes_per_line + header_size], height - line - 1);
	}
	GlobalUnlock(hGmem);

	fh = CreateFile(filename->CStr(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (fh == INVALID_HANDLE_VALUE)
	{
		GlobalFree(hGmem);
		return FALSE;
	}

	// Write the file header
	WriteFile(fh, (LPSTR)&fileheader, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);

	// Write the DIB
	WriteFile(fh, buffer, len, &dwWritten, NULL);
	CloseHandle(fh);
	GlobalFree(hGmem);

	return TRUE;
}

OP_STATUS DesktopPiUtil::SetDesktopBackgroundImage(URL& url)
{
    OpString tmp;
	RETURN_IF_LEAVE(url.GetAttributeL(URL::KSuggestedFileName_L, tmp, TRUE));

	OpString osTmp;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_PICTURES_FOLDER, osTmp));
	RETURN_IF_ERROR(osTmp.Append(tmp));

	OpString osNewSkinFile;
	RETURN_IF_ERROR(GetUniqueFileName(osTmp, osNewSkinFile));

	Image img = UrlImageContentProvider::GetImageFromUrl(url);
	if(img.IsEmpty())	
	{
		return OpStatus::ERR;
	}

	osNewSkinFile.Delete(osNewSkinFile.FindLastOf('.'));
	RETURN_IF_ERROR(osNewSkinFile.Append(".bmp"));

	OpBitmap* bitmap = img.GetBitmap(NULL);
	if(!bitmap)	
	{
		return OpStatus::ERR;
	}

	if(!WriteBitmapToFile(&osNewSkinFile, bitmap) )
	{
		img.ReleaseBitmap();
		return OpStatus::ERR;
	}
	img.ReleaseBitmap();

	if( !SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (PVOID)osNewSkinFile.CStr(), SPIF_SENDCHANGE) )
	{
		return OpStatus::ERR;
	}

	if (!SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (PVOID)osNewSkinFile.CStr(), SPIF_UPDATEINIFILE) )
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

