/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/updater/auupdater.h"
#include "adjunct/autoupdate/updater/pi/aufileutils.h" 
#include "adjunct/autoupdate/updater/pi/auinstaller.h"
#include "adjunct/autoupdate/updater/austringutils.h"
#include "adjunct/autoupdate/updater/audatafile_reader.h"
#include "adjunct/desktop_util/version/operaversion.h"

#include "adjunct/quick/managers/LaunchManager.h" 


BOOL AUUpdater::m_wait_file_was_present = FALSE;

//////////////////////////////////////////////////////////////////////////////////////////////////

AUUpdater::AUUpdater() :
	m_mode(Checker),
	m_leave_text_file(FALSE),
	m_au_fileutils(NULL),
	m_au_installer(NULL),
	m_au_datafile(NULL),
	m_text_file_path(NULL),
	m_wait_file_path(NULL)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
 
AUUpdater::~AUUpdater() 
{
	// If waiting don't kill the autoupdate text file
	if (!m_leave_text_file)
		m_au_fileutils->Delete(m_text_file_path);

	delete m_au_fileutils;
	delete m_au_installer;
	delete m_au_datafile;
	delete [] m_text_file_path;
	delete [] m_wait_file_path;
}

BOOL AUUpdater::Run()
{
	
	// Initalise
	if (Init() == AUUpdater::OK)
	{
		// If this is running as the updater then try to update!
		if (m_mode == Installer)
		{
//printf("I'm an Installer!\n");
			// Install the update, no need to test the return we are restarting to Opera regardless
			Install();

//printf("trying to launch Opera now!\n");

			// We need to delete the autoupdate.txt file NOW or else we will get problems like
			// DSK-346331, when launching a legitimate Opera installation spawns the binary
			// from the temp-autoupdate-install folder since the autoupdate.txt file is still
			// there while the legitimate binary is being lanuched.
			if (!m_leave_text_file)
				m_au_fileutils->Delete(m_text_file_path);

			// Create the mutex that will allow starting an Opera.exe despite
			// that OperaUpgrader.exe is still running, we need this so that
			// the upgrader can run the newly updated Opera process.
			m_au_installer->CreateUpgraderMutex();

			// Restart the updated Opera
			if (LaunchOpera())
			{
				/**
				 * DSK-347506: An attempt to fix the problem on Windows XP, that is suspected to be caused
				 * by the mutex being destroyed before the OperaUpgrader process exits and thus preventing
				 * the Opera process to start.
				 */
#ifdef MSWIN
				m_au_installer->Sleep(3000);
#endif // MSWIN
				return FALSE;
			}
		}
		else
		{
//printf("Check upgrade\n");
			// See if there is an update ready to be applied
			if (IsUpgradeReady())
			{
//printf("Launch Upgrade!\n");
				// If there is we need to restart it!
				if (LaunchUpgrade())
				{
					return FALSE;
				}
				else if(m_au_fileutils)
				{
					m_au_fileutils->Delete(m_text_file_path);
				}
			}
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

AUUpdater::AUU_Status AUUpdater::Init()
{
	// Create the file util object
	m_au_fileutils = AUFileUtils::Create();
	if (!m_au_fileutils)
		return ERR_MEM;

	// Create the installer object
	m_au_installer = AUInstaller::Create();
	if (!m_au_installer)
		return ERR_MEM;

	// Create the data file reader object
	m_au_datafile = new AUDataFileReader;
	if (!m_au_datafile)
		return ERR_MEM;

	// Set the mode
	BOOL is_updater;
	if (m_au_fileutils->IsUpdater(is_updater) == AUFileUtils::OK && is_updater)
		m_mode = Installer;
	else
		m_leave_text_file = TRUE;		// Always leave the autoupdate.txt when you're Opera
		
	// Make all the filenames now to save doing this multiple times and easy cleanup
	if (m_mode == Installer)
	{
		uni_char* exe_path = NULL, *update_name = NULL;

		if(m_au_fileutils->GetExePath(&exe_path) != AUFileUtils::OK)
		{
			delete [] exe_path;
			return AUUpdater::ERR_MEM;
		}		

		if(m_au_fileutils->GetUpdateName(&update_name) != AUFileUtils::OK)
		{
			delete [] exe_path;
			delete [] update_name;
			return AUUpdater::ERR_MEM;
		}		

		BOOL success = FALSE;

		// Create a buffer large enough for the autoupdate.txt
		if (CreateUpgradeFileString(exe_path, update_name, AUTOUPDATE_UPDATE_TEXT_FILENAME, &m_text_file_path) == AUUpdater::OK)
		{
			if (CreateUpgradeFileString(exe_path, update_name, AUTOUPDATE_UPDATE_WAITFILE, &m_wait_file_path) == AUUpdater::OK)
			{
				success = TRUE;
			}
		}

		// Clean up
		delete [] exe_path;
		delete [] update_name;

		// Return a memory error if this has failed
		if (!success)
			return AUUpdater::ERR_MEM;
	}
	else
	{
		uni_char *update_path = NULL;

		// Get the upgrade folder
		if (m_au_fileutils->GetUpgradeFolder(&update_path) != AUFileUtils::OK)
		{
			delete [] update_path;
			return ERR_MEM;
		}

		BOOL success = FALSE;

		// Create a buffer large enough for the autoupdate.txt
		if (CreateOperaFileString(update_path, AUTOUPDATE_UPDATE_TEXT_FILENAME, &m_text_file_path) == AUUpdater::OK)
		{
			if (CreateOperaFileString(update_path, AUTOUPDATE_UPDATE_WAITFILE, &m_wait_file_path) == AUUpdater::OK)
			{
				success = TRUE;
			}
		}

		// Clean up
		delete [] update_path;

		// Return a memory error if this has failed
		if (!success)
			return AUUpdater::ERR_MEM;
	}
	
	return AUUpdater::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL AUUpdater::IsUpgradeReady()
{
    if (!m_au_fileutils)
        return FALSE;

	unsigned int size = 0;

	// If there is a wait file then we never apply an update THIS TIME, 
	// but we remove the file so next time it will run
	if (m_au_fileutils->Exists(m_wait_file_path, size) == AUFileUtils::OK)
	{
		m_wait_file_was_present = TRUE;
		m_au_fileutils->Delete(m_wait_file_path);
		return FALSE;
	}

	// Check if the upgrade text file is in the upgrade folder
	if (m_au_fileutils->Exists(m_text_file_path, size) == AUFileUtils::OK && size > 0)
	{
		// Check if update is not out-of-date
		if(OpStatus::IsError(m_au_datafile->Init()))
			return FALSE;
		if (OpStatus::IsError(m_au_datafile->LoadFromOpera()))
			return FALSE;

		// Compare version of the current Opera installation and the one that came from autoupdate.
		// In order to be able to test autoupdate, it's possible to fake the version of the current
		// installation using the autoupdate.ini file located in the Opera binary directory.
		// OperaVersion constructor will set the version to the current one automatically.
		OperaVersion current_opera_version;
		// Allow this object to read the faked version from the autoupdate.ini file.
		current_opera_version.AllowAutoupdateIniOverride();

		// Read the version that came with the autoupdate.
		OperaVersion update_opera_version;
		uni_char* update_version = m_au_datafile->GetVersion();
		uni_char* update_buildnumber = m_au_datafile->GetBuildNum();
		OP_STATUS set_result = update_opera_version.Set(update_version, update_buildnumber);
		OP_DELETEA(update_version);
		OP_DELETEA(update_buildnumber);

		if(OpStatus::IsSuccess(set_result) && update_opera_version > current_opera_version)
		{
			return TRUE;
		}
		return FALSE;
	}
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL AUUpdater::Install()
{
//printf("trying to install now!\n");
	// Read the file in
	if(OpStatus::IsError(m_au_datafile->Init()))
		return FALSE;
	
	if (OpStatus::IsError(m_au_datafile->Load()))
		return FALSE;

	uni_char* install_path = m_au_datafile->GetInstallPath();

	// Wait a bit if Opera is still running
	for(int i=0; i<5 && m_au_installer->IsOperaRunning(install_path); i++)
	{
		m_au_installer->Sleep(1000);
	}

	// Check if this user has admin prvilidges, if not we need to show a dialog so they can opt not to install
	if (!m_au_installer->HasInstallPrivileges(install_path))
	{
		uni_char* caption = m_au_datafile->GetDialogCaption();
		uni_char* message = m_au_datafile->GetDialogMessage();
		uni_char* ok = m_au_datafile->GetDialogOk();
		uni_char* cancel = m_au_datafile->GetDialogCancel();

		if(!caption || !message || !ok || !cancel)
		{
		    delete [] caption;
		    delete [] message;
		    delete [] ok;
		    delete [] cancel;
			delete [] install_path;
		    install_path = NULL;
			
			
			return FALSE;
		}
			

		// Show the dialog asking if you want to upgrade
		BOOL dialog_ret = m_au_installer->ShowStartDialog(caption, message, ok, cancel);

		delete [] caption;
		delete [] message;
		delete [] ok;
		delete [] cancel;

		if (!dialog_ret)
		{
			// Create the wait file so that the update will not launch on next run
			m_au_fileutils->CreateEmptyFile(m_wait_file_path);
			
			// Set the wait flag so the autoupdate.txt isn't deleted
			m_leave_text_file = TRUE;
			
			delete [] install_path;
			install_path = NULL;
			
			return FALSE;
		}
	}
	
	if (m_au_datafile->GetType() == AUDataFile::Full)
	{
		uni_char* update_file = m_au_datafile->GetUpdateFile();

		AUInstaller::AUI_Status status = m_au_installer->Install(install_path, update_file);
		
		delete [] install_path;
		install_path = NULL;
		delete [] update_file;
		update_file = NULL;	
		
		// If the installer returns no access we'll try again next time
		if (status == AUInstaller::NO_ACCESS)
		{
			// Create the wait file so that the update will not launch on next run
			m_au_fileutils->CreateEmptyFile(m_wait_file_path);
			
			// Set the wait flag so the autoupdate.txt isn't deleted
			m_leave_text_file = TRUE;

			return TRUE;
		}

		if(status == AUInstaller::ERR) // update did not succeeded...
		{
			unsigned int size = 0;
			if(m_au_fileutils->Exists(m_wait_file_path, size) == AUFileUtils::OK) // ... because user did not agree to install it
			{
				m_leave_text_file = TRUE;
			}
		}

		return (status == AUInstaller::OK);
	}
	
	delete [] install_path;
	install_path = NULL;
	
	// Add code for patching here later

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL AUUpdater::LaunchOpera()
{
	uni_char* install_path = m_au_datafile->GetInstallPath();
	if (!install_path)
		return FALSE;

	// Launch with the original arguments
	BOOL ret = g_launch_manager->Launch(install_path, g_launch_manager->GetArgc(), g_launch_manager->GetArgv());

	delete [] install_path;

	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL AUUpdater::LaunchUpgrade()
{
	uni_char		*update_path = NULL;
	
	// Get the name of the update
	if(m_au_fileutils->GetUpdatePath(&update_path) != AUFileUtils::OK)
	{
		delete [] update_path;
		return FALSE;
	}

	// Launch with the original arguments
	BOOL ret = g_launch_manager->Launch(update_path, g_launch_manager->GetArgc(), g_launch_manager->GetArgv());

	delete [] update_path;

	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

AUUpdater::AUU_Status AUUpdater::CreateUpgradeFileString(uni_char *exe_path, uni_char *update_name, uni_char *file_name, uni_char **file_path)
{
	// Create a buffer large enough for the autoupdate.txt
	int len = u_strlen(exe_path);
	len += u_strlen(file_name);
	*file_path = new uni_char[len + 1];
	if (!*file_path)
		return AUUpdater::ERR_MEM;

	// Set the base path then
	// Copy the name of the text file in over the top of the name of the exe
	u_strcpy(*file_path, exe_path);
	u_strcpy(*file_path + u_strlen(exe_path) - u_strlen(update_name), file_name);
	
	return AUUpdater::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

AUUpdater::AUU_Status AUUpdater::CreateOperaFileString(uni_char *update_path, uni_char *file_name, uni_char **file_path)
{
	// Get the lengths and allocate the memory
	int upgrade_len  = u_strlen(update_path);
	int filename_len = u_strlen(file_name);
	*file_path = new uni_char[upgrade_len + filename_len + 1];
	if (!*file_path)
		return AUUpdater::ERR_MEM;

	// Append the upgrade text filename
	u_strcpy(*file_path, update_path);
	u_strcpy(*file_path + upgrade_len, file_name);

	return AUUpdater::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

