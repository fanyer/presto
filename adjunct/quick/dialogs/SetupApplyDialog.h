/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// SUGGESTED mimetype
//     To: ietf-types@iana.org
//     Subject: Registration of MIME media type application/x-opera-skin
//
//     MIME media type name: application
//
//     MIME subtype name: x-opera-skin
//
//     Required parameters: n/a
//
//     Optional parameters: version=x.y
//
//     Encoding considerations: n/a
//
//     Security considerations: n/a
//
//     Interoperability considerations: Content is a standard ZIP file containing description of userinterface widgets in Opera
//
//     Published specification: http://www.opera.com/support/search/supsearch.dml?index=214&session=2dbefaa5b11c96120048c3adefaa2015
//
//     Applications which use this media type: Opera browser version 7 and higher
//
//     Additional information:
//
//       Magic number(s):
//       File extension(s): zip
//       Macintosh File Type Code(s): 
//
//     Person & email address to contact for further information: 
//
//     Intended usage: LIMITED USE
//
//     Author/Change controller: Opera Software ASA http://www.opera.com/
//

#ifndef SETUPAPPLYDIALOG_H
#define SETUPAPPLYDIALOG_H

#include "SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "modules/windowcommander/src/TransferManager.h"

#include "modules/util/zipload.h"


class URL;
class Window;

class SetupApplyListener
{
public:
	virtual ~SetupApplyListener() {}

	virtual void OnSetupCancelled() = 0;
	virtual void OnSetupAccepted() = 0;
};

class SetupApplyDialog : public Dialog , public OpTransferListener, public SetupApplyListener
{
private:
	/**
	 * Class for keeping track of multiple configuration files. It holds information about the type and location
	 * of the different setup files that are extracted from the zip, and information about the files they replace
	 * so that the settings can be rolled back.
	 */
	class ConfigurationCollection
	{
	public:
		/**
		 * This struct contains rollback information that keeps track of
		 * which settings need to be rolled back and what the original setting was.
		 */
		struct RollbackInformation
		{
			BOOL		menu_rollback_needed;
			OpString	menu_rollback_setup;
			BOOL		keyboard_rollback_needed;
			OpString	keyboard_rollback_setup;
			BOOL		toolbar_rollback_needed;
			OpString	toolbar_rollback_setup;
			BOOL		mouse_rollback_needed;
			OpString	mouse_rollback_setup;
			BOOL		language_rollback_needed;
			OpString	language_rollback_setup;
			BOOL		skin_rollback_needed;
			OpString	skin_rollback_setup;
			BOOL		session_rollback_needed;
			OpString	session_rollback_setup;
			STARTUPTYPE	session_startuptype_rollback_state;
			BOOL		home_page_rollback_needed;
			OpString	home_page_rollback_setup;
			BOOL		fallback_encoding_rollback_needed;
			OpString8	fallback_encoding_rollback_setup;

			/** Constructor that sets the struct to an inital, empty state. */
			RollbackInformation()
			{
				menu_rollback_needed = FALSE;
				menu_rollback_setup.Empty();
				keyboard_rollback_needed = FALSE;
				keyboard_rollback_setup.Empty();
				toolbar_rollback_needed = FALSE;
				toolbar_rollback_setup.Empty();
				mouse_rollback_needed = FALSE;
				mouse_rollback_setup.Empty();
				language_rollback_needed = FALSE;
				language_rollback_setup.Empty();
				skin_rollback_needed = FALSE;
				skin_rollback_setup.Empty();
				session_rollback_needed = FALSE;
				session_rollback_setup.Empty();
				session_startuptype_rollback_state = STARTUP_BLANK;
				home_page_rollback_needed = FALSE;
				home_page_rollback_setup.Empty();
				fallback_encoding_rollback_needed = FALSE;
				fallback_encoding_rollback_setup.Empty();
			}
		};
		/**
		 * This is a superclass in the hierarchy of setup file type representations.
		 * This hierarchy is just a way of differentiating between the different file types.
		 * Hardly any code reuse is taking place.
		 */
		class ConfigurationFile
		{
		public:
			/** A pointer to a mem file holding the content of one specific file extracted from the zip. */
			OpMemFile*					mem_file;
		};

		/**
		 * Class that represents the readme file that holds the author name, version number and other comments.
		 */
		class ReadmeFile : public ConfigurationFile
		{
		};

		/**
		 * Class that represents the opera.ini-file that holds the settings to be used. 
		 */
		class MainIniFile : public ConfigurationFile
		{
			/** The name of the backup ini file that holds the original setup that this setup overwrites. */
			OpString					backup_ini_file;
		};

