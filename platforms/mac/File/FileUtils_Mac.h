/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _FILEUTILS_MAC_H_
#define _FILEUTILS_MAC_H_

class OpFile;

// Main folders that hold the preferences (helps build the full paths easier)
enum MacOpFileFolder
{
	MAC_OPFOLDER_APPLICATION,	// /User/username/Library/Application support/Opera
	MAC_OPFOLDER_CACHES,		// /User/username/Library/Caches/Opera
	MAC_OPFOLDER_PREFERENCES,	// /User/username/Library/Preferences/Opera Preferences
	MAC_OPFOLDER_LIBRARY,		// /User/username/Library

	MAC_OPFOLDER_COUNT
};

class OpFileUtils
{
public:
	static Boolean		ConvertUniPathToFSRef(const uni_char *pathname, FSRef &fsref);
	static OP_STATUS	ConvertFSRefToUniPath(const FSRef *fsref, OpString *outPath);
	static Boolean		ConvertFSRefToURL(const FSRef *fsref, uni_char *url, size_t buflen);
	static Boolean		ConvertURLToUniPath(const uni_char *url, OpString *path);

	static Boolean		GetFriendlyAppName(const uni_char *full_app_name, OpString &nice_app_name);

	static OpStringC	GetPathRelativeToBundleResource(const OpStringC &path);

	static Boolean		FindFolder(OSType osSpecificSpecialFolder, OpString &path, Boolean createFolder = false, short vRefNum = -1);
	static void 		AppendFolder(OpString &path, const OpStringC& customFolder, const uni_char* folder_suffix = NULL);

	static void			InitHomeDir(const char* arg0);
	static Boolean		SetToMacFolder(MacOpFileFolder mac_folder, OpString &folder, const uni_char *profile_name = NULL);
#ifndef USE_COMMON_RESOURCES
	static Boolean		SetToSpecialFolder(OpFileFolder specialFolder, OpString &path);
#endif // !USE_COMMON_RESOURCES
	static OP_STATUS	MakeDir(const uni_char* path);
	static OP_STATUS	MakePath(uni_char* path, uni_char* path_parent = NULL);
	
	static OP_STATUS	ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len, const OpStringC &resource_dir, const OpStringC &largepref_dir, const OpStringC &smallpref_dir);
	static OP_STATUS	SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len, const OpStringC &resource_dir, const OpStringC &largepref_dir, const OpStringC &smallpref_dir);

			// DO NOT USE! DO NOT USE! DO NOT USE! (please)
	static Boolean		ConvertUniPathToFileSpec(const uni_char *pathname, FSSpec &fss);
	static Boolean		ConvertUniPathToVolumeAndId(const uni_char *pathname, short &volume, long &id);
#ifdef WIDGET_RUNTIME_SUPPORT
    // checks if Opera process has write access to /Applications folder
    static OP_STATUS    HasAdminAccess(BOOL &has_admin_access);

	static void			SetOperaBundle(CFURLRef opera_loc) { sOperaLoc = opera_loc; }
	static const CFURLRef GetOperaBundle() { return sOperaLoc; }
#endif //WIDGET_RUNTIME_SUPPORT

	// Hack for DSK-334964:
	static void			UpgradePreferencesFolder();
	static void			LinkOldPreferencesFolder();

private:
	static Boolean		homeValid;
	static OpString		sResourcesPath;
	static OpAutoINT32HashTable<OpString> sPaths;
	static OpString		m_profile_name;
	static OpString		m_profile_fullpath;			// Full path for the profile if used with -pd or -createsingleprofile
	static Boolean		m_single_profle_checked;	// Set once the the single profile check has been done for optimisation
	static Boolean		m_next_checked;				// Set after the next file check is done
	static CFURLRef		sOperaLoc;
public:
	static FSRef		contentsFSRef;
};

#endif // _FILEUTILS_MAC_H_

