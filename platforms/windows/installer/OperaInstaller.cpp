// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

#include "platforms/windows/Installer/OperaInstaller.h"

#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/desktop_util/resources/resourcedefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/shortcuts/desktopshortcutinfo.h"
#include "adjunct/desktop_util/string/string_convert.h"
#include "adjunct/desktop_util/transactions/CreateFileOperation.h"
#include "adjunct/desktop_util/transactions/Optransaction.h"
#include "adjunct/quick/quick-version.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefsfile/prefsfile.h"

#include "platforms/windows/WindowsLaunch.h"
#include "platforms/windows/pi/WindowsOpDesktopResources.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/WindowsShortcut.h"
#include "platforms/windows/installer/ChangeRegValueOperation.h"
#include "platforms/windows/installer/DeleteRegKeyOperation.h"
#include "platforms/windows/installer/DeleteFileOperation.h"
#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/InstallerWizard.h"
#include "platforms/windows/installer/MoveFileOperation.h"
#include "platforms/windows/installer/OperaInstallerUI.h"
#include "platforms/windows/installer/OperaInstallLog.h"
#include "platforms/windows/installer/ProcessModel.h"
#include "platforms/windows/installer/RegistryKey.h"
#include "platforms/windows/installer/ReplaceFileOperation.h"
#include "platforms/windows/utils/authorization.h"
#include "platforms/windows/windows_ui/res/#buildnr.rci"
#include "platforms/windows/windows_ui/registry.h"
#include "platforms/windows/windows_ui/OpShObjIdl.h"

#include <AccCtrl.h>
#include <AclAPI.h>
#include <LMCons.h>

extern OpString g_spawnPath;

//Files to be deleted after we return control to Opera.exe (when uninstalling)
OpVector<OpString>* g_uninstall_files = NULL;
OpVector<OpFile>* OperaInstaller::s_delete_profile_files = NULL;

const OperaInstaller::RegistryOperation OperaInstaller::m_installer_regops [] =
{
#include "platforms/windows/Installer/file-handlers-registry.inc"	
#include "platforms/windows/Installer/installer-registry.inc"	
};

const OperaInstaller::RegistryOperation OperaInstaller::m_file_handlers_regops [] =
{
#include "platforms/windows/Installer/file-handlers-registry.inc"	
};

//Declares the registry operations for all file handlers
#include "platforms/windows/Installer/file-assoc-registry.inc"	

const OperaInstaller::RegistryOperation* OperaInstaller::m_association_regops[LAST_ASSOC] =
{
	OEX_assoc,
	HTM_assoc,
	HTML_assoc,
	XHT_assoc,
	XHTM_assoc,
	XHTML_assoc,
	MHT_assoc,
	MHTML_assoc,
	XML_assoc,
	TORRENT_assoc,
	BMP_assoc,
	JPG_assoc,
	JPEG_assoc,
	PNG_assoc,
	SVG_assoc,
	GIF_assoc,
	XBM_assoc,
	OGA_assoc,
	OGV_assoc,
	OGM_assoc,
	OGG_assoc,
	WEBM_assoc,


	HTTP_assoc,
	HTTPS_assoc,
	FTP_assoc,
	MAILTO_assoc,
	NNTP_assoc,
	NEWS_assoc,
	SNEWS_assoc,
};

const uni_char* OperaInstaller::m_check_assoc_keys[LAST_ASSOC] =
{
	UNI_L(".oex"),
	UNI_L(".htm"),
	UNI_L(".html"),
	UNI_L(".xht"),
	UNI_L(".xhtm"),
	UNI_L(".xhtml"),
	UNI_L(".mht"),
	UNI_L(".mhtml"),
	UNI_L(".xml"),
	UNI_L(".torrent"),
	UNI_L(".bmp"),
	UNI_L(".jpg"),
	UNI_L(".jpeg"),
	UNI_L(".png"),
	UNI_L(".svg"),
	UNI_L(".gif"),
	UNI_L(".xbm"),
	UNI_L(".oga"),
	UNI_L(".ogv"),
	UNI_L(".ogm"),
	UNI_L(".ogg"),
	UNI_L(".webm"),

	UNI_L("http"),
	UNI_L("https"),
	UNI_L("ftp"),
	UNI_L("mailto"),
	UNI_L("nntp"),
	UNI_L("news"),
	UNI_L("snews"),
};

const OperaInstaller::RegistryOperation OperaInstaller::m_default_browser_start_menu_regop =
{UNI_L("Software\\Clients\\StartMenuInternet"), NULL, REG_SZ, UNI_L("Opera{Product}"), OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0};

const OperaInstaller::RegistryOperation OperaInstaller::m_default_mailer_start_menu_regop =
{UNI_L("Software\\Clients\\Mail"), NULL, REG_SZ, UNI_L("Opera{Product}"), OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 0};

//different last install path entries for 32/64 bit
#ifndef _WIN64
const OperaInstaller::RegistryOperation OperaInstaller::m_set_last_install_path_regop =
{UNI_L("Software\\Opera Software"), UNI_L("Last{SPProduct} Install Path"), REG_SZ, UNI_L("{Resources}"), OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1};
#else
const OperaInstaller::RegistryOperation OperaInstaller::m_set_last_install_path_regop =
{UNI_L("Software\\Opera Software"), UNI_L("Last{SPProduct} Install Path x64"), REG_SZ, UNI_L("{Resources}"), OLDER_THAN_WIN2K, NULL, PRODUCT_OPERA | PRODUCT_OPERA_NEXT, 1};
#endif

const OperaInstaller::RegistryOperation OperaInstaller::m_surveyed_regop =
{UNI_L("Software\\Opera Software"), UNI_L("Surveyed"), REG_SZ, UNI_L("Yes"), OLDER_THAN_WIN2K, HKEY_CURRENT_USER, 0};

/** Inspired from core's RETURN_IF_ERROR. This macro does essentially the same
  * but will call the ShowError method of the m_opera_installer_ui if an error occue
  * in order to display a message to the user then return
  *
  * @param error_code		An installer error code as defined by OperaInstaller::ErrorCode
  * @param item_affected	A string that provides more information about the error (can be empty)
  * @param operation		The instruction to execute.
  *
  */

#define INST_HANDLE_ERROR(error_code, item_affected, operation) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = operation; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) { \
			m_opera_installer_ui->ShowError(error_code, item_affected); \
			return RETURN_IF_ERROR_TMP; \
	}} while (0)

/** Similar to INST_HANDLE_ERROR, but accepts recovering from errors
  *
  * @param error_code		An installer error code as defined by OperaInstaller::ErrorCode
  * @param item_affected	A string that provides more information about the error (can be empty)
  * @param error_out		The name of a variable to receive the actual error code, if the user decided to ignore it.
  * @param operation		The instruction to execute.
  */
#define INST_HANDLE_NONFATAL_ERROR(error_code, item_affected, error_out, operation) \
	do { \
		error_out = operation; \
		if (OpStatus::IsError(error_out)) { \
			if (m_opera_installer_ui->ShowError(error_code, item_affected, NULL, FALSE) == FALSE) \
				return error_out; \
			else \
				m_installer_transaction->ResetState(); \
	}} while (0)

/** RETRY_IF_ERROR(error, item, full_path, new_operation)
  *
  * Will perform an operation as part of the m_installer_transaction.
  * If it fails, CheckIfFileLocked will be called to see if another process is locking the file
  * and give the opportunity to the user to close that process. If the user chooses to cancel
  * the installation, the error that happened when attempting the operation is returned.
  * If the user doesn't choose to cancel, the operation is tried a second time.
  * For this second try, the behavior is essentially the same as for INST_HANDLE_ERROR.
  *
  * @param error_code		An installer error code as defined by OperaInstaller::ErrorCode.
  * @param item_affected	A string that provides more information about the error (can be empty)
  *							Usually, this is the path relative to the installation directory
  * @param full_path		Absolute path to the file that is the target of operation
  * @param operation		An OpAutoPtr<OpUndoableOperation> object. This operation will be
  *							done as part of m_installer_transaction
  */
#define RETRY_IF_ERROR(error_code, item_affected, full_path, operation) \
	do { \
		OP_STATUS RETRY_IF_ERROR_TMP = m_installer_transaction->Continue(operation); \
		if (OpStatus::IsError(RETRY_IF_ERROR_TMP)) \
		{ \
			if (OpStatus::IsSuccess(CheckIfFileLocked(full_path)) && !m_was_canceled) \
			{ \
					m_installer_transaction->ResetState(); \
					RETRY_IF_ERROR_TMP = m_installer_transaction->Continue(operation); \
			} \
			if (OpStatus::IsError(RETRY_IF_ERROR_TMP) || m_was_canceled) \
			{ \
				if (!m_was_canceled) \
					m_opera_installer_ui->ShowError(error_code, item_affected); \
				return RETRY_IF_ERROR_TMP; \
			} \
		} \
	} while (0)

OperaInstaller::OperaInstaller()
	: m_operation(OpNone)
	, m_install_log(NULL)
	, m_old_install_log(NULL)
	, m_error_log(NULL)
	, m_step(0)
	, m_step_index(0)
	, m_installer_transaction(NULL)
	, m_get_write_permission_event(NULL)
	, m_is_running(FALSE)
	, m_was_canceled(FALSE)
	, m_delete_profile(KEEP_PROFILE)
	, m_run_yandex_script(FALSE)
	, m_country_checker(NULL)
	, m_update_country_pref(false)
	, m_country_check_pause(false)
{
	m_install_language.Empty();
	m_existing_multiuser_setting = -1;
	m_existing_product_setting = -1;
	m_install_folder.Empty();
	m_opera_installer_ui = OP_NEW(OperaInstallerUI, (this));

	//Initializing of the different values related to the product type.
	switch (g_desktop_product->GetProductType())
	{
		case PRODUCT_TYPE_OPERA:
			m_product_flag = PRODUCT_OPERA;
			m_product_name = NULL;
			m_long_product_name.Empty();
			m_product_icon_nr = 0;
			break;

		case PRODUCT_TYPE_OPERA_NEXT:
			m_product_flag = PRODUCT_OPERA_NEXT;
			m_product_name = UNI_L("Next");
			m_long_product_name.Set("Next");
			m_product_icon_nr = 1;
			break;

		case PRODUCT_TYPE_OPERA_LABS:
			m_product_flag = PRODUCT_OPERA_LABS;
			m_product_name = UNI_L("Labs");
			m_long_product_name.Set(g_desktop_product->GetLabsProductName());
			m_long_product_name.Insert(0, "Labs ");
			m_product_icon_nr = 2;
			break;
	}

	m_app_reg_name.Set("Opera");
	if (m_product_name.HasContent())
		m_app_reg_name.AppendFormat(" %s", m_product_name);
	m_app_reg_name.Append(" Internet Browser");

	CommandLineManager* cl_manager = CommandLineManager::GetInstance();
	if (cl_manager && cl_manager->GetArgument(CommandLineManager::OWIInstall))
	{
		//We need to initialize setings from command line first because
		//they are used to determine the default installation folder
		//(in case the installation folder isn't specified on the command line)
		SetSettingsFromCommandLine();
		SetInstallFolderFromCommandLine();
		SetInstallLanguageFromCommandLine();
	}
	else
	{
		//Let's initialize the installer from the information found in the
		//current folder.
		//We are either uninstalling, using the installer object for handling file
		//association or trying to determine whether installation will require elevation.
		//The last case is used to decide whether we should show the UAC shield icon in the
		//autoupdate dialogs, i.e. the AutoUpdateReadyController and AutoUpdateDialog.
		OpString install_folder;
		install_folder.Set(g_spawnPath);
		int pos = install_folder.FindLastOf('\\');
		if (pos != KNotFound)
		{
			install_folder.Delete(pos);
			CheckAndSetInstallFolder(install_folder);
		}
	}

	//This condition will be true if installer is running in silent mode and this is either
	//autoupdate or elevated installer started from interactive installer.
	if (cl_manager->GetArgument(CommandLineManager::OWIUpdateCountryCode) ||
		(m_operation == OpUpdate && cl_manager->GetArgument(CommandLineManager::OWIAutoupdate)))
	{
		m_update_country_pref = true;
	}
	//Country code to write into default prefs file. If not provided via command line and
	//m_update_country_pref is true installer will have to contact autoupdate server to get it.
	if (cl_manager->GetArgument(CommandLineManager::OWICountryCode))
	{
		OpStatus::Ignore(m_country.Set(cl_manager->GetArgument(CommandLineManager::OWICountryCode)->m_string_value));
	}
}

OperaInstaller::~OperaInstaller()
{
	OP_DELETE(m_installer_transaction);
	OP_DELETE(m_install_log);
	OP_DELETE(m_old_install_log);
	OP_DELETE(m_opera_installer_ui);
	OP_DELETE(m_country_checker);

	if (m_get_write_permission_event != NULL)
	{
		PulseEvent(m_get_write_permission_event);
		CloseHandle(m_get_write_permission_event);
		m_get_write_permission_event = NULL;
	}

	if (m_error_log)
	{
		m_error_log->Close();
		if (m_error_log->GetFileLength() == 0)
			m_error_log->Delete();
		OP_DELETE(m_error_log);
	}
}

BOOL OperaInstaller::SetSettings(Settings settings, BOOL preserve_multiuser)
{
	OP_ASSERT(!m_is_running);	//We should not be calling this once Run has been called
	if (m_is_running)
		return FALSE;

	CommandLineManager* cl_manager = CommandLineManager::GetInstance();
	
	if (!cl_manager)
		return FALSE;

	m_settings.launch_opera = settings.launch_opera;
	m_settings.copy_only = settings.copy_only;
	//Usually, we want to keep the single profile setting, regardless of the settiings passed, if the siingle profile setting was
	//specified in operaprefs_default.in of the installation folder or specified at the command line. This is because the UI doesn't
	//allow changing this setting explicitly
	if ((m_existing_multiuser_setting == -1 && !cl_manager->GetArgument(CommandLineManager::OWISingleProfile)) || !preserve_multiuser)
		m_settings.single_profile = settings.single_profile;

	//When using copy only, some other settings cannot be used.
	//Warn if any of the settings passed were not taken into account because of this by returning FALSE.
	if (m_settings.copy_only)
	{
		m_settings.all_users = FALSE;
		m_settings.set_default_browser = FALSE;
		m_settings.desktop_shortcut = FALSE;
		m_settings.start_menu_shortcut = FALSE;
		m_settings.quick_launch_shortcut = FALSE;
		m_settings.pinned_shortcut = FALSE;

		return (!settings.all_users && !settings.set_default_browser && !settings.start_menu_shortcut && !settings.desktop_shortcut && !settings.quick_launch_shortcut && !settings.pinned_shortcut);
	}
	else
	{
		m_settings.all_users = settings.all_users;
		m_settings.set_default_browser = settings.set_default_browser;
		m_settings.desktop_shortcut = settings.desktop_shortcut;
		m_settings.start_menu_shortcut = settings.start_menu_shortcut;
		m_settings.pinned_shortcut = IsSystemWin7() ? settings.pinned_shortcut : FALSE;
		m_settings.quick_launch_shortcut = settings.quick_launch_shortcut;

		m_settings.update_start_menu_shortcut = settings.update_start_menu_shortcut;
		m_settings.update_desktop_shortcut = settings.update_desktop_shortcut;
		m_settings.update_quick_launch_shortcut = settings.update_quick_launch_shortcut;
		m_settings.update_pinned_shortcut = settings.update_pinned_shortcut;
		
		if (!IsSystemWin7() && settings.pinned_shortcut)
			return FALSE;
	}

	return TRUE;
}

void OperaInstaller::SetSettingsFromCommandLine()
{
	CommandLineManager* cl_manager = CommandLineManager::GetInstance();
	
	if (!cl_manager)
		return;
		
	//First, we initialize everything to default values
	m_settings.all_users = WindowsUtils::IsPrivilegedUser(TRUE);
	m_settings.copy_only = FALSE;
	m_settings.launch_opera = TRUE;

#ifndef VER_BETA
	m_settings.set_default_browser = g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA;
#else
	m_settings.set_default_browser = FALSE;
#endif
	m_settings.single_profile = FALSE;

	m_settings.desktop_shortcut = TRUE;
	m_settings.start_menu_shortcut = TRUE;
	m_settings.quick_launch_shortcut = !IsSystemWin7();
	m_settings.pinned_shortcut = IsSystemWin7();
	m_settings.update_desktop_shortcut = FALSE;
	m_settings.update_start_menu_shortcut= FALSE;
	m_settings.update_quick_launch_shortcut = FALSE;
	m_settings.update_pinned_shortcut = FALSE;

	//We override the default values depending on what was set on the command line
	if (cl_manager->GetArgument(CommandLineManager::OWILaunchOpera))
		m_settings.launch_opera = cl_manager->GetArgument(CommandLineManager::OWILaunchOpera)->m_int_value != 0;

	if (cl_manager->GetArgument(CommandLineManager::OWICopyOnly))
		m_settings.copy_only = cl_manager->GetArgument(CommandLineManager::OWICopyOnly)->m_int_value != 0;

	//When using copy only, some other settings cannot be used.
	if (m_settings.copy_only)
	{
		m_settings.all_users = FALSE;
		m_settings.set_default_browser = FALSE;
		m_settings.desktop_shortcut = FALSE;
		m_settings.start_menu_shortcut = FALSE;
		m_settings.quick_launch_shortcut = FALSE;
		m_settings.pinned_shortcut = FALSE;
		//copy_only changes the default value for single_profile.
		m_settings.single_profile = TRUE;
	}
	else
	{
		if (cl_manager->GetArgument(CommandLineManager::OWIAllUsers))
			m_settings.all_users = cl_manager->GetArgument(CommandLineManager::OWIAllUsers)->m_int_value != 0;
		if (cl_manager->GetArgument(CommandLineManager::OWISetDefaultBrowser))
			m_settings.set_default_browser = cl_manager->GetArgument(CommandLineManager::OWISetDefaultBrowser)->m_int_value != 0;

		//When we are doing a non-interactive update, the shortcuts should just be kept as they are unless the corresponding option was
		//Specifically passed on the command line. In that case, the update setting for that shortcut is set to TRUE
		if (cl_manager->GetArgument(CommandLineManager::OWIDesktopShortcut))
		{
			m_settings.desktop_shortcut = cl_manager->GetArgument(CommandLineManager::OWIDesktopShortcut)->m_int_value != 0;
			m_settings.update_desktop_shortcut = TRUE;
		}

		if (cl_manager->GetArgument(CommandLineManager::OWIStartMenuShortcut))
		{
			m_settings.start_menu_shortcut = cl_manager->GetArgument(CommandLineManager::OWIStartMenuShortcut)->m_int_value != 0;
			m_settings.update_start_menu_shortcut = TRUE;
		}
		if (cl_manager->GetArgument(CommandLineManager::OWIQuickLaunchShortcut))
		{
			m_settings.quick_launch_shortcut = cl_manager->GetArgument(CommandLineManager::OWIQuickLaunchShortcut)->m_int_value != 0;
			m_settings.update_quick_launch_shortcut = TRUE;
		}
		if (IsSystemWin7() && cl_manager->GetArgument(CommandLineManager::OWIPinToTaskbar))
		{
			m_settings.pinned_shortcut = cl_manager->GetArgument(CommandLineManager::OWIPinToTaskbar)->m_int_value != 0;
			m_settings.update_pinned_shortcut = TRUE;
		}
	}

	if (cl_manager->GetArgument(CommandLineManager::OWISingleProfile))
		m_settings.single_profile = cl_manager->GetArgument(CommandLineManager::OWISingleProfile)->m_int_value != 0;

}

void OperaInstaller::SetInstallFolderFromCommandLine()
{
	OpString install_folder;
	install_folder.Empty();

	CommandLineManager* cl_manager = CommandLineManager::GetInstance();
	
	if (cl_manager && cl_manager->GetArgument(CommandLineManager::OWIInstallFolder))
	{
		//The installation folder is specified on the command line
		RETURN_VOID_IF_ERROR(install_folder.Set(cl_manager->GetArgument(CommandLineManager::OWIInstallFolder)->m_string_value));
	}
	else
	{
		//Try to obtain the previous installation folder from the registry
		//different last install path entries for Beta/Final and for 32/64 bit
#ifndef _WIN64
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA)
		{
			OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Install Path"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Beta Install Path"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Directory3"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Directory3"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Beta Directory"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Beta Directory"));
		}
		else if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		{
			OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Next Install Path"));
		}
