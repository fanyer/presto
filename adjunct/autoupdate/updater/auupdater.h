/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _AUUPDATER_H_INCLUDED_
#define _AUUPDATER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

class AUFileUtils;
class AUInstaller;
class AUDataFileReader;

class AUUpdater
{
public:
	enum Mode
	{
		Checker,
		Installer
	};

	enum AUU_Status
	{
		OK,			// Succeeded
		ERR,		// Generic error
		ERR_MEM		// Out of memory
	};
	
	AUUpdater();
	virtual ~AUUpdater(); 
	
	/**
	 * This function will contain all of the autoupdate
	 * logic for Opera itself and Opera running as the Update app.
	 *
	 * @return BOOL, FALSE: Opera should exit immediately
	 *				 TRUE:  Opera should start normally 
	 */
	BOOL Run();

	static BOOL WaitFileWasPresent() { return m_wait_file_was_present; }

	static void SetWaitFileWasPresent() { m_wait_file_was_present = TRUE; }

	/**
	 * DSK-345746 introduces a requirement that Opera cannot start while
	 * OperaUpgrader is running in order to not to mess with the profile
	 * while it is being upgraded.
	 *
	 * This is realised in such a way that we look for the OperaUpgrader.exe
	 * process when starting and refuse to start if the process is there.
	 * However this poses another problem, since after the upgrade we need
	 * to launch the Opera.exe and that is done from within the OperaUpgrader
	 * process. That means that we need a system-wide indication of the
	 * moment when the upgrade is complete and we allow to start Opera.exe.
	 * In order to signal that we use a system-wide mutex that is created
	 * right after the update of the profile is finished and destroyed
	 * after the OperaUpgrader.exe is finished.
	 *
	 * When a starting Opera.exe sees the OperaUpgrader process within the
	 * system's processes, it checks for existence of the global mutex to
	 * see if it is allowed to start.
	 */

private:
	/**
	 * Initalises the AUUpdater object, MUST be called before
	 * any other function calls
	 *
	 * @return AUU_Status. 
	 */
	AUUpdater::AUU_Status Init();
	
	/**
	 * Checks if there is a package update waiting to be applied
	 * This function will also read and parse the text file and 
	 * display a dialog box asking if you would like to apply the 
	 * upgrade if one is found
	 *
	 * @return BOOL TRUE if an upgrade is found and selected to be applied 
	 */
	BOOL IsUpgradeReady();

	/**
	 * Installs the upgrade
	 *
	 * @return BOOL TRUE if was was successfully installed
	 */
	BOOL Install();

	/**
	 * Launches the upgraded Opera installation
	 *
	 * @return BOOL TRUE if was was successfully launched
	 */
	BOOL LaunchOpera();

	/**
	 * Launches the upgrade executable
	 *
	 * @return BOOL TRUE if was was successfully launched
	 */
	BOOL LaunchUpgrade();

private:
	// Helper functions to make the code cleaner when allocating memory
	AUUpdater::AUU_Status CreateUpgradeFileString(uni_char *exe_path, uni_char *update_name, uni_char *file_name, uni_char **file_path);
	AUUpdater::AUU_Status CreateOperaFileString(uni_char *update_path, uni_char *file_name, uni_char **file_path);


private:
	Mode				m_mode;
	BOOL				m_leave_text_file;	// Set if you are going to install on next run
	static BOOL			m_wait_file_was_present; // Set to TRUE if wait file was present during startup
	AUFileUtils			*m_au_fileutils;
	AUInstaller			*m_au_installer;
	AUDataFileReader	*m_au_datafile;
	
	uni_char			*m_text_file_path;	// Path to the autoupdate.txt file
	uni_char			*m_wait_file_path;	// Path to the wait file
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _AUUPDATER_H_INCLUDED_
