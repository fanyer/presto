/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

/**
	Convert between X and classic path styles. In particular how volume names are handeled.
	in Mac OS X, boot volume is / while other volumes are /Volumes/volume_name/
	in Mac OS 9, Volume name was always listed.
*/
#include "platforms/mac/util/PathConverter.h"

#ifndef MAX_PATH
#define MAX_PATH		_MAX_PATH
#endif

void CopyProtocolInformation(uni_char* &dest, const uni_char* &inPath)
{
	if (0 == uni_strncmp(inPath, UNI_L("file://"), 7))
	{
		memcpy(dest, inPath, 7*sizeof(uni_char));
		dest += 7;
		inPath += 7;
		if (0 == uni_strncmp(inPath, UNI_L("localhost"), 9))
		{
			memcpy(dest, inPath, 9*sizeof(uni_char));
			dest += 9;
			inPath += 9;
		}
	}
	if (*inPath == '/')
	{
		*dest++ = '/';
		inPath++;
	}
	*dest = '\0';
}

const uni_char* GetBootVolumeName()
{
	static uni_char* buffer = new uni_char[256];
	HFSUniStr255 volName;
	*buffer = 0;
	if(noErr == FSGetVolumeInfo(0, 1, NULL, kFSVolInfoNone, NULL, &volName, NULL))
	{
		memcpy(buffer, (uni_char*)volName.unicode, volName.length*sizeof(uni_char));
		buffer[volName.length] = 0;
	}
	return buffer;
}

Boolean IsBootVolume(const uni_char* path)
{
	const uni_char *bootVol = GetBootVolumeName();
	int len = uni_strlen(bootVol);
	return (0 == uni_strncmp(path, bootVol, len));
}

Boolean IsVolume(const uni_char* path)
{
	OSStatus err = noErr;
	HFSUniStr255 volName;
	for(int i = 1; err == noErr; i++)
	{
		err = FSGetVolumeInfo(0, i, NULL, kFSVolInfoNone, NULL, &volName, NULL);
		if(err == noErr)
		{
			if(0 == uni_strnicmp(path, (uni_char*)volName.unicode, volName.length))
			{
				return true;
			}
		}
	}
	return false;
}

uni_char* ConvertClassicToXPathStyle(const uni_char* inPath)
{
	static uni_char* buffer = new uni_char[MAX_PATH];
	uni_char* dest = buffer;
	CopyProtocolInformation(dest, inPath);
	if (IsBootVolume(inPath))
	{
		inPath = uni_strchr(inPath, UNI_L('/'));	//skip volume
		if (inPath)
		{
			inPath++;
			uni_strcpy(dest, inPath);
		}
	}
	else if(IsVolume(inPath)) // add "Volumes/" but only if it is a volumename
	{
		uni_strcpy(dest, UNI_L("Volumes/"));
		dest += 8;
		uni_strcpy(dest, inPath);
	}
	else
	{
		// A file on the system disk "/Users/foo/bar.txt" or something similar
		uni_strcpy(dest, inPath);
	}
	return buffer;
}

uni_char* ConvertXToClassicPathStyle(const uni_char* inPath)
{
	static uni_char* buffer = new uni_char[MAX_PATH];
	uni_char* dest = buffer;
	CopyProtocolInformation(dest, inPath);
	if (uni_strncmp(inPath, UNI_L("Volumes/"), 8))
	{
		inPath += 8;
		uni_strcpy(dest, inPath);
	}
	else
	{
		const uni_char *bootVol = GetBootVolumeName();
		int len = uni_strlen(bootVol);
		uni_strcpy(dest, bootVol);
		dest += len;
		*dest++ = '/';
		uni_strcpy(dest, inPath);
	}
	return buffer;
}