#else
		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA)
		{
			OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Install Path x64"));

			if (install_folder.IsEmpty())
				OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Beta Install Path x64"));
		}
		else if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_NEXT)
		{
			OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), install_folder, UNI_L("Last Next Install Path x64"));
		}
#endif

		if (!install_folder.IsEmpty())
		{
			OpFile install_folder_file;
			install_folder_file.Construct(install_folder);
			BOOL exists;
			if (OpStatus::IsError(install_folder_file.Exists(exists)) || ! exists || install_folder_file.GetMode() != OpFileInfo::DIRECTORY)
				install_folder.Empty();
		}

		if (install_folder.IsEmpty())
		{
			//Determine and use the default installation folder

			if (m_settings.all_users)
			{
				WindowsOpDesktopResources::GetFolderPath(FOLDERID_ProgramFiles, CSIDL_PROGRAM_FILES, install_folder);
			}
			else
			{
				if(IsSystemWin7())
					WindowsOpDesktopResources::GetFolderPath(FOLDERID_UserProgramFiles, CSIDL_LOCAL_APPDATA, install_folder);
				else
				{
					WindowsOpDesktopResources::GetFolderPath(FOLDERID_LocalAppData, CSIDL_LOCAL_APPDATA, install_folder);
					RETURN_VOID_IF_ERROR(install_folder.Append(PATHSEP "Programs"));
				}
			}

			RETURN_VOID_IF_ERROR(install_folder.Append(PATHSEP "Opera"));

			//The installation path should include the full product name
			if (m_long_product_name.HasContent())
				RETURN_VOID_IF_ERROR(install_folder.AppendFormat(" %s", m_long_product_name.CStr()));
#ifdef _WIN64
			RETURN_VOID_IF_ERROR(install_folder.Append(UNI_L(" x64")));
#endif
		}
	}

	//We can safely ignore this. Calls to Run will fail if the install folder wasn't valid.
	OpStatus::Ignore(CheckAndSetInstallFolder(install_folder));
}

void OperaInstaller::SetInstallLanguageFromCommandLine()
{
	CommandLineManager* cl_manager = CommandLineManager::GetInstance();
	if (cl_manager && cl_manager->GetArgument(CommandLineManager::OWILanguage))
		OpStatus::Ignore(m_install_language.Set(cl_manager->GetArgument(CommandLineManager::OWILanguage)->m_string_value));
}

OP_STATUS OperaInstaller::SetInstallFolder(const OpStringC& install_folder)
{
	OP_ASSERT(!m_is_running);	//We should not be calling this once Run has been called
	if (m_is_running)
		return OpStatus::ERR;

	return CheckAndSetInstallFolder(install_folder);

}

