// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund && Julien Picalausa
//


#ifndef OPERAINSTALLERUI_H
#define OPERAINSTALLERUI_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "platforms/windows/installer/OperaInstaller.h"

class OperaInstaller;
class InstallerWizard;
class ProcessItem;

class OperaInstallerUI: private MessageObject, private DesktopWindowListener
{
public:
	
	OperaInstallerUI(OperaInstaller* opera_installer);
	~OperaInstallerUI();

	void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void Pause(BOOL pause);

	/** Creates and shows the installer/uninstaller wizard.
	  * This method is meant to be called exactly one time by the opera installer object.
	  * The dialog created lives until the installer terminates.
	  */
	OP_STATUS	Show();

	/** Shows the progress page of the installer wizard. If the wizard hasn't been created before by calling Show, it will be created by this method.
	  * This method is meant to be called exactly one time by the opera installer object.
	  * If this method creates the dialog, it again lives until the installer terminates.
	  */
	OP_STATUS	ShowProgress(UINT steps_count);

	/** Creates a new instance (different from the one created by Show/ShowProgress) of the wizard dialog and shows the page for locked file handlers.
	  * This method can be called several times from the opera installer object.
	  * This method doesn't return until the dialog it created was destroyed. The dialog created by Show/ShowProgress is hidden in the meanwhile.
	  */
	OP_STATUS	ShowLockingProcesses(const uni_char* file_name, OpVector<ProcessItem>& processes);

	/** Creates and display an error message using a SimpleDialog.
	  * This method can be called any number of times.
	  * This method doesn't return until the error dialog has been dismissed by the user.
	  */
	BOOL		ShowError(OperaInstaller::ErrorCode error, const uni_char* item_affected = NULL, const uni_char* secondary_error_code = NULL, BOOL is_critical = TRUE);

	/** Creates and display a SimpleDialog, asking the user whether they want to reboot.
	  * This method can be called any number of time by the opera installer object but it hardly makes sense to call it more than once.
	  * This method doesn't return until the error dialog has been dismissed by the user.
	  */
	void		ShowAskReboot(BOOL has_rights, BOOL &reboot);

	void		ForbidClosing(BOOL forbid_closing);
	void		HideAndClose();

private:

	struct ErrorToStringMapping
	{
		OperaInstaller::ErrorCode error_code;
		int str_code;
	};

	static const ErrorToStringMapping m_error_mapping[];

	OperaInstaller*			m_opera_installer;
	InstallerWizard*		m_dialog;
	INT						m_paused;
};

#endif // OPERAINSTALLERUI_H