		/**
		 * Class that represents a configuration file that is to be installed. 
		 */
		class InstallableFile : public ConfigurationFile
		{
		public:
			/** The location that the file has in the zip archive. This is the suggested location and file name of the file. */
			OpString					listed_location;
			/** 
			 * The actual location that the file is inserted into. Can be different from the listed 
			 * location if the listed filename already existed when the setup was downloaded. 
			 */
			OpString					real_location;
		};
		/** A list of installable files that represents the setups that are included in the multi setup download. */
		OpVector<InstallableFile>		installable_list;
		/** Pointer to the readme file representation */
		ReadmeFile*						readme_file;
		/** Pointer to the main ini file representation. */
		MainIniFile*					main_ini_file;
		/** The rollback information */
		RollbackInformation				rollback_information;
		/** Constructor. Sets all pointers to null and clears all lists. */
		ConfigurationCollection();
		/** Destructor. Clears out the list of installable files and deletes all contents. */
		~ConfigurationCollection();
	};

	URL*							m_url;					// only points to m_transfer.m_url
	OpString						m_urlstring;
	OpTransferItem*					m_transfer			;
	PrefsFile*						m_oldsetupfile;			// used for cancel operation
	OpFile*							m_tempmergefile;
	OpFile							*m_oldopfile;			// used with m_oldsetupfile
	OpString						m_setupfilename;
	OpSetupType						m_setuptype;
	OpSetupManager::SetupPatchMode	m_patchtype;			// if file downloaded should be used as a patch, and how this should be included in the current setup
	INT32							m_gadget_treemodelitem_id;     ///< The treemodelitem id of an opened gadget (If that is what is downloaded.) Used for closing the gadget if it is not wanted.


	OP_STATUS			SaveDownload(OpStringC directory);
	OP_STATUS			EnableSetupAndAskUser();

	BOOL				IsAThreat(OpTransferItem* transferItem);

	/*
	 * Check if a file starts with TAG20, if not the file is deleted!
	 */
	BOOL				CheckFileHeader(URL* url);
	
	/**	
	 * The member that holds all information about downloaded setups, 
	 * the files it contains, the paths of the files and the rollback 
	 * information. 
	 */
	ConfigurationCollection* m_configuration_collection;
	/**
	 * Function that is used to extract the allowed files to memory.
	 * The supplied configuration collection is populated with the information about the files 
	 * that are found in the received document.
	 * Any illegal file/name combination in the file that is not allowed will just be ignored, and 
	 * never unzipped. If the file contains errors, or if the content is not a zip, then an error will be returned.
	 *
	 * @param configuration_collection The configuration collection that specifies what was found and where. Will be modified
	 * by the function and populated with content.
	 * @return OK if at least one file was successfully extracted, ERR if something failed so that nothing was extracted at all.
	 */
	OP_STATUS			ExtractMultiConfigurationFromReceivedZip(OpTransferItem* transferItem);
	/**
	 * Function to check whether the received configuration is a threat. If any part of the setup contains execute statements
	 * and the setup does not come from opera.com, it is a threat.
	 */
	BOOL				ScanMultiSetupForExecuteProgramStatements();
	/**
	 * Stores the setup files to their respective locations.
	 */
	OP_STATUS			SaveMultiSetupFiles();
	/**
	 * Install setup files
	 */
	OP_STATUS			InstallSetupFiles();
	/**
	 * Install other settings from opera.ini that are allowed.
	 */
	OP_STATUS			InstallOtherSettings();
	/**
	 * Rollback to previous setup
	 */
	OP_STATUS			RollbackToPreviousSetup();
	/**
	 * Install other prefs that are set in opera.ini
	 */
	OP_STATUS			SetOtherPrefs();
	// Helper functions that examine filenames from the zip to determine what to do with them.
	/** 
	 * Determines if this is a file that should be unzipped to its designated location 
	 * @param filename The name and location that the file requestes for itself on the disk. 
	 * @return TRUE if the filename is permissable, FALSE otherwise.
	 */
	BOOL				IsInstallableSetupFile(const OpStringC filename);
	/**
	 * Determines if this file is opera.ini, containing setup options that are to be merged in with the existing options.
	 * @param filename The name and location that the file requestes for itself on the disk. 
	 * @return TRUE if the filename is permissable, FALSE otherwise.
	 */
	BOOL				IsMergeableIniFile(const OpStringC filename);
	/**
	 * Determines if this is a readme file that is to be read to extract simple information about the package.
	 * @param filename The name and location that the file requestes for itself on the disk. 
	 * @return TRUE if the filename is permissable, FALSE otherwise.
	 */
	BOOL				IsReadmeFile(const OpStringC filename);
	/** 
	 * Function that saves a file from memory to disk to a specific location. If the location is not found, the file is stored to a different
	 * file name [(1) added at the end] in the same folder.
	 * @param mem_file The file to store.
	 * @param intended_location The location that the file is to be stored to. 
	 * @param real location The location that the file actually got stored to.
	 * @return The status of the storing.
	 */
	OP_STATUS			SaveMemFile(OpMemFile* mem_file, const OpStringC intended_location, OpString &real_location);
	/**
	 * Function that activates a new language to use.
	 *
	 * The first parameter is the path to the new language file that is to be set. The second is set by the function to make it easier to roll back later.
	 * @param new_language_file_path The path to the new language file to use. Must be set correctly when the function is called.
	 * @param old_language_file_path The path to the current setup file if the new language was successfully set. Can be anything at call time, but will be set to
	 * the old language file path when the function returns, provided that everything went ok.
	 * @return If the language change worked, it will return OK. If not, it will return ERR, and the value of old_language_file_path is undefined.
	 */
	OP_STATUS SetAndActivateNewLanguage(const OpStringC new_language_file_path, OpString &old_language_file_path);
	/**
	 * This function is supposed to set the key corresponding to the supplied file path. If no key matches the file path, 
	 * the function will return ERR, and the value of key will be undefined.
	 * @param file_location The path to the location that we want a key corresponding to
	 * @param key The key that is to be set according to the location.
	 * @return OK if the file location corresponds to a key, ERR otherwise.
	 */
	OP_STATUS			GetIniKeyCorrespondingToPath(const OpStringC file_location, OpString8 &key);
	/**
	 * Determines if the file path is the path to a language file.
	 */
	BOOL IsLanguagePath(const OpStringC file_path);


