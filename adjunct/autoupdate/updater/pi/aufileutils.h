/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file declares the AUFileUtils interface which is
 * a minimal set of functions required to apply updates
 *
 * Note: Your implementation of this class should not use any
 * core functionality, since this will be used before core
 * has been loaded up
 *
 * @author Adam Minchinton
 */

#ifndef _AUFILEUTILS_H_INCLUDED_
#define _AUFILEUTILS_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

// Name of the text file used to hold the update info between restarts
// This file lives next to the Update program
#define AUTOUPDATE_UPDATE_TEXT_FILENAME			UNI_L("autoupdate.txt")
// Name of the file containing the fake Opera version for testing autoupdate
// See OperaVersion::AllowAutoupdateIniOverride() for details
#define AUTOUPDATE_UPDATE_FAKE_FILENAME			UNI_L("autoupdate.ini")
#define AUTOUPDATE_UPDATE_WAITFILE				UNI_L("wait")

class AUFileUtils
{
public:
	enum AUF_Status
	{
		OK,				// Succeeded
		ERR,			// Generic error
		ERR_MEM,		// Out of memory
		NO_ACCESS,		// No permission to access the file
		FILE_NOT_FOUND	// File doesn't exist
	};

	virtual ~AUFileUtils() {}

	/** Create an AUFileUtils object.
	 *
	 * This does not create or open the file.
	 *
	 * @return a pointer to the object or NULL.
	 */
	static AUFileUtils* Create();

	/**
	 * Gets the location of the upgrade folder. This folder should be a temp
	 * location on your system that doesn't automatically get deleted plus
	 * the path to th exe. (i.e. a path that can be consistenly gotten from the
	 * same executable)
	 *
	 * This function MUST end in a slash
	 *
	 * The temp_path MUST released by the caller with delete []
	 *
	 * @param temp_path  Full path of temp folder in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetUpgradeFolder(uni_char **temp_path) = 0;

	/**
	 * Gets the complete path of the executable, this should be the
	 * path to the executable (on Windows) or to the package on (Mac).
	 * e.g. C:\Program Files\Opera\Opera.exe or /Application/Opera.app
	 *
	 * The exe_path MUST released by the caller with delete []
	 *
	 * @param exe_path  Full path of executable in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetExePath(uni_char **exe_path) = 0;

	/**
	 * Gets the complete path to where the Update exectable is for the
	 * platform. This is the file that is copied FROM and then copied TO using
	 * the GetUpdatePath function.
	 *
	 * e.g. On Windows: C:\Program Files\Opera\Opera.exe
	 * or Mac:  /Application/Opera.app/Contents/Resources/Update.exe
	 *
	 * The exe_path MUST released by the caller with delete []
	 *
	 * @param exe_path  Full path of executable in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetUpgradeSourcePath(uni_char **exe_path) = 0;

	/**
	 * Returns if this class should be running as the updater or Opera.
	 * The filename of the executable will be update, Update or Update.exe
	 *
	 * @param is_updater returns true if this is the updater. 
	 *
	 * @return AUF_Status
	 */
	virtual AUFileUtils::AUF_Status IsUpdater(BOOL &is_updater) = 0;

	/**
	 * Gets the name of the update executable. i.e. update, Update or Update.exe
	 *
	 * The name MUST released by the caller with delete []
	 *
	 * @param name Name of the exe in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetUpdateName(uni_char **name) = 0;

	/**
	 * Gets the path to the update executable.
	 *
	 * The name MUST released by the caller with delete []
	 * @see GetUpgradeFolder()
	 * @see GetUpdateName()
	 * 
	 * @param name Path to the exe in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetUpdatePath(uni_char **path) = 0;

	/**
	 * Gets the path to the wait file.
	 *
	 * The name MUST released by the caller with delete []
	 * 
	 * @param name Path to the wait file in UTF16
	 * @param look_in_exec_folder if set to TRUE wait file will be searched in exec folder.
	 *        If set to FALSE wait file will be searched in update (temp) folder.
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status GetWaitFilePath(uni_char** wait_file_path, BOOL look_in_exec_folder = FALSE) { return ERR; }

	/**
	 * Checks if a file exists and retrieves its size if it does
	 *
	 * @param file  Full path of file to get the size of in UTF16
	 * @param size  Size of the file in bytes
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status Exists(uni_char *file, unsigned int &size) = 0;

	/**
	 * Reads the complete contents of a file into a buffer
	 * This read is designed to read an ascii text file into memory
	 * The buffer MUST released by the caller with delete []
	 *
	 * @param file      Full path of file to read in UTF16
	 * @param buffer    Pointer to the buffer to hold the file contents 
	 * @param buf_size  Pointer to size of returned buffer 
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status Read(uni_char *file, char** buffer, size_t& buf_size) = 0;

	/**
	 * Deletes a file from disk
	 *
	 * @param file  Full path of file to delete in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status Delete(uni_char *file) = 0;

	/**
	 * Creates an empty file on disk which can be used for testing in other
	 * instances of Opera or Update running
	 *
	 * @param file  Full path of file to create in UTF16
	 *
	 * @return AUF_Status. 
	 */
	virtual AUF_Status CreateEmptyFile(uni_char *file) = 0;

};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _AUFILEUTILS_H_INCLUDED_