OP_STATUS OperaInstaller::CheckAndSetInstallFolder(const OpStringC& new_install_folder)
{
	//First, reset the state, so if this method fails, Run will fail as well.
	m_install_folder.Empty();

	Operation old_operation = m_operation;
	m_operation = OpNone;

	if (new_install_folder.IsEmpty())
		return OpStatus::ERR;

 	OpString install_folder;
	RETURN_IF_ERROR(install_folder.Set(new_install_folder));
	StrTrimRight(install_folder.CStr(), UNI_L(" "));

	//remove trailing path separator
	if (install_folder[install_folder.Length() -1] == PATHSEPCHAR)
	{
		install_folder.Delete(install_folder.Length() -1);
	}

	//Check if a log file exists in the installation folder
	OpString install_log;
	RETURN_IF_ERROR(install_log.Set(install_folder));
	RETURN_IF_ERROR(install_log.Append(UNI_L(PATHSEP) UNI_L("opera_install_log.xml")));

	OP_DELETE(m_install_log);
	OP_DELETE(m_old_install_log);

	m_old_install_log = NULL;
	RETURN_OOM_IF_NULL(m_install_log = OP_NEW(OperaInstallLog, ()));
	RETURN_IF_ERROR(m_install_log->Construct(install_log));

	BOOL exists = OpStatus::IsSuccess(m_install_log->Exists(exists)) && exists;

	//If changing the folder to a different existing installation or
	//switching form upgrading to installing, reset all settings.
	if (exists || old_operation == OpUpdate)
		SetSettingsFromCommandLine();

	if (exists)
	{
		//If a log file was found, use it to override some installer settings so
		//that they reflect the state of that installation
		m_old_install_log = m_install_log;
		m_install_log = NULL;

		RETURN_IF_ERROR(m_old_install_log->Parse());

		//These settings can never be changed by upgrading.
		const Settings* log_settings = m_old_install_log->GetInstallMode();
		m_settings.all_users = log_settings->all_users;
		m_settings.copy_only = log_settings->copy_only;

		BOOL desktop_shortcut = FALSE;
		BOOL start_menu_shortcut = FALSE;
		BOOL quick_launch_shortcut = FALSE;
		BOOL pinned_shortcut = FALSE;

		//Determine if there were shortcuts installed by the previous installation and if they
		//still exist. Change the settings to reflect that unless command line settings overrides
		//them.
		const ShortCut* shortcut;
		while (shortcut = m_old_install_log->GetShortcut())
		{
			OpFile shortcut_file;
			BOOL shortcut_exists;
			RETURN_IF_ERROR(shortcut_file.Construct(shortcut->GetPath()));
			if (OpStatus::IsError(shortcut_file.Exists(shortcut_exists)) || !shortcut_exists)
				continue;

			switch (shortcut->GetDestination())
			{
				case DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP:
				case DesktopShortcutInfo::SC_DEST_DESKTOP:
					desktop_shortcut = TRUE;
					break;

				case DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH:
					quick_launch_shortcut = TRUE;
					break;

				case DesktopShortcutInfo::SC_DEST_COMMON_MENU:
				case DesktopShortcutInfo::SC_DEST_MENU:
					start_menu_shortcut = TRUE;
					break;
			}
		}

		//Check if application is pinned to Windows 7 taskbar.
		if (IsSystemWin7())
		{
			OpString app_path;
			RETURN_IF_ERROR(app_path.Set(install_folder));
			RETURN_IF_ERROR(app_path.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));
			OpStatus::Ignore(WindowsShortcut::CheckIfApplicationIsPinned(app_path.CStr(), pinned_shortcut));
		}

		if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIDesktopShortcut))
			m_settings.desktop_shortcut = desktop_shortcut;
		if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIQuickLaunchShortcut))
			m_settings.quick_launch_shortcut = quick_launch_shortcut;
		if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIPinToTaskbar))
			m_settings.pinned_shortcut = pinned_shortcut;
		if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIStartMenuShortcut))
			m_settings.start_menu_shortcut = start_menu_shortcut;
	}

	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall))
	{
		if (exists)
		{
			m_operation = OpUpdate;
			//When updating, we write to m_install_log and we read from m_old_install_log
			RETURN_OOM_IF_NULL(m_install_log = OP_NEW(OperaInstallLog, ()));
			RETURN_IF_ERROR(m_install_log->Construct(install_log));
		}
		else
			m_operation = OpInstall;
	}
	else if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIUninstall) && exists)
	{
		m_operation = OpUninstall;
	}

	RETURN_IF_ERROR(m_install_folder.Set(install_folder));

	//If a operaprefs_default.ini file is present in the installation folder...
	m_existing_multiuser_setting = -1;
	m_existing_product_setting = -1;

	OpString global_settings_path;
	RETURN_IF_ERROR(global_settings_path.Set(install_folder));
	RETURN_IF_ERROR(global_settings_path.Append(UNI_L(PATHSEP) DESKTOP_RES_OPERA_PREFS_GLOBAL));

	OpFile global_settings_file;
	RETURN_IF_ERROR(global_settings_file.Construct(global_settings_path));

	exists = FALSE;
	if (OpStatus::IsError(global_settings_file.Exists(exists)) || !exists)
		return OpStatus::OK;

	//...read the Opera product setting from it, for later use...
	PrefsFile global_prefsfile(PREFS_STD);
	TRAPD(status, global_prefsfile.ConstructL(); \
		global_prefsfile.SetFileL(&global_settings_file); \
		m_existing_product_setting = global_prefsfile.ReadIntL("System", "Opera Product", -1));

	OpStatus::Ignore(status);

	//...read the MultiUser setting from it and use it to change the value of the single profile
	//setting, unless it was set on the command line...
	TRAP(status, m_existing_multiuser_setting = global_prefsfile.ReadIntL("System", "Multi User", -1));

	OpStatus::Ignore(status);

	CommandLineArgument* profiles_option = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISingleProfile);

	if (m_existing_multiuser_setting == 0 && !profiles_option)
		m_settings.single_profile = TRUE;
	else if(m_existing_multiuser_setting == 1 && !profiles_option)
		m_settings.single_profile = FALSE;

	//... and also read the Language File option and use it to set the installation language, unless it was set from the command line
	CommandLineArgument* lang_option = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWILanguage);
	if (lang_option == NULL && m_operation == OpUpdate)
	{
		OpString existing_language;
		TRAP(status, global_prefsfile.ReadStringL("User Prefs", "Language File", existing_language, OpStringC()));
		do
		{
			if(OpStatus::IsError(status) || existing_language.IsEmpty())
				break;

			int path_sep_pos = existing_language.FindLastOf(PATHSEPCHAR);
			int dot_pos = existing_language.FindLastOf('.');

			if (path_sep_pos == KNotFound || dot_pos == KNotFound || dot_pos <= ++path_sep_pos || uni_strncmp(existing_language.CStr() + dot_pos, UNI_L(".lng"), 5) != 0)
				break;

			existing_language.Delete(dot_pos);
			existing_language.Delete(0, path_sep_pos);
			
			m_install_language.Set(existing_language);
		}
		while (FALSE);
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::SetInstallLanguage(OpStringC &install_language)
{
	OP_ASSERT(!m_is_running);	//We should not be calling this once Run has been called
	if (m_is_running)
		return OpStatus::ERR;

	return m_install_language.Set(install_language);
}

OP_STATUS OperaInstaller::RunWizard()
{
	OP_ASSERT(!m_is_running);	//We should not be calling this once Run has been called
	if (m_is_running)
		return OpStatus::ERR;

	if (m_operation == OpInstall || m_operation == OpUpdate)
	{
		m_update_country_pref = true; // interactive installer - always update the pref
		OpStatus::Ignore(StartCountryCheckIfNeeded());
	}

	//Just show the wizard. Once the dialog is started, we can return control to the message loop
	return m_opera_installer_ui->Show();
}

OP_STATUS OperaInstaller::Run()
{
	// For profiling the installer
	OP_PROFILE_MSG("Opera Installation");
	OP_PROFILE_METHOD("Run");

	OP_ASSERT(!m_is_running);	//We should not be calling this once Run has been called
	if (m_is_running)
		return OpStatus::ERR;

	if (m_install_folder.IsEmpty())
	{
		//Usually means CheckAndSetInstallFolder has failed
		m_opera_installer_ui->ShowError(INSTALLER_INIT_FOLDER_FAILED);
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	if (m_operation == OpUpdate || m_operation == OpInstall)
	{
		OpStatus::Ignore(StartCountryCheckIfNeeded());
	}

	switch (m_operation)
	{
		case OpNone:
			//Usually means CheckAndSetInstallFolder has failed
			m_opera_installer_ui->ShowError(INSTALLER_INIT_OPERATION_FAILED);
			return OpStatus::ERR;

		case OpInstall:
		{
			m_install_log->SetInstallMode(m_settings);
			INST_HANDLE_ERROR(INSTALLER_INIT_LOG_FAILED, NULL, m_install_log->SetInstallPath(m_install_folder));
			m_step = 1;
			break;
		}

		case OpUpdate:
		{
			const Settings* log_settings = m_old_install_log->GetInstallMode();
			//Re-override these settings in case a call to SetSettings would have changed them
			//since CheckAndSetInstallFolder was called. These settings cannot be altered by
			//an update.
			m_settings.all_users = log_settings->all_users;
			m_settings.copy_only = log_settings->copy_only;
			//Update set_default_browser accordingly as well (in case SetSettings changed it too).
			if (m_settings.copy_only)
				m_settings.set_default_browser = FALSE;
			m_install_log->SetInstallMode(m_settings);
			INST_HANDLE_ERROR(INSTALLER_INIT_LOG_FAILED, NULL, m_install_log->SetInstallPath(m_install_folder));
			m_step = 101;
			break;
		}

		case OpUninstall:
		{
			const Settings* log_settings = m_old_install_log->GetInstallMode();
			//Make sure these settings are correct before uninstalling
			m_settings.all_users = log_settings->all_users;
			m_settings.copy_only = log_settings->copy_only;
			//Remember this so we know if we need to register a different handler for http after uninstalling.
			m_was_http_handler = HasAssociation(HTTP);
			m_step = 201;
			break;
		}
	}

	//has_priv is TRUE if we have full admin privileges/are elevated
	BOOL has_priv = WindowsUtils::IsPrivilegedUser(FALSE);

	//can_write reflects wether we have sufficient access to the directory
	BOOL can_write = TRUE;

	//Ensure that the install path exists, that it is a folder and that we can write to it.
	//Create the folder if needed.
	//Or indicate that we can't write to it or create it.

	if (!PathIsDirectory(m_install_folder))
	{
		if (PathFileExists(m_install_folder) || m_operation != OpInstall)
		{
			//We should never be creating a new folder if we are updating or uninstalling
			m_opera_installer_ui->ShowError(INVALID_FOLDER);
			return OpStatus::ERR_FILE_NOT_FOUND;
		}

		OpString parent_path;
		INST_HANDLE_ERROR(PARENT_PATH_INIT_FAILED, NULL, parent_path.Set(m_install_folder));

		int pathsep_pos;

		//Find the first ancestor of the installation directory that exist and creates all the subdirectories needed
		while((pathsep_pos = parent_path.FindLastOf(PATHSEPCHAR)) != KNotFound)
		{
			INST_HANDLE_ERROR(PARENT_PATH_INIT_FAILED, NULL, parent_path.Set(parent_path.SubString(0, pathsep_pos)));

			OpFile parentfolder;
			if (!PathFileExists(parent_path))
				continue;

			if(!PathIsDirectory(parent_path))
			{
				//Can't create a subdirectory of a file
				m_opera_installer_ui->ShowError(INVALID_FOLDER);
				return OpStatus::ERR_FILE_NOT_FOUND;
			}

			//If we are installing for all users and are not an elevated process,
			//we will start an elevated process that will take care of this, so skip it now.
			if (!m_settings.all_users || has_priv)
			{
				if (!WindowsUtils::CheckObjectAccess(parent_path, SE_FILE_OBJECT, FILE_ADD_SUBDIRECTORY))
				{
						can_write = FALSE;
				}
				else
				{
					OpFile folder;
					INST_HANDLE_ERROR(CREATE_FOLDER_FAILED, NULL, folder.Construct(m_install_folder));
					if (OpStatus::IsError(folder.MakeDirectory()))
					{
						m_opera_installer_ui->ShowError(CREATE_FOLDER_FAILED);
						return OpStatus::ERR_FILE_NOT_FOUND;
					}
				}
			}

			break;
		}
		if (pathsep_pos == KNotFound)
		{
			//No valid parent directory was found for he specified path.
			m_opera_installer_ui->ShowError(INVALID_FOLDER);
			return OpStatus::ERR_FILE_NOT_FOUND;
		}
	}

	//We never save the last installation path for Opera Labs, since we would need to do it for every different labs build, which is useless cluttering
	if ((m_operation == OpInstall || m_operation == OpUpdate) && !m_settings.copy_only && g_desktop_product->GetProductType() != PRODUCT_TYPE_OPERA_LABS)
	{
		//Save the last installation path
		OpString last_install_path_key_path;
		INST_HANDLE_ERROR(LAST_INSTALL_PATH_KEY_STRING_SET_FAILED, NULL, last_install_path_key_path.Set(m_set_last_install_path_regop.key_path));
		OpStatus::Ignore(DoRegistryOperation(HKEY_CURRENT_USER, m_set_last_install_path_regop, last_install_path_key_path, NULL, NULL));
	}

	if (can_write && !WindowsUtils::CheckObjectAccess(m_install_folder, SE_FILE_OBJECT, FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE))
		can_write = FALSE;

	if (m_operation == OpInstall && m_settings.single_profile && m_existing_multiuser_setting == -1 && !can_write)
	{
		//Refuse to make a new installation using single profile if the user doesn't have write access to the directory.
		m_opera_installer_ui->ShowError(CANT_INSTALL_SINGLE_PROFILE_WITHOUT_WRITE_ACCESS);
		return OpStatus::ERR_NO_ACCESS;
	}

	//This is the hack of the century.
	//When running elevated (or somehow with all privileges) and upgrading a single profile installation,
	//give full access to the current user. This should ensure that most people who have installed with single
	//profile in Program Files can keep using their profile. It assumes they are only running one admin account.
	if (m_operation == OpInstall && m_settings.single_profile && m_existing_multiuser_setting != -1 && has_priv)
	{
		WindowsUtils::RestoreAccessInfo* t = NULL;
		WindowsUtils::GiveObjectAccess(m_install_folder, SE_FILE_OBJECT, GENERIC_ALL, TRUE, t);
	}

	//Installing, updating or uninstalling for all_users (machine-wide) requires Admin privileges
	//So, if we are not admin, start an elevated process.
	if (m_settings.all_users && !has_priv)
		return RunElevated();

	// Test if Opera is running. If so show the blocking dialog, and give the user a chance to shut down Opera
	// or give the user a possibility to let the installer do it for them.
	if (IsOperaRunningAt(m_install_folder.CStr()))
		return OpStatus::ERR_NO_ACCESS;

	//If we don't have write permission to the installation folder but are not running with admin privileges,
	//attempt to get write permissions. If we can't write, but have admin privileges already, just fail.
	if (!can_write)
	{
		if (has_priv)
		{
			m_opera_installer_ui->ShowError(CANT_GET_FOLDER_WRITE_PERMISSIONS);
			return OpStatus::ERR;
		}
		RETURN_IF_ERROR(GetWritePermission(m_install_folder, m_settings.single_profile));
		//If we still don't have write permission, fail.
		if (!WindowsUtils::CheckObjectAccess(m_install_folder, SE_FILE_OBJECT, FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE))
		{
			if (m_get_write_permission_event != NULL)
			{
				PulseEvent(m_get_write_permission_event);
				CloseHandle(m_get_write_permission_event);
				m_get_write_permission_event = NULL;
			}
			m_opera_installer_ui->ShowError(CANT_OBTAIN_WRITE_ACCESS);
			return OpStatus::ERR_NO_ACCESS;
		}
	}

	OpString error_log_path;
	INST_HANDLE_ERROR(INIT_ERROR_LOG_PATH_FAILED, NULL, error_log_path.Set(m_install_folder));
	INST_HANDLE_ERROR(INIT_ERROR_LOG_PATH_FAILED, NULL, error_log_path.Append(UNI_L(PATHSEP) UNI_L("opera_inst_errors.log")));

	OpAutoPtr<OpFile> error_log(OP_NEW(OpFile, ()));
	if (!error_log.get())
	{
		m_opera_installer_ui->ShowError(INIT_ERROR_LOG_FAILED);
		return OpStatus::ERR;
	}

	INST_HANDLE_ERROR(INIT_ERROR_LOG_FAILED, NULL, error_log->Construct(error_log_path));
	INST_HANDLE_ERROR(INIT_ERROR_LOG_FAILED, NULL, error_log->Open(OPFILE_WRITE));

	m_error_log = error_log.release();

	//Obtain the list of files to copy. If this fails, the package itself is broken.
	if (m_operation != OpUninstall)
		RETURN_IF_ERROR(ReadFilesList());

	RETURN_OOM_IF_NULL(m_installer_transaction = OP_NEW(OpTransaction, ()));

	//Initialization is done, now we get down to business.
	m_is_running = TRUE;

	//Dispalys the progress dialog. m_step_counts is just the amount of steps to use for the
	//progress bar, so it isn't critical that it is right. We use an ASSERT later on to check it though.
	//Since file copies are somewhat slow, each of them count for a step.
	m_steps_count = 0;
	if (m_operation == OpInstall)
		m_steps_count = 7 + m_files_list.GetCount();
	else if (m_operation == OpUpdate)
		m_steps_count = 8 + m_files_list.GetCount();
	else if (m_operation == OpUninstall)
		m_steps_count = 5;

	if (m_opera_installer_ui)
	{
		INST_HANDLE_ERROR(SHOW_PROGRESS_BAR_FAILED, NULL, m_opera_installer_ui->ShowProgress(m_steps_count));
	}

	return OpStatus::OK;
}

void OperaInstaller::Pause(BOOL pause)
{
	OP_ASSERT(m_is_running);	//We should not be calling this before Run has been called
	if (!m_is_running)
		return;

	//m_opera_installer_ui is the real handler for pause. This class just proxies it for the wizard
	m_opera_installer_ui->Pause(pause);
}



OP_STATUS OperaInstaller::DoStep()
{
	OP_STATUS status = OpStatus::ERR;
	OpString8 prof;
	RETURN_IF_ERROR(prof.AppendFormat("%d,%d: ", m_step, m_step_index));

	switch (m_step)
	{
		case 0:		//This indicates that there are no more steps.
					//It's not really an error but we want to bail out
					//when it happens.
		{
			OP_ASSERT(m_steps_count == 0);

			OP_PROFILE_MSG("Opera Installation End");
			OP_PROFILE_METHOD("End");

			//If we have been elevated (to install for all users), warn the parent process that this completed successfulyly.
			OpAutoHANDLE evt(OpenEvent(EVENT_ALL_ACCESS, FALSE, UNI_L("OperaInstallerCompletedSuccessfully")));
			if (evt.get())
				PulseEvent(evt);

			break;
		}

		//
		// Installation steps
		//
		case 1: 
		{
			OP_PROFILE_METHOD("Install_remove_old_installers");
			status = Install_Remove_old();
			break; 
		}
		case 2: 
		{
			prof.AppendFormat("Install_copy_file - %s", StringConvert::UTF8FromUTF16(m_files_list.Get(m_step_index)->CStr()));
			OP_PROFILE_METHOD(prof.CStr());
			status = Install_Copy_file(); 
			break; 
		}
		case 3:
		{
			OP_PROFILE_METHOD("Install_registry");
			status = Install_Registry();
			break;
		}
		case 4:
		{
			OP_PROFILE_METHOD("Install_MAPI_libraries");
			status = Install_MAPI_Libraries();
			break;
		}
		case 5:
		{
			prof.Append("Install_Notify_system");
			status = Install_Notify_system();
			break;
		}
		case 6:
		{ 
			prof.Append("Instal_Default_prefs");
			status = Install_Create_shortcuts();
			break;
		}
		case 7:
		{
			prof.Append("Install_Default_prefs");
			status = Install_Default_prefs();
			break;
		}
		case 8:
		{
			prof.Append("Install_Finalize");
			status = Install_Finalize();
			break;
		}

		//
		// Upgrade steps
		//
		case 101:
		{
			OP_PROFILE_METHOD("Update_Remove_old_files");
			status = Update_Remove_old_files();
			break;
		}
		case 102:
		{
			prof.AppendFormat("Install_copy_file - %s", StringConvert::UTF8FromUTF16(m_files_list.Get(m_step_index)->CStr()));
			OP_PROFILE_METHOD(prof.CStr());
			status = Install_Copy_file(); 
			break; 
		}
		case 103:
		{
			OP_PROFILE_METHOD("Uninstall_Registry");
			status = Uninstall_Registry();
			break;
		}
		case 104:
		{
			OP_PROFILE_METHOD("Install_registry");
			status = Install_Registry();
			break;
		}
		case 105:
		{
			OP_PROFILE_METHOD("Install_MAPI_Libraries");
			status = Install_MAPI_Libraries();
			break;
		}
		case 106:
		{
			OP_PROFILE_METHOD("Install_Notify_system");
			status = Install_Notify_system();
			break;
		}
		case 107:
		{
			OP_PROFILE_METHOD("Update_Shortcuts");
			status = Update_Shortcuts();
			break;
		}
		case 108:
		{
			prof.Append("Install_Default_prefs");
			status = Install_Default_prefs();
			break;
		}
		case 109:
		{
			OP_PROFILE_METHOD("Install_Finalize");
			status = Install_Finalize();
			break;
		}

		//
		// Uninstallation steps
		//
		case 201:
		{
			OP_PROFILE_METHOD("Uninstall_Remove_Shortcuts");
			status = Uninstall_Remove_Shortcuts();
			break;
		}
		case 202:
		{
			OP_PROFILE_METHOD("Uninstall_Registry");
			status = Uninstall_Registry();
			break;
		}
		case 203:
		{
			OP_PROFILE_METHOD("Install_Notify_system");
			status = Install_Notify_system();
			break;
		}
		case 204:
		{
			OP_PROFILE_METHOD("Uninstall_MAPI_libraries");
			status = Uninstall_MAPI_Libraries();
			break;
		}
		case 205:
		{
			OP_PROFILE_METHOD("Uninstall_Finalize");
			status = Uninstall_Finalize();
			break;
		}
	}

	if (status == OpStatus::ERR_YIELD)
	{
		Pause(TRUE);
	}
	else
	{
		m_steps_count--;
	}

	return status;
}

OP_STATUS OperaInstaller::GetErrorLog(OpFile*& log)
{
	log = m_error_log;
	if (log)
		return OpStatus::OK;
	else
		return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS OperaInstaller::Install_Remove_old()
{
	//Remove any installation made with a previous installer in the install folder.
	BOOL found_installer;
	RETURN_IF_ERROR(FindAndRemoveMSIInstaller(found_installer));
	if (!found_installer)
		RETURN_IF_ERROR(FindAndRemoveWiseInstaller(found_installer));

	//If we are upgrading from a previous installer, we should refuse to let the user
	//cancel, otherwise they'll end up without opera installed since we can't
	//roll back the uninstallation of previous versions.
	if (found_installer)
		m_opera_installer_ui->ForbidClosing(TRUE);

	m_step++;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_Copy_file()
{
	const UINT i = m_step_index; // Just for convenience to not make the code lines too long.

	//This should never happen. unless the stepping mechanism is somehow broken.
	OP_ASSERT(i < m_files_list.GetCount());
	if (!m_files_list.Get(i))
	{
		m_opera_installer_ui->ShowError(COPY_FILE_INIT_FAILED);
		return OpStatus::ERR;
	}

	OpFile src_file;
	OpFile dst_file;
	INST_HANDLE_ERROR(COPY_FILE_INIT_FAILED, m_files_list.Get(i)->CStr(), src_file.Construct(*(m_files_list.Get(i)), OPFILE_RESOURCES_FOLDER));

	OpString dst_file_path;
	INST_HANDLE_ERROR(COPY_FILE_INIT_FAILED, m_files_list.Get(i)->CStr(), dst_file_path.Set(m_install_folder));
	INST_HANDLE_ERROR(COPY_FILE_INIT_FAILED, m_files_list.Get(i)->CStr(), dst_file_path.Append(PATHSEP));
	INST_HANDLE_ERROR(COPY_FILE_INIT_FAILED, m_files_list.Get(i)->CStr(), dst_file_path.Append(*(m_files_list.Get(i))));

	INST_HANDLE_ERROR(COPY_FILE_INIT_FAILED, m_files_list.Get(i)->CStr(), dst_file.Construct(dst_file_path));

	RETRY_IF_ERROR(COPY_FILE_REPLACE_FAILED, m_files_list.Get(i)->CStr(), dst_file_path.CStr(), OpAutoPtr<OpUndoableOperation>(OP_NEW(ReplaceFileOperation, (dst_file, OpFileInfo::FILE))));

	INST_HANDLE_ERROR(COPY_FILE_WRITE_FAILED, m_files_list.Get(i)->CStr(), dst_file.CopyContents(&src_file, TRUE));

	INST_HANDLE_ERROR(WRITE_FILE_TO_LOG_FAILED, NULL, m_install_log->SetFile(m_files_list.Get(i)->CStr()));

	//Ensures that we copy the next file when we call this method next
	m_step_index++;

	//When we have copied the last file, we move on to the next step.
	if (m_step_index == m_files_list.GetCount())
		m_step++;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_MAPI_Libraries()
{
	m_step++;
#ifdef _WIN64
	if (!m_settings.all_users)
		return OpStatus::OK;

	const uni_char* mapi_path[] =
	{
		{UNI_L("mapi\\OperaMapi.dll")},
		{UNI_L("mapi\\OperaMapi_32.dll")},
	};

	OP_STATUS status = OpStatus::OK;
	OpAutoVector<OpString> pf_path;
	OpString dst_folder;
	do
	{
		OpString tmp;
		if (OpStatus::IsError(status = WindowsOpDesktopResources::GetFolderPath(FOLDERID_ProgramFiles, CSIDL_PROGRAM_FILES, tmp, FALSE)))
			break;
		OpAutoPtr<OpString> ptr(OP_NEW(OpString,(tmp)));
		if (!ptr.get())
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		if (OpStatus::IsError(status = pf_path.Add(ptr.release())))
			break;
		if (OpStatus::IsError(status = WindowsOpDesktopResources::GetFolderPath(FOLDERID_ProgramFilesX86, CSIDL_PROGRAM_FILESX86, tmp, FALSE)))
			break;
		ptr.reset(OP_NEW(OpString,(tmp)));
		if (!ptr.get())
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		if (OpStatus::IsError(status = pf_path.Add(ptr.release())))
			break;
		if (OpStatus::IsError(status = dst_folder.Set(m_install_folder)))
			break;
	}while(0);

	if (OpStatus::IsError(status))
	{
		m_opera_installer_ui->ShowError(MAPI_LIBRARIES_INST_INIT_FAILED, NULL, NULL, FALSE);
		return OpStatus::OK;
	}

	if (dst_folder.CompareI(pf_path.Get(0)->CStr(), pf_path.Get(0)->Length()) == 0)
		dst_folder.Delete(0, pf_path.Get(0)->Length());
	else
		return OpStatus::OK;

	for (UINT32 i = 0; i < pf_path.GetCount(); i++)
	{
		OpString dst_file_path;
		OpFile src_file, dst_file;
		if (OpStatus::IsError(status = src_file.Construct(mapi_path[i], OPFILE_RESOURCES_FOLDER)))
		{
			m_opera_installer_ui->ShowError(MAPI_LIBRARIES_SRC_FILE_INIT_FAILED, mapi_path[i], NULL, FALSE);
			break;
		}
		if (OpStatus::IsError(status = dst_file_path.AppendFormat("%s%s\\OperaMAPI.dll",pf_path.Get(i)->CStr(), dst_folder.CStr())) ||
			OpStatus::IsError(status = dst_file.Construct(dst_file_path)))
		{
			m_opera_installer_ui->ShowError(MAPI_LIBRARIES_DST_FILE_INIT_FAILED, dst_file_path.CStr(), NULL, FALSE);
			break;
		}

		INST_HANDLE_NONFATAL_ERROR(MAPI_LIBRARIES_REPLACE_FAILED, dst_file_path.CStr(), status, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(ReplaceFileOperation, (dst_file, OpFileInfo::FILE)))));
		if (OpStatus::IsError(status))
			break;

		INST_HANDLE_NONFATAL_ERROR(MAPI_LIBRARIES_WRITE_FAILED, dst_file_path.CStr(), status, dst_file.CopyContents(&src_file, TRUE));
		if (OpStatus::IsError(status))
			break;
	}

	if (OpStatus::IsError(status))
		return OpStatus::OK;
	do
	{
		OpString mapi_dll_path;
		if (OpStatus::IsError(status = mapi_dll_path.AppendFormat("%%PROGRAMFILES%%%s\\OperaMAPI.dll", dst_folder.CStr())))
			break;
		RegistryOperation default_mapi_regop = {UNI_L("Software\\Clients\\Mail\\Opera{Product}"),
												UNI_L("DLLPath"),
												REG_EXPAND_SZ,
												mapi_dll_path.CStr(),
												WIN2K,
												HKEY_LOCAL_MACHINE,
												PRODUCT_OPERA | PRODUCT_OPERA_NEXT,
												0};
		OpString key_path;
		if (OpStatus::IsError(status = key_path.Set(default_mapi_regop.key_path)))
			break;
		status = DoRegistryOperation(HKEY_LOCAL_MACHINE, default_mapi_regop, key_path, m_installer_transaction, m_install_log);
	} while(0);
	if (OpStatus::IsError(status))
		m_opera_installer_ui->ShowError(MAPI_LIBRARIES_INSTALLATION_FAILED, NULL, NULL, FALSE);
#endif
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_Registry()
{
	if (m_settings.copy_only)
	{
		m_step++;
		return OpStatus::OK;
	}

	//When installing for all users (machine-wide), changes are maed to HKLM.
	HKEY root_key = m_settings.all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER;
	INST_HANDLE_ERROR(WRITE_REG_HIVE_TO_LOG_FAILED, NULL, m_install_log->SetRegistryHive(m_settings.all_users?UNI_L("HKLM"):UNI_L("HKCU")));

	UINT reg_op_count;
	reg_op_count = sizeof(m_installer_regops)/sizeof(RegistryOperation);

	//Build a unique name for the registry key containing uninstallation information,
	//To allow for installing the same version several time without conflict.
	OpString uninstall_key_name;
	INST_HANDLE_ERROR(REGISTRY_INIT_UNINSTALL_KEY_FAILED, NULL, uninstall_key_name.Set(UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Opera ") VER_NUM_STR_UNI UNI_L(".") UNI_L(VER_BUILD_NUMBER_STR)));
	int uninstall_key_suffix = 1;
	HKEY key;

	//Ensure we are creating a new uninstall key
	while (RegOpenKeyEx(root_key, uninstall_key_name, 0, KEY_READ, &key) != ERROR_FILE_NOT_FOUND)
	{
		RegCloseKey(key);
			
		INST_HANDLE_ERROR(REGISTRY_INIT_UNINSTALL_KEY_FAILED, NULL, uninstall_key_name.Set(UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Opera ") VER_NUM_STR_UNI UNI_L(".") UNI_L(VER_BUILD_NUMBER_STR)));
		INST_HANDLE_ERROR(REGISTRY_INIT_UNINSTALL_KEY_FAILED, NULL, uninstall_key_name.AppendFormat( UNI_L("_%d"), uninstall_key_suffix++));
	}


	//Execute the registry operations. For those operations that specify {Uninstall} as key, use the uninstall key name we just created.
	for (UINT i = 0; i < reg_op_count; i++)
	{
		OpString key_path;
		BOOL uninstall_key = FALSE;
		INST_HANDLE_ERROR(REGISTRY_INIT_REGOP_KEY_FAILED, m_installer_regops[i].key_path, key_path.Set(m_installer_regops[i].key_path));
		uninstall_key = key_path.Compare(UNI_L("{Uninstall}")) == 0;
		if (uninstall_key)
			INST_HANDLE_ERROR(REGISTRY_INIT_REGOP_KEY_FAILED, m_installer_regops[i].key_path, key_path.Set(uninstall_key_name));

		OpString key_reported;
		key_reported.Empty();
		INST_HANDLE_ERROR(INIT_KEY_REPORTED_FAILED, NULL, key_reported.AppendFormat(UNI_L("%s")UNI_L(PATHSEP)UNI_L("%s"), m_installer_regops[i].key_path, m_installer_regops[i].value_name ));
		if (uninstall_key)
		{
			INST_HANDLE_ERROR(REGISTRY_OPERATION_FAILED, key_reported.CStr(), DoRegistryOperation(root_key, m_installer_regops[i], key_path, m_installer_transaction, m_install_log));
		}
		else
		{
			OP_STATUS error;
			INST_HANDLE_NONFATAL_ERROR(REGISTRY_OPERATION_FAILED, key_reported.CStr(), error, DoRegistryOperation(root_key, m_installer_regops[i], key_path, m_installer_transaction, m_install_log));
		}
	}

	m_step++;

	return OpStatus::OK;

}

OP_STATUS OperaInstaller::Install_Notify_system()
{
	if (m_settings.copy_only)
	{
		m_step++;
		return OpStatus::OK;
	}
	//Warn running applications that we have changed registry settings
	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\Mail"));
	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\News"));
	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\StartMenuInternet"));

	m_step++;
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_Create_shortcuts()
{
	if (m_settings.copy_only)
	{
		m_step++;
		return OpStatus::OK;
	}

	OpINT32Vector destinations;

	//Build the list of places where we want to create a shortcut.
	//The shortcuts location is different for a machine-wide installation
	if (m_settings.all_users)
	{
		if (m_settings.desktop_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP));
		if (m_settings.start_menu_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_COMMON_MENU));
		if (m_settings.quick_launch_shortcut)
				INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH));
	}
	else
	{
		if (m_settings.desktop_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_DESKTOP));
		if (m_settings.start_menu_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_MENU));
		if (m_settings.quick_launch_shortcut)
				INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH));
	}

	//Create the shortcuts.
	BOOL use_opera_name = TRUE;
	RETURN_IF_ERROR(AddShortcuts(destinations, use_opera_name));

	//Pin applications to Windows 7 taskbar.
	if (IsSystemWin7() && m_settings.pinned_shortcut)
		RETURN_IF_ERROR(AddOperaToTaskbar());
	
	m_step++;
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::AddShortcuts(OpINT32Vector &destinations, BOOL& use_opera_name)
{
	DesktopShortcutInfo shortcut_info;

	//Set the shortcut informations that are common to all shortcuts
	INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_command.Set(m_install_folder));
	INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_command.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));
	INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_working_dir_path.Set(m_install_folder));

	shortcut_info.m_icon_index = m_product_icon_nr;
	INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_icon_name.Set(m_install_folder));
	INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_icon_name.Append(UNI_L(PATHSEP) UNI_L("Opera.dll")));

	//If we want to try naming the shortcuts Opera.lnk, we should check if any shortcut with that name
	//already exists. If any such shortcut does exist, switch to use the full Opera [version]. lnk name.
	if (use_opera_name)
	{
		INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_file_name.Set("Opera"));
		if (m_long_product_name.HasContent())
			INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_file_name.AppendFormat(" %s", m_long_product_name.CStr()));
		for (UINT i = 0; i < destinations.GetCount(); i++)
		{
			shortcut_info.m_destination = (DesktopShortcutInfo::Destination)destinations.Get(i);

			const uni_char* shortcut_report_name = ShortCut::DestNum2DestStr(shortcut_info.m_destination);

			WindowsShortcut shortcut;
			INST_HANDLE_ERROR(SHORTCUT_INIT_FAILED, shortcut_report_name, shortcut.Init(shortcut_info));
						
			OpFile shortcut_file;
			INST_HANDLE_ERROR(SHORTCUT_FILE_CONSTRUCT_FAILED, shortcut.GetShortcutFilePath().CStr(), shortcut_file.Construct(shortcut.GetShortcutFilePath().CStr()));

			BOOL exists;
			if (OpStatus::IsSuccess(shortcut_file.Exists(exists)) && exists)
			{
				use_opera_name = FALSE;
				break;
			}
		}
	}

	if (!use_opera_name)
	{
		INST_HANDLE_ERROR(SHORTCUT_INFO_SET_NEW_NAME_FAILED, NULL, shortcut_info.m_file_name.Set(UNI_L("Opera ")));
		if (m_long_product_name.HasContent())
			INST_HANDLE_ERROR(SHORTCUT_INFO_INIT_FAILED, NULL, shortcut_info.m_file_name.AppendFormat("%s ", m_long_product_name.CStr()));
		INST_HANDLE_ERROR(SHORTCUT_INFO_SET_NEW_NAME_FAILED, NULL, shortcut_info.m_file_name.Append(VER_NUM_STR_UNI UNI_L(" ") UNI_L(VER_BUILD_NUMBER_STR)));
	}

	for (UINT i = 0; i < destinations.GetCount(); i++)
	{
		//Set the shortcut location and create the shortcut.
		shortcut_info.m_destination = (DesktopShortcutInfo::Destination)destinations.Get(i);
		
		const uni_char* shortcut_report_name = ShortCut::DestNum2DestStr(shortcut_info.m_destination);

		WindowsShortcut shortcut;
		INST_HANDLE_ERROR(SHORTCUT_INIT_FAILED, shortcut_report_name, shortcut.Init(shortcut_info));

		OpFile shortcut_file;
		OP_STATUS err;
		INST_HANDLE_ERROR(SHORTCUT_FILE_CONSTRUCT_FAILED, shortcut.GetShortcutFilePath().CStr(), shortcut_file.Construct(shortcut.GetShortcutFilePath().CStr()));
		INST_HANDLE_NONFATAL_ERROR(SHORTCUT_REPLACE_FAILED, shortcut.GetShortcutFilePath().CStr(), err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(ReplaceFileOperation, (shortcut_file, OpFileInfo::FILE)))));
		if (OpStatus::IsError(err))
			continue;

		err = shortcut.Deploy();
		if (!OpStatus::IsSuccess(err))
		{
			BOOL ignore;
			if (err != OpStatus::ERR)
				ignore = m_opera_installer_ui->ShowError(SHORTCUT_DEPLOY_FAILED, shortcut.GetShortcutFilePath().CStr(), UNI_L("0"), FALSE);
			else
			{
				HRESULT hr;
				UINT loc;
				shortcut.GetErrData(hr, loc);

				OpString error_info;
				error_info.AppendFormat("%lu,%u", hr, loc);
				ignore = m_opera_installer_ui->ShowError(SHORTCUT_DEPLOY_FAILED, shortcut.GetShortcutFilePath().CStr(), error_info.CStr(), FALSE);
			}
			if (ignore)
				continue;

			return err;
		}
		
		ShortCut short_cut;
		short_cut.SetDestination(shortcut_info.m_destination);
		short_cut.SetPath(shortcut.GetShortcutFilePath().CStr());
		INST_HANDLE_ERROR(WRITE_SHORTCUT_TO_LOG_FAILED, NULL, m_install_log->SetShortcut(short_cut));
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::AddOperaToTaskbar()
{
	//Create temporary shortcut
	DesktopShortcutInfo shortcut_info;
	shortcut_info.m_destination = DesktopShortcutInfo::SC_DEST_TEMP;
	
	INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_file_name.Set("Opera"));
	if (m_long_product_name.HasContent())
		INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_file_name.AppendFormat(" %s", m_long_product_name.CStr()));
	INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_file_name.Append(VER_NUM_STR_UNI UNI_L(" ") UNI_L(VER_BUILD_NUMBER_STR)));

	INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_command.Set(m_install_folder));
	INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_command.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));
	INST_HANDLE_ERROR(TEMP_OPERA_PATH_INIT_FAILED, NULL, shortcut_info.m_working_dir_path.Set(m_install_folder));

	shortcut_info.m_icon_index = m_product_icon_nr;
	INST_HANDLE_ERROR(TEMP_SHORTCUT_INIT_FAILED, NULL, shortcut_info.m_icon_name.Set(m_install_folder));
	INST_HANDLE_ERROR(TEMP_SHORTCUT_INIT_FAILED, NULL, shortcut_info.m_icon_name.Append(UNI_L(PATHSEP) UNI_L("Opera.dll")));

	WindowsShortcut shortcut;
	OP_STATUS err;
	INST_HANDLE_ERROR(TEMP_SHORTCUT_INIT_FAILED, ShortCut::DestNum2DestStr(DesktopShortcutInfo::SC_DEST_TEMP), shortcut.Init(shortcut_info));
	INST_HANDLE_NONFATAL_ERROR(TEMP_SHORTCUT_DEPLOY_FAILED, ShortCut::DestNum2DestStr(DesktopShortcutInfo::SC_DEST_TEMP), err, shortcut.Deploy());
	if (OpStatus::IsSuccess(err))
	{
		//Pin Opera to taskbar
		INST_HANDLE_NONFATAL_ERROR(PIN_TO_TASKBAR_FAILED, NULL, err, WindowsShortcut::PinToTaskbar(shortcut.GetShortcutFilePath().CStr()));

		//Delete newly created temporary shortcut.
		OpFile file;
		OpStringC old_shortcut_path = shortcut.GetShortcutFilePath();
		INST_HANDLE_ERROR(TEMP_INIT_OLD_SHORTCUT_FILE_FAILED, old_shortcut_path, file.Construct(old_shortcut_path));
		INST_HANDLE_NONFATAL_ERROR(TEMP_INIT_OLD_SHORTCUT_FILE_FAILED, old_shortcut_path, err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (file, FALSE)))));
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_Default_prefs()
{
	if (m_country_checker && !m_country_check_pause &&
		m_country_checker->GetStatus() == CountryChecker::CheckInProgress)
	{
		// pause until country check finishes or until it times out
		m_country_check_pause = true;
		return OpStatus::ERR_YIELD;
	}

	BOOL exists;

	//Check if operaprefs_default.ini already exists.
	OpString global_settings_path;
	INST_HANDLE_ERROR(DEFAULT_PREFS_FILE_INIT_FAILED, NULL, global_settings_path.Set(m_install_folder));
	INST_HANDLE_ERROR(DEFAULT_PREFS_FILE_INIT_FAILED, NULL, global_settings_path.Append(UNI_L(PATHSEP) DESKTOP_RES_OPERA_PREFS_GLOBAL));

	OpFile global_settings_file;
	INST_HANDLE_ERROR(DEFAULT_PREFS_FILE_INIT_FAILED, NULL, global_settings_file.Construct(global_settings_path));

	INST_HANDLE_ERROR(DEFAULT_PREFS_FILE_INIT_FAILED, NULL, global_settings_file.Exists(exists));

	int current_product_setting = g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::OperaProduct);
	OpString labs_product_name;
	labs_product_name.Set(g_desktop_product->GetLabsProductName());

	//We prepare for writing to the operaprefs_default.ini file if:
	//-This is a first time installation and the Multi User setting needs to be changed or added. (Multi User setting should never be touched on upgrade)
	//-The Opera Product setting needs to be changed or added.
	//-A labs product name is present
	//-The installer language has been set, in this case we want to also set it as the default language of that installation.
	//-The -setdefaultpref command line option was used.
	//-Installer received IP-based country code from parent process or from the autoupdate server.
	if ((((m_settings.single_profile == TRUE && (m_existing_multiuser_setting != 0 || !exists)) ||
		(m_settings.single_profile == FALSE && m_existing_multiuser_setting == 0)) && m_operation != OpUpdate) ||
		(m_existing_product_setting != -1 && current_product_setting != m_existing_product_setting ) || (m_existing_product_setting == -1 && current_product_setting != 0) ||
		labs_product_name.HasContent() ||
		(m_update_country_pref && m_country.HasContent()) ||
		m_install_language.HasContent() || (CommandLineManager::GetInstance() && CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISetDefaultPref)))
	{
		if (!exists)
			RETRY_IF_ERROR(DEFAULT_PREFS_FILE_CREATE_FAILED, NULL, global_settings_path, OpAutoPtr<OpUndoableOperation>(OP_NEW(CreateFileOperation, (global_settings_file, OpFileInfo::FILE))));

		PrefsFile global_prefsfile(PREFS_STD);
		TRAPD(status, global_prefsfile.ConstructL(); \
			global_prefsfile.SetFileL(&global_settings_file));
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(DEFAULT_PREFS_FILE_CONSTRUCT_FAILED);
			return status;
		}

		//Write the Opera Product setting if it needs to be changed or added.
		if ((m_existing_product_setting != -1 && current_product_setting != m_existing_product_setting ) || (m_existing_product_setting == -1 && current_product_setting != 0))
		{
			TRAP(status, global_prefsfile.WriteIntL("System", "Opera Product", current_product_setting));

			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(SET_OPERANEXT_PREF_FAILED);
				return status;
			}
		}

		//Write the Labs product name if it exists.
		if (labs_product_name.HasContent())
		{
			if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_LABS)
				TRAP(status, global_prefsfile.WriteStringL("System", "Opera Labs Name", labs_product_name));
		}

		//Write the MultiUser setting if it needs to be changed or added.
		if (((m_settings.single_profile == TRUE && (m_existing_multiuser_setting != 0 || !exists)) ||
			(m_settings.single_profile == FALSE && m_existing_multiuser_setting == 0)) && m_operation != OpUpdate)
		{
			if (m_settings.single_profile)
				TRAP(status, global_prefsfile.WriteIntL("System", "Multi User", 0));
			else
				TRAP(status, global_prefsfile.WriteIntL("System", "Multi User", 1));

			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(SET_MULTIUSER_PREF_FAILED);
				return status;
			}
		}

		//Write the default language to be the same as the installer language
		if (m_install_language.HasContent())
		{
			OpString lng_path;
			INST_HANDLE_ERROR(INIT_LANG_PATH_FAILED, NULL, lng_path.Set(UNI_L("{Resources}locale") UNI_L(PATHSEP)));
			INST_HANDLE_ERROR(INIT_LANG_PATH_FAILED, NULL,lng_path.Append(m_install_language));
			TRAP(status, global_prefsfile.WriteStringL("User Prefs", "Language Files Directory", lng_path));
			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(SET_LANG_FILE_DIR_PREF_FAILED);
				return status;
			}
			INST_HANDLE_ERROR(INIT_LANG_PATH_FAILED, NULL,lng_path.Append(UNI_L(PATHSEP)));
			INST_HANDLE_ERROR(INIT_LANG_PATH_FAILED, NULL,lng_path.Append(m_install_language));
			INST_HANDLE_ERROR(INIT_LANG_PATH_FAILED, NULL,lng_path.Append(UNI_L(".lng")));
			TRAP(status, global_prefsfile.WriteStringL("User Prefs", "Language File", lng_path));
			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(SET_LANG_FILE_PREF_FAILED);
				return status;
			}
		}

		//Write anything that was specified using the -setdefaultpref command line option.
		if (CommandLineManager::GetInstance() && CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISetDefaultPref))
		{
			OpVector<OpString> &prefs_settings_list = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISetDefaultPref)->m_string_list_value;
			RETURN_IF_ERROR(AddPrefsFromStringsList(prefs_settings_list, global_prefsfile));
		}

		//Write IP-based country code if it was received from the Autoupdate server
		if (m_update_country_pref && m_country.HasContent())
		{
			TRAP(status, global_prefsfile.WriteStringL("Auto Update", "Country Code", m_country));
			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(SET_COUNTRY_CODE_PREF_FAILED);
				return status;
			}
		}

		//Save the changes to the actual file.
		TRAP(status, global_prefsfile.CommitL());
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(DEFAULT_PREFS_FILE_COMMIT_FAILED);
			return status;
		}

	}

	//If we are installing for all users, try to rename to fixed prefs file from its old name to its new name if it exists
	//Also add anything to fixed prefs that was specified by the -setfixedpref command line option
	if (m_settings.all_users && m_operation != OpUpdate)
	{
		OpDesktopResources *resources;
		INST_HANDLE_ERROR(INIT_DESKTOP_RESOURCES_FAILED, NULL, OpDesktopResources::Create(&resources));
		OpAutoPtr<OpDesktopResources> desktop_resources(resources);

		OpString fixed_prefs_old_path;
		OpString fixed_prefs_new_path;
		INST_HANDLE_ERROR(GET_FIXED_PREFS_FOLDER_FAILED, NULL, desktop_resources->GetFixedPrefFolder(fixed_prefs_old_path));
		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_new_path.Set(fixed_prefs_old_path));
		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_old_path.Append(UNI_L(PATHSEP) DESKTOP_RES_OLD_OPERA_PREFS));
		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_new_path.Append(UNI_L(PATHSEP) DESKTOP_RES_OPERA_PREFS_FIXED));

		OpFile fixed_prefs_old;
		OpFile fixed_prefs_new;

		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_old.Construct(fixed_prefs_old_path));
		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_new.Construct(fixed_prefs_new_path));

		INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_new.Exists(exists));
		//if the fixed prefs file exists under its old name but not its new name, copy it and give the copy the new name.
		if (!exists)
		{
			INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_old.Exists(exists));
			if (exists)
			{
				RETRY_IF_ERROR(FIXED_PREFS_MAKE_COPY_FAILED, NULL, fixed_prefs_new_path, OpAutoPtr<OpUndoableOperation> (OP_NEW(CreateFileOperation, (fixed_prefs_new, OpFileInfo::FILE))));
				INST_HANDLE_ERROR(FIXED_PREFS_COPY_CONTENT_FAILED, NULL, fixed_prefs_new.CopyContents(&fixed_prefs_old, TRUE));
			}
		}

		//If -setfixedpref was used, open the fixed prefs file and add those prefs to it.
		if (CommandLineManager::GetInstance() && CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISetFixedPref))
		{
			OpVector<OpString> &prefs_settings_list = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISetFixedPref)->m_string_list_value;

			INST_HANDLE_ERROR(FIXED_PREFS_INIT_FAILED, NULL, fixed_prefs_new.Exists(exists));

			if (!exists)
				RETRY_IF_ERROR(FIXED_PREFS_FILE_CREATE_FAILED, NULL, fixed_prefs_new_path, OpAutoPtr<OpUndoableOperation>(OP_NEW(CreateFileOperation, (fixed_prefs_new, OpFileInfo::FILE))));

			PrefsFile fixed_prefsfile(PREFS_STD);
			TRAPD(status, fixed_prefsfile.ConstructL(); \
				fixed_prefsfile.SetFileL(&fixed_prefs_new));
			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(FIXED_PREFS_FILE_CONSTRUCT_FAILED);
				return status;
			}
			RETURN_IF_ERROR(AddPrefsFromStringsList(prefs_settings_list, fixed_prefsfile));

			TRAP(status, fixed_prefsfile.CommitL());
			if (OpStatus::IsError(status))
			{
				m_opera_installer_ui->ShowError(FIXED_PREFS_FILE_COMMIT_FAILED);
				return status;
			}
		}
	}

	m_step++;
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::AddPrefsFromStringsList(OpVector<OpString> &prefs_settings_list, PrefsFile &prefs_file)
{
	OP_STATUS status;

	for (UINT i = 0; i < prefs_settings_list.GetCount(); i++)
	{
		//Parse strings of the form [Section]Key=Value

		OpString* entry = prefs_settings_list.Get(i);
		if ((*entry)[0] != '[')
			continue;

		int section_end = entry->FindFirstOf(']');
		int key_end = entry->FindFirstOf('=');

		if (section_end == KNotFound || key_end == KNotFound)
			continue;

		OpStringC section = entry->SubString(1, section_end - 1, &status);
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(INIT_PREF_ENTRY_FAILED);
			return status;
		}

		OpStringC key = entry->SubString(section_end + 1, key_end - (section_end + 1), &status);
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(INIT_PREF_ENTRY_FAILED);
			return status;
		}

		OpStringC value = entry->SubString(key_end + 1, -1, &status);
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(INIT_PREF_ENTRY_FAILED);
			return status;
		}

		//Write the pref deduced from the string parsing.
		TRAP(status, prefs_file.WriteStringL(section.CStr(), key.CStr(), value.CStr()));
		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(SET_PREF_FAILED);
			return status;
		}
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Install_Finalize()
{
	INST_HANDLE_ERROR(DELETE_OLD_LOG_FAILED, NULL, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (*m_install_log, FALSE)))));
	INST_HANDLE_ERROR(SAVE_LOG_FAILED, NULL, m_install_log->SaveToFile());

	INST_HANDLE_ERROR(COMMIT_TRANSACTION_FAILED, NULL, m_installer_transaction->Commit());

	//Do some finalization tasks
	//No need to report errors occuring after this point
	//as the installation is complete.


	OpString exe_path;
	RETURN_IF_ERROR(exe_path.Set(m_install_folder));
	RETURN_IF_ERROR(exe_path.Append(UNI_L(PATHSEP) UNI_L("pluginwrapper") UNI_L(PATHSEP) UNI_L("opera_plugin_wrapper.exe")));

	OpStringC plugin_wrapper_str(UNI_L("Opera Internet Browser - Plugin wrapper"));
	OpStatus::Ignore(WindowsOpSystemInfo::AddToWindowsFirewallException(exe_path, plugin_wrapper_str));

	RETURN_IF_ERROR(exe_path.Set(m_install_folder));
	RETURN_IF_ERROR(exe_path.Append(UNI_L(PATHSEP) UNI_L("pluginwrapper") UNI_L(PATHSEP) UNI_L("opera_plugin_wrapper_32.exe")));

	OpStringC plugin_wrapper_32bit_str(UNI_L("Opera Internet Browser - Plugin wrapper (32-bit)"));
	OpStatus::Ignore(WindowsOpSystemInfo::AddToWindowsFirewallException(exe_path, plugin_wrapper_32bit_str));

	RETURN_IF_ERROR(exe_path.Set(m_install_folder));
	RETURN_IF_ERROR(exe_path.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));

	OpStringC opera_str(UNI_L("Opera Internet Browser"));
	OpStatus::Ignore(WindowsOpSystemInfo::AddToWindowsFirewallException(exe_path, opera_str));

	if (!m_settings.copy_only && m_settings.set_default_browser)
	{
		if (m_settings.all_users)
			OpStatus::Ignore(SetDefaultBrowserAndAssociations(TRUE));

		OpStatus::Ignore(SetDefaultBrowserAndAssociations(FALSE));

	}

	if (m_run_yandex_script)
	{
		OpFile yandex_script;
		if (OpStatus::IsSuccess(yandex_script.Construct(UNI_L("Yandex.exe"), OPFILE_RESOURCES_FOLDER)))
			ShellExecute(NULL, NULL, yandex_script.GetFullPath(), UNI_L("--partner opera --distr /quiet /msicl \"YAHOMEPAGE=y YAQSEARCH=y\""), NULL, SW_HIDE);
	}

	//Refresh desktop icons.
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	if (m_settings.launch_opera)
		ShellExecute(NULL, NULL, exe_path.CStr(), NULL, m_install_folder.CStr(), SW_SHOW);

	m_step = 0;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Update_Remove_old_files()
{
	const uni_char* file_name;
	//In order to remove files that were created by the previous installation but not replaced
	//by this package, we need to compare the names of all the files installed with the names of
	//all the files to be installed. Since two files can't have the same name, we use a matches
	//array to keep track of all the files of this package that have already been matched, so they
	//are not used for future comparisons. Because the lists are hopefully always written in almost
	//the same order, this should allow close to n comparisons instead of n squared.
	BOOL* matches = OP_NEWA(BOOL, m_files_list.GetCount());
	if (!matches)
	{
		m_opera_installer_ui->ShowError(OLD_FILE_MATCHES_INIT_FAILED);
		return OpStatus::ERR_NO_MEMORY;
	}
	OpAutoArray<BOOL> matches_ptr(matches);
	ZeroMemory(matches, m_files_list.GetCount()*sizeof(BOOL));

	while((file_name = m_old_install_log->GetFile()) != NULL)
	{
		BOOL found = FALSE;
		for (UINT i = 0; i < m_files_list.GetCount(); i++)
		{
			if (!matches[i] && m_files_list.Get(i)->CompareI(file_name) == 0)
			{
				matches[i] = TRUE;
				found = TRUE;
				break;
			}
		}

		//The file was part of previous installation but won't be replaced by a file of this package -> remove it
		if (!found)
		{
			OpString file_path;
			INST_HANDLE_ERROR(INIT_OLD_FILE_PATH_FAILED, file_name, file_path.Set(m_install_folder));
			INST_HANDLE_ERROR(INIT_OLD_FILE_PATH_FAILED, file_name, file_path.Append(PATHSEP));
			INST_HANDLE_ERROR(INIT_OLD_FILE_PATH_FAILED, file_name, file_path.Append(file_name));

			OpFile file;
			INST_HANDLE_ERROR(INIT_OLD_FILE_PATH_FAILED, file_name, file.Construct(file_path));
			RETRY_IF_ERROR(DELETE_OLD_FILE_FAILED, NULL, file_path, OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (file))));
		}
	}

	m_step++;
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Update_Shortcuts()
{
	if (m_settings.copy_only)
	{
		m_step++;
		return OpStatus::OK;
	}

	//This method works in four steps. It will leave any shortcut for which the update setting isn't set unmodified.
	//First it marks for adding all shortcuts for which the shortcut setting is on.
	//Then it checks if files exists and for those that do, it unmarks them for adding or marks them for removing if their shortcut setting is off.
	//It then takes care of adding or removing shortcuts as needed.
	//Finally, shortcuts that have neither been added or removed are written to the new log file and 
	//  renamed to reflect the new version/build number (if they have the version number in their name), regardless of their update settings

	OpString old_shortcut_name;
	old_shortcut_name.Empty();

	//Mark the shortcut for adding only if an update of that shortcut has been requested
	BOOL add_desktop_shortcut = m_settings.update_desktop_shortcut && m_settings.desktop_shortcut;
	BOOL add_start_menu_shortcut = m_settings.update_start_menu_shortcut && m_settings.start_menu_shortcut;
	BOOL add_quick_launch_shortcut = m_settings.update_quick_launch_shortcut && m_settings.quick_launch_shortcut;

	BOOL desktop_shortcut_exists = FALSE;
	BOOL start_menu_shortcut_exists = FALSE;
	BOOL quick_launch_shortcut_exists = FALSE;

	m_old_install_log->ResetShortcutsIterator();

	const ShortCut* shortcut;
	//Go through the list of shortcut installed by the previous installation
	while ((shortcut = m_old_install_log->GetShortcut()) != NULL)
	{
		//Find the name of the shortcut used from the full path. We only need to do this
		//For the same shortcut because they always have identical names.
		if (old_shortcut_name.IsEmpty())
		{
			const uni_char* old_name = shortcut->GetPath();
			if (!old_name || !(old_name = uni_strrchr(old_name, PATHSEPCHAR) + 1))
			{
				m_opera_installer_ui->ShowError(INIT_OLD_SHORTCUT_NAME);
				return OpStatus::ERR;
			}
			INST_HANDLE_ERROR(INIT_OLD_SHORTCUT_NAME, shortcut->GetPath(), old_shortcut_name.Set(old_name));
		}

		BOOL delete_check = FALSE;
		OpFile shortcut_file;
		INST_HANDLE_ERROR(FIND_OLD_SHORTCUT, shortcut->GetPath(), shortcut_file.Construct(shortcut->GetPath()));
		OP_STATUS err;

		//Check if the shortcut exists.
		//If it exists, mark it for deletion if the settings indicate that it shouldn't be present and should be updated.
		//If it exists, unmark it for adding.
		switch (shortcut->GetDestination())
		{
			case DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP:
			case DesktopShortcutInfo::SC_DEST_DESKTOP:
				err = shortcut_file.Exists(desktop_shortcut_exists);
				delete_check = OpStatus::IsSuccess(err) && desktop_shortcut_exists && m_settings.update_desktop_shortcut && !m_settings.desktop_shortcut;
				if (desktop_shortcut_exists || OpStatus::IsError(err))
					add_desktop_shortcut = FALSE;
				break;

			case DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH:
				err = shortcut_file.Exists(quick_launch_shortcut_exists);
				delete_check = OpStatus::IsSuccess(err) && quick_launch_shortcut_exists && m_settings.update_quick_launch_shortcut && !m_settings.quick_launch_shortcut;
				if (quick_launch_shortcut_exists || OpStatus::IsError(err))
					add_quick_launch_shortcut = FALSE;
				break;

			case DesktopShortcutInfo::SC_DEST_COMMON_MENU:
			case DesktopShortcutInfo::SC_DEST_MENU:
				err = shortcut_file.Exists(start_menu_shortcut_exists);
				delete_check = OpStatus::IsSuccess(err) && start_menu_shortcut_exists && m_settings.update_start_menu_shortcut && !m_settings.start_menu_shortcut;
				if (start_menu_shortcut_exists || OpStatus::IsError(err))
					add_start_menu_shortcut = FALSE;
				break;
		}

		//If the shortcut was marked for deletion, delete it
		if (delete_check)
		{
			OpFile file;
			INST_HANDLE_ERROR(INIT_OLD_SHORTCUT_FILE_FAILED, shortcut->GetPath(), file.Construct(shortcut->GetPath()));
			INST_HANDLE_NONFATAL_ERROR(DELETE_OLD_SHORTCUT_FAILED, shortcut->GetPath(), err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (file, FALSE)))));
		}
	}

	//(Un)Pin application to/from Windows 7 taskbar.
	if (IsSystemWin7())
	{
		OpString app_path;
		INST_HANDLE_ERROR(PIN_OPERA_PATH_INIT_FAILED, NULL, app_path.Set(m_install_folder));
		INST_HANDLE_ERROR(PIN_OPERA_PATH_INIT_FAILED, NULL, app_path.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));

		BOOL pinned_shortcut_exists = FALSE;
		OP_STATUS status = WindowsShortcut::CheckIfApplicationIsPinned(app_path.CStr(), pinned_shortcut_exists);
		if (OpStatus::IsSuccess(status))
		{
			if (pinned_shortcut_exists && m_settings.update_pinned_shortcut && !m_settings.pinned_shortcut)
				RETURN_IF_ERROR(RemoveOperaFromTaskbar());
		}
		
		if (OpStatus::IsSuccess(status) && !pinned_shortcut_exists 
			&& m_settings.update_pinned_shortcut && m_settings.pinned_shortcut)
			RETURN_IF_ERROR(AddOperaToTaskbar());
	}

	OpINT32Vector destinations;

	//Prepare the list of places where a shortcut should be added now that we know which ones are marked for adding.
	if (m_settings.all_users)
	{
		if (add_desktop_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP));
		if (add_start_menu_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_COMMON_MENU));
		if (add_quick_launch_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH));
	}
	else
	{
		if (add_desktop_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_DESKTOP));
		if (add_start_menu_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_MENU));
		if (add_quick_launch_shortcut)
			INST_HANDLE_ERROR(BUILD_SHORTCUT_LIST_FAILED, NULL, destinations.Add(DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH));
	}

	//If previously existing shortcuts used a name without version number,
	//attempt to use a name without version number when creating new shortcuts.
	OpString compare_name;
	INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.Set("Opera"));
	if (m_long_product_name.HasContent())
		INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.AppendFormat(" %s", m_long_product_name.CStr()));
	INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.Append(".lnk"));

	BOOL use_opera_name = old_shortcut_name.IsEmpty() || old_shortcut_name.CompareI(compare_name) == 0;
	RETURN_IF_ERROR(AddShortcuts(destinations, use_opera_name));

	m_old_install_log->ResetShortcutsIterator();

	while ((shortcut = m_old_install_log->GetShortcut()) != NULL)
	{
		BOOL move_check = FALSE;

		//Shortcuts that we have not just added or deleted (but existed before) need to be written to the new log file, but also renamed if they used a version number
		switch (shortcut->GetDestination())
		{
			case DesktopShortcutInfo::SC_DEST_COMMON_DESKTOP:
			case DesktopShortcutInfo::SC_DEST_DESKTOP:
				//			already existed					was not deleted														was not added
				move_check = desktop_shortcut_exists && (!m_settings.update_desktop_shortcut || m_settings.desktop_shortcut) && !add_desktop_shortcut ;
				break;

			case DesktopShortcutInfo::SC_DEST_QUICK_LAUNCH:
				move_check = quick_launch_shortcut_exists && (!m_settings.update_quick_launch_shortcut || m_settings.quick_launch_shortcut) && !add_quick_launch_shortcut ;
				break;

			case DesktopShortcutInfo::SC_DEST_COMMON_MENU:
			case DesktopShortcutInfo::SC_DEST_MENU:
				move_check = start_menu_shortcut_exists && (!m_settings.update_start_menu_shortcut || m_settings.start_menu_shortcut) && !add_start_menu_shortcut ;
				break;
		}

		if (move_check)
		{
			ShortCut new_shortcut;
			new_shortcut.SetDestination(shortcut->GetDestination());

			const uni_char* old_path = shortcut->GetPath();
			if (!old_path)
			{
				m_opera_installer_ui->ShowError(INIT_NEW_SHORTCUT_PATH);
				return OpStatus::ERR;
			}

			OpString compare_name;
			INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.Set("Opera"));
			if (m_long_product_name.HasContent())
				INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.AppendFormat(" %s", m_long_product_name.CStr()));
			INST_HANDLE_ERROR(MAKE_SHORTCUT_COMPARE_NAME_FAILED, NULL, compare_name.Append(VER_NUM_STR_UNI UNI_L(" ") UNI_L(VER_BUILD_NUMBER_STR) UNI_L(".lnk")));

			//Rename the shortcut if needed.
			if (!use_opera_name && old_shortcut_name.CompareI(compare_name) != 0)
			{
				OpString new_path;
				INST_HANDLE_ERROR(INIT_NEW_SHORTCUT_PATH, old_path, new_path.Set(old_path));
				int last_sep = new_path.FindLastOf(PATHSEPCHAR);
				if (last_sep == KNotFound)
				{
					m_opera_installer_ui->ShowError(INIT_NEW_SHORTCUT_PATH, old_path);
					return OpStatus::ERR;
				}
				new_path.Delete(last_sep + 1);

				INST_HANDLE_ERROR(INIT_NEW_SHORTCUT_PATH, old_path, new_path.Append(UNI_L("Opera ")));
				if (m_long_product_name.HasContent())
					INST_HANDLE_ERROR(INIT_NEW_SHORTCUT_PATH, NULL, new_path.AppendFormat("%s ", m_long_product_name.CStr()));
				INST_HANDLE_ERROR(INIT_NEW_SHORTCUT_PATH, old_path, new_path.Append(VER_NUM_STR_UNI UNI_L(" ") UNI_L(VER_BUILD_NUMBER_STR) UNI_L(".lnk")));

				OpFile old_shortcut_file;
				OpFile new_shortcut_file;
				OP_STATUS err;
				INST_HANDLE_ERROR(INIT_OLD_SHORTCUT_FILE, old_path, old_shortcut_file.Construct(old_path));
				INST_HANDLE_ERROR(INIT_NEW_SHORTCUT_FILE, old_path, new_shortcut_file.Construct(new_path));
				INST_HANDLE_NONFATAL_ERROR(MOVE_SHORTCUT_FAILED, old_path, err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(ReplaceFileOperation, (new_shortcut_file, OpFileInfo::FILE)))));
				if (OpStatus::IsSuccess(err))
					INST_HANDLE_NONFATAL_ERROR(MOVE_SHORTCUT_FAILED, old_path, err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(MoveFileOperation, (old_shortcut_file, new_shortcut_file)))));

				INST_HANDLE_ERROR(SET_SHORTCUT_NEW_PATH, old_path, new_shortcut.SetPath(new_path.CStr()));
			}
			else
				//Otherwise, just use its current path
				INST_HANDLE_ERROR(SET_SHORTCUT_NEW_PATH, old_path, new_shortcut.SetPath(old_path));

			INST_HANDLE_ERROR(WRITE_SHORTCUT_TO_LOG_FAILED, NULL, m_install_log->SetShortcut(new_shortcut));
		}
	}

	m_step++;
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Uninstall_Remove_Shortcuts()
{
	//(Un)Pinning of applications to/from Windows 7 taskbar is handled differently than others.
	//For unknown reasons unpinning should be done before removal of any shortcut, and if not 
	//done this way, it may fail to unpin.
	if (IsSystemWin7() && m_settings.pinned_shortcut)
		RETURN_IF_ERROR(RemoveOperaFromTaskbar());
	
	m_old_install_log->ResetShortcutsIterator();

	const ShortCut* shortcut;
	while ((shortcut = m_old_install_log->GetShortcut()) != NULL)
	{
		OpFile file;
		OP_STATUS err;
		INST_HANDLE_ERROR(INIT_OLD_SHORTCUT_FILE_FAILED, shortcut->GetPath(), file.Construct(shortcut->GetPath()));
		INST_HANDLE_NONFATAL_ERROR(DELETE_OLD_SHORTCUT_FAILED, shortcut->GetPath(), err, m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (file, FALSE)))));
	}

	m_step++;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Uninstall_Registry()
{
	const RegistryKey* key_log;
	HKEY root_key = m_settings.all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER;

	while ((key_log = m_old_install_log->GetRegistryKey()) != NULL)
	{
		OpString key_reported;
		key_reported.Empty();
		INST_HANDLE_ERROR(INIT_KEY_REPORTED_FAILED, NULL, key_reported.AppendFormat(UNI_L("%d|%s")UNI_L(PATHSEP)UNI_L("%s"),key_log->GetCleanKey(), key_log->GetKeyPath(), key_log->GetValueName()));

		//Use the clean value that was stored in the log to find out what we need to remove.
		if (key_log->GetCleanKey() == 0)
			//Clean was set to 0. We just want to remove the value we have set.
			INST_HANDLE_ERROR(DELETE_OLD_REG_VALUE_FAILED, key_reported.CStr(), m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(ChangeRegValueOperation, (root_key, key_log->GetKeyPath() , key_log->GetValueName(), NULL, REG_DELETE, 0)))));
		else
		{
			//We want to remove the key itself and (clean - 1) of its parents
			OpString key_path;
			INST_HANDLE_ERROR(INIT_OLD_KEY_FAILED, key_reported.CStr(), key_path.Set(key_log->GetKeyPath()));
			for (UINT i = 1; i < key_log->GetCleanKey(); i++)
			{
				int p = key_path.FindLastOf('\\');
				if (p == KNotFound)
				{
					m_opera_installer_ui->ShowError(INIT_OLD_KEY_FAILED, key_reported.CStr());
					return OpStatus::ERR;
				}

				key_path.Delete(p);
			}
			INST_HANDLE_ERROR(DELETE_OLD_KEY_FAILED, key_reported.CStr(), m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteRegKeyOperation, (root_key, key_path)))));
		}
	}
	m_step++;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Uninstall_MAPI_Libraries()
{
	m_step++;
	OP_STATUS status;

	if (!m_settings.all_users)
		return OpStatus::OK;
#ifdef _WIN64
	OpAutoVector<OpString> pf_path;
	OpString install_folder;
	do
	{
		OpString tmp;
		if (OpStatus::IsError(status = WindowsOpDesktopResources::GetFolderPath(FOLDERID_ProgramFiles, CSIDL_PROGRAM_FILES, tmp, FALSE)))
			break;
		OpAutoPtr<OpString> ptr(OP_NEW(OpString,(tmp)));
		if (!ptr.get())
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		if (OpStatus::IsError(status = pf_path.Add(ptr.release())))
			break;
		if (OpStatus::IsError(status = WindowsOpDesktopResources::GetFolderPath(FOLDERID_ProgramFilesX86, CSIDL_PROGRAM_FILESX86, tmp, FALSE)))
			break;
		ptr.reset(OP_NEW(OpString,(tmp)));
		if (!ptr.get())
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		if (OpStatus::IsError(status = pf_path.Add(ptr.release())))
			break;
		if (OpStatus::IsError(status = install_folder.Set(m_install_folder)))
			break;
	}while(0);

	if (OpStatus::IsError(status))
	{
		m_opera_installer_ui->ShowError(MAPI_LIBRARIES_UNINST_INIT_FAILED, NULL, NULL, FALSE);
		return OpStatus::OK;
	}

	if (install_folder.CompareI(pf_path.Get(0)->CStr(), pf_path.Get(0)->Length()) == 0)
	{
		install_folder.Delete(0, pf_path.Get(0)->Length());

		for (UINT32 i = 0; i < pf_path.GetCount(); i++)
		{
			OpString file_path;
			OpFile file;
			if (OpStatus::IsError(status = file_path.AppendFormat("%s%s\\OperaMAPI.dll",pf_path.Get(i)->CStr(), install_folder.CStr())) ||
				OpStatus::IsError(status = file.Construct(file_path)))
			{
				break;
			}
			INST_HANDLE_NONFATAL_ERROR(MAPI_LIBRARIES_DELETE_FAILED, file_path.CStr(), status,  m_installer_transaction->Continue(OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (file, TRUE)))));
		}

		if (OpStatus::IsError(status))
		{
			m_opera_installer_ui->ShowError(MAPI_LIBRARIES_UNINSTALLATION_FAILED, NULL, NULL, FALSE);
			return OpStatus::OK;
		}
	}
