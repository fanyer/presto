/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include <sys/stat.h>

#include "platforms/mac/QuickOperaApp/macaufileutils.h"

#include "adjunct/desktop_util/string/string_convert.h"

/////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils* AUFileUtils::Create()
{
	// Can't use OP_NEW as this is used outside of core
	return new MacAUFileUtils();
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::GetUpgradeFolder(uni_char **temp_path)
{
	char temp_path_8[MAX_PATH+1];

	*temp_path = NULL;

	if (GetUpgradeFolderUTF8(temp_path_8) == AUFileUtils::OK)
	{
//	printf("GetUpgradeFolder %s\n", (char*)temp_path_8);
		uni_char* uc = StringConvert::UTF16FromUTF8(temp_path_8);
		if (uc)
		{
			*temp_path = uc;
			return AUFileUtils::OK;
		}
	}
	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

MacAUFileUtils::AUF_Status MacAUFileUtils::GetExePath(uni_char **exe_path)
{
	char temp_path_8[MAX_PATH+1];

	*exe_path = NULL;

	// Just get the exe path and then convert it to UTF16
	if (GetExePathUTF8(temp_path_8) == AUFileUtils::OK)
	{
//	printf("GetExePath %s\n", (char*)temp_path_8);
		uni_char* uc = StringConvert::UTF16FromUTF8(temp_path_8);
		if (uc)
		{
			*exe_path = uc;
			return AUFileUtils::OK;
		}
	}

	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

MacAUFileUtils::AUF_Status MacAUFileUtils::GetUpgradeSourcePath(uni_char **exe_path)
{
	char temp_path_8[MAX_PATH+1];

	*exe_path = NULL;

	if (GetExePathUTF8(temp_path_8) == AUFileUtils::OK)
	{
		// Get the path inside the package to the Update.app
		strcat(temp_path_8, "/Contents/Resources/");
		strcat(temp_path_8, AUTOUPDATE_MAC_UPDATE_EXE_NAME);
//	printf("GetUpgradeSourcePath %s\n", (char*)temp_path_8);
		uni_char* uc = StringConvert::UTF16FromUTF8(temp_path_8);
		if (uc)
		{
			*exe_path = uc;
			return AUFileUtils::OK;
		}
	}

	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::IsUpdater(BOOL &is_updater)
{
	is_updater = FALSE;
	char temp_path_8[MAX_PATH+1], *p = NULL;

	// Get ful path and what the name should be
	if (GetExePathUTF8(temp_path_8) == AUFileUtils::OK)
	{
		// The name of the exe is at the end
		if ((p = strrchr(temp_path_8, '/') + 1) != NULL)
		{
			// Check if the name is what it should be
			if (!strcmp(p, AUTOUPDATE_MAC_UPDATE_EXE_NAME))
				is_updater = TRUE;
		}
	}
	else
	{
		return ERR;
	}

	return OK;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::GetUpdateName(uni_char **name)
{
	// Allocate the memory for the name
	*name = new uni_char[sizeof(UNI_L(AUTOUPDATE_MAC_UPDATE_EXE_NAME))+1];
	if (!(*name))
		return AUFileUtils::ERR_MEM;

	// Memcpy to save messing around
	memcpy(*name, UNI_L(AUTOUPDATE_MAC_UPDATE_EXE_NAME), sizeof(UNI_L(AUTOUPDATE_MAC_UPDATE_EXE_NAME)));

	return AUFileUtils::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::GetUpdatePath(uni_char **path)
{
	char temp_path_8[MAX_PATH+1];

	*path = NULL;

	if (GetUpgradeFolderUTF8(temp_path_8) == AUFileUtils::OK)
	{
		// We know it ends in a slash so add on the Update name
		strcat(temp_path_8, AUTOUPDATE_MAC_UPDATE_EXE_NAME);
		uni_char* uc = StringConvert::UTF16FromUTF8(temp_path_8);
		if (uc)
		{
			*path = uc;
			return AUFileUtils::OK;
		}
	}

	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::Exists(uni_char *file, unsigned int &size)
{
	char* mbc = StringConvert::UTF8FromUTF16(file);
	struct stat r;
	int result = stat(mbc, &r);
	delete [] mbc;
	if (result == 0) {
		size = r.st_size;
		return AUFileUtils::OK;
	}
	int err = errno;
	if (err == ENOENT || err == ENOTDIR)
		return AUFileUtils::FILE_NOT_FOUND;
	if (err == EACCES || err == EROFS || err == EPERM)
		return AUFileUtils::NO_ACCESS;
	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::Read(uni_char *file, char** buffer, size_t& buf_size)
{
	if(!buffer)
		return AUFileUtils::ERR;

	*buffer = NULL;

	char* mbc = StringConvert::UTF8FromUTF16(file);
	FILE* f = fopen(mbc, "r");
	if (f)
	{
		struct stat r;
		int result = stat(mbc, &r);
		if (result == 0)
		{
			int filesize = r.st_size;
			*buffer = new char[filesize];
			if(!*buffer)
			{
				fclose(f);
				return AUFileUtils::ERR;
			}
			int bytesread = fread(*buffer, 1, filesize, f);
			if (bytesread == filesize)
			{
				buf_size = filesize;
				delete [] mbc;
				fclose(f);
				return AUFileUtils::OK;
			}
		}
		fclose(f);
	}
	delete [] *buffer;
	delete [] mbc;
	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::Delete(uni_char *file)
{
	char* mbc = StringConvert::UTF8FromUTF16(file);

	errno = 0;
	if (access(mbc, F_OK) == 0)
	{
		struct stat st;
		if (0 == stat(mbc, &st) && // existed and
			0 == (S_ISDIR(st.st_mode) // is a directory
				  ? rmdir(mbc) // so we need rmdir
				  : unlink(mbc))) // else unlink should suffice
		{
			delete [] mbc;
			return AUFileUtils::OK;
		}
	}

	delete [] mbc;

	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

AUFileUtils::AUF_Status MacAUFileUtils::CreateEmptyFile(uni_char *file)
{
	char* mbc = StringConvert::UTF8FromUTF16(file);

	FILE* f;
	if ((f = fopen(mbc, "w")) == 0)
		fclose(f);

	delete [] mbc;

	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

MacAUFileUtils::AUF_Status MacAUFileUtils::GetExePathUTF8(char *exe_path)
{
	ProcessSerialNumber psn;
	FSRef				ref_app;

	// Get this process info
	if (GetCurrentProcess(&psn) == noErr)
	{
		// Get the path to this process
		if (GetProcessBundleLocation(&psn, &ref_app) == noErr)
		{
			// Get the path to the app converted from a fsref
			if (FSRefMakePath(&ref_app, (UInt8*)exe_path, MAX_PATH+1) == noErr)
				return AUFileUtils::OK;
		}
	}
	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

MacAUFileUtils::AUF_Status MacAUFileUtils::GetUpgradeFolderUTF8(char *temp_path_8)
{
	FSRef ref;

	if (FSFindFolder(kUserDomain, kUserSpecificTmpFolderType, true, &ref) == noErr)
	{
		if (FSRefMakePath(&ref, (UInt8*)temp_path_8, MAX_PATH+1) == noErr)
		{
			// Add "Opera" to the path
			strcat(temp_path_8, "/Opera/");
			char *p = &temp_path_8[strlen(temp_path_8)];

			char temp_path_8_2[MAX_PATH+1];

			// Add on the path to this exe so different applications will not clash
			if (GetExePathUTF8(temp_path_8_2) == AUFileUtils::OK)
			{
				UINT32	hashval = 5381;
				char	*str = temp_path_8_2;

				while (*str)
					hashval = ((hashval << 5) + hashval) + *str++; // hash*33 + c

				// Add the hash vaule and make sure it ends it a slash!
				sprintf(p, "%u/", hashval);

				return AUFileUtils::OK;
			}
		}
	}
	return AUFileUtils::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

