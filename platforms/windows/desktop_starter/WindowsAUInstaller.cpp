/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "WindowsAUInstaller.h"

#include "adjunct/autoupdate/updater/pi/aufileutils.h"
#include "adjunct/desktop_pi/launch_pi.h"

extern HINSTANCE hInst;	// Handle to instance.

const uni_char* WindowsAUInstaller::UPGRADER_MUTEX_NAME = UNI_L("Global\\OP_MUTEX_UPGRADER_READY");

AUInstaller* AUInstaller::Create()
{
	return (AUInstaller*) new WindowsAUInstaller ();
}

WindowsAUInstaller::WindowsAUInstaller():
	m_upgrader_mutex(NULL)
{
}

WindowsAUInstaller::~WindowsAUInstaller()
{
	if (m_upgrader_mutex)
		CloseHandle(m_upgrader_mutex);
}

AUInstaller::AUI_Status WindowsAUInstaller::Install(uni_char *install_path, uni_char *package_file)
{

	AUFileUtils* file_utils = AUFileUtils::Create();
		
	uni_char* module_path = NULL;
	uni_char exe_path[MAX_PATH];
	uni_char dll_path[MAX_PATH];

	if (!file_utils || file_utils->GetExePath(&module_path) != AUFileUtils::OK)
		return AUInstaller::ERR;

	uni_strncpy(exe_path, module_path, MAX_PATH);
	uni_strncpy(uni_strrchr(exe_path, '\\') + 1, UNI_L("Opera.exe"), 10);
	uni_strncpy(dll_path, module_path, MAX_PATH);
	uni_strncpy(uni_strrchr(dll_path, '\\') + 1, UNI_L("Opera.dll"), 10);

#ifndef _DEBUG
	LaunchPI* launch_pi = LaunchPI::Create();
	if (!launch_pi || !launch_pi->VerifyExecutable(exe_path) || !launch_pi->VerifyExecutable(dll_path))
	{
		delete launch_pi;
		return AUInstaller::ERR;
	}

	delete launch_pi;
#endif // _DEBUG

	*(uni_strrchr(install_path,'\\')) = 0;
	uni_char args[MAX_PATH + 50];
	uni_strcpy(args, UNI_L("/install /silent /autoupdate /launchopera 0 /setdefaultbrowser 0 /installfolder \""));
	uni_strncat(args, install_path, MAX_PATH);
	uni_strcat(args, UNI_L("\""));

	SHELLEXECUTEINFO info;
	memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpFile = exe_path;
	info.lpParameters = args;
	info.nShow = SW_SHOW;

	HANDLE evt = CreateEvent(NULL, TRUE, FALSE, UNI_L("OperaInstallerCompletedSuccessfully"));

	if (!evt)
		return AUInstaller::ERR;

	if (!ShellExecuteEx(&info))
		return AUInstaller::ERR;
	HANDLE wait_objects[2] = { info.hProcess, evt };

	DWORD res = WaitForMultipleObjects(2, wait_objects, FALSE, INFINITE);

	return (res == (WAIT_OBJECT_0 + 1))?AUInstaller::OK:AUInstaller::ERR;
}

BOOL WindowsAUInstaller::HasInstallPrivileges(uni_char *install_path)
{
	return TRUE;
}

inline LPWORD WindowsAUInstaller::lpwAlign(LPWORD lpIn)
{
	return (LPWORD)(((DWORD_PTR)lpIn + 3) & ~3);
}