#endif //_WIN64
	OpString key_path;
	RETURN_IF_ERROR(key_path.Set(m_default_mailer_start_menu_regop.key_path));
	OpString client_name;
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, key_path, client_name, NULL)) && client_name.CompareI(m_product_name))
		INST_HANDLE_NONFATAL_ERROR(MAPI_LIBRARIES_RESET_DEFAULT_MAIL_CLIENT, NULL, status, ResetDefaultMailClient(FALSE));
	else if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, key_path, client_name, NULL)) && client_name.CompareI(m_product_name))
		INST_HANDLE_NONFATAL_ERROR(MAPI_LIBRARIES_RESET_DEFAULT_MAIL_CLIENT, NULL, status, ResetDefaultMailClient(TRUE));
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::ResetDefaultMailClient(bool all_users)
{
	//TODO Improve this code: probably this method should become one of the Installation steps
	if (IsSystemWinVista() && m_settings.all_users && !all_users)
	{
		IApplicationAssociationRegistration* pAAR;
 		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			hr = pAAR->ClearUserAssociations();
			pAAR->Release();
		}
		return SUCCEEDED(hr)?OpStatus::OK:OpStatus::ERR;
	}

	RegistryOperation tmp = m_default_mailer_start_menu_regop;
	tmp.value_data = UNI_L("");
	OpString key_path;
	RETURN_IF_ERROR(key_path.Set(m_default_mailer_start_menu_regop.key_path));
	RETURN_IF_ERROR(DoRegistryOperation(all_users ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, m_default_mailer_start_menu_regop, key_path, NULL, NULL));

	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\Mail"));
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::Uninstall_Finalize()
{
	RETRY_IF_ERROR(DELETE_OLD_LOG_FAILED, NULL, m_old_install_log->GetFullPath(), OpAutoPtr<OpUndoableOperation>(OP_NEW(DeleteFileOperation, (*m_old_install_log))));

	//The task of removing the files is not critical for considering the uninstallation a sucess.
	//Let's just close the transaction now and ignore any further error.
	INST_HANDLE_ERROR(COMMIT_TRANSACTION_FAILED, NULL, m_installer_transaction->Commit());

	//Refresh desktop icons.
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	if (!m_settings.copy_only)
	{
		BOOL handler_ok = TRUE;
		//We don't care if anything in this block fails
		//Tries to Set IE back as default browser if we were the handler for http and then display the survey.
		do
		{
			OpString http_assoc;
			if (!m_was_http_handler)
				break;

			handler_ok = FALSE;

			//On XP, there is no reliable way to set IE as default again.
			if (!IsSystemWinVista())
				break;

			//If we are installing for all usrs, we are admin and can ask IE to
			//set itself as default for the system
			if (m_settings.all_users)
			{
				OpString ReinstallIECommand;
				if (OpStatus::IsError(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Clients\\StartMenuInternet\\IEXPLORE.EXE\\InstallInfo"), ReinstallIECommand, UNI_L("ReinstallCommand"))))
					break;

				int split;
				if (ReinstallIECommand[0] == '"')
				{
					ReinstallIECommand.Delete(0,1);
					split = ReinstallIECommand.FindFirstOf('"');
				}
				else
					split = ReinstallIECommand.FindFirstOf(' ');

				if (split == KNotFound)
					break;

				ReinstallIECommand[split] = '\0';

				SHELLEXECUTEINFO info;
				op_memset(&info, 0, sizeof(info));

				info.cbSize = sizeof(info);
				info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_DOENVSUBST;
				info.lpFile = ReinstallIECommand.CStr();
				info.lpParameters = ReinstallIECommand.CStr() + (split+1);
				info.nShow = SW_SHOW;
				if (!ShellExecuteEx(&info))
					break;

				WaitForSingleObject(info.hProcess, INFINITE);
				CloseHandle(info.hProcess);
			}

			IApplicationAssociationRegistration* pAAR;

			HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
			if (SUCCEEDED(hr))
			{
				//Since we are resetting the association for http, we can just as well do the most common ones.
				//But we just care about whether http assoc is set correctly.
				pAAR->SetAppAsDefault(UNI_L("Internet Explorer"), UNI_L(".htm"), AT_FILEEXTENSION);
				pAAR->SetAppAsDefault(UNI_L("Internet Explorer"), UNI_L(".html"), AT_FILEEXTENSION);
				hr = pAAR->SetAppAsDefault(UNI_L("Internet Explorer"), UNI_L("http"), AT_URLPROTOCOL);
				pAAR->SetAppAsDefault(UNI_L("Internet Explorer"), UNI_L("https"), AT_URLPROTOCOL);
				pAAR->Release();
			}

			if (FAILED(hr))
				break;

			handler_ok = TRUE;
		}
		while(FALSE);

		if (!CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWISilent))
		{
			//Attempts to show the survey.
			SHELLEXECUTEINFO info = {};

			info.cbSize = sizeof(info);
			info.lpVerb = UNI_L("open");
			info.fMask = SEE_MASK_DOENVSUBST;
			info.nShow = SW_SHOW;

			int timestamp = g_pcui->GetIntegerPref(PrefsCollectionUI::FirstRunTimestamp);
			OpString edition;
			RETURN_IF_ERROR(edition.Set(g_pcnet->GetStringPref(PrefsCollectionNetwork::IspUserAgentId)));

			OpString parameters;
			RETURN_IF_ERROR(parameters.Set(UNI_L("http://redir.opera.com/uninstallsurvey/?version=") VER_NUM_STR_UNI UNI_L(".") UNI_L(VER_BUILD_NUMBER_STR) UNI_L("&firstruntime=")));
			RETURN_IF_ERROR(parameters.AppendFormat("%lu", timestamp));
			if (edition.HasContent())
				RETURN_IF_ERROR(parameters.AppendFormat("%s%s", UNI_L("&edition="), edition));

			if (handler_ok)
			{
				info.lpFile = parameters.CStr();
			}
			else
			{
				info.lpFile = UNI_L("%ProgramFiles%\\Internet Explorer\\iexplore.exe");
				info.lpParameters = parameters.CStr();
			}

			if (ShellExecuteEx(&info))
			{
				OpString surveyed_key_path;
				RETURN_IF_ERROR(surveyed_key_path.Set(m_surveyed_regop.key_path));
				OpStatus::Ignore(DoRegistryOperation(HKEY_CURRENT_USER, m_surveyed_regop, surveyed_key_path, NULL, NULL));
			}
		}
	}

	RETURN_OOM_IF_NULL(g_uninstall_files = OP_NEW(OpVector<OpString>, ()));

	// Make a list of files locked by the installer itself.
	OpAutoVector<OpString> file_handle_list;
	HandleInfo handle_info;

	if (OpStatus::IsSuccess(handle_info.Init()))
		OpStatus::Ignore(handle_info.GetOpenFileList(GetCurrentProcessId(), file_handle_list));

	const uni_char* file_name;
	//Go through all the files installed and attempt to delete them
	while((file_name = m_old_install_log->GetFile()) != NULL)
	{
		OpString file_path;
		RETURN_IF_ERROR(file_path.Set(m_install_folder));
		RETURN_IF_ERROR(file_path.Append(PATHSEP));
		RETURN_IF_ERROR(file_path.Append(file_name));

		OpFile file;
		RETURN_IF_ERROR(file.Construct(file_path));

		OP_STATUS state;
		state = file.Delete(TRUE);

		//If we fail to remove the file, check if the file is locked.
		//And try to delete it again
		if (OpStatus::IsError(state))
		{
			//If the installer itself is locking the file, don't prompt the user about it.
			//TODO: What if another program also locks the file?
			BOOL installer_file = FALSE;
			UINT32 count = file_handle_list.GetCount();

			for (UINT32 i = 0; i < count; i++)
			{
				if (file_path.CompareI(file_handle_list.Get(i)->CStr()) == 0)
				{
					installer_file = TRUE;
					break;
				}
			}

			if (!installer_file && OpStatus::IsSuccess(CheckIfFileLocked(file_path.CStr())) && !m_was_canceled)
				state = file.Delete(TRUE);
		}
		
		if (OpStatus::IsSuccess(state))
		{
			//Attempt to delete the folder that contained the file and its parents
			//This is safe, because attempts to delete folders that still contain files
			//are refused.
			OpString folder;
			OpFile parent;
			RETURN_IF_ERROR(parent.Copy(&file));
			do
			{
				RETURN_IF_ERROR(parent.GetDirectory(folder));
				RETURN_IF_ERROR(parent.Construct(folder));
			}
			while (RemoveDirectory(folder.CStr()) != FALSE);
		}
		else
		{
			// If we still can't delete it, add the file to the list of files that will be deleted after the DLL is unloaded.
			OpString* uninstall_file = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(uninstall_file);
			RETURN_IF_ERROR(uninstall_file->Set(file_path));
			RETURN_IF_ERROR(g_uninstall_files->Add(uninstall_file));
		}
	}

	if (m_delete_profile != KEEP_PROFILE)
		DeleteProfile(m_delete_profile);

	m_step= 0;

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::RemoveOperaFromTaskbar()
{
	OpString app_path;
	OP_STATUS err;
	INST_HANDLE_ERROR(UNPIN_OPERA_PATH_INIT_FAILED, NULL, app_path.Set(m_install_folder));
	INST_HANDLE_ERROR(UNPIN_OPERA_PATH_INIT_FAILED, NULL, app_path.Append(UNI_L(PATHSEP) UNI_L("Opera.exe")));
	INST_HANDLE_NONFATAL_ERROR(UNPIN_FROM_TASKBAR_FAILED, NULL, err, WindowsShortcut::UnPinFromTaskbar(app_path.CStr())); 			
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::GetRerunArgs(OpString &args)
{
	switch (m_operation)
	{
		case OpInstall:
		{
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.Set(UNI_L("/install /silent /launchopera 0")));
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /installfolder \"%s\""), m_install_folder.CStr()));
			if (m_install_language.HasContent())
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /language \"%s\""), m_install_language.CStr()));
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /singleprofile %d"), m_settings.single_profile?1:0));
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /copyonly %d"), m_settings.copy_only?1:0));
			if (!m_settings.copy_only)
			{
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /allusers %d"), m_settings.all_users?1:0));
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /setdefaultbrowser %d"), m_settings.set_default_browser?1:0));
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /startmenushortcut %d"), m_settings.start_menu_shortcut?1:0));
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /desktopshortcut %d"), m_settings.desktop_shortcut?1:0));
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /quicklaunchshortcut %d"), m_settings.quick_launch_shortcut?1:0));
				if (IsSystemWin7())
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /pintotaskbar %d"), m_settings.pinned_shortcut?1:0));
			}
			if (m_update_country_pref)
			{
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.Append(UNI_L(" /updatecountrycode")));
				if (m_country.HasContent())
				{
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /countrycode %s"), m_country.CStr()));
				}
			}
			break;
		}
		//Some command line settings are irrelevant for update so we don't pass them
		case OpUpdate:
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.Set(UNI_L("/install /silent /launchopera 0")));
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /installfolder \"%s\""), m_install_folder.CStr()));
			if (m_install_language.HasContent())
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /language \"%s\""), m_install_language.CStr()));
			if (!m_settings.copy_only)
			{
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /setdefaultbrowser %d"), m_settings.set_default_browser?1:0));
				if (m_settings.update_start_menu_shortcut)
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /startmenushortcut %d"), m_settings.start_menu_shortcut?1:0));
				if (m_settings.update_desktop_shortcut)
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /desktopshortcut %d"), m_settings.desktop_shortcut?1:0));
				if (m_settings.update_quick_launch_shortcut)
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /quicklaunchshortcut %d"), m_settings.quick_launch_shortcut?1:0));
				if (IsSystemWin7() && m_settings.update_pinned_shortcut)
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /pintotaskbar %d"), m_settings.pinned_shortcut?1:0));
			}
			if (m_update_country_pref)
			{
				INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.Append(UNI_L(" /updatecountrycode")));
				if (m_country.HasContent())
				{
					INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.AppendFormat(UNI_L(" /countrycode %s"), m_country.CStr()));
				}
			}
			break;
		case OpUninstall:
			INST_HANDLE_ERROR(INIT_RERUN_ARGS_FAILED, NULL, args.Set("/uninstall /silent"));
			break;
		default:
			m_opera_installer_ui->ShowError(WRONG_RERUN_OPERATION, NULL);
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::RunElevated()
{
	//We cannot run elevated if UAC is disabled.
	if (IsSystemWinVista())
	{
		DWORD UAC_state;
		if (OpRegReadDWORDValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"), UNI_L("EnableLUA"), &UAC_state) == ERROR_SUCCESS && UAC_state == 0)
		{
			m_opera_installer_ui->ShowError(CANT_ELEVATE_WITHOUT_UAC, NULL);
			return OpStatus::ERR_NO_ACCESS;
		}
	}

	OpString args;
	RETURN_IF_ERROR(GetRerunArgs(args));

	SHELLEXECUTEINFO info;
	op_memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpVerb = UNI_L("runas");
	info.lpFile = g_spawnPath.CStr();
	info.lpParameters = args.CStr();
	info.nShow = SW_SHOW;

	//Event used to wait until the elevated installer process has succeeded.
	OpAutoHANDLE evt(CreateEvent(NULL, TRUE, FALSE, UNI_L("OperaInstallerCompletedSuccessfully")));

	if (!evt.get())
	{
		m_opera_installer_ui->ShowError(ELEVATION_CREATE_EVENT_FAILED, NULL);
		return FALSE;
	}

	//Somehow, calling this on Vista fails but it isn't needed anyway.
	if (!IsSystemWinVista())
		FixEventACL(evt);

	if (!ShellExecuteEx(&info))
    {
		DWORD err = GetLastError();
		if (err == ERROR_CANCELLED)
			return OpStatus::ERR_NO_ACCESS;
		m_opera_installer_ui->ShowError(ELEVATION_SHELL_EXECUTE_FAILED, NULL);
		return OpStatus::ERR;
	}
	else
	{
		m_opera_installer_ui->HideAndClose();

		HANDLE wait_objects[2] = { info.hProcess, evt };

		//evt is the second object in the array. If the installer succeeds, it will end the wait
		//and res will be WAIT_OBJECT_0 + 1
		DWORD res = WaitForMultipleObjects(2, wait_objects, FALSE, INFINITE);

		if (res == WAIT_FAILED)
		{
			m_opera_installer_ui->ShowError(ELEVATION_WAIT_FAILED, NULL);
			return OpStatus::ERR;
		}

		if (res == (WAIT_OBJECT_0 + 1))
		{
			if (m_run_yandex_script)
			{
				OpFile yandex_script;
				if (OpStatus::IsSuccess(yandex_script.Construct(UNI_L("Yandex.exe"), OPFILE_RESOURCES_FOLDER)))
					ShellExecute(NULL, NULL, yandex_script.GetFullPath(), UNI_L("--partner opera --distr /quiet /msicl \"YAHOMEPAGE=y YAQSEARCH=y\""), NULL, SW_HIDE);
			}

			//We launch opera from the non-elevated process
			if (m_settings.launch_opera)
			{
				OpString exe_path;
				exe_path.Set(m_install_folder);
				exe_path.Append(UNI_L(PATHSEP) UNI_L("Opera.exe"));

				ShellExecute(NULL, NULL, exe_path.CStr(), NULL, m_install_folder.CStr(), SW_SHOW);
			}

			if (m_operation == OpInstall || m_operation == OpUpdate)
			{
				if (!m_settings.copy_only && m_settings.set_default_browser)
					OpStatus::Ignore(SetDefaultBrowserAndAssociations(FALSE));
				else
					OpStatus::Ignore(RestoreFileHandlers(FALSE));
			}

			//We delete the profile from the non elevated process as well,
			//since it should be done only for the user who started the installer.
			if (m_operation == OpUninstall && m_delete_profile != KEEP_PROFILE)
				DeleteProfile(m_delete_profile);

		}

		return (res == (WAIT_OBJECT_0 + 1))?OpStatus::OK:OpStatus::ERR;
	}
}

OP_STATUS OperaInstaller::PathRequireElevation(BOOL &elevation_required)
{
	if (m_install_folder.IsEmpty())
		return OpStatus::ERR_FILE_NOT_FOUND;

	if (!PathIsDirectory(m_install_folder))
	{
		if (PathFileExists(m_install_folder) || m_operation != OpInstall)
			return OpStatus::ERR_FILE_NOT_FOUND;

		OpString parent_path;
		parent_path.Set(m_install_folder);

		do
		{
			int last_sep = parent_path.FindLastOf(PATHSEPCHAR);
			if (last_sep == KNotFound)
				return OpStatus::ERR_FILE_NOT_FOUND;

			RETURN_IF_ERROR(parent_path.Set(m_install_folder.SubString(0, last_sep)));

		}
		while (!PathIsDirectory(parent_path));

		elevation_required = !WindowsUtils::CheckObjectAccess(parent_path, SE_FILE_OBJECT, FILE_ADD_SUBDIRECTORY);
		return OpStatus::OK;
	}

	elevation_required = !WindowsUtils::CheckObjectAccess(m_install_folder, SE_FILE_OBJECT, FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE);
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::FixEventACL(HANDLE evt)
{
	PSECURITY_DESCRIPTOR sec_desc;
	PACL event_dacl;
	PACL new_event_dacl;
	EXPLICIT_ACCESS explicit_access;

	if (GetSecurityInfo(evt, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &event_dacl, NULL, &sec_desc) != ERROR_SUCCESS)
	{
		m_opera_installer_ui->ShowError(ELEVATION_CANT_FIX_EVENT_ACL, NULL);
		return OpStatus::ERR;
	}

	// make sure the administrator group has access to the event, so the process with admin privilege can report back.
	BuildExplicitAccessWithName(&explicit_access, UNI_L("Administrators"), GENERIC_ALL, GRANT_ACCESS, NO_INHERITANCE);
	if (SetEntriesInAcl(1, &explicit_access, event_dacl, &new_event_dacl) != ERROR_SUCCESS)
	{
		m_opera_installer_ui->ShowError(ELEVATION_CANT_FIX_EVENT_ACL, NULL);
		return OpStatus::ERR;
	}

	DWORD err = SetSecurityInfo(evt, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, new_event_dacl, NULL);
	LocalFree(new_event_dacl);

	if (err != ERROR_SUCCESS)
	{
		m_opera_installer_ui->ShowError(ELEVATION_CANT_FIX_EVENT_ACL, NULL);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::GetWritePermission(OpString &folder_path, BOOL permanent)
{
	//We cannot run elevated if UAC is disabled.
	if (IsSystemWinVista())
	{
		DWORD UAC_state;
		if (OpRegReadDWORDValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"), UNI_L("EnableLUA"), &UAC_state) == ERROR_SUCCESS && UAC_state == 0)
		{
			m_opera_installer_ui->ShowError(CANT_ELEVATE_WITHOUT_UAC, NULL);
			return OpStatus::ERR_NO_ACCESS;
		}
	}

	DWORD buf_size = UNLEN + 1;
	uni_char user_name[UNLEN + 1];

	if (!GetUserName(user_name, &buf_size))
	{
		m_opera_installer_ui->ShowError(GET_USER_NAME_FAILED, NULL);
		return OpStatus::ERR;
	}

	OpString args;
	INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Set("/give_folder_write_access \""));

	//Somewhat hackish way to pass the permanent parameter, but it avoids having yet another command line option
	if (permanent)
		INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Append(UNI_L("*")));

	INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Append(folder_path));
	INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Append("\" /give_write_access_to \""));
	INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Append(user_name));
	INST_HANDLE_ERROR(INIT_ELEVATION_ARGS_FAILED, NULL, args.Append("\""));

	SHELLEXECUTEINFO info;
	op_memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpVerb = UNI_L("runas");
	info.lpFile = g_spawnPath.CStr();
	info.lpParameters = args.CStr();
	info.nShow = SW_SHOW;

	//Event used to know when the elevated process has given us access to the folder.
	HANDLE evt = CreateEvent(NULL, TRUE, FALSE, UNI_L("OperaInstallerGetWritePermission"));

	if (!evt)
	{
		m_opera_installer_ui->ShowError(ELEVATION_CREATE_EVENT_FAILED, NULL);
		return OpStatus::ERR;
	}
	
	//Somehow, calling this on Vista fails but it isn't needed anyway.
	if (!IsSystemWinVista())
		FixEventACL(evt);

	if (!ShellExecuteEx(&info))
	{
		CloseHandle(evt);
		DWORD err = GetLastError();
		if (err == ERROR_CANCELLED)
			return OpStatus::ERR_NO_ACCESS;
		m_opera_installer_ui->ShowError(CANT_GET_FOLDER_WRITE_PERMISSIONS);
		return OpStatus::ERR;
	}

	HANDLE wait_objects[2] = { info.hProcess, evt };

	//The return from WaitForMultipleObjects is WAIT_OBJECT_0 + 1 if the event is signaled first
	//and WAIT_OBJECT_0 if the process ends without signaling the event, indicating it failed
	//at changing the folder permissions.
	BOOL res = WaitForMultipleObjects(2, wait_objects, FALSE, INFINITE) == (WAIT_OBJECT_0 + 1);

	CloseHandle(info.hProcess);

	if  (!res)
	{
		m_opera_installer_ui->ShowError(CANT_GET_FOLDER_WRITE_PERMISSIONS);
		CloseHandle(evt);
	}
	else
		//We reuse the event to notify the elevated process that we are done using the folder
		//So it can reset the permissions on it.
		m_get_write_permission_event = evt;

	return res?OpStatus::OK:OpStatus::ERR;
}

