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

#include "adjunct/autoupdate/updater/audatafile_reader.h"

#include "adjunct/autoupdate/updater/pi/aufileutils.h"
#include "adjunct/autoupdate/updater/austringutils.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

AUDataFileReader::AUDataFileReader()
: m_au_fileutils(NULL),
  m_install_path(NULL),
  m_update_file(NULL),
  m_size(NULL),
  m_version(NULL),
  m_buildnum(NULL),
  m_caption(NULL),
  m_message(NULL),
  m_ok(NULL),
  m_cancel(NULL),
  m_show_information(TRUE),
  m_text_file_path(NULL)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
 
AUDataFileReader::~AUDataFileReader() 
{
	delete [] m_update_file;
	delete [] m_install_path;
	delete m_au_fileutils;
	delete [] m_text_file_path;
	delete [] m_size;
	delete [] m_version;
	delete [] m_buildnum;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::Init() 
{
	// Create the file util object
	m_au_fileutils = AUFileUtils::Create();
	if (m_au_fileutils)
		return OpStatus::OK;
		
	return OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetUpdateFile()
{
	if(!m_update_file)
		return NULL;
	uni_char* update_file = new uni_char[u_strlen(m_update_file) + 1];
	if(!update_file)
		return NULL;
	u_strcpy(update_file, m_update_file);
	return update_file;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetInstallPath()
{
	if(!m_install_path)
		return NULL;
	uni_char* install_path = new uni_char[u_strlen(m_install_path) + 1];
	if(!install_path)
		return NULL;
	u_strcpy(install_path, m_install_path);
	return install_path;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetVersion()
{
	if(!m_version)
		return NULL;
	uni_char* version = new uni_char[u_strlen(m_version) + 1];
	if(!version)
		return NULL;
	u_strcpy(version, m_version);
	return version;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetBuildNum()
{
	if(!m_buildnum)
		return NULL;
	uni_char* buildnum = new uni_char[u_strlen(m_buildnum) + 1];
	if(!buildnum)
		return NULL;
	u_strcpy(buildnum, m_buildnum);
	return buildnum;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetDialogCaption()
{
	if(!m_caption)
		return NULL;
	uni_char* caption = new uni_char[u_strlen(m_caption) + 1];
	if(!caption)
		return NULL;
	u_strcpy(caption, m_caption);
	return caption;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetDialogMessage()
{
	if(!m_message)
		return NULL;
	uni_char* message = new uni_char[u_strlen(m_message) + 1];
	if(!message)
		return NULL;
	u_strcpy(message, m_message);
	return message;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetDialogOk()
{
	if(!m_ok)
		return NULL;
	uni_char* ok = new uni_char[u_strlen(m_ok) + 1];
	if(!ok)
		return NULL;
	u_strcpy(ok, m_ok);
	return ok;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uni_char* AUDataFileReader::GetDialogCancel()
{
	if(!m_cancel)
		return NULL;
	uni_char* cancel = new uni_char[u_strlen(m_cancel) + 1];
	if(!cancel)
		return NULL;
	u_strcpy(cancel, m_cancel);
	return cancel;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::Load()
{
	if(m_au_fileutils)
	{
		uni_char* exe_path = NULL, *update_name = NULL;
		
		if(m_au_fileutils->GetExePath(&exe_path) != AUFileUtils::OK)
		{
			delete [] exe_path;
			return OpStatus::ERR;
		}		

		if(m_au_fileutils->GetUpdateName(&update_name) != AUFileUtils::OK)
		{
			delete [] exe_path;
			delete [] update_name;
			return OpStatus::ERR;
		}		
		
		// Create a buffer large enough
		uni_char* file_name_start = u_strrchr(exe_path, PATHSEPCHAR) + strlen(PATHSEP);
		int path_len = file_name_start - exe_path;
		int len = path_len + u_strlen(AUTOUPDATE_UPDATE_TEXT_FILENAME);
		uni_char* text_file_path = new uni_char[len + 1];
		if (!text_file_path)
			return OpStatus::ERR_NO_MEMORY;

		// Set the base path
		u_strncpy(text_file_path, exe_path, path_len);

		// Copy the name of the text file in over the top of the name of the exe
		u_strncpy(text_file_path + path_len, AUTOUPDATE_UPDATE_TEXT_FILENAME, u_strlen(AUTOUPDATE_UPDATE_TEXT_FILENAME) + 1);
		delete [] update_name;
		delete [] exe_path;
		
		OP_STATUS ret = LoadFromPath(text_file_path);
		delete [] text_file_path;
		return ret;
		
	}
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::LoadFromOpera()
{
	if(m_au_fileutils)
	{
		uni_char* upgrade_folder = NULL;
		if(m_au_fileutils->GetUpgradeFolder(&upgrade_folder) != AUFileUtils::OK || !upgrade_folder)
		{
			return OpStatus::ERR;
		}		
		
		// Create a buffer large enough
		int len = u_strlen(upgrade_folder) + u_strlen(AUTOUPDATE_UPDATE_TEXT_FILENAME);;
		uni_char* text_file_path = new uni_char[len + 1];
		if (!text_file_path)
			return OpStatus::ERR_NO_MEMORY;

		// Set the base path
		u_strcpy(text_file_path, upgrade_folder);
		delete [] upgrade_folder;

		// Add the text file name
		u_strcat(text_file_path, AUTOUPDATE_UPDATE_TEXT_FILENAME);

		OP_STATUS ret = LoadFromPath(text_file_path);
		delete [] text_file_path;
		return ret;
		
	}
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::LoadFromPath(const uni_char* path)
{
	if(!path)
		return OpStatus::ERR;

	// Kill the buffer if allocated already
	if (m_text_file_path)
	{
		delete [] m_text_file_path;
		m_text_file_path = NULL;
	}

	// Create a buffer large enough
	m_text_file_path = new uni_char[u_strlen(path) + 1];
	if (!m_text_file_path)
		return OpStatus::ERR_NO_MEMORY;
	u_strcpy(m_text_file_path, path);
	
	// Read the file
	char* buf = NULL;
	size_t buf_size = 0;
	if(m_au_fileutils->Read(m_text_file_path, &buf, buf_size) != AUFileUtils::OK)
	{
		delete [] m_text_file_path;
		m_text_file_path = NULL;
		return OpStatus::ERR;
	}
	
	uni_char* uni_buf = (uni_char*)buf;
	const uni_char* end_buf = (uni_char*)((char*)buf + buf_size);

    // Header
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_HEADER));
	
	// Version
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_VERSION));
	
	// Executable
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_EXECUTABLE));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_install_path));
	
	// File
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_FILE));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_update_file));
	
	// Type
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_TYPE));
	
	uni_char* type = NULL;
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &type));
	
	if(u_strncmp(type, AU_DATAFILE_TYPE_FULL, u_strlen(AU_DATAFILE_TYPE_FULL)) == 0)
	{
		m_type = AUDataFile::Full;
	}
	else
	{
		if(u_strncmp(type, AU_DATAFILE_TYPE_PATCH, u_strlen(AU_DATAFILE_TYPE_PATCH)) == 0)
		{
			m_type = AUDataFile::Patch;
		}
		else
		{
			delete [] type;
			return OpStatus::ERR;
		}
	}
	delete [] type;
	
	// Size
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_SIZE));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_size));		
	
	// VersionNum
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_VERSION_NUM));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_version));		
	
	// BuildNum
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_BUILD_NUM));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_buildnum));

	// Caption
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_DIALOG_CAPTION));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_caption));

	// Message
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_DIALOG_MESSAGE));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_message));
	
	// Ok text
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_DIALOG_OK));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_ok));

	// Cancel text
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_DIALOG_CANCEL));
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &m_cancel));

	// Show information if update should be shown (see comment for value m_show_information in header file).
	RETURN_IF_ERROR(MatchValue(uni_buf, end_buf, AU_DATAFILE_SHOW_INFORMATION));
	uni_char* showInformation = NULL;
	RETURN_IF_ERROR(ReadValue(uni_buf, end_buf, &showInformation));
	if(showInformation != NULL && showInformation[0] == '1')
	{
		m_show_information = TRUE;
	}
	else
	{
		m_show_information = FALSE;
	}
	delete [] showInformation;
	
	delete [] buf;

	return OpStatus::OK;	
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void AUDataFileReader::Delete()
{
	// If the file was read we can delete it
	if (m_text_file_path && m_au_fileutils)
		m_au_fileutils->Delete(m_text_file_path);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::ReadValue(uni_char*& uni_buf, const uni_char* end_buf, uni_char** value)
{
	uni_char* p = uni_buf;
	while(*p && *p != '\n' && p < end_buf)
	{
		p++;
	}

	if(*p == '\n')
	{
		*value = new uni_char[p - uni_buf + 1];
		if (!*value)
			return OpStatus::ERR_NO_MEMORY;

		u_strncpy(*value, uni_buf, p - uni_buf);
		(*value)[p - uni_buf] = 0;

		uni_buf = p + 1;
		
		return OpStatus::OK;
	}
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFileReader::MatchValue(uni_char*& uni_buf, const uni_char* end_buf, const uni_char* value)
{
	if(uni_buf + u_strlen(value) > end_buf)
		return OpStatus::ERR;

	if(u_strncmp(uni_buf, value, u_strlen(value)) == 0)
	{
		uni_buf += u_strlen(value);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

