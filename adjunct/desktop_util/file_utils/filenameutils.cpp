/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/str.h"
#include "modules/formats/uri_escape.h"

OP_STATUS ConvertURLtoFullPath(OpString& filename)
{
	const uni_char * prefix = 0;

#if defined(MSWIN)
	prefix = UNI_L("file://localhost/");
#else
	prefix = UNI_L("file://localhost");
#endif

	if(!prefix)
	{
		return OpStatus::OK;
	}

	if(0 == filename.Find(prefix))
	{
		unsigned int len = uni_strlen(prefix);
		filename.Set(filename.SubString(len));
	}

	// Remove any HTML encoded characters since this is a local file
	int len = filename.Length();
	if (len)
	{
		UriUnescape::ReplaceChars(filename.CStr(), len, UriUnescape::LocalfileUtf8);
		filename.CStr()[len] = 0;
	}

#ifdef MSWIN
	for (int i=0;filename[i] != UNI_L('\0');i++)
	{
		if (filename[i] == UNI_L('/'))
		{
			filename[i] = UNI_L('\\');
		}
	}
#endif //MSWIN

	return OpStatus::OK;
}

OP_STATUS ConvertFullPathtoURL(OpString& URL, const uni_char * file_name)
{
	if(!file_name)
		return OpStatus::ERR_NULL_POINTER;

	if(!URL.Reserve(uni_strlen(file_name)*3+1))  //Worst case
		return OpStatus::ERR_NO_MEMORY;

	UriEscape::Escape(URL.CStr(), file_name, UriEscape::Filename);
	
	const uni_char * prefix = 0;
#ifdef MSWIN
	// The file_name might already have a starting slash (happens with local path autocompletion)
	if (URL[0] != UNI_L('/'))
		prefix = UNI_L("file://localhost/");
	else
#endif //MSWIN
	prefix = UNI_L("file://localhost");
	

	URL.Insert(0, prefix);

	return OpStatus::OK;
}

OP_STATUS GetDirFromFullPath(OpString& full_path)
{
	OP_ASSERT(full_path.CStr());

	int index = full_path.FindLastOf(PATHSEPCHAR);

	if (index != -1)
		full_path.Delete(index + 1);
		
	return index != -1 ? OpStatus::OK : OpStatus::ERR;
}


OP_STATUS GetDirFromFullPath(const uni_char* full_path, OpString& path)
{
	OP_ASSERT(full_path);

	if (!full_path)
		return OpStatus::ERR;

	if (OpStatus::IsError(path.Set(full_path)))
		return OpStatus::ERR;
	
	int index = path.FindLastOf(PATHSEPCHAR);

	if (index != -1)
		path.Delete(index + 1);
		
	return index != -1 ? OpStatus::OK : OpStatus::ERR;
}

BOOL HasTrailingSlash(const uni_char* path)
{
	OP_ASSERT(path && *path);

	if (!path || !*path)
		return FALSE;

	int len = uni_strlen(path) - 1;

	return uni_strcmp(path + len, UNI_L(PATHSEP)) == 0;
}

BOOL HasExtension(const uni_char* file, const char* extension)
{
	if (!file || !*file || !extension || !*extension)
		return FALSE;

	const uni_char* dot = uni_strrchr(file, '.');

	if (!dot)
		return FALSE;

	return uni_stricmp(dot, extension) == 0;
}