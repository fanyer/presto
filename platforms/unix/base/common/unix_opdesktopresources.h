/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef _UNIX_OP_DESKTOPRESOURCES_H_
#define _UNIX_OP_DESKTOPRESOURCES_H_

#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"

class OpFolderLister;

class UnixOpDesktopResources: public OpDesktopResources
{
public:
	struct Folders
	{
		OpString base;
		OpString read;
		OpString write;
		OpString binary;
		OpString home;
		OpString fixed;
		OpString tmp;
		OpString locale;
		OpString desktop;
		OpString download;
		OpString documents;
		OpString music;
		OpString pictures;
		OpString videos;
		OpString public_folder;
		OpString update_checker;
#ifdef WIDGET_RUNTIME_SUPPORT
		OpString gadgets;
#endif // WIDGET_RUNTIME_SUPPORT
	};

public:
	~UnixOpDesktopResources();
	
	OP_STATUS Init();
	OP_STATUS GetResourceFolder(OpString &folder);
	OP_STATUS GetBinaryResourceFolder(OpString &folder);
	OP_STATUS GetFixedPrefFolder(OpString &folder);
	OP_STATUS GetGlobalPrefFolder(OpString &folder);


	OP_STATUS GetLargePrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	OP_STATUS GetSmallPrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	OP_STATUS GetTempPrefFolder(OpString &folder, const uni_char *profile_name = NULL);

	OP_STATUS GetTempFolder(OpString &folder);

	OP_STATUS GetDesktopFolder(OpString &folder);
	OP_STATUS GetDocumentsFolder(OpString &folder);
	OP_STATUS GetDownloadsFolder(OpString &folder);
	OP_STATUS GetPicturesFolder(OpString &folder);
	OP_STATUS GetVideosFolder(OpString &folder);
	OP_STATUS GetMusicFolder(OpString &folder);
	OP_STATUS GetDefaultShareFolder(OpString &folder);
	OP_STATUS GetPublicFolder(OpString &folder);

#ifdef WIDGET_RUNTIME_SUPPORT
	OP_STATUS GetGadgetsFolder(OpString &folder);
#endif // WIDGET_RUNTIME_SUPPORT
	OP_STATUS GetUpdateCheckerPath(OpString &checker_path);

	OP_STATUS GetLocaleFolder(OpString &folder);
	OpFolderLister* GetLocaleFolders(const uni_char* locale_root);
	OP_STATUS GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code = UNI_L(""));
	OP_STATUS GetLanguageFileName(OpString& file_name, OpStringC lang_code = UNI_L(""));
	OP_STATUS GetCountryCode(OpString& country_code);
	OP_STATUS EnsureCacheFolderIsAvailable(OpString &folder);
	OP_STATUS ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len);
	OP_STATUS SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len);
	BOOL IsSameVolume(const uni_char* path1, const uni_char* path2);
	OP_STATUS GetOldProfileLocation(OpString& old_profile_path);

	/**
	 * Return the home folder as prepared in @ref Init()
	 *
	 * @param folder home folder is returned here
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS GetHomeFolder(OpString &folder);

	/**
	 * Expand system variables in string and strips "'s
	 *
	 * @param folder source folder to examine
	 * @param target target string where modified is saved on return
	 *
	 * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS PrepareFolder(const OpStringC& folder, OpString& target);

	/**
	 * Set desktop, download, documents, music, pictures, videos and public_folder 
	 * of the 'folders' argument from what is defined in XDG_CONFIG_HOME/user_dirs.dirs, 
	 * or if XDG_CONFIG_HOME variable is not set, in ~/.config/user-dirs.dirs
	 * If none of these are available, the folders are not set.
	 *
	 * Folders::home must be set before this function is called
	 *
	 * @param folders. Detected folders are place here
	 *
	 * @return OpStatus::OK (even if nothing has been set) on success, otherwise OpStatus::ERR_NO_MEMORY on OOM
	 */
	static OP_STATUS SetupPredefinedFolders(Folders& folders);

private:
	OP_STATUS Normalize(OpString &path);
	OP_STATUS AppendPathSeparator(OpString& path);

	OpString m_language_code;
	static Folders m_folder;
};

#endif