INT_PTR CALLBACK WindowsAUInstaller::StartDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
			case IDYES:
				EndDialog(hwndDlg, 2);
				return TRUE;
			case IDNO:
				EndDialog(hwndDlg, 1);
				return TRUE;
		}
	}
	else if (uMsg == WM_INITDIALOG)
	{
		//HICON icon = (HICON)LoadImageA(hInst, "ZHOTLISTICON", IMAGE_ICON, 16, 16, LR_SHARED | LR_CREATEDIBSECTION);
		HICON icon = LoadIconA(hInst, "OPERA");
		SendMessageA(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM) icon);

		return TRUE;
	}
	else if (uMsg == WM_SETICON)
	{
		return TRUE;
	}
	return FALSE;
}
BOOL WindowsAUInstaller::ShowStartDialog(uni_char *caption, uni_char *message, uni_char *yes_text, uni_char *no_text)
{

	LPDLGTEMPLATE lpdt;
	LPDLGITEMTEMPLATE lpdit;
	LPWORD lpw;
	LPWSTR lpwsz;

	uni_char* lpFontName = UNI_L("Ms Shell Dlg");

	lpdt = (LPDLGTEMPLATE)GlobalAlloc(GMEM_ZEROINIT, 2048);
	if (!lpdt)
		return -1;

	//---------------------
	// Define a dialog box.
	//---------------------
	lpdt->style = (DWORD)WS_POPUP | WS_BORDER | DS_MODALFRAME | DS_CENTER | DS_SETFOREGROUND | WS_CAPTION | DS_3DLOOK | DS_SHELLFONT;
	lpdt->cdit = (WORD)3;											// Number of controls
	lpdt->x = (WORD)30; lpdt->y = (WORD)30;							// Position
	lpdt->cx = (WORD)240; lpdt->cy = (WORD)60;						// Size
	
	lpw = (LPWORD) (lpdt + 1);
	lpw++;															// No menu
	lpw++;															// Predefined dialog box class (by default)
	for (lpwsz = (LPWSTR)lpw; (*lpwsz++ = *caption++) != 0; );		// Title
	lpw = lpwsz;
	*lpw++ = (WORD)8;												// Point size (text)
	for (lpwsz = (LPWSTR)lpw; (*lpwsz++ = *lpFontName++) != 0; );	// Font name

	//-----------------------
	// Define the Yes button.
	//-----------------------
	lpw = lpwAlign(lpwsz);											// Align DLGITEMTEMPLATE on DWORD boundary
	lpdit = (LPDLGITEMTEMPLATE) lpw;
	lpdit->x  = 125; lpdit->y  = 40;								// Position
	lpdit->cx = 50; lpdit->cy = 15;									// Size
	lpdit->id = IDYES;												// Yes button identifier
	lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;		// Button style

	lpw = (LPWORD) (lpdit + 1);
	*lpw++ = 0xFFFF;
	*lpw++ = 0x0080;												// Button class

	for (lpwsz = (LPWSTR)lpw; (*lpwsz++ = *yes_text++) != 0; );		// Button text
	lpw = lpwsz;
	lpw++;															// No creation data

	//-----------------------
	// Define the No button.
	//-----------------------
	lpw = lpwAlign (lpw);											// Align DLGITEMTEMPLATE on DWORD boundary
	lpdit = (LPDLGITEMTEMPLATE) lpw;
	lpdit->x  = 185; lpdit->y  = 40;								// Position
	lpdit->cx = 50; lpdit->cy = 15;									// Size
	lpdit->id = IDNO;												// No button identifier
	lpdit->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;			// Button style

	lpw = (LPWORD) (lpdit + 1);
	*lpw++ = 0xFFFF;
	*lpw++ = 0x0080;												// Button class atom

	for (lpwsz = (LPWSTR)lpw; (*lpwsz++ = *no_text++) != 0; );
	lpw = lpwsz;
	lpw++;															// No creation data

	//-----------------------
	// Define a static text control.
	//-----------------------
	lpw = lpwAlign (lpw);											// Align DLGITEMTEMPLATE on DWORD boundary
	lpdit = (LPDLGITEMTEMPLATE) lpw;
	lpdit->x  = 10; lpdit->y  = 6;									// Position
	lpdit->cx = 230; lpdit->cy = 25;								// Size
	lpdit->id = 200;												// Text identifier
	lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;					// Text style

	lpw = (LPWORD) (lpdit + 1);
	*lpw++ = 0xFFFF;
	*lpw++ = 0x0082;												// Static class

	for (lpwsz = (LPWSTR)lpw; (*lpwsz++ = *message++) != 0; );
//    lpw = lpwsz;
//    lpw++;														// No creation data

	int res = DialogBoxIndirectA(hInst, lpdt, NULL, WindowsAUInstaller::StartDialogProc);
	GlobalFree(lpdt); 

	return (res == 2);

}

void WindowsAUInstaller::CreateUpgraderMutex()
{
	// Ignore the return value, we won't be able to do much if creating the mutex fails anyway.
	m_upgrader_mutex = CreateMutex(NULL, TRUE, UPGRADER_MUTEX_NAME);
}

bool WindowsAUInstaller::IsUpgraderMutexPresent()
{
	// Try to open the mutex to see if it is present in the system
	HANDLE mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, UPGRADER_MUTEX_NAME);

	if (mutex)
	{
		// If we actually did succeed in opening the mutex, don't forget to close it
		// since we don't want to hold it over the lifetime of OperaUpgrader.exe.
		CloseHandle(mutex);
		return true;
	}

	return false;
}

void WindowsAUInstaller::Sleep(UINT32 ms)
{
	::Sleep(ms);
}

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT
