/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _AUDATAFILE_READER_H_INCLUDED_
#define _AUDATAFILE_READER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/updater/audatafile.h"

class AUFileUtils;


class AUDataFileReader
{
public:
	AUDataFileReader(); 
	virtual ~AUDataFileReader(); 

	OP_STATUS Init();
	
	////////////////////////////////////////////////////////////////////////////////
	// All of this below needs to work outside of core
	
	/**
	 * Function to get install path
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetInstallPath();
	
	/**
	 * Function to get update file
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetUpdateFile();
	
	/**
	 * Function to get the update type
	 */
	AUDataFile::Type GetType() { return m_type; }

	/**
	 * Function to get version
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetVersion();

	/**
	 * Function to get build number
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetBuildNum();

	/**
	 * Function to get dialog caption
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetDialogCaption();
	
	/**
	 * Function to get dialog message
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetDialogMessage();
	
	/**
	 * Function to get dialog ok text
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetDialogOk();

	/**
	 * Function to get dialog cancel text
	 * Caller is responsible of deleting the string
	 */
	uni_char*	GetDialogCancel();

	/**
	 * If m_show_information is set to FALSE there will be no user notification before update
	 * is installed. If it is set to TRUE user will be asked if update should be installed.
	 * It should be set to FALSE when user chose to install update just after it's downloaded.
	 */
	BOOL		ShowInformation() const { return m_show_information; }
	
	/** 
	 * Load file and parse the content and save it internally
	 * Use this function to load the file from the Updater
	 */
	OP_STATUS	Load();

	/** 
	 * Load file and parse the content and save it internally
	 * Use this function to load the file from within Opera
	 */
	OP_STATUS	LoadFromOpera();
	
	/**
	 * Deletes the loaded file from disk
	 */
	void		Delete();
	
private:
	/** 
	 * Load file from path
	 */
	OP_STATUS LoadFromPath(const uni_char* path);
		
	/**
	 * Read value from buf til end of line and put it into value
	 */
	OP_STATUS ReadValue(uni_char*& buf, const uni_char* end_buf, uni_char** value);

	/**
	 * Match value from param value in param buf
	 */
	OP_STATUS MatchValue(uni_char*& buf, const uni_char* end_buf, const uni_char* value);
	
	AUFileUtils*		m_au_fileutils;
	uni_char*			m_install_path;
	uni_char*			m_update_file;
	AUDataFile::Type	m_type;
	uni_char*			m_size;
	uni_char*			m_version;
	uni_char*			m_buildnum;
	uni_char*			m_caption;
	uni_char*			m_message;
	uni_char*			m_ok;
	uni_char*			m_cancel;
	BOOL				m_show_information;
	
	// Private files not exposed
	uni_char*			m_text_file_path;		// Path to the text file with the upgrade information
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _AUDATAFILE_READER_H_INCLUDED_
