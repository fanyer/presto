/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file declares the AUInstaller interface which is
 * the functions required to install a package on a platform
 * and the associated tasks
 *
 * @author Adam Minchinton
 */

#ifndef _AUINSTALLER_H_INCLUDED_
#define _AUINSTALLER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

class AUInstaller
{
public:
	enum AUI_Status
	{
		OK,				// Succedded
		ERR,			// Generic error
		ERR_MEM,		// Out of memory
		NO_ACCESS,		// No permission to install the package/patch
		FILE_NOT_FOUND	// File doesn't exist
	};	

	virtual ~AUInstaller() {}

	/** Create an AUInstaller object.
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	static AUInstaller* Create();

	/**
	 * Installs a package
	 *
	 * @param install_path  Full path to the installation
	 * @param package_file  Full path of package to install
	 *
	 * @return AUF_Status. 
	 */
	virtual AUI_Status Install(uni_char *install_path, uni_char *package_file) = 0;

	/**
	 * Installs a patch
	 *
	 * @param install_path  Full path to the installation
	 * @param patch_file  Full path of patch to install
	 *
	 * @return AUF_Status. 
	 */
//	virtual AUI_Status Patch(uni_char *install_path, uni_char *patch_file) = 0;

	/**
	 * Checks if the user has the priviledges to do the
	 * install
	 *
	 * @param install_path  Full path to the installation
	 *
	 * @return True they can. 
	 */
	virtual BOOL HasInstallPrivileges(uni_char *install_path) = 0;

	/**
	 * Dialog show to check if the installation is to be applied at this startup
	 *
	 * @param caption  Text on the dialog caption
	 * @param message  Text on the dialog message
	 * @param yes_text Text on the dialog yes button
	 * @param no_text  Text on the dialog no button
	 *
	 * @return True they can. 
	 */
	virtual BOOL ShowStartDialog(uni_char *caption, uni_char *message, uni_char *yes_text, uni_char *no_text) = 0;

	/**
	 * Check if Opera is already running
	 *
	 * @param install_path  Full path to the Opera executable
	 *
	 * @return BOOL. 
	 */	
	virtual BOOL IsOperaRunning(const uni_char *install_path) { return FALSE; }

	/**
	 * Check if the upgrader mutex is present in the system. The mutex name is 
	 * determined by the AUUpdater::UPGRADER_MUTEX_NAME member.
	 * This method is expected to not to hold the mutex after it finishes.
	 *
	 * @return true in case the mutex is present, false in case it is not.
	 */
	virtual bool IsUpgraderMutexPresent() { return true; }

	/**
	 * Create the upgrader mutex in the system.
	 * The mutex is expected to die along with OperaUpgrader.exe, so any checks for 
	 * the mutex being present should not hold it any longer than necessary.
	 */
	virtual void CreateUpgraderMutex() {}

	/**
	 * Suspend current thread
	 *
	 * @param ms  Time to sleep, in milliseconds
	 *
	 */	
	virtual void Sleep(UINT32 ms) {}
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _AUINSTALLER_H_INCLUDED_