void OperaInstaller::GiveFolderWriteAccess()
{
	CommandLineManager* cl_manager = CommandLineManager::GetInstance();

	if (!(cl_manager->GetArgument(CommandLineManager::OWIGiveFolderWriteAccess) && cl_manager->GetArgument(CommandLineManager::OWIGiveWriteAccessTo)))
		return;

	//Ensure we can communicate
	HANDLE evt = OpenEvent(EVENT_ALL_ACCESS, FALSE, UNI_L("OperaInstallerGetWritePermission"));
	if (!evt)
		return;

	//Counterpart of the hack in GetWritePermission
	OpString& folder_path = cl_manager->GetArgument(CommandLineManager::OWIGiveFolderWriteAccess)->m_string_value;
	BOOL permanent = folder_path[0] == '*';
	if (permanent)
		folder_path.Delete(0,1);

	OpFile folder;
	RETURN_VOID_IF_ERROR(folder.Construct(folder_path));
	RETURN_VOID_IF_ERROR(folder.MakeDirectory());

	PSECURITY_DESCRIPTOR sec_desc;
	PACL folder_dacl;
	PACL new_folder_dacl;
	EXPLICIT_ACCESS explicit_access;

	if (GetNamedSecurityInfo(folder_path.CStr(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &folder_dacl, NULL, &sec_desc) != ERROR_SUCCESS)
		return;

	BuildExplicitAccessWithName(&explicit_access, cl_manager->GetArgument(CommandLineManager::OWIGiveWriteAccessTo)->m_string_value.CStr(), GENERIC_ALL, GRANT_ACCESS, SUB_CONTAINERS_AND_OBJECTS_INHERIT);

	do
	{
		if (SetEntriesInAcl(1, &explicit_access, folder_dacl, &new_folder_dacl) != ERROR_SUCCESS)
			break;

		DWORD res = SetNamedSecurityInfo(folder_path.CStr(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, new_folder_dacl, NULL);
		LocalFree(new_folder_dacl);

		if (res != ERROR_SUCCESS)
			break;

		//If permanent was set, we can just stop after signaling success since we don't need to revert permission changes.
		if (permanent)
		{
			PulseEvent(evt);
			break;
		}

		if (PulseEvent(evt))
		{
			//Wait until we know that the isntaller process is done using the folder.
			WaitForSingleObject(evt, INFINITE);
			//Give time to uninstaller to do final cleanup
			Sleep(10000);
		}

		//Revert the permissions
		SetNamedSecurityInfo(folder_path.CStr(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, folder_dacl, NULL);
	}
	while (FALSE);


	LocalFree(sec_desc);
}

OP_STATUS OperaInstaller::ReadFilesList()
{
	OpFile files_list;
	INST_HANDLE_ERROR(INIT_FILES_LIST_FAILED, NULL, files_list.Construct(UNI_L("files_list"), OPFILE_RESOURCES_FOLDER));

	BOOL exists = FALSE;
	if (OpStatus::IsError(files_list.Exists(exists)) || ! exists)
	{
		m_opera_installer_ui->ShowError(FILES_LIST_MISSING);
		return OpStatus::ERR;
	}

	files_list.Open(OPFILE_READ);

	OpString8 file8;
	OpString* file;

	//Each line of the files list file is a file name with relative path
	//We store all of them in m_files_list
	while (!files_list.Eof())
	{
		INST_HANDLE_ERROR(FILES_LIST_READ_FAILED, NULL, files_list.ReadLine(file8));
		RETURN_OOM_IF_NULL(file = OP_NEW(OpString, ()));
		INST_HANDLE_ERROR(FILES_LIST_READ_FAILED, NULL, file->SetFromUTF8(file8));
		file->Strip();

		if (file->HasContent())
			INST_HANDLE_ERROR(FILES_LIST_READ_FAILED, NULL, m_files_list.Add(file));
		else
			OP_DELETE(file);
	}

	OpStatus::Ignore(files_list.Close());

	return OpStatus::OK;

}

OP_STATUS OperaInstaller::FindAndRemoveMSIInstaller(BOOL &found)
{
	// Open the registry keys that we need to search.
	HKEY uninstall_key;

	DWORD n_subkeys;
	DWORD max_key_name_length;

	found = FALSE;
	const uni_char* uninstall_key_path = UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\");

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, uninstall_key_path, 0, KEY_READ, &uninstall_key) != ERROR_SUCCESS ||
		RegQueryInfoKey(uninstall_key, NULL, NULL, NULL, &n_subkeys, &max_key_name_length, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
	{
		m_opera_installer_ui->ShowError(CANT_READ_MSI_UNINSTALL_KEY);
		return OpStatus::ERR;
	}

	OpString key_name;
	if (!key_name.Reserve(max_key_name_length))
	{
		m_opera_installer_ui->ShowError(INIT_MSI_UNINSTALL_KEY_FAILED);
		return OpStatus::ERR_NO_MEMORY;
	}

	OpString install_folder_with_pathsep;
	INST_HANDLE_ERROR(INIT_MSI_UNINSTALL_FOLDER_FAILED, NULL, install_folder_with_pathsep.Set(m_install_folder));
	INST_HANDLE_ERROR(INIT_MSI_UNINSTALL_FOLDER_FAILED, NULL, install_folder_with_pathsep.Append(PATHSEP));

	// Go through the list of keys representing applications that can be uninstalled and
	// see if we can find an MSI package that was installed in our folder.
	for (UINT32 i=0;i<n_subkeys;i++)
	{
		DWORD name_length = max_key_name_length+1;

		if (RegEnumKeyEx(uninstall_key, i, key_name.CStr(), &name_length, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
			break;

		//Skip keys that were created by this installer
		if (key_name.Compare(UNI_L("Opera "), 6) == 0)
			continue;

		OpString app_location;
		//Check that the folder is our folder
		if (OpStatus::IsError(OpSafeRegQueryValue(uninstall_key, key_name.CStr(), app_location, UNI_L("InstallLocation"))) || (app_location.CompareI(m_install_folder) != 0 && app_location.CompareI(install_folder_with_pathsep) != 0))
			continue;

		OpString app_publisher;
		//Check that it is an Opera installation
		if (OpStatus::IsError(OpSafeRegQueryValue(uninstall_key, key_name.CStr(), app_publisher, UNI_L("Publisher"))) || app_publisher.CompareI(UNI_L("Opera Software ASA")) != 0)
			continue;

		found = TRUE;

		HKEY survey_key;
		BOOL remove_survey_value_again = FALSE;

		// If the appropriate key could be opened with query and set access And a Surveyed entry is NOT found, set it to be sure to not show the survey.
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Opera Software"), NULL, (KEY_QUERY_VALUE | KEY_SET_VALUE), &survey_key) == ERROR_SUCCESS) 
		{
			if (RegQueryValueEx(survey_key, UNI_L("Surveyed"), NULL, NULL, NULL, 0) == ERROR_FILE_NOT_FOUND)
			{
				RegSetValueEx(survey_key, UNI_L("Surveyed"), 0, REG_SZ, (LPBYTE)("Yes"), 4);
				remove_survey_value_again = TRUE;
			}
			RegCloseKey(survey_key);
		}

		//On windows XP, the MSI installer can be installed on a limited account. Check if this is the case.
		//If it is, the MSI uninstaller will not automatically run with higher permissions, so we should start
		//it with the "runas" verb
		BOOL should_elevate = FALSE;
		if (!WindowsUtils::IsPrivilegedUser(FALSE) && !IsSystemWinVista())
		{
			//Build the path to the product code key for this particular installation
			OpString product_code_key;
			WindowsUtils::GetOwnStringSID(product_code_key);
			product_code_key.Insert(0, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\");
			product_code_key.Append("\\Products\\");

			uni_char* c = key_name.CStr();

			for (UINT i = 0; i < 3; i++)
			{
				OpString product_code_part;
				while (*(++c) != '-')
					product_code_part.Insert(0, c, 1);

				product_code_key.Append(product_code_part);
			}

			while(*c)
			{
				while (*(++c) != '-' && *c && *(++c) != '-' && *c)
				{
					product_code_key.Append(c, 1);
					product_code_key.Append(c - 1, 1);
				}
			}

			HKEY key;
			//If the product code key is found in LOCAL_MACHINE, then  everything is ok. Otherwise, we'll need to
			//manually request more permission.
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, product_code_key.CStr(), 0, KEY_READ, &key) == ERROR_SUCCESS)
				RegCloseKey(key);
			else
				should_elevate = TRUE;
		}

		INST_HANDLE_ERROR(INIT_MSI_UNINSTALL_ARGS_FAILED, NULL, key_name.Insert(0, "/qn /qb! /X "));

		//Run the MSI uninstaller
		SHELLEXECUTEINFO info;
		op_memset(&info, 0, sizeof(info));

		info.cbSize = sizeof(info);
		info.fMask = SEE_MASK_NOCLOSEPROCESS;
		info.lpFile = UNI_L("msiexec.exe");
		info.lpParameters = key_name.CStr();
		info.nShow = SW_SHOW;

		if (should_elevate)
			info.lpVerb = UNI_L("runas");

		OP_STATUS result = OpStatus::OK;
		if (!ShellExecuteEx(&info) || (WaitForSingleObject(info.hProcess, INFINITE) == WAIT_FAILED))
		{
			m_opera_installer_ui->ShowError(MSI_UNINSTALL_SHELLEXECUTE_FAILED);
			result = OpStatus::ERR;
		}

		OpAutoHANDLE inst_process(info.hProcess);

		//Restore the survey setting if we changed it earlier
		if (remove_survey_value_again && RegOpenKeyEx(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), NULL, (KEY_QUERY_VALUE | KEY_SET_VALUE), &survey_key) == ERROR_SUCCESS)
		{
			RegDeleteValue(survey_key, UNI_L("Surveyed"));
			RegCloseKey(survey_key);
		}

		//If somehow we can't get the exit code, just act as if it succeeded
		DWORD exit_code;
		if (!GetExitCodeProcess(inst_process, &exit_code))
			return result;

		switch (exit_code)
		{
			case ERROR_SUCCESS:
			case ERROR_PRODUCT_UNINSTALLED:
				//Uninstallation succeeded. Just stop here.
				break;

			case ERROR_INSTALL_SOURCE_ABSENT:
			case ERROR_UNKNOWN_PRODUCT:
				//Uninstallation failed because of missing data on the
				//MSI side of things. Let's just pretend that we didn't find this entry,
				//since there might be other installations in that folder.
				//This is probably caused by duplicate entries we had issues
				//with in previous versions.
				found = FALSE;
				break;

			case ERROR_INSTALL_SUSPEND:
			case ERROR_SUCCESS_REBOOT_INITIATED:
			case ERROR_SUCCESS_REBOOT_REQUIRED:
			{
				//The MSI uninstaller requested to reboot for completing its task.
				//We should also prompt for reboot and be ready to resume installation
				//after rebooting.
				result = OpStatus::ERR;

				//For resuming installation after rebooting, we write an entry in runonce.
				//The entry needs to point to the 7z sfx archive that this process is launched from,
				//since this executable is deleted by the sfx module once it terminates.
				//Thankfully, we can find the path to that sfx archive by knowing that it launched 
				//the installer process and is thus its parent. So, it is the parent of this process or
				//if we had to run the installer in an elevated process, it is the parent of the parent
				//of this process.
				//So, we will examine both this process's parent and its parent and find out which we want.
				HandleInfo h;
				OpAutoPtr<ProcessItem> parent_process;
				OpString proc_path;
				//If we can't find a parent that fits our expectations, just default to our own path.
				INST_HANDLE_ERROR(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED, NULL, proc_path.Set(g_spawnPath));
				INST_HANDLE_ERROR(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED, NULL, h.Init());
				INST_HANDLE_ERROR(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED, NULL, h.GetParentProcessItem(GetCurrentProcessId(), parent_process));

				do
				{
					if (!parent_process.get() || !parent_process->GetProcessPath())
						break;

					//The parent has the same path as this process. So we want to look at its parent instead.
					if (g_spawnPath.CompareI(parent_process->GetProcessPath()) == 0)
					{
						DWORD PID = parent_process->GetProcessID();
						parent_process.reset();
						INST_HANDLE_ERROR(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED, NULL, h.GetParentProcessItem(PID, parent_process));
					}

					if (!parent_process.get() || !parent_process->GetProcessPath())
						break;

					OpAutoPtr<LaunchPI> launch_pi = LaunchPI::Create();
					if (!launch_pi.get())
					{
						m_opera_installer_ui->ShowError(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED);
						return OpStatus::ERR_NO_MEMORY;
					}

					//Check that the process we found is signed by us. If it isn't, it can't be the
					//SFX archive and we can give up.
					if (!launch_pi->VerifyExecutable(parent_process->GetProcessPath()))
						break;

					//Set the path of the parent we found as the one to run
					INST_HANDLE_ERROR(GET_INSTALLER_PATH_FOR_RUNONCE_FAILED, NULL, proc_path.Set(parent_process->GetProcessPath()));
				}
				while (false);

				//runonce will refuse to launch files that look like they are installers.
				//We trick it into doing this anyway by writing the actual command in a batch file
				//in the installation folder and make the runonce entry point to that one instead.
				OpString run_once_string;
				//Add the command line arguments to the path we just determined, then write the rest of the batch file.
				INST_HANDLE_ERROR(INIT_RUNONCE_STRING, NULL, GetRerunArgs(run_once_string));
				INST_HANDLE_ERROR(INIT_RUNONCE_STRING, NULL, run_once_string.Insert(0, "\" "));
				INST_HANDLE_ERROR(INIT_RUNONCE_STRING, NULL, run_once_string.Insert(0, proc_path));
				INST_HANDLE_ERROR(INIT_RUNONCE_STRING, NULL, run_once_string.Insert(0, "@echo off\r\n\""));
				INST_HANDLE_ERROR(INIT_RUNONCE_STRING, NULL, run_once_string.Append("\r\ndel %0\r\n"));

				//Write the batch file.
				OpFile runonce_file;
				OpString runonce_file_path;
				INST_HANDLE_ERROR(WRITE_RUNONCE_FILE, NULL, runonce_file_path.Set(m_install_folder));
				INST_HANDLE_ERROR(WRITE_RUNONCE_FILE, NULL, runonce_file_path.Append(PATHSEP "rerun.bat"));
				INST_HANDLE_ERROR(WRITE_RUNONCE_FILE, NULL, runonce_file.Construct(runonce_file_path));
				INST_HANDLE_ERROR(WRITE_RUNONCE_FILE, NULL, runonce_file.Open(OPFILE_WRITE));
				INST_HANDLE_ERROR(WRITE_RUNONCE_FILE, NULL, runonce_file.WriteUTF8Line(run_once_string));
				runonce_file.Close();

				//Write the RunOnce entry
				OpAutoHKEY key;
				if (RegOpenKeyEx(m_settings.all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER, UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce"), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
				{
					m_opera_installer_ui->ShowError(SET_RUNONCE_FAILED);
					break;
				}

				if (RegSetValueEx(key, UNI_L("Opera"), NULL, REG_SZ,(BYTE*)(runonce_file_path.CStr()), (runonce_file_path.Length()+1)*sizeof(uni_char) ) != ERROR_SUCCESS)
					m_opera_installer_ui->ShowError(SET_RUNONCE_FAILED);

				//Try to obtain the privilege to initiate a reboot, prompt the user and reboot if we can.
				BOOL has_rights = OpStatus::IsSuccess(WindowsUtils::SetPrivilege(SE_SHUTDOWN_NAME, TRUE));

				BOOL reboot;
				m_opera_installer_ui->ShowAskReboot(has_rights, reboot);

				if (reboot)
					ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED);

				break;
			}

			default:
				//Uninstallation failed with an unexpected error. Just warn the user and die.
				OpString error;
				OpStatus::Ignore(error.AppendFormat(UNI_L("%d"), exit_code));
				m_opera_installer_ui->ShowError(MSI_UNINSTALL_FAILED, NULL, error.CStr());
				result = OpStatus::ERR;
		}

		if(found)
		{
			RegCloseKey(uninstall_key);
			return result;
		}
	}
	RegCloseKey(uninstall_key);

	return OpStatus::OK;

}

OP_STATUS OperaInstaller::FindAndRemoveWiseInstaller(BOOL &found)
{
	BOOL exist;
	OpString path;
	OpFile file;
	OpString log_path;
	OpFile log_file;

	found = FALSE;

	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, path.Set(m_install_folder));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, path.Append(UNI_L("\\opera.dll")));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, file.Construct(path));
	if ( OpStatus::IsError(file.Exists(exist)) || !exist )
		return OpStatus::OK;

	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, path.Set(m_install_folder));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, path.Append(UNI_L("\\uninst\\unwise.exe")));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_PATH_FAILED, NULL, file.Construct(path));
	if ( OpStatus::IsError(file.Exists(exist)) || !exist )
		return OpStatus::OK;

	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_ARGS_FAILED, NULL, log_path.Set(m_install_folder));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_ARGS_FAILED, NULL, log_path.Append(UNI_L("\\uninst\\install.log")));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_ARGS_FAILED, NULL, log_file.Construct(log_path));
	if ( OpStatus::IsError(log_file.Exists(exist)) || !exist )
		return OpStatus::OK;

	found = TRUE;

	DWORD short_path_len = GetShortPathName(log_path.CStr(), NULL, 0);
	if (short_path_len == 0)
	{
		m_opera_installer_ui->ShowError(INIT_WISE_UNINSTALL_ARGS_FAILED);
		return OpStatus::ERR;
	}
	uni_char* short_log_path = OP_NEWA(uni_char, short_path_len);
	if (!short_log_path)
	{
		m_opera_installer_ui->ShowError(INIT_WISE_UNINSTALL_ARGS_FAILED);
		return OpStatus::ERR_NO_MEMORY;
	}
	if (GetShortPathName(log_path.CStr(), short_log_path, short_path_len) == 0)
	{
		OP_DELETEA(short_log_path);
		m_opera_installer_ui->ShowError(INIT_WISE_UNINSTALL_ARGS_FAILED);
		return OpStatus::ERR;
	}

	OpString args;

	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_ARGS_FAILED, NULL, args.Set(UNI_L("/A /S /Z ")));
	INST_HANDLE_ERROR(INIT_WISE_UNINSTALL_ARGS_FAILED, NULL, args.Append(short_log_path));

	OP_DELETEA(short_log_path);

	SHELLEXECUTEINFO info;
	op_memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpFile = path.CStr();
	info.lpParameters = args.CStr();
	info.nShow = SW_SHOW;

	OP_STATUS result = OpStatus::OK;
	if (!ShellExecuteEx(&info) || (WaitForSingleObject(info.hProcess, INFINITE) == WAIT_FAILED))
	{
		m_opera_installer_ui->ShowError(WISE_UNINSTALL_SHELLEXECUTE_FAILED);
		result = OpStatus::ERR;
	}

	CloseHandle(info.hProcess);
	return result;
}

