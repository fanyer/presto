/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file declares the WindowsAUFileUtils interface which is
 * a minimal set of functions required to apply updates
 */

#ifndef _WINDOWSAUFILEUTILS_H_INCLUDED_
#define _WINDOWSAUFILEUTILS_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/updater/pi/aufileutils.h"

class WindowsAUFileUtils : public AUFileUtils
{
public:
	AUF_Status GetUpgradeFolder(uni_char **temp_path) { return GetUpgradeFolder(temp_path, FALSE); }
	AUF_Status GetExePath(uni_char **exe_path);
	AUF_Status GetUpgradeSourcePath(uni_char **exe_path) { return GetExePath(exe_path); }
	AUF_Status IsUpdater(BOOL &is_updater);
	AUF_Status GetUpdateName(uni_char **name);
	AUF_Status GetUpdatePath(uni_char **path) { return GetUpgradeFolder(path, TRUE); }
	AUF_Status GetWaitFilePath(uni_char **wait_file_path, BOOL look_in_exec_folder = FALSE);
	AUF_Status Exists(uni_char *file, unsigned int &size);
	AUF_Status Read(uni_char *file, char** buffer, size_t& buf_size);
	AUF_Status Delete(uni_char *file);
	AUF_Status CreateEmptyFile(uni_char *file);

private:
	AUF_Status ConvertError();
	AUF_Status ConvertError(DWORD error_code);
	AUF_Status GetUpgradeFolder(uni_char **temp_path, BOOL with_exe_name);

};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // _WINDOWSAUFILEUTILS_H_INCLUDED_
