/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "WindowsAUFileUtils.h"

#define UPDATE_EXECUTABLE_NAME "OperaUpgrader.exe"

#include "win_file_utils.h"


AUFileUtils* AUFileUtils::Create()
{
	return new WindowsAUFileUtils;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::GetUpgradeFolder(uni_char **temp_path, BOOL with_exe_name)
{
	// returns a subdirectory of the Windows Temp Path,
	// where the name of that subdirectory is the Opera installation path
	// without the colon and slashes
 
	uni_char tempdir[MAX_PATH*2];
	uni_char *path_end = tempdir;

	if (!GetTempPath(MAX_PATH, path_end))
		return ERR;

	do { path_end++; } while (*path_end);

	if (*(path_end-1) != '\\')
		*path_end++ = '\\';

	if (!GetModuleFileName(NULL, path_end, MAX_PATH))
		return ERR;

	uni_char *filename = uni_strrchr(path_end, '\\');

	uni_char *path_pos = path_end;
	while (path_pos < filename)
	{
		if (*path_pos != '\\' && *path_pos != ':')
			*path_end++ = *path_pos;

		path_pos++;
	}
	*path_end++ = '\\';
	*path_end = 0;

	UINT path_len = path_end-tempdir;

	*temp_path = new uni_char[path_len  + (with_exe_name ? ARRAY_SIZE(UNI_L(UPDATE_EXECUTABLE_NAME)) : 1)];
	if (!*temp_path)
		return ERR_MEM;

	uni_strcpy(*temp_path, tempdir);

	if (with_exe_name)
		uni_strcpy(*temp_path + path_len, UNI_L(UPDATE_EXECUTABLE_NAME));

	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::GetExePath(uni_char **exe_path)
{
	uni_char fullpath[MAX_PATH];

	DWORD length = GetModuleFileName(NULL, fullpath, MAX_PATH);
	if (!length)
		return ERR;

	*exe_path = new uni_char[length+1];
	if (!*exe_path)
		return ERR_MEM;

	uni_strcpy(*exe_path, fullpath);

	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::GetWaitFilePath(uni_char **wait_file_path, BOOL look_in_exec_folder)
{
	uni_char* folder_to_search = NULL;
	AUF_Status status = ERR;
	if(look_in_exec_folder)
	{
		do
		{
			status = GetExePath(&folder_to_search);
			if(status != OK)
				break;
			
			int path_sep_char_index = uni_strlen(folder_to_search) - 1;
			for(; path_sep_char_index >= 0; --path_sep_char_index)
			{
				if(folder_to_search[path_sep_char_index] == UNI_L(PATHSEPCHAR))
					break;
			}
			if(path_sep_char_index < 0)
			{
				OP_DELETEA(folder_to_search);
				status = ERR;
				break;
			}
			folder_to_search[path_sep_char_index + 1] = UNI_L('\0');
		} while(0);
	}
	else
	{
		status = GetUpgradeFolder(&folder_to_search, FALSE);
	}
	if(status != OK)
		return status;
	int buffer_size = uni_strlen(folder_to_search) + uni_strlen(AUTOUPDATE_UPDATE_WAITFILE) + 1;
	*wait_file_path = OP_NEWA(uni_char, buffer_size);
	if(!*wait_file_path)
	{
		OP_DELETEA(folder_to_search);
		return ERR_MEM;
	}
	uni_strcpy(*wait_file_path, folder_to_search);
	uni_strcat(*wait_file_path, AUTOUPDATE_UPDATE_WAITFILE);
	OP_DELETEA(folder_to_search);
	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::IsUpdater(BOOL &is_updater)
{
	is_updater = FALSE;
	uni_char fullpath[MAX_PATH];
			
	if (!GetModuleFileName(NULL, fullpath, MAX_PATH))
		return ERR;

	uni_char *filename = uni_strrchr(fullpath, '\\');
	if (!filename)
		filename = fullpath;
	else
		filename++;

	uni_char *updater_name = UNI_L(UPDATE_EXECUTABLE_NAME);

	// inlined uni_stricmp, making it available here from the stdlib module would be a major PITA
	while (*updater_name)
	{
		if ((*filename | 0x20) != (*updater_name | 0x20))
			return OK;

		filename++;
		updater_name++;
	}

	is_updater = TRUE;
	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::GetUpdateName(uni_char **name)
{
	*name = new uni_char[ARRAY_SIZE(UNI_L(UPDATE_EXECUTABLE_NAME))];
	if (!*name)
		return ERR_MEM;

	uni_strcpy(*name, UNI_L(UPDATE_EXECUTABLE_NAME));

	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::ConvertError()
{
	switch (GetLastError())
	{
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return FILE_NOT_FOUND;

	case ERROR_ACCESS_DENIED:
		return NO_ACCESS;
	
	case ERROR_NOT_ENOUGH_MEMORY:
		return ERR_MEM;
	}
	return ERR;
}


AUFileUtils::AUF_Status WindowsAUFileUtils::ConvertError(DWORD error_code)
{
	switch (error_code)
	{
	case ERROR_SUCCESS:
		return AUFileUtils::OK;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return FILE_NOT_FOUND;

	case ERROR_ACCESS_DENIED:
		return NO_ACCESS;
	
	case ERROR_NOT_ENOUGH_MEMORY:
		return ERR_MEM;
	}
	return ERR;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::Exists(uni_char *file, unsigned int &size)
{
	WIN32_FIND_DATA findfiledata;

	HANDLE handle = FindFirstFile(file, &findfiledata);
	if (handle == INVALID_HANDLE_VALUE)
		return ConvertError();

	size = findfiledata.nFileSizeLow;
	FindClose(handle);
	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::Read(uni_char *file, char** buffer, size_t& buf_size)
{
	return ConvertError(WinFileUtils::Read(file,buffer,buf_size));
}

AUFileUtils::AUF_Status WindowsAUFileUtils::Delete(uni_char *file)
{
	if (!DeleteFile(file))
		return ConvertError();

	return OK;
}

AUFileUtils::AUF_Status WindowsAUFileUtils::CreateEmptyFile(uni_char *file)
{
	HANDLE handle = CreateFile(file, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
		return ConvertError();

	CloseHandle(handle);

	return OK;
}

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT
