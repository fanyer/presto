/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#include "adjunct/desktop_util/version/operaversion.h"
#include "adjunct/autoupdate/updater/austringutils.h"
#include "adjunct/autoupdate/updater/pi/aufileutils.h"

////////////////////////////////////////////////////////////////////////////////////////////////
//
//	OperaVersion
//

OperaVersion::OperaVersion() :
	m_version(0),
	m_version_string(NULL)
{
	SetMajor(VER_NUM_MAJOR);
	SetMinor(VER_NUM_MINOR);
	SetBuild(VER_BUILD_NUMBER);
}

OperaVersion::OperaVersion(UINT32 major, UINT32 minor, UINT32 build)
	: m_version(0)
	, m_version_string(NULL)
{
	SetMajor(major);
	SetMinor(minor);
	SetBuild(build);
}

OperaVersion::OperaVersion(const OperaVersion& other)
	: m_version(other.m_version)
	, m_version_string(NULL)
{
}

OperaVersion& OperaVersion::operator=(OperaVersion other)
{
	other.Swap(*this);
	return *this;
}

void OperaVersion::Swap(OperaVersion& other)
{
	op_swap(m_version, other.m_version);
	op_swap(m_version_string, other.m_version_string);
}

OperaVersion::~OperaVersion() 
{
	OP_DELETEA(m_version_string);
}