	// This variable is really a hack... It makes this class complicated and hard to maintain... The multi_setup type setup should 
	// be a setup in parallell with all other setup types. This doesen't work, however, as it breaks the setup-system because that
	// system can't deal with multi-setups....
	/**
	 * Flag that is set if the downloaded setup has the application/x-opera-configuration MIME type. This will cause the variable m_setuptype to be ignored.
	 */
	BOOL				m_setup_is_multi_setup;

	/**
	 * Static class pointer to the current running download.
	 * NULL if no download is running.
	 * If someone tries to start a second download, it must refuse to
	 * start and highlight the running download.
	 */
	static SetupApplyDialog* c_last_started_download;

	/**
	 * Member variable pointer to the next download in line.
	 * Used to queue downloads if a download is already running.
	 */
	SetupApplyDialog* m_next_download_in_line;

	/**
	 * Member variable pointer to the previous download in line.
	 * Used to queue downloads if a download is already running.
	 */
	SetupApplyDialog* m_previous_download_in_line;

	/**
	 * Member flag that informs the instance that it was queued, and shouldn't proceed 
	 * with the download until signalled from the download that is ahead of it in line.
	 */
	BOOL m_queued;

	/**
	 * Functiont that inserts the download in the queue if there is any. If this is
	 * the only download, it will be first in line. The function sets the member 
	 * m_queued to TRUE if the download is inserted in the queue and must wait 
	 * with starting the download, to FALSE
	 * if the download is the only currently running and must wait in line.
	 */
	void InsertInQueue();

	/**
	 * Function that removes the download from the queue when it is being destroyed.
	 * Signals the next in line to move ahead and start downloading.
	 */
	void RemoveFromQueue();

public:
						SetupApplyDialog();
						SetupApplyDialog(URL* url);
						SetupApplyDialog(const OpStringC & full_url, URLContentType content_type);
						~SetupApplyDialog();
	
	OP_STATUS			SetURL(URL * url);
	OP_STATUS			SetURLInfo(const OpStringC & full_url, URLContentType content_type);

	/**
	 * Helper to SetURL, also used to postpone starting the download. It needs to know
	 * if it was queued before starting, because it will sometimes be run before OnInit,
	 * sometimes after, and when it is run after, it needs to take care of checking the 
	 * transfer, which is otherwise done in OnInit.
	 */
	OP_STATUS			StartDownloading(BOOL was_queued); 

	Type				GetType()				{return DIALOG_TYPE_DOWNLOAD_SETUP;}
	const char*			GetWindowName()			{return "Setup Download Dialog";}
	BOOL				GetModality()			{return GetParentDesktopWindow() ? GetParentDesktopWindow()->IsDialog() : FALSE ;}

	void				OnInit(); 
	void				OnCancel();
	UINT32				OnOk();

	void				OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void				OnReset(OpTransferItem* transferItem) {   }
	void				OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);
	void				RegularSetup();

	// SetupApplyListener
	void				OnSetupCancelled()	{ CloseDialog(TRUE); }
	void				OnSetupAccepted()	{ RegularSetup(); }

protected:
	inline void SetNextInLine(SetupApplyDialog* next) { m_next_download_in_line = next; } ///< Setter. Should only be accessed by the InsertInQueue and RemoveFromQueue functions.
	inline void SetAheadInLine(SetupApplyDialog* prev) { m_previous_download_in_line = prev; } ///< Setter. Should only be accessed by the InsertInQueue and RemoveFromQueue functions.

private:
	void InitData();
};

// ***************************************************************************
//
//	SetupApplyDialog_ConfirmDialog
//
// ***************************************************************************

class SetupApplyDialog_ConfirmDialog : public SimpleDialog
{
public:
	SetupApplyDialog_ConfirmDialog();
	OP_STATUS			Init(SetupApplyListener* listener, DesktopWindow *parent_window, char* servername, INT32* result, BOOL executed_by_gadget = FALSE);
	virtual UINT32		OnOk();
	virtual void		OnCancel();

private:
	SetupApplyListener* m_listener;
};

#endif //SETUPAPPLYDIALOG_H
