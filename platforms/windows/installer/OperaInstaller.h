// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef OPERAINSTALLER_H
#define OPERAINSTALLER_H

#include "platforms/windows/win_handy.h"

#include "adjunct/autoupdate/country_checker.h"

#define REG_DEL_KEY (DWORD)-2

class OpTransaction;
class OperaInstallLog;
class OperaInstallerUI;

/*

The opera Installer class is used for two purposes:
 -running the opera installer
 -keeping track of which extensions and protocols are associated to opera
  and changing those associations.

  For running the installer, an installer object should be construced on the heap
  and the RunWizard method should be called (or Run for non-interactive installation).
  If no error is returned, control can be returned to the message loop and the installer
  installer will manage itself and ultimately delete itself and exit.
  
  When managing extension and protocol associations, the installer can be created on the
  stack and the methods related to associations handling can be called

*/

class OperaInstaller
	: private CountryCheckerListener
{
public:
	/** The constructor will initialize the installer object depending on the options
	 *  present on the command line.
	 *
	 *  If the -install option is present, the installer settings to use are read from the
	 *  command line and determines whether it is going to perform an upgrade.
	 *
	 *  If the -install option isn't present, the object is initialized using the informations
	 *  found in the log file created when installing the current opera instance.
	 */
	OperaInstaller();
	~OperaInstaller();

	/******** Installer run ********/

	/** Starts the (un)installation in a non-interactive manner.
	  *
	  * @return OpStatus::OK if the installation process was successfully initialized.
	  *         OpStatus::ERR if an error was encountered during initialization that would prevent any future call to run to succeed.
	  *         Other error codes, except OOM indicate issues that can be fixed by changing the installation path or other settings.
	  *
	  */
	OP_STATUS Run();

	/** Starts the (un)installation wizard
	  */
	OP_STATUS RunWizard();

	/******** Associations handing ********/

	/** List of files and protocols that Opera can be set to handle.
	  */
	enum AssociationsSupported
	{
		OEX =  0,
		HTM,
		HTML,
		XHT,
		XHTM,
		XHTML,
		MHT,
		MHTML,
		XML,
		TORRENT,
		BMP,
		JPG,
		JPEG,
		PNG,
		SVG,
		GIF,
		XBM,
		OGA,
		OGV,
		OGM,
		OGG,
		WEBM,

		HTTP,
		HTTPS,
		FTP,
		MAILTO,
		NNTP,
		NEWS,
		SNEWS,
		LAST_ASSOC
	};

	/** Sets Opera as the default browser and handler for the most commonly supported files/protocols
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only.
	  */
	OP_STATUS SetDefaultBrowserAndAssociations(BOOL all_users = FALSE);
	/** Sets Opera as the default mailer and handler for email related files/protocols
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */
	OP_STATUS SetDefaultMailerAndAssociations(BOOL all_users = FALSE);
	/** Sets Opera as handler for news related files/protocols
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */
	OP_STATUS SetNewsAssociations(BOOL all_users = FALSE);

	/** Rewrites the opera file handlers (Opera.HTML, Opera.Image, ...) to the registry.
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */
	OP_STATUS RestoreFileHandlers(BOOL all_users = FALSE);

	/**
	  * Find out whether Opera is the handler for a given file type or protocol
	  *
	  * @param assoc The association to test for
	  * 
	  * @return BOOL Whether Opera is set as handler for this extension/protocol
	  */
	BOOL HasAssociation(AssociationsSupported assoc);

	/** Find out whether Opera is the default browser (on start menu)
	  */
	BOOL IsDefaultBrowser();

	/**Find out whether Opera is the default mailer (on start menu)
	  */
	BOOL IsDefaultMailer();

	/** Makes Opera the handler for a given file type or protocol
	  *
	  * @param assoc Specifies which file type/protocol to handle
	  * 
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */
	OP_STATUS AssociateType(AssociationsSupported assoc, BOOL all_users = FALSE);

	/** Sets Opera as the default browser (on start menu)
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */

	OP_STATUS BecomeDefaultBrowser(BOOL all_users = FALSE);

	/** Sets Opera as the default mailer (on start menu)
	  *
	  * @param all_users If TRUE, the change is made system-wide (if possible).
	  *                  Otherwise, the change is made for the current user only
	  */
	OP_STATUS BecomeDefaultMailer(BOOL all_users = FALSE);

	/** Returns whether Opera should use the system's default programs dialog or should
	  * use it's own file associations dialog.
	  */
	BOOL UseSystemDefaultProgramsDialog() {return IsSystemWin8() || IsSystemWinVista() && m_settings.all_users;}

	/** Reports whether the installer object is in a state where it can reliably be used to examine
	  * or change file and protocol associations.
	  *
	  * This should only ever return FALSE if opera was started with the install or uninstall options
	  *	or if the installer log file is not found in the opera directory. Even if FALSE is returned,
	  * all methods related to associations handling will still work, but the result will not be predictable
	  */
	BOOL AssociationsAPIReady() {return m_operation == OpNone && m_old_install_log != NULL; }


	/******** Data structures used by the installer classes ********/

	/** Structure defining the different settings the installer should respect when
	  * running
	  */
	typedef struct Settings
	{
		BOOL copy_only;					//Installer should only copy the files to destination and nothing else

		BOOL single_profile;			//The installer should set up opera to use a single user profile instead of windows user profiles

		BOOL launch_opera;				//The installer will launch opera after completing installation

		//All of the following should be FALSE if copy_only is true
		BOOL all_users;					//The installer should make system-wide change to install opera for all users (requires admin privileges)
		BOOL set_default_browser;		//The installer will make Opera the default browser for the current user (or for the system if all_users is true)

		//Shortucts that the installer should create
		BOOL desktop_shortcut;
		BOOL start_menu_shortcut;
		BOOL quick_launch_shortcut;
		BOOL pinned_shortcut;				//This option is applicable on Windows 7

		//Whether the installer will actually delete or create these shortcuts when updating
		BOOL update_desktop_shortcut;
		BOOL update_start_menu_shortcut;
		BOOL update_quick_launch_shortcut;
		BOOL update_pinned_shortcut;

	} Settings;

	/** List of operations that the installer can perform. Used to know which
	  * mode the installer is in.
	  */
	typedef enum Operation
	{
		OpNone = 0,
		OpInstall,
		OpUpdate,	// ToDo: Can we change this to upgrade. It's not a really good name when used in the wizard.
		OpUninstall
	} Operation;

	/** Flags indicating which parts of the profile should be removed when
	  * uninstalling. Full provile is used when the entire profile folder
	  * should be removed.
	  */
	enum DeleteProfileParts
	{
		KEEP_PROFILE			= 0x0000,		//Don't remove anything
		REMOVE_GENERATED		= 0x0001,		//History, cache, thumbnails, temporary downloads, etc...
		REMOVE_PASSWORDS		= 0x0002,		//Wand and certificates
		REMOVE_CUSTOMIZATION	= 0x0004,		//toolbars, keyboard shortcuts, skin,...
		REMOVE_EMAIL			= 0x0008,		//mail folder
		REMOVE_BOOKMARKS		= 0x0010,		//bookmarks
		REMOVE_UNITE_APPS		= 0x0020,		//Unite apps
		FULL_PROFILE			= 0xFFFF,		//Remove the fill profile directory
	};

	/** Used by RegistryOperation to express for which product an operation should be performed
	  */
	enum Product
	{
		PRODUCT_OPERA		= 0x01,
		PRODUCT_OPERA_NEXT	= 0x02,
		PRODUCT_OPERA_LABS	= 0x04,
		PRODUCT_ALL			= 0xFF,
	};

	/** Structure used in to define lists of registry changes to perform.
	 */
	struct RegistryOperation
	{
		uni_char* key_path;		//The key to change
		uni_char* value_name;	//The name of the value to change (NULL for default value)
		DWORD value_type;		//The value type. This can be either of the value type specified by the registry API when adding a value or REG_DELETE or REG_DEL_KEY
		void* value_data;		//The value data. The actual type to be casted to void* depends on value_type
								// REG_NONE, REG_DELETE, REG_DELETE_KEY -> NULL
								// REG_SZ, REG_EXPAND_SZ -> uni_char*
								// REG_DWORD -> DWORD
								// REG_BINARY -> char*
								// Other types are not supported as of yet.
		WinType min_os_version;	//Minimum OS version required to perform operation. The operation will be ignored otherwise
		HKEY root_key_limit;	//Set to HKEY_CURRENT_USER if the operation should only be performed when installing for the current user
								//or to HKEY_LOCAL_MACHINE if the operation should only be performed for installing for all users
								//NULL if it can be performed in both cases.
		UINT product_limit;	//Indicate to only perform a registry operation if the correct product is being installed
		UINT clean_parents;		//How many ancestor levels of this key should be removed along with the value when uninstalling
	};

	/** List of errors that can occur during (un)installation.
	  */
	typedef enum ErrorCode
	{
		GET_USER_NAME_FAILED,
		INSTALLER_INIT_FOLDER_FAILED,
		INSTALLER_INIT_OPERATION_FAILED,
		LAST_INSTALL_PATH_KEY_STRING_SET_FAILED,
		INVALID_FOLDER,
		CREATE_FOLDER_FAILED,
		CANT_GET_FOLDER_WRITE_PERMISSIONS,
		SHOW_PROGRESS_BAR_FAILED,
		COPY_FILE_INIT_FAILED,
		COPY_FILE_REPLACE_FAILED,
		REGISTRY_INIT_UNINSTALL_KEY_FAILED,
		REGISTRY_INIT_REGOP_KEY_FAILED,
		SHORTCUT_INFO_INIT_FAILED,
		BUILD_SHORTCUT_LIST_FAILED,
		SHORTCUT_INIT_FAILED,
		SHORTCUT_FILE_CONSTRUCT_FAILED,
		SHORTCUT_REPLACE_FAILED,
		SHORTCUT_DEPLOY_FAILED,
		DEFAULT_PREFS_FILE_INIT_FAILED,
		DEFAULT_PREFS_FILE_CREATE_FAILED,
		DEFAULT_PREFS_FILE_CONSTRUCT_FAILED,
		SET_MULTIUSER_PREF_FAILED,
		INIT_LANG_PATH_FAILED,
		SET_LANG_FILE_DIR_PREF_FAILED,
		SET_LANG_FILE_PREF_FAILED,
		DEFAULT_PREFS_FILE_COMMIT_FAILED,
		INIT_DESKTOP_RESOURCES_FAILED,
		GET_FIXED_PREFS_FOLDER_FAILED,
		FIXED_PREFS_INIT_FAILED,
		FIXED_PREFS_MAKE_COPY_FAILED,
		FIXED_PREFS_COPY_CONTENT_FAILED,
		SAVE_LOG_FAILED,
		COMMIT_TRANSACTION_FAILED,
		OLD_FILE_MATCHES_INIT_FAILED,
		INIT_OLD_FILE_PATH_FAILED,
		DELETE_OLD_FILE_FAILED,
		INIT_OLD_SHORTCUT_FILE_FAILED,
		DELETE_OLD_SHORTCUT_FAILED,
		INIT_KEY_REPORTED_FAILED,
		REGISTRY_OPERATION_FAILED,
		DELETE_OLD_REG_VALUE_FAILED,
		INIT_OLD_KEY_FAILED,
		DELETE_OLD_KEY_FAILED,
		DELETE_OLD_LOG_FAILED,
		INIT_FILES_LIST_FAILED,
		FILES_LIST_MISSING,
		FILES_LIST_READ_FAILED,
		INIT_RERUN_ARGS_FAILED,
		WRONG_RERUN_OPERATION,
		ELEVATION_SHELL_EXECUTE_FAILED,
		ELEVATION_WAIT_FAILED,
		ELEVATION_CREATE_EVENT_FAILED,
		CANT_READ_MSI_UNINSTALL_KEY,
		INIT_MSI_UNINSTALL_KEY_FAILED,
		INIT_MSI_UNINSTALL_FOLDER_FAILED,
		INIT_MSI_UNINSTALL_ARGS_FAILED,
		MSI_UNINSTALL_SHELLEXECUTE_FAILED,
		INIT_WISE_UNINSTALL_PATH_FAILED,
		INIT_WISE_UNINSTALL_ARGS_FAILED,
		WISE_UNINSTALL_SHELLEXECUTE_FAILED,
		CANT_INSTALL_SINGLE_PROFILE_WITHOUT_WRITE_ACCESS,
		SHORTCUT_INFO_SET_NEW_NAME_FAILED,
		PARENT_PATH_INIT_FAILED,
		INSTALLER_INIT_LOG_FAILED,
		WRITE_FILE_TO_LOG_FAILED,
		WRITE_REG_HIVE_TO_LOG_FAILED,
		WRITE_SHORTCUT_TO_LOG_FAILED,
		INIT_OLD_SHORTCUT_NAME,
		FIND_OLD_SHORTCUT,
		INIT_NEW_SHORTCUT_PATH,
		INIT_NEW_SHORTCUT_FILE,
		INIT_OLD_SHORTCUT_FILE,
		MOVE_SHORTCUT_FAILED,
		SET_SHORTCUT_NEW_PATH,
		OTHER_USERS_ARE_LOGGED_ON,
		CANT_ELEVATE_WITHOUT_UAC,
		MSI_UNINSTALL_FAILED,
		INIT_ELEVATION_ARGS_FAILED,
		GET_INSTALLER_PATH_FOR_RUNONCE_FAILED,
		INIT_RUNONCE_STRING,
		WRITE_RUNONCE_FILE,
		SET_RUNONCE_FAILED,
		FIXED_PREFS_FILE_CREATE_FAILED,
		FIXED_PREFS_FILE_CONSTRUCT_FAILED,
		FIXED_PREFS_FILE_COMMIT_FAILED,
		INIT_PREF_ENTRY_FAILED,
		SET_PREF_FAILED,
		COPY_FILE_WRITE_FAILED,
		CANT_OBTAIN_WRITE_ACCESS,
		ELEVATION_CANT_FIX_EVENT_ACL,
		SET_OPERANEXT_PREF_FAILED,
		MAKE_SHORTCUT_COMPARE_NAME_FAILED,
		PIN_TO_TASKBAR_FAILED,
		UNPIN_FROM_TASKBAR_FAILED,
		PIN_OPERA_PATH_INIT_FAILED,
		TEMP_OPERA_PATH_INIT_FAILED,
		UNPIN_OPERA_PATH_INIT_FAILED,
		TEMP_INIT_OLD_SHORTCUT_FILE_FAILED,
		PIN_DELETE_OLD_SHORTCUT_FAILED,
		TEMP_SHORTCUT_SET_NEW_NAME_FAILED,
		TEMP_SHORTCUT_INIT_FAILED,
		TEMP_SHORTCUT_DEPLOY_FAILED,
		INIT_ERROR_LOG_PATH_FAILED,
		INIT_ERROR_LOG_FAILED,
		SET_COUNTRY_CODE_PREF_FAILED,
		MAPI_LIBRARIES_INSTALLATION_FAILED,
		MAPI_LIBRARIES_INST_INIT_FAILED,
		MAPI_LIBRARIES_SRC_FILE_INIT_FAILED,
		MAPI_LIBRARIES_DST_FILE_INIT_FAILED,
		MAPI_LIBRARIES_REPLACE_FAILED,
		MAPI_LIBRARIES_WRITE_FAILED,
		MAPI_LIBRARIES_UNINSTALLATION_FAILED,
		MAPI_LIBRARIES_UNINST_INIT_FAILED,
		MAPI_LIBRARIES_DELETE_FAILED,
		MAPI_LIBRARIES_RESET_DEFAULT_MAIL_CLIENT
	} ErrorCode;

	/******** Communication with the wizard ********/

	/** Returns the operation the installer is performing or ready to perform
	  */
	OperaInstaller::Operation GetOperation() const {return m_operation;}

	/** Returns the currently set install folder
	  */
	OP_STATUS GetInstallFolder(OpString &install_folder) const {return install_folder.Set(m_install_folder);}
	/** Changes the installation folder. If an opera installation is found in the given folder,
	  * the installer will be set to update mode and all the installer settings and the installer
	  * language will be updated to reflect the settings used by that installation.
	  */
	OP_STATUS SetInstallFolder(const OpStringC& install_folder);

	/** Returns the installation language
	  */
	OP_STATUS GetInstallLanguage(OpString &install_language) {return install_language.Set(m_install_language);};
	/** Changes the installation language
	  */
	OP_STATUS SetInstallLanguage(OpStringC &install_language);

	/** If this returns TRUE, the installer is performing (un)installation operations and its
	  * settings can no longer be affected.
	  */
	BOOL IsRunning() {return m_is_running;}

	/** Checks whether installing in the currently set  installation folder will require privilege elevation.
	  */
	OP_STATUS PathRequireElevation(BOOL &elevation_required);

	/** Temporarily interrupts the installer.
	  *
	  * @param pause If true, the installation sequence will be suspended. If false, it is resumed.
	  */
	void Pause(BOOL pause);

	/** Returns the settings the installer is using
	  */
	Settings GetSettings() {return m_settings;}

	/** Specfies the settings to be used by the installer
	  * 
	  * @param settings Settings to use
	  *
	  * @param preserve_multiuser Wether to keep any pre-existing single profile setting instead of overriding it
	  *
	  * @return FALSE if some settings given were incompatible
	  * with other settings given. In this case, the incompatible
	  * settings were corrected before getting applied. TRUE otherwise.
	  */
	BOOL SetSettings(Settings settings, BOOL preserve_multiuser = TRUE);

	/* Informs the installer to run the Yandex script to set all default search engines
	*
	*  @param run		Whether to run the script
	*/
	void RunYandexScript(BOOL run) {m_run_yandex_script = run;}

	/** Informs the installer of which part of the current user's profile
	  * should be removed when uninstalling.
	  *
	  * @param parts	See DeleteProfileParts
	  */
	void SetDeleteProfile(UINT parts) {m_delete_profile = parts;}

	/** Informs the installer that the installation was cancelled by
	  * the user when displaying the locked files dialog
	  */
	void WasCanceled(BOOL canceled) { m_was_canceled = canceled; };

	/******** Other ********/

	/** Performs the next installation step and reports any error
	  * that happened. The installer should be terminated when
	  * an error is returned. This is used by the OperaInstallerUI class.
	  */
	OP_STATUS DoStep();

	/** Request the installer error log file in order to report an issue to it.
	  */
	OP_STATUS GetErrorLog(OpFile*& log);

	/** Performs the actual deletion of the profile once the files aren't locked.
	  * This should ONLY be called right before opera stops.
	  */
	static void DeleteProfileEffective();

	/** Called when Opera is started with the -give_folder_write_access command line
	  * option is given. used to make an elevated opera process temporarily
	  * give write access to a folder to an installer instance running as a non
	  * privileged user
	  */
	static void GiveFolderWriteAccess();

	/** Called when country check is finished.
	  * Resumes installer if it is waiting for m_country_checker.
	  */
	virtual void CountryCheckFinished();

private:

	/******** Initialization and preparation of installation ********/

	/** Initializes the settings used for installation from command line options.
	  * The following options are recognized:
	  * 
	  * Option               | Sets the setting      |  Default, if option not specified
	  * ---------------------|-----------------------|-----------------------------------
	  * -copyonly            | copy_only             | FALSE
	  * -allusers            | all_users             | TRUE if user has an administrator account, FALSE otherwise.
	  * -singleprofile       | single_profile        | FALSE
	  * -setdefaultbrowser   | set_default_browser   | TRUE if VER_BETA isn't defined
	  * -startmenushortcut   | start_menu_shortcut   | TRUE
	  * -desktopshortcut     | desktop_shortcut      | TRUE
	  * -quicklaunchshortcut | quick_launch_shortcut | FALSE if running on Windows 7. TRUE otherwise.
	  * -pintotaskbar        | pinned_shortcut       | TRUE if running on Windows 7. FALSE otherwise.
	  * -launchopera         | launch_opera          | TRUE
	  * 
	  * update_start_menu_shortcut is set to TRUE if -startmenushortcut is present. FALSE otherwise
	  * update_desktop_shortcut is set to TRUE if -desktopshortcut is present. FALSE otherwise
	  * update_quick_launch_shortcut is set to TRUE if -quicklaunchshortcut is present. FALSE otherwise
	  * update_pinned_shortcut is set to TRUE if -pintotaskbar is present and OS is Windows 7. FALSE otherwise
	  *
	  * if copy_only is set to TRUE:
	  *    all_users, set_default_browser and the shortcut settings are always set to FALSE
	  *    single_profile is always set to TRUE
	  */
	void SetSettingsFromCommandLine();
	/** Initializes the installation folder from the -installfolder command line option if it is specified.
	  * Otherwise, attempts to use the folder used by the last installer run.
	  * Otherwise, uses the default installation folder.
	  */
	void SetInstallFolderFromCommandLine();
	/** Initializes the installer language from the -language command line option
	  * if it is specified.
	  */
	void SetInstallLanguageFromCommandLine();

	/** Called when the installation folder is set from the command line or changed by SetInstallFolder
	  *
	  * This method checks if a log file is present at a given folder and uses the information
	  * from it to decide what operation the installer will execute as well as initializing a
	  * few installer settings from the information found in the log file and in an eventual operaprefs_default.ini
	  */
	OP_STATUS CheckAndSetInstallFolder(const OpStringC& new_install_folder);

	/******** (Un)Installation steps ********/

	/** Attempts to remove installations made with the MSI or classic installer
	  */
	OP_STATUS Install_Remove_old();
	/** Copies one file from the files list. This step is called once for each file
	  * to be copied.
	  */
	OP_STATUS Install_Copy_file();
	/** Copies MAPI libraries.
	  */
	OP_STATUS Install_MAPI_Libraries();
	/** Executes all the registry operations needed for installation. Those operations are
	  * specified by m_installer_regops
	  */
	OP_STATUS Install_Registry();
	/** Notifies the system that some settings have changed
	  */
	OP_STATUS Install_Notify_system();
	/** Writes shortcuts
	  */
	OP_STATUS Install_Create_shortcuts();
	/** Does any change needed to operaprefs_default.ini and operaprefs_fixed.ini
	  * This also adds the preferences requested via the -setdefaultpref and -setfixedpref
	  * command line options.
	  */
	OP_STATUS Install_Default_prefs();
	/** Writes the log, sets Opera as default browser if needed and starts Opera if needed.
	  */
	OP_STATUS Install_Finalize();

	/** Removes files that were installed by the previous version but not needed in this version
	  * any longer
	  */
	OP_STATUS Update_Remove_old_files();
	/** Add, remove or rename shortcuts depending on installer settings
	  */
	OP_STATUS Update_Shortcuts();

	/** Remove all shortcuts that were installed
	  */
	OP_STATUS Uninstall_Remove_Shortcuts();
	/** Cleans up the registry
	  */
	OP_STATUS Uninstall_Registry();
	/** Removes MAPI libraries.
	  */
	OP_STATUS Uninstall_MAPI_Libraries();
	/** Shows the survey and removes the installed files.
	  */
	OP_STATUS Uninstall_Finalize();


	/******** Helper methods ********/

	/** Finds an existing installation done with MSI in the installation folder and uninstalls it.
	  *
	  * @param found Returns whether an installation done with MSI was found.
	  */
	OP_STATUS FindAndRemoveMSIInstaller(BOOL &found);
	/** Finds an existing installation done with Wise in the installation folder and uninstalls it.
	  *
	  * @param found Returns whether an installation done with Wise was found.
	  */
	OP_STATUS FindAndRemoveWiseInstaller(BOOL &found);

	/** When trying to install with all_users set to FALSE, to a folder where the current user
	  * doesn't have write access, Run() calls this method to launch an elevated process
	  * that takes care to give write access to the user. See GiveFolderWriteAccess
	  *
	  * @param folder_path	The fodler to which the current user should get access.
	  * @param permanent	Whether to give access permanently. If this is FALSE, the elevated
	  *						process will revert the permissions on the folder when the installer
	  *						is done.
	  */
	OP_STATUS GetWritePermission(OpString &folder_path, BOOL permanent);
	/** Launches an elevated non-interactive instance of the installer that takes care of the
	  * actual installation. If this method succeeds, it takes care of terminating this instance.
	  */
	OP_STATUS RunElevated();
	/** Reads the files_list file that contains the list of files to install and initializes m_files_list
	  */
	OP_STATUS ReadFilesList();
	/** Takes a RegistryOperation and executes it.
	  *
	  * @param root_key		The key on which we are operating. Either HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER
	  * @param operation	See the definition of RegistryOperation
	  * @param key_path		The key to write to. This overrides the key_path provided in RegistryOperation but is needed because
	  *						the key path can contain placeholders to be replaced by the caller.
	  * @param transaction	If provided, the action performed will be made part of this transaction.
	  * @param log			If provided and the operation adds a value, it will be written to this log.
	  */
	OP_STATUS DoRegistryOperation(HKEY root_key, const RegistryOperation operation, OpStringC &key_path, OpTransaction *transaction, OperaInstallLog *log);
	/** Helper function used by DoRegistryOperation. Replaces the different possible placeholder
	  * (keywords between {}) contained in a string by their actual value.
	  */
	OP_STATUS ExpandPlaceholders(OpString& string);
	/** To be called when an operation on a file fails. This will attempt to see if the file is locked by another process and if
	  * it is, it will prompt the user to close the offending process.
	  */
	OP_STATUS CheckIfFileLocked(const uni_char* full_path);
	/** This method attempts to determine whether there is an instance of Opera running that was started
	  * from the Opera executable present at a given path. The answer is not always reliable and it is
	  * possible that it doesn't detect a running instance when there is one, thus returning FALSE instead
	  * of TRUE in some cases.
	  */
	BOOL IsOperaRunningAt(const uni_char* full_path);	

	/** Modifies the ACL of an event obtained by CreateEvent, so that the Administrators group is
	  * allowed to access it using OpenEvent. This ensures that elevated processes can communicate
	  * with the non-elevated process that started them. This only appears to be an issue on XP so far.
	  */
	OP_STATUS FixEventACL(HANDLE evt);
	/** Prepares a string containing the command line arguments needed to launch a non interactive
	  * installer process with the same settings as the current installer instance. This is used both
	  * for starting an elevated instance of the installer and for writing runonce registry entries
	  * sometimes needed in FindAndRemoveMSIInstaller
	  */
	OP_STATUS GetRerunArgs(OpString &args);
	/** Actually write shortcut once the details of where they should be written have been collected
	  *
	  * @param destinations		A list of DesktopShortcutInfo::Destinations indicating where shortcuts are needed
	  * @param use_opera_name	Whether we should attempt to create shortcuts named "Opera.lnk" and only fallback to "Opera [version].lnk"
	  *							if such shortcuts already exists. If false, shortcuts with name of type "Opera [version].lnk" will always be
	  *							used.
	  */
	OP_STATUS AddShortcuts(OpINT32Vector &destinations, BOOL& use_opera_name);
	/** Pin application to taskbar. Call to this function is valid for Windows 7 platform.
	  * 
	  */
	OP_STATUS AddOperaToTaskbar();
	/** Unpin application from taskbar. Call to this function is valid for Windows 7 platform.
	  * 
	  */
	OP_STATUS RemoveOperaFromTaskbar();
	/** Parses a list of string of the form [Section]Key=Value and uses them to write new preferences to a PrefsFile
	  */
	OP_STATUS AddPrefsFromStringsList(OpVector<OpString> &prefs_settings_list, PrefsFile &prefs_file);
	/** Prepares the list of files to be removed from the profile just before the uninstaller terminates.
	  */
	OP_STATUS DeleteProfile(UINT parts);

	/** Sends country code request to the autoupdate server if not sent already and
	  * if m_update_country_pref is true. Answer is added to default prefs if it is
	  * received before installer finishes default prefs step.
	  */
	OP_STATUS StartCountryCheckIfNeeded();

	/**
	  *
	  */
	OP_STATUS ResetDefaultMailClient(bool all_users);

	/******** Static installer data ********/

	//List of registry operations done by Install()
	static const RegistryOperation m_installer_regops[];
	//List of registry operations done by RestoreFileHandlers
	static const RegistryOperation m_file_handlers_regops[];
	//Registry operation used by BecomeDefaultBrowser
	static const RegistryOperation m_default_browser_start_menu_regop;
	//Registry operation used by BecomeDefaultMailer
	static const RegistryOperation m_default_mailer_start_menu_regop;
	//Registry operation for setting the last install path
	static const RegistryOperation m_set_last_install_path_regop;
	//Registry operation for indicating that the uninstall survey has been answered at least once
	static const RegistryOperation m_surveyed_regop;

	//List of registry operations for each AssociationSupported
	//describing how to make opera the handler
	static const RegistryOperation* m_association_regops[LAST_ASSOC];
	//Key name to check under HKEY_CLASSES_ROOT for each AssociationSupported
	static const uni_char* m_check_assoc_keys[LAST_ASSOC];

	/******** Installer settings ********/
	
	OpString m_install_folder;			//When installing/upgrading, this is the destination folder. Otherwise, this becomes the current Opera folder.
	Settings m_settings;				//Settings to use for installing or retreived from the current installation.
	OpString m_install_language;		//Language to be set as default and to use for the installer UI
	UINT m_delete_profile;				//Indicates which parts of the profile should be removed when uninstalling, if any.
	int m_existing_multiuser_setting;	//Whether an existing multiuser setting value was set in the default prefs file of the installation folder
	int m_existing_product_setting;		//Whether an existing Opera Product value was set in the default prefs file of the installation folder

	/******** Installer state ********/

	Operation m_operation;			//The current mode of operation
	BOOL m_is_running;				//Whether Run() has been called

	UINT m_step;					// Indicates what group of operations we are currently doing (ie. copy files).
	UINT m_step_index;				// Indicates which operation within the current group we are doing (ie. copy file nr. 3).
	UINT m_steps_count;				//Keeps track of how many steps have been done. Useful for debugging

	BOOL m_was_http_handler;		// Indicates whether Opera was set as the default handler for the http protocol, before being uninstalled
	BOOL m_was_canceled;			// Indicates if the user has canceled the install by clicking cancel.

	Product m_product_flag;			//The product flag corresponding to the product currently being installed
	OpStringC m_product_name;		//The product name to append after "Opera"
	OpString m_long_product_name;	//The product name to append after "Opera", long version. This may be the same as m_product_name
	UINT m_product_icon_nr;			//The index of the icon to use in the dll for the current product.
	OpString m_app_reg_name;		//The name of the application used for the association API on Vista and higher

	BOOL m_run_yandex_script;		//Whether to run the Yandex script to change all default search engines


	/******** Installer data ********/
	
	OpAutoVector<OpString>		m_files_list;				//List of files that the installer should copy
	static OpVector<OpFile>*	s_delete_profile_files;		//List of files that should be deleted when DeleteProfileEffective is called. Initialized by DeleteProfile

	/******** Helper objects ********/
	
	OperaInstallLog*	m_old_install_log;				// The log file found inside the old instalation on the users PC.
	OpTransaction*		m_installer_transaction;		// The transaction on which all the (un)installation operations are done
	OperaInstallLog*	m_install_log;					// The current log file for the installation we are currently doing.
	OperaInstallerUI*	m_opera_installer_ui;			// The part of the installer that handles messages and the UI.
	HANDLE				m_get_write_permission_event;	// Event used to communicate with the elevated process giving write permissions to the user.
	OpFile*				m_error_log;					// File of unspecified format used to record information about errors.

	CountryChecker*		m_country_checker;				// Retrieves IP-based country code from the autoupdate server.
	OpString			m_country;						// Country code received from parent process or from the autoupdate server.
	bool				m_update_country_pref;			// True if installer should update country code preference in default prefs file.
	bool				m_country_check_pause;			// True if installer is paused waiting for country check.
	static const unsigned long COUNTRY_CHECK_TIMEOUT = 5000; // Timeout for country check in ms.
};

#endif //OPERAINSTALLER_H