void OperaVersion::AllowAutoupdateIniOverride()
{
#if defined(AUTOUPDATE_ENABLE_AUTOUPDATE_INI) && defined(AUTOUPDATE_PACKAGE_INSTALLATION)
	AUFileUtils* au_file_utils = AUFileUtils::Create();
	if (!au_file_utils)
		return;

	uni_char* exe_path = NULL;
	uni_char* file_name_start = NULL;
	uni_char* text_file_path = NULL;
	unsigned file_size;

	// Get the path to Opera binary
	if (au_file_utils->GetExePath(&exe_path) != AUFileUtils::OK || !exe_path)
	{
		delete au_file_utils;
		return;
	}

	// Strip the binary name
	// Find the position of the first character after the last PATHSEPCHAR in the exe path
	file_name_start = u_strrchr(exe_path, PATHSEPCHAR) + 1;
	int path_len = file_name_start - exe_path;
	int len = path_len + u_strlen(AUTOUPDATE_UPDATE_FAKE_FILENAME);
	text_file_path = new uni_char[len + 1];
	if (!text_file_path)
	{
		delete au_file_utils;
		delete [] exe_path;
		return;
	}

	// Construct the full path to the autoupdate.ini file, the path to Opera binary first...
	u_strncpy(text_file_path, exe_path, path_len);
	const uni_char* autoupdate_ini_name = AUTOUPDATE_UPDATE_FAKE_FILENAME;

	// ...and the autoupdate.ini filename next.
	u_strncpy(text_file_path + path_len, autoupdate_ini_name, u_strlen(autoupdate_ini_name) + 1);

	if (au_file_utils->Exists(text_file_path, file_size) == AUFileUtils::OK)
	{
		// Maximum length of the full version string, which is expected to have a "MAJOR.MINOR.BUILDNO" format.
		const unsigned MAX_TOKEN_SIZE = 20;
		uni_char full_version[MAX_TOKEN_SIZE];
		full_version[0] = '\0';

		char* buffer = new char[file_size];
		if (!buffer)
		{
			delete au_file_utils;
			delete [] exe_path;
			delete [] text_file_path;
			return;
		}

		size_t read_size;
		if (au_file_utils->Read(text_file_path, &buffer, read_size) == AUFileUtils::OK)
		{
			unsigned char_no = 0;
			char current_char;
			bool failed = FALSE;

			for (unsigned i=0; i<file_size; i++)
			{
				if (failed)
					break;

				current_char = buffer[i];
				switch (current_char)
				{
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					continue;
				default:
					// Don't go past the available memory
					if (char_no + 1 >= MAX_TOKEN_SIZE)
					{
						failed = true;
						continue;
					}
					full_version[char_no++] = current_char;
					full_version[char_no] = '\0';

					break;
				}
			}

			// Ignore errors, we don't return an errcode from here anyway
			if (!failed)
				OpStatus::Ignore(Set(full_version));
		}

		delete [] buffer;
	}

	delete au_file_utils;
	delete [] exe_path;
	delete [] text_file_path;
#else // AUTOUPDATE_ENABLE_AUTOUPDATE_INI && AUTOUPDATE_PACKAGE_INSTALLATION
	// This won't build on **ix since AUFileUtils is not implemented for that platform
#endif // AUTOUPDATE_ENABLE_AUTOUPDATE_INI && AUTOUPDATE_PACKAGE_INSTALLATION
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaVersion::Set(const uni_char* version)
{
	// Reset the version
	m_version = 0;
	if (u_strchr(version, '.'))
	{
		// The string should be in the format "major.minor.build"
		UINT32 major, minor, buildnumber;

		if(!OpStatus::IsError(ParseVersionString(version, major, minor, buildnumber)))
		{
			SetMajor(major);
			SetMinor(minor);
			SetBuild(buildnumber);

			return OpStatus::OK;
		}
	}
	else
	{
		// try to parse it in the old format ("92" and "964")
		UINT version_num = u_str_to_uint32(version);
		UINT major, minor;
		if (version_num < 100)
			version_num *= 10;

		// 9 = 964 / 100
		major = version_num / 100;
		minor = version_num - major*100;

		SetMajor(major);
		SetMinor(minor);
		SetBuild(1);
		return OpStatus::OK;
	}
	
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaVersion::Set(const uni_char* version, const uni_char* buildnumber)
{
	if(!version || !buildnumber)
		return OpStatus::ERR_NULL_POINTER;
	int buffer_size = u_strlen(version) + u_strlen(buildnumber) + 2; // 2 == '.' + '\0'
	uni_char* full_str = OP_NEWA(uni_char, buffer_size);
	if(!full_str)
		return OpStatus::ERR_NO_MEMORY;
	u_strcpy(full_str, version);
	u_strcat(full_str, UNI_L("."));
	u_strcat(full_str, buildnumber);
	OP_STATUS result = Set(full_str);
	OP_DELETEA(full_str);
	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////

const uni_char* OperaVersion::GetFullString() const
{
	OP_DELETEA(m_version_string);
	m_version_string = OP_NEWA(uni_char, 12);
	if(!m_version_string)
		return NULL;

	RETURN_VALUE_IF_ERROR(u_uint32_to_str(m_version_string, 3, GetMajor()), NULL);
	uni_char* ptr = m_version_string + u_strlen(m_version_string);
	*ptr++ = '.';
	*ptr++ = '0' + GetMinor() / 10;
	*ptr++ = '0' + GetMinor() % 10;
	*ptr++ = '.';
	RETURN_VALUE_IF_ERROR(u_uint32_to_str(ptr, 5, GetBuild()), NULL);
	return m_version_string;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OperaVersion::SetMajor(UINT32 major_version) 
{ 
	m_version |= (major_version << OV_MAJOR_VER_SHIFT); 
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OperaVersion::SetMinor(UINT32 minor_version) 
{ 
	m_version |= (minor_version & OV_MINOR_VER_MASK) << OV_MINOR_VER_SHIFT; 
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OperaVersion::SetBuild(UINT32 build_number) 
{ 
	m_version |= (build_number & OV_BUILD_NUM_MASK); 
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaVersion::ParseVersionString(const uni_char* version, UINT32& major, UINT32& minor, UINT32& buildnumber)
{
	if(!version)
		return OpStatus::ERR_NULL_POINTER;
	uni_char* version_str = OP_NEWA(uni_char, u_strlen(version) + 1);
	if(!version_str)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = OpStatus::ERR;
	do
	{
		u_strcpy(version_str, version);
		uni_char* major_start_position = version_str;
		// look for first dot (placed between major and minor)
		uni_char* dot = u_strchr(version_str, UNI_L('.'));
		if(!dot)
			break;
		*dot = UNI_L('\0');
		++dot;
		uni_char* minor_start_position = dot;
		// look for second dot (placed between minor and buildnumber)
		dot = u_strchr(minor_start_position, UNI_L('.'));
		if(!dot)
			break;
		*dot = UNI_L('\0');
		++dot;
		uni_char* buildnumber_start_position = dot;

		int major_length = u_strlen(major_start_position);
		int minor_length = u_strlen(minor_start_position);
		int buildnumber_length = u_strlen(buildnumber_start_position);
		
		if( major_length > 2 || (minor_length != 1 && minor_length != 2) || buildnumber_length > 9)
			break;
		
		major = u_str_to_uint32(major_start_position);
		minor = u_str_to_uint32(minor_start_position);
		buildnumber = u_str_to_uint32(buildnumber_start_position);

		if(minor_length == 1)
			minor *= 10; // needed as 9.5 and 9.50 are the same

		result = OpStatus::OK;
	} while(0);

	OP_DELETEA(version_str);
	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////
