/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef RESOURCE_UPGRADE_H
#define RESOURCE_UPGRADE_H

class PrefsFile;

namespace ResourceUpgrade
{
	struct PrefsResourceFileUpgrade {
		const char* section;
		const char* key;
		const uni_char* olddefaultpath;
		OpFileFolder olddefaultparent;
		const uni_char* newpath;
		OpFileFolder newparent;
	};

	static const PrefsResourceFileUpgrade mResourceUpgrades[] = {
        {NULL, NULL, UNI_L("pstorage"), OPFILE_HOME_FOLDER, UNI_L("pstorage"), OPFILE_LOCAL_HOME_FOLDER},
#if defined(_MACINTOSH_)
# include "platforms/mac/macresources.inc"
#elif defined (MSWIN)
# include "platforms/windows/windowsresources.inc"
#elif defined(_UNIX_DESKTOP_)
# include "platforms/unix/base/common/unix_upgrade_resources.inc"		
#endif
		{NULL, NULL, NULL, OPFILE_ABSOLUTE_FOLDER, NULL, OPFILE_ABSOLUTE_FOLDER}
	};

	/**
	 * Tries to create a new (Opera 10) preference file, based on an old pref file.
	 * Mostly this means copying it, but it also involves changing the paths of all resources
	 * that have changed location.
	 * Does nothing if a new preference file already exists.
	 * Does nothing if no old preferences could be located.
	 * @param old_filename the name of the old file in OPFILE_USERPREFS_FOLDER to be upgraded
	 * @param new_filename the name of the updated prefs file to be written.
	 * @param pinfo init structure containing default paths.
	 * @return OpStatus::OK if nothing needed to be updated or update was successful.
	*/
	OP_STATUS UpdatePref(const uni_char* old_filename, const uni_char* new_filename, OperaInitInfo *pinfo);

	/**
	 * Copy old resources to new locations based on information in mResourceUpgrades
	 * @param pinfo init structure containing default paths.
	 * @return OpStatus::OK if nothing needed to be updated or update was successful.
	*/
	OP_STATUS CopyOldResources(OperaInitInfo *pinfo);

	/**
	 * Removes the Language file preference from a preference file
	 * if the language file is earlier than version 811 as these
	 * language files are no longer compatiable with Opera
	 *
	 * @param pf  Prefs file to search for the language entry
	 *
	 * @return OpStatus::OK if no entry found or entry successfully deleted
	 */
	OP_STATUS RemoveIncompatibleLanguageFile(PrefsFile *pf);
};

#endif // RESOURCE_UPGRADE_H
