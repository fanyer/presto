/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef _WINDOWSOPAUINSTALLER_H_
#define _WINDOWSOPAUINSTALLER_H_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/updater/pi/auinstaller.h"

class WindowsAUInstaller : public AUInstaller
{
private:
	LPWORD lpwAlign (LPWORD lpIn);

	static const uni_char* UPGRADER_MUTEX_NAME;
	HANDLE m_upgrader_mutex;

public:
	WindowsAUInstaller();
	~WindowsAUInstaller();

	virtual AUI_Status Install(uni_char *install_path, uni_char *package_file);
	virtual BOOL HasInstallPrivileges(uni_char *install_path);
	virtual BOOL ShowStartDialog(uni_char *caption, uni_char *message, uni_char *yes_text, uni_char *no_text);
	virtual bool IsUpgraderMutexPresent();
	virtual void CreateUpgraderMutex();

	virtual void Sleep(UINT32 ms);

	static INT_PTR CALLBACK StartDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif // AUTOUPDATE_USE_UPDATER
#endif // AUTO_UPDATE_SUPPORT

#endif // _WINDOWSOPAUINSTALLER_H_
