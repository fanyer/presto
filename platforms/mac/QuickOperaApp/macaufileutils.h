/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author
 */

#ifndef _MACAUFILEUTILS_H_INCLUDED_
#define _MACAUFILEUTILS_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#define AUTOUPDATE_MAC_UPDATE_EXE_NAME "Update.app"

#include "adjunct/autoupdate/updater/pi/aufileutils.h"

class MacAUFileUtils : public AUFileUtils
{
public:
	virtual ~MacAUFileUtils() {}

	/**
	 * Gets the location of the temp folder. Must match what Opera
	 * does on your platform
	 * The temp_path MUST released by the caller with delete []
	 *
	 * @param temp_path  Full path of temp folder in UTF16
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status GetUpgradeFolder(uni_char **temp_path);

	/**
	 * Gets the complete path of the package or exe
	 * e.g. C:\Program Files\Opera\Opera.exe or /Application/Opera.app
	 *
	 * The exe_path MUST released by the caller with delete []
	 *
	 * @param exe_path  Full path of executable in UTF16
	 * @param executable Set for platforms where the actual executable is inside a package if you
	 *						want the actual path to the executable.
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status GetExePath(uni_char **exe_path);

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
	virtual AUF_Status GetUpgradeSourcePath(uni_char **exe_path);

	/**
	 * Returns if this class should be running as the updater or Opera.
	 * The filename of the executable will be update, Update or Update.exe
	 *
	 * The temp_path MUST released by the caller with delete []
	 *
	 * @return BOOL True if this is the updater.
	 */
	virtual AUF_Status IsUpdater(BOOL &is_updater);

	/**
	 * Gets the name of the update executable. i.e. update, Update or Update.exe
	 *
	 * The name MUST released by the caller with delete []
	 *
	 * @param name Name of the exe in UTF16
	 *
	 * @return BOOL True if this is the updater.
	 */
	virtual AUF_Status GetUpdateName(uni_char **name);

	/**
	 * Gets the path to the update executable.
	 *
	 * The name MUST released by the caller with delete []
	 * @see GetUpgradeFolder()
	 * @see GetUpdateName()
	 *
	 * @param name Path to the exe in UTF16
	 * @param executable Set for platforms where the actual executable is inside a package if you
	 *						want the actual path to the executable.
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status GetUpdatePath(uni_char **path);

	/**
	 * Checks if a file exists and retrieves its size if it does
	 *
	 * @param file  Full path of file to get the size of in UTF16
	 * @param size  Size of the file in bytes
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status Exists(uni_char *file, unsigned int &size);

	/**
	 * Reads the complete contents of a file into a buffer
	 * This read is designed to read an ascii text file into memory
	 * The buffer MUST released by the caller with delete []
	 *
	 * @param file   Full path of file to read in UTF16
	 * @param buffer Pointer to the buffer to hold the file contents
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status Read(uni_char *file, char** buffer, size_t& buf_size);

	/**
	 * Deletes a file from disk
	 *
	 * @param file  Full path of file to delete in UTF16
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status Delete(uni_char *file);

	/**
	 * Creates an empty file on disk which can be used for testing in other
	 * instances of Opera or Update running
	 *
	 * @param file  Full path of file to create in UTF16
	 *
	 * @return AUF_Status.
	 */
	virtual AUF_Status CreateEmptyFile(uni_char *file);

private:
	// Internal function to get the path to the Opera.app bundle or Opera executable
	// exe_path must already be allocated and have MAX_PATH space
	AUF_Status GetExePathUTF8(char *exe_path);
	AUF_Status GetUpgradeFolderUTF8(char *temp_path_8);
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _MACAUFILEUTILS_H_INCLUDED_
