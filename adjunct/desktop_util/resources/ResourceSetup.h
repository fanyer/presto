/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef RESOURCE_SETUP_H
#define RESOURCE_SETUP_H

#include "adjunct/desktop_util/resources/ResourceDefines.h"

class OperaInitInfo;
class PrefsFile;

namespace ResourceSetup
{
	/**
	 * Copies a default file from the pack and into the user prefs.
	 * Note this will try to copy first from the locale version of the file
	 * and then from the non-locale version
	 * This will ONLY copy the file if the destination file does not exist
	 *
	 * @param filename_source	Name of the source file (name only no path)
	 * @param filename_dest		Name of the destination file (name only no path)
	 * @param pinfo				pointer to the opera init info (has folder information)
	 *
	 * @return OpStatus::OK if the files already exists or is copied successfully
	 */
	OP_STATUS CopyDefaultFile(const OpStringC &filename_source, const OpStringC &filename_dest, OperaInitInfo *pinfo);

	/**
	 * Find a default file in the pack.
	 *
	 * In addition to folders with default prefs files, visited also by GetDefaultPrefsFile, this function
	 * looks into *_CUSTOM_PACKAGE_FOLDER folders. This means that it can locate file in "custom" folder
	 * even before that folder is copied into profile.
	 *
	 * @param filename		Name of the source file (name only no path)
	 * @param file			The file that is found
	 * @param pinfo			pointer to the opera init info (has folder information), if NULL will use g_desktop_bootstrap->GetInitInfo()
	 *
	 * @return OpStatus::OK if file was found, OpStatus::ERR_FILE_NOT_FOUND if file was not found, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetPackageFile(const OpStringC &filename, OpFile& file, OperaInitInfo *pinfo = NULL);

	/**
	 * Callback used by GetDefaultPrefsFile to check if Opera can load a prefs file from given location.
	 */
	class PrefsFileChecker
	{
	public:
		virtual ~PrefsFileChecker() {}

		/**
		 * Check if a pref file exists and is valid.
		 *
		 * @param folder folder of the file
		 * @param filename name of the file
		 * @param is_valid result of file check
		 *
		 * @return status of call. If this function returns OK, then result of file check is stored in is_valid.
		 */
		virtual OP_STATUS CheckFile(OpFileFolder folder, const OpStringC& filename, BOOL& is_valid) = 0;
	};

	/**
	 * Get default preference file (operaprefs.ini, search.ini, bookmarks.adr, etc.).
	 *
	 * This function is similar to GetPackageFile, but it doesn't look into *_CUSTOM_PACKAGE_FOLDER folders.
	 * OpFile::Construct is used to create paths, so Core must be initialized.
	 *
	 * @param filename name of the file
	 * @param file the file that is found
	 * @param checker (optional) called to check if file can be loaded from given location (if NULL this function only checks if file exists)
	 * @param is_custom (optional) is set to true if file was found in one of *_CUSTOM_FOLDER folders, false otherwise
	 *
	 * @return OpStatus::OK if file was found, OpStatus::ERR_FILE_NOT_FOUND if file was not found, OpStatus::ERR_NO_MEMORY on 
	 */
	OP_STATUS GetDefaultPrefsFile(const OpStringC& filename, OpFile& file, PrefsFileChecker* checker = NULL, bool* is_custom = NULL);

	/**
	 * Copy over the custom folder tree from the package into the user prefs.
	 * This will handle updating when required as well
	 *
	 * @param pinfo		pointer to the opera init info (has folder information)
	 *
	 * @return OpStatus::OK if the folder already exists or is copied successfully
	 */
	OP_STATUS CopyCustomFolders(OperaInitInfo *pinfo);

#ifdef GADGET_SUPPORT
#ifndef WIDGET_RUNTIME_SUPPORT
	/**
	 * Installs any custom gadgets that are included with the package
	 * This function can only be called after the opera->Init call as it needs
	 * the hotlist
	 *
	 * @return OpStatus::OK if installed successfully
	 */
	OP_STATUS InstallGadgets();
#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_SUPPORT

	/**
	 * Installs any custom unite services that are included with the package
	 * This function can only be called after the opera->Init call as it needs
	 * the hotlist
	 *
	 * @return OpStatus::OK if installed successfully
	 */
	OP_STATUS InstallUniteServices();
};

#endif // RESOURCE_SETUP_H
