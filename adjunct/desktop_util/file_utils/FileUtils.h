/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

class PrefsFile;

namespace FileUtils
{
	/**
	 * Check if path points to a directory.
	 *
	 * @param path Path to check.
	 * @return Status.
	 */
	OP_STATUS IsDirectory(const OpStringC& path, BOOL& is_directory);

	/**
	 * Deletes all files in the given directory
	 * @param folder_id folder identifier
	 * @return error code
	 */
	OP_STATUS DeleteFolderFiles(OpFileFolder folder_id);

	/**
	 * Determines path to turbo settings file and sets appropriate file pref.
	 * Path is constructed using latest country code received from the Autoupdate
	 * server if that information is available. Otherwise current region is used.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM
	 */
	OP_STATUS SetTurboSettingsPath();

#ifdef SELFTEST
	// internal function, exposed only for selftests
	bool PathEndsWith(OpStringC path, OpStringC name);
#endif

	/**
	 * Updates desktop-specific folders (subfolders of "custom" and "region" folders)
	 * to match active region and language set in g_languageManager.
	 *
	 * @param force if true folders are updated without checking if language or region changed
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM
	 */
	OP_STATUS UpdateDesktopLocaleFolders(bool force = false);

#ifndef USE_COMMON_RESOURCES
	/**
	 * Translates all of the ini files that have changed names and resets any preferences
	 * if needed.
	 *
	 * Note: this must be called before the Init call to the opera object
	 *
	 * @param pf     pointer to the just opened preference file
	 * @param pinfo  pointer to the opera init info
	 *
	 * @return OpStatus::OK if the translation is completed successfully
	 */
	OP_STATUS TranslateIniFilenames(PrefsFile *pf, OperaInitInfo *pinfo);

	/**
	 * Translates the name of ini files if and when we choose to change them. It will only
	 * change them if the new_filename does not exist and the old_filename does.
	 *
	 * This has become an INTERNAL function and should only be called from TranslateIniFilenames
	 *
	 * @param path          full path to where the file is located
	 * @param old_filename  old name of the file
	 * @param new_filename  new name of the file
	 * @return OpStatus::OK if the translation is completed successfully
	 */
	OP_STATUS TranslateIniFilename(const OpStringC& path, const OpStringC& old_filename, const OpStringC& new_filename);
#endif // USE_COMMON_RESOURCES
};

#endif // FILE_UTILS_H