OP_STATUS OperaInstaller::DeleteProfile(UINT level)
{
	OpAutoPtr<OpFile> prefs;

	RETURN_OOM_IF_NULL(s_delete_profile_files = OP_NEW(OpVector<OpFile>, ()));

	if (level == FULL_PROFILE)
	{
		//Prepare to entirely remove the profile folders.
		prefs.reset(OP_NEW(OpFile,()));
		RETURN_OOM_IF_NULL(prefs.get());
		RETURN_IF_ERROR(prefs->Construct(UNI_L(""), OPFILE_HOME_FOLDER));
		RETURN_IF_ERROR(s_delete_profile_files->Add(prefs.get()));
		prefs.release();

		prefs.reset(OP_NEW(OpFile,()));
		RETURN_OOM_IF_NULL(prefs.get());
		RETURN_IF_ERROR(prefs->Construct(UNI_L(""), OPFILE_LOCAL_HOME_FOLDER));
		RETURN_IF_ERROR(s_delete_profile_files->Add(prefs.get()));
		prefs.release();

		return OpStatus::OK;
	}
	
	OpINT32Vector folders_list;
	OpINT32Vector files_list;

	//Prepares the list of files and folders to remove.
	if (level & REMOVE_GENERATED)
	{
		OpStatus::Ignore(folders_list.Add(OPFILE_CACHE_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_APP_CACHE_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_OCACHE_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_VPS_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_DICTIONARY_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_ICON_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_THUMBNAIL_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_TEMPDOWNLOAD_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_BITTORRENT_METADATA_FOLDER));

		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::DirectHistoryFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::GlobalHistoryFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::ConsoleErrorLogName));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::DownloadedOverridesFile));
	}

	if (level & REMOVE_PASSWORDS)
	{
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::WandFile));
	}

	if (level & REMOVE_CUSTOMIZATION)
	{
		OpStatus::Ignore(folders_list.Add(OPFILE_SKIN_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_BUTTON_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_MENUSETUP_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_TOOLBARSETUP_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_MOUSESETUP_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_KEYBOARDSETUP_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_GADGET_FOLDER));
		OpStatus::Ignore(folders_list.Add(OPFILE_WEBFEEDS_FOLDER));

		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::OverridesFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::DialogConfig));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::FastForwardFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::UserStyleIniFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::GadgetListFile));
	}

	if (level & REMOVE_EMAIL)
	{
		OpStatus::Ignore(folders_list.Add(OPFILE_MAIL_FOLDER));
	}

	if (level & REMOVE_BOOKMARKS)
	{
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::HotListFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::NoteListFile));
		OpStatus::Ignore(files_list.Add(PrefsCollectionFiles::ContactListFile));
	}

	if (level & REMOVE_UNITE_APPS)
	{
		OpStatus::Ignore(folders_list.Add(OPFILE_WEBSERVER_FOLDER));
	}

	//Gets the path of the files and folders we listed and adds those to the list of paths to remove.
	for(UINT i = 0; i < folders_list.GetCount(); i++)
	{
		prefs.reset(OP_NEW(OpFile,()));
		RETURN_OOM_IF_NULL(prefs.get());
		if (OpStatus::IsSuccess(prefs->Construct(UNI_L(""), (OpFileFolder)folders_list.Get(i))))
		{
			OpStatus::Ignore(s_delete_profile_files->Add(prefs.get()));
			prefs.release();
		}
	}

	for(UINT i = 0; i < files_list.GetCount(); i++)
	{
		prefs.reset(OP_NEW(OpFile,()));
		RETURN_OOM_IF_NULL(prefs.get());
		if (OpStatus::IsSuccess(prefs->Copy(g_pcfiles->GetFile((PrefsCollectionFiles::filepref)files_list.Get(i)))))
		{
			RETURN_IF_ERROR(s_delete_profile_files->Add(prefs.get()));
			prefs.release();
		}
	}

	return OpStatus::OK;

}

