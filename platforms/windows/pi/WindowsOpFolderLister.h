/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPFOLDERLISTER_H
#define WINDOWSOPFOLDERLISTER_H

#include "modules/pi/system/OpFolderLister.h"

class WindowsOpFolderLister : public OpFolderLister
{
public:
	WindowsOpFolderLister();
	~WindowsOpFolderLister();

	OP_STATUS Construct(const uni_char* path, const uni_char* pattern);

	BOOL Next();
	const uni_char* GetFileName() const;
	const uni_char* GetFullPath() const;
	OpFileLength GetFileLength() const;

	BOOL IsFolder() const;

private:
	void UpdateFullPath();
	/**
	 * Function that uses the supplied pattern to search for files and directories
     *
	 * @param handle [out] Gives the status of the search handle. 
	 * @param path_pattern [in] The supplied pattern to search for.
	 * @param data [out] The data for the found element.
	 * @return True if there is another file to get, false if all files have been visited.
	 */ 
	BOOL SearchForFirst(HANDLE &handle, const OpString &path_pattern, WIN32_FIND_DATA &data);

	/**
	 * Function that uses the supplied pattern to search for files and directories
     *
	 * @param handle [in] Gives the status of the search handle. 
	 * @param data [out] The data for the found element.
	 * @return True if there is another file to get, false if all files have been visited.
	 */ 
	BOOL SearchForNext(const HANDLE &handle, WIN32_FIND_DATA &data);

	WIN32_FIND_DATA m_find_data;
	HANDLE m_find_handle;
	OpString m_path_pattern;
	uni_char* m_path;
	int m_path_length;
	DWORD m_drives;
	int m_drive;
};

#endif // !WINDOWSOPFOLDERLISTER_H
