/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef MACOPDESKTOPRESOURCES_H
#define MACOPDESKTOPRESOURCES_H

#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"

class OpFolderLister;

class MacOpDesktopResources : public OpDesktopResources
{
public:
	virtual ~MacOpDesktopResources() {}
	
	virtual OP_STATUS GetResourceFolder(OpString &folder);
	virtual OP_STATUS GetBinaryResourceFolder(OpString &folder) { return GetResourceFolder(folder); }
	virtual OP_STATUS GetFixedPrefFolder(OpString &folder);
	virtual OP_STATUS GetGlobalPrefFolder(OpString &folder);
	virtual OP_STATUS GetLargePrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	virtual OP_STATUS GetSmallPrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	virtual OP_STATUS GetTempPrefFolder(OpString &folder, const uni_char *profile_name = NULL);
	virtual OP_STATUS GetTempFolder(OpString &folder);
	virtual OP_STATUS GetDesktopFolder(OpString &folder);
	virtual OP_STATUS GetDocumentsFolder(OpString &folder);
	virtual OP_STATUS GetDownloadsFolder(OpString &folder);
	virtual OP_STATUS GetPicturesFolder(OpString &folder);
	virtual OP_STATUS GetVideosFolder(OpString &folder);
	virtual OP_STATUS GetMusicFolder(OpString &folder);
	virtual OP_STATUS GetPublicFolder(OpString &folder);
	virtual OP_STATUS GetDefaultShareFolder(OpString &folder);
#ifdef WIDGET_RUNTIME_SUPPORT	
	virtual OP_STATUS GetGadgetsFolder(OpString &folder);
	OP_STATUS GetGadgetsFolder(OpString &folder, BOOL create);
#endif //WIDGET_RUNTIME_SUPPORT	
	virtual OP_STATUS GetLocaleFolder(OpString &folder);
	virtual OpFolderLister* GetLocaleFolders(const uni_char* locale_root);
	virtual OP_STATUS GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code = UNI_L(""));
	virtual OP_STATUS GetLanguageFileName(OpString& file_name, OpStringC lang_code = UNI_L(""));
	virtual OP_STATUS GetCountryCode(OpString& country_code);
	virtual OP_STATUS EnsureCacheFolderIsAvailable(OpString &folder);
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len);
	virtual OP_STATUS SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len);
	virtual BOOL	  IsSameVolume(const uni_char* path1, const uni_char* path2);
	virtual OP_STATUS GetOldProfileLocation(OpString& old_profile_path);
	void FixBrokenSandboxing();
    virtual OP_STATUS GetUpdateCheckerPath(OpString &checker_path);
private:
	bool SandboxPrefsExistWithIsNewestVersionLowerThan12();
	bool PrefsInCommonLocation();
	OpString m_language_code;
};

#endif // MACOPDESKTOPRESOURCES_H