void OperaInstaller::DeleteProfileEffective()
{
	//Goes through the list of paths prepared by DeleteProfile and remove them all
	if (s_delete_profile_files != NULL)
	{
		for (UINT32 i = 0; s_delete_profile_files->Get(i) != NULL; i++)
		{
			s_delete_profile_files->Get(i)->Delete();
		}
		s_delete_profile_files->DeleteAll();

	}
}

BOOL OperaInstaller::HasAssociation(AssociationsSupported assoc)
{
	//QueryAppIsDefault doesn't work on Win8.
	if (IsSystemWinVista() && m_settings.all_users && !IsSystemWin8())
	{
		IApplicationAssociationRegistration* pAAR;
		BOOL pfDefault = FALSE;

		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			pAAR->QueryAppIsDefault(m_check_assoc_keys[assoc], (m_check_assoc_keys[assoc][0] == '.')?AT_FILEEXTENSION:AT_URLPROTOCOL, AL_EFFECTIVE, m_app_reg_name.CStr(), &pfDefault);
			pAAR->Release();
		}

		return pfDefault;
	}

	const uni_char* type = m_check_assoc_keys[assoc];
	OpString check_key;
	OpString value;

	OP_STATUS err = OpStatus::OK;

	if (IsSystemWinVista())
	{
		RETURN_VALUE_IF_ERROR(check_key.Set(type), FALSE);
		RETURN_VALUE_IF_ERROR(check_key.Append("\\UserChoice"), FALSE);
		if (type[0] == '.')
			RETURN_VALUE_IF_ERROR(check_key.Insert(0, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\"), FALSE);
		else
			RETURN_VALUE_IF_ERROR(check_key.Insert(0, "Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"), FALSE);

		err = OpSafeRegQueryValue(HKEY_CURRENT_USER, check_key, value, UNI_L("ProgId"));
		if (!IsSystemWin8() && err != OpStatus::ERR_FILE_NOT_FOUND)
			return FALSE;
	}

	if(IsSystemWin8() && err == OpStatus::OK)
		check_key.Set(value);
	else if (type[0] == '.')
		RETURN_VALUE_IF_ERROR(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, type, check_key), FALSE);
	else
		RETURN_VALUE_IF_ERROR(check_key.Set(type), FALSE);

	RETURN_VALUE_IF_ERROR(check_key.Append("\\shell\\open\\command"), FALSE);

	RETURN_VALUE_IF_ERROR(OpSafeRegQueryValue(HKEY_CLASSES_ROOT, check_key, value), FALSE);

	return value.FindI(g_spawnPath) != KNotFound;
}

