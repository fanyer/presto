/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _AUDATAFILE_H_INCLUDED_
#define _AUDATAFILE_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

class AUFileUtils;

#define AU_DATAFILE_HEADER		UNI_L("/* Autoupdate upgrade file */\n")
#define AU_DATAFILE_VERSION		UNI_L("Version=1.0\n")
#define AU_DATAFILE_EXECUTABLE	UNI_L("Executable=")
#define AU_DATAFILE_FILE		UNI_L("File=")
#define AU_DATAFILE_TYPE		UNI_L("Type=")
#define AU_DATAFILE_TYPE_FULL	UNI_L("full")
#define AU_DATAFILE_TYPE_PATCH	UNI_L("patch")
#define AU_DATAFILE_SIZE		UNI_L("Size=")
#define AU_DATAFILE_VERSION_NUM	UNI_L("VersionNum=")
#define AU_DATAFILE_BUILD_NUM	UNI_L("Buildnum=")
#define AU_DATAFILE_DIALOG_CAPTION	UNI_L("Caption=")
#define AU_DATAFILE_DIALOG_MESSAGE	UNI_L("Message=")
#define AU_DATAFILE_DIALOG_OK		UNI_L("Ok=")
#define AU_DATAFILE_DIALOG_CANCEL	UNI_L("Cancel=")
#define AU_DATAFILE_SHOW_INFORMATION	UNI_L("ShowInformation=")

class AUDataFile
{
public:
	enum Type 
	{
		Full,
		Patch
	};

	AUDataFile(); 
	virtual ~AUDataFile(); 

	OP_STATUS Init();

	////////////////////////////////////////////////////////////////////////////////
	// All of this below will be used inside of core
	
	// Sets the install path to use
	OP_STATUS SetInstallPath(const uni_char *install_path);
	
	// Sets the downloaded file to use in the update and it's type (Full/Patch)
	OP_STATUS SetUpdateFile(const uni_char *downloaded_file, AUDataFile::Type type);
	
	// Sets the version
	OP_STATUS SetVersion(const uni_char *version);

	// Sets the buildnum
	OP_STATUS SetBuildnum(const uni_char *build_num);

	/**
	 * If m_show_information is set to FALSE there will be no user notification before update
	 * is installed. If it is set to TRUE user will be asked if update should be installed.
	 * It should be set to FALSE when user chose to install update just after it's downloaded.
	 */
	void SetShowInformation(BOOL show) { m_show_information = show; }

	// Write out the file
	OP_STATUS Write();
	
	// Delete the file, used if something failed
	OP_STATUS Delete();

	// Reads values from existing data file and sets inner values
	OP_STATUS LoadValuesFromExistingFile();

private:
	AUFileUtils*	m_au_fileutils;
	OpString		m_install_path;
	OpString		m_update_file;
	OpString		m_file_name;
	OpString		m_version;
	OpString		m_buildnum;
	Type			m_type;
	BOOL			m_show_information;
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _AUDATAFILE_H_INCLUDED_