BOOL OperaInstaller::IsDefaultBrowser()
{
	if (IsSystemWinVista() && m_settings.all_users)
	{
		IApplicationAssociationRegistration* pAAR;
		BOOL pfDefault = FALSE;

		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			pAAR->QueryAppIsDefault(UNI_L("StartmenuInternet"), AT_STARTMENUCLIENT, AL_EFFECTIVE, m_app_reg_name.CStr(), &pfDefault);
			pAAR->Release();
		}

		return pfDefault;
	}

	OpString client_name;
	RETURN_VALUE_IF_ERROR(client_name.Set("Opera"), FALSE);
	RETURN_VALUE_IF_ERROR(client_name.Append(m_product_name), FALSE);

	OpString value;
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Clients\\StartMenuInternet"), value)))
		return (value.CompareI(client_name.CStr()) == 0);

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("Software\\Clients\\StartMenuInternet"), value)))
		return (value.CompareI(client_name.CStr()) == 0);

	return FALSE;
}

BOOL OperaInstaller::IsDefaultMailer()
{
	if (IsSystemWinVista() && m_settings.all_users)
	{
		IApplicationAssociationRegistration* pAAR;
		BOOL pfDefault = FALSE;

		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			pAAR->QueryAppIsDefault(UNI_L("Mail"), AT_STARTMENUCLIENT, AL_EFFECTIVE, m_app_reg_name.CStr(), &pfDefault);
			pAAR->Release();
		}

		return pfDefault;
	}

	OpString client_name;
	RETURN_VALUE_IF_ERROR(client_name.Set("Opera"), FALSE);
	RETURN_VALUE_IF_ERROR(client_name.Append(m_product_name), FALSE);

	OpString value;
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Clients\\Mail"), value)))
		return (value.CompareI(client_name.CStr()) == 0);

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("Software\\Clients\\Mail"), value)))
		return (value.CompareI(client_name.CStr()) == 0);

	return FALSE;
}

OP_STATUS OperaInstaller::SetDefaultBrowserAndAssociations(BOOL all_users)
{
	if (IsSystemWin8())
	{
		if (!all_users)
		{
			OPENASINFO open_as_info = {};
			open_as_info.pcszFile = UNI_L("http");
			open_as_info.oaifInFlags = OAIF_URL_PROTOCOL | OAIF_FORCE_REGISTRATION | OAIF_REGISTER_EXT;
			HRESULT hr = OPSHOpenWithDialog(NULL, &open_as_info);

			if (FAILED(hr))
				return OpStatus::ERR;
		}
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(RestoreFileHandlers(all_users));

	RETURN_IF_ERROR(BecomeDefaultBrowser(all_users));
	RETURN_IF_ERROR(AssociateType(HTM, all_users));
	RETURN_IF_ERROR(AssociateType(HTML, all_users));
	RETURN_IF_ERROR(AssociateType(XHT, all_users));
	RETURN_IF_ERROR(AssociateType(XHTM, all_users));
	RETURN_IF_ERROR(AssociateType(XHTML, all_users));
	RETURN_IF_ERROR(AssociateType(MHT, all_users));
	RETURN_IF_ERROR(AssociateType(MHTML, all_users));
	RETURN_IF_ERROR(AssociateType(HTTP, all_users));
	RETURN_IF_ERROR(AssociateType(HTTPS, all_users));

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::SetDefaultMailerAndAssociations(BOOL all_users)
{
	RETURN_IF_ERROR(RestoreFileHandlers(all_users));

	RETURN_IF_ERROR(BecomeDefaultMailer(all_users));
	RETURN_IF_ERROR(AssociateType(MAILTO, all_users));

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::SetNewsAssociations(BOOL all_users)
{
	RETURN_IF_ERROR(RestoreFileHandlers(all_users));

	RETURN_IF_ERROR(AssociateType(NNTP, all_users));
	RETURN_IF_ERROR(AssociateType(NEWS, all_users));
	RETURN_IF_ERROR(AssociateType(SNEWS, all_users));

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::RestoreFileHandlers(BOOL all_users)
{
	HKEY root_key = all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER;
	UINT reg_op_count;

	reg_op_count = sizeof(m_file_handlers_regops)/sizeof(RegistryOperation);

	for (UINT i = 0; i < reg_op_count; i++)
	{
		OpString key_path;
		RETURN_IF_ERROR(key_path.Set(m_file_handlers_regops[i].key_path));

		RETURN_IF_ERROR(DoRegistryOperation(root_key, m_file_handlers_regops[i], key_path, NULL, NULL));
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::AssociateType(AssociationsSupported assoc, BOOL all_users)
{
	if (IsSystemWinVista() && m_settings.all_users && !all_users)
	{
		IApplicationAssociationRegistration* pAAR;

		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			hr = pAAR->SetAppAsDefault(m_app_reg_name.CStr(), m_check_assoc_keys[assoc], (m_check_assoc_keys[assoc][0] == '.')?AT_FILEEXTENSION:AT_URLPROTOCOL);
			pAAR->Release();
		}

		return SUCCEEDED(hr)?OpStatus::OK:OpStatus::ERR;
	}

	const RegistryOperation* assoc_regops;
	assoc_regops = m_association_regops[assoc];

	HKEY root_key = all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER;

	UINT i = 0;
	while (assoc_regops[i].key_path != NULL)
	{
		OpString key_path;
		RETURN_IF_ERROR(key_path.Set(assoc_regops[i].key_path));

		RETURN_IF_ERROR(DoRegistryOperation(root_key, assoc_regops[i], key_path, NULL, NULL));

		i++;
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::BecomeDefaultBrowser(BOOL all_users)
{
	if (IsSystemWinVista() && m_settings.all_users && !all_users)
	{
		IApplicationAssociationRegistration* pAAR;

		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
		if (SUCCEEDED(hr))
		{
			hr = pAAR->SetAppAsDefault(m_app_reg_name.CStr(), UNI_L("StartmenuInternet"), AT_STARTMENUCLIENT);
			pAAR->Release();
		}
		return SUCCEEDED(hr)?OpStatus::OK:OpStatus::ERR;
	}

	OpString key_path;
	RETURN_IF_ERROR(key_path.Set(m_default_browser_start_menu_regop.key_path));
	RETURN_IF_ERROR(DoRegistryOperation(all_users?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER, m_default_browser_start_menu_regop, key_path, NULL, NULL));

	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\StartMenuInternet"));
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::BecomeDefaultMailer(BOOL all_users)
{
	if (IsSystemWinVista() && m_settings.all_users && !all_users)
 	{
 		IApplicationAssociationRegistration* pAAR;

 		HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration), (void**)&pAAR);
 		if (SUCCEEDED(hr))
 		{
 			hr = pAAR->SetAppAsDefault(m_app_reg_name.CStr(), UNI_L("Mail"), AT_STARTMENUCLIENT);
 			pAAR->Release();
 		}
 		return SUCCEEDED(hr)?OpStatus::OK:OpStatus::ERR;
 	}

	OpString key_path;
	RETURN_IF_ERROR(key_path.Set(m_default_mailer_start_menu_regop.key_path));
	RETURN_IF_ERROR(DoRegistryOperation(all_users ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, m_default_mailer_start_menu_regop, key_path, NULL, NULL));

	SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)UNI_L("Software\\Clients\\Mail"));
	return OpStatus::OK;
}

OP_STATUS OperaInstaller::DoRegistryOperation(HKEY root_key, const RegistryOperation operation, OpStringC &key_path, OpTransaction *transaction, OperaInstallLog *log)
{
	//This method creates a regular Undoable operation then adds it to a transaction if transaction is given, otherwise it just calls Do on the operation

	//Skip the operation if it's not for our OS version
	if (GetWinType() < operation.min_os_version)
		return OpStatus::OK;

	//Skip the operation if we are operating on a root key to which it shouldn't apply
	if (operation.root_key_limit && root_key != operation.root_key_limit)
		return OpStatus::OK;

	//Skip the operation if it's not used for this product
	if (!(operation.product_limit & m_product_flag))
		return OpStatus::OK;

	static UINT reg_backup_count = 0;

	//Key deletion is a special case as it uses a DeleteRegKeyOperation
	if (operation.value_type == REG_DEL_KEY)
	{
		OpString backup_file_name;
		backup_file_name.Empty();
		RETURN_IF_ERROR(backup_file_name.AppendFormat(UNI_L("reg_backup_%d.bak"), reg_backup_count++));
		OpFile backup_file;
		RETURN_IF_ERROR(backup_file.Construct(backup_file_name, OPFILE_RESOURCES_FOLDER));

		OpAutoPtr<OpUndoableOperation> transaction_operation(OP_NEW(DeleteRegKeyOperation, (root_key, key_path)));
		RETURN_OOM_IF_NULL(transaction_operation.get());

		if (transaction)
			RETURN_IF_ERROR(transaction->Continue(transaction_operation));
		else
			RETURN_IF_ERROR(transaction_operation->Do());
	}
	else
	{	
		//Readies the BYTE* structure containing the value data to write.
		BYTE* value_data = (BYTE*)(operation.value_data);
		DWORD value_data_size;
		OpString expand_value;

		switch (operation.value_type)
		{
			case REG_DELETE:
			case REG_NONE:
				value_data = NULL;
				value_data_size = 0;
				break;

			case REG_DWORD:
				//DWORD values are specified as the actual value, casted as a void pointer in the RegistryOperation structure
				//We need to create a new DWORD with that value and cast it to BYTE*
				RETURN_OOM_IF_NULL(value_data = (BYTE*)(OP_NEW(DWORD, ((DWORD)operation.value_data))));
				value_data_size = sizeof(DWORD);
				break;

			case REG_SZ:
			case REG_EXPAND_SZ:
			{
				//Do the relevant replacement of placeholders in string types
				value_data_size = 0;
				RETURN_IF_ERROR(expand_value.Set((uni_char*)(operation.value_data)));
				RETURN_IF_ERROR(ExpandPlaceholders(expand_value));
				break;
			}

			case REG_BINARY:
				value_data_size = (strlen((char*)operation.value_data)) * sizeof(char);
				break;

			default:
				return OpStatus::ERR;
		}

		OpUndoableOperation* transaction_operation;
		OP_STATUS status = OpStatus::OK;

		//Replace placeholders in the key path and value name
		OpString expand_key;
		RETURN_IF_ERROR(expand_key.Set(key_path));
		RETURN_IF_ERROR(ExpandPlaceholders(expand_key));

		OpString expand_value_name;
		RETURN_IF_ERROR(expand_value_name.Set(operation.value_name));
		RETURN_IF_ERROR(ExpandPlaceholders(expand_value_name));

		//Make the actual ChangeRegValueOperation
		if (expand_value.IsEmpty())
			transaction_operation = OP_NEW(ChangeRegValueOperation, (root_key, expand_key, expand_value_name, value_data, operation.value_type, value_data_size));
		else
			transaction_operation = OP_NEW(ChangeRegValueOperation, (root_key, expand_key, expand_value_name, (BYTE*)expand_value.CStr(), operation.value_type, (expand_value.Length()+1)*sizeof(uni_char) ));

		RETURN_OOM_IF_NULL(transaction_operation);

		if (transaction)
			status = transaction->Continue(OpAutoPtr<OpUndoableOperation>(transaction_operation));
		else
			status = transaction_operation->Do();

		if (operation.value_type == REG_DWORD)
			OP_DELETE((DWORD*)value_data);

		RETURN_IF_ERROR(status);

		if (log && operation.value_type != REG_DELETE)
		{
			RegistryKey log_key;
			RETURN_IF_ERROR(log_key.Copy(key_path, operation));
			if (expand_value.HasContent())
				RETURN_IF_ERROR(log_key.SetValueData(expand_value.CStr()));
			RETURN_IF_ERROR(log->SetRegistryKey(log_key));
		}
	}

	return OpStatus::OK;
}

OP_STATUS OperaInstaller::ExpandPlaceholders(OpString& string)
{
		OpString resources_folder;
		RETURN_IF_ERROR(resources_folder.Set(m_install_folder));
		RETURN_IF_ERROR(resources_folder.Append(PATHSEP));

		int pos;

		//The installation folder
		pos = string.Find(UNI_L("{Resources}"));
		if (pos != KNotFound)
		{
			string.Delete(pos, 11);
			RETURN_IF_ERROR(string.Insert(pos, resources_folder));
		}
		//The product name
		pos = string.Find(UNI_L("{Product}"));
		if (pos != KNotFound)
		{
			string.Delete(pos, 9);
			if (m_product_name.HasContent())
			{
				RETURN_IF_ERROR(string.Insert(pos, m_product_name));
			}
		}
		//The product name, preceded by a space
		pos = string.Find(UNI_L("{SPProduct}"));
		if (pos != KNotFound)
		{
			string.Delete(pos, 11);
			if (m_product_name.HasContent())
			{
				RETURN_IF_ERROR(string.Insert(pos, m_product_name));
				RETURN_IF_ERROR(string.Insert(pos, " "));
			}
		}
		//The long product name
		pos = string.Find(UNI_L("{ProductLong}"));
		if (pos != KNotFound)
		{
			string.Delete(pos, 13);
			if (m_long_product_name.HasContent())
			{
				RETURN_IF_ERROR(string.Insert(pos, m_long_product_name));
				RETURN_IF_ERROR(string.Insert(pos, " "));
			}
		}
		//The icon index to be used in the dll
		pos = string.Find(UNI_L("{ProductIconNr}"));
		if (pos != KNotFound)
		{
			string.Delete(pos, 15);
			OpString number;
			number.AppendFormat("%u", m_product_icon_nr);
			RETURN_IF_ERROR(string.Insert(pos, number));
		}

		return OpStatus::OK;
}

OP_STATUS OperaInstaller::CheckIfFileLocked(const uni_char* full_path)
{
	OP_ASSERT(full_path && uni_strrchr(full_path, '\\'));

	if (!full_path)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS status = OpStatus::ERR;
	OpAutoVector<ProcessItem> processes;

	//
	// If we don't support HandleInfo, there is nothing more we can do
	//
	HandleInfo h;
	BOOL can_use_handle_info = OpStatus::IsSuccess(h.Init());

	//
	// Check if the file is in use
	//
	if (can_use_handle_info && OpStatus::IsError(status))
	{
		status = h.IsFileInUse(full_path, processes, TRUE);

		if (processes.GetCount() > 0)
			status = m_opera_installer_ui->ShowLockingProcesses(full_path, processes);
	}

	//
	// Check if there is an other Opera process running in the destination foler
	//
	if (can_use_handle_info && OpStatus::IsError(status))
	{
		if (!IsOperaRunningAt(full_path))
			status = OpStatus::OK;
	}

	//
	// Check if there is an other user logged in on the machine
	//
	if (OpStatus::IsError(status))
	{
		UINT other_user;
		if (OpStatus::IsSuccess(IsOtherUsersLoggedOn(other_user)) && other_user > 0)
		{
			uni_char err_msg[4] = {0};
			m_opera_installer_ui->ShowError(OTHER_USERS_ARE_LOGGED_ON, uni_itoa(MIN(other_user, 999), err_msg, 10));
		}
	}


	return status;
}

BOOL OperaInstaller::IsOperaRunningAt(const uni_char* full_path)
{
	OpAutoVector<ProcessItem> processes;
	HandleInfo h;

	if (OpStatus::IsError(h.Init()))
		return FALSE;

	// Check if Opera is running at the install path
	if (OpStatus::IsError(h.IsOperaRunningAt(m_install_folder.CStr(), processes, TRUE)))
		return FALSE;

	// If we found one or more processes, show the locking dialog.
	if (processes.GetCount() > 0)
	{
		if (OpStatus::IsError(m_opera_installer_ui->ShowLockingProcesses(m_install_folder.CStr(), processes)))
			return FALSE;

		processes.DeleteAll();

		// Check if Opera is still running at the installation path.
		if (OpStatus::IsError(h.IsOperaRunningAt(m_install_folder.CStr(), processes, TRUE)))
			return FALSE;
	}

	return processes.GetCount() > 0 ? TRUE : FALSE;

}

OP_STATUS OperaInstaller::StartCountryCheckIfNeeded()
{
	if (!m_country_checker && m_update_country_pref && m_country.IsEmpty())
	{
		OpAutoPtr<CountryChecker> country_checker(OP_NEW(CountryChecker, ()));
		RETURN_OOM_IF_NULL(country_checker.get());
		RETURN_IF_ERROR(country_checker->Init(this));
		RETURN_IF_ERROR(country_checker->CheckCountryCode(COUNTRY_CHECK_TIMEOUT));
		m_country_checker = country_checker.release();
	}
	return OpStatus::OK;
}

void OperaInstaller::CountryCheckFinished()
{
	if (m_country_checker)
	{
		if (m_country_check_pause)
		{
			m_country_check_pause = false;
			Pause(FALSE);
		}
		if (m_country_checker->GetStatus() == CountryChecker::CheckSucceded)
		{
			OpStatus::Ignore(m_country.Set(m_country_checker->GetCountryCode()));
		}
	}
}
