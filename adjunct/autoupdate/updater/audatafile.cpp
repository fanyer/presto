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

#include "adjunct/autoupdate/updater/audatafile.h"

#include "adjunct/autoupdate/updater/audatafile_reader.h"
#include "adjunct/autoupdate/updater/pi/aufileutils.h"

#include "modules/util/opfile/opfile.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpSystemInfo.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

AUDataFile::AUDataFile()
: m_au_fileutils(NULL),
  m_show_information(TRUE)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
 
AUDataFile::~AUDataFile() 
{
	delete m_au_fileutils;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::Init() 
{
	m_au_fileutils = AUFileUtils::Create();
	if(m_au_fileutils)
		return OpStatus::OK;
	return OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::LoadValuesFromExistingFile()
{
	AUDataFileReader data_file_reader;
	OP_STATUS status = data_file_reader.Init();
	if(OpStatus::IsError(status))
	{
		return status;
	}
	status = data_file_reader.LoadFromOpera();
	if(OpStatus::IsError(status))
	{
		return status;
	}
	uni_char* install_path = data_file_reader.GetInstallPath();
	uni_char* update_file = data_file_reader.GetUpdateFile();
	uni_char* version = data_file_reader.GetVersion();
	uni_char* build_number = data_file_reader.GetBuildNum();
	if(install_path && update_file && version && build_number)
	{
		status = m_install_path.Set(install_path);
		status |= m_update_file.Set(update_file);
		status |= m_version.Set(version);
		status |= m_buildnum.Set(build_number);
		m_type = data_file_reader.GetType();
		m_show_information = data_file_reader.ShowInformation();
	}
	else
	{
		status = OpStatus::ERR_NO_MEMORY;
	}
	OP_DELETEA(install_path);
	OP_DELETEA(update_file);
	OP_DELETEA(version);
	OP_DELETEA(build_number);
	return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::SetInstallPath(const uni_char *install_path)
{
	return m_install_path.Set(install_path);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::SetUpdateFile(const uni_char *downloaded_file, AUDataFile::Type type)
{
	RETURN_IF_ERROR(m_update_file.Set(downloaded_file));
	m_type = type;
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::SetVersion(const uni_char *version)
{
	return m_version.Set(version);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::SetBuildnum(const uni_char *buildnum)
{
	return m_buildnum.Set(buildnum);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::Write()
{
	if(m_au_fileutils)
	{
		uni_char* temp_folder = NULL;
		
		if(m_au_fileutils->GetUpgradeFolder(&temp_folder) != AUFileUtils::OK)
		{
			return OpStatus::ERR;
		}		

		if(temp_folder)
		{
			RETURN_IF_ERROR(m_file_name.Set(temp_folder));
			delete [] temp_folder;
			RETURN_IF_ERROR(m_file_name.Append(PATHSEP));
			RETURN_IF_ERROR(m_file_name.Append(AUTOUPDATE_UPDATE_TEXT_FILENAME));
			
			// Check if package file exists.
			OpFile package;
			RETURN_IF_ERROR(package.Construct(m_update_file));
			BOOL exists;
			RETURN_IF_ERROR(package.Exists(exists));
			if(!exists)
				return OpStatus::ERR;
			
			// Get size
			OpFileLength size;
			RETURN_IF_ERROR(package.GetFileLength(size));
			OpString size_str;
			OpString8 size_str8;
			RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString(size, &size_str8));
			RETURN_IF_ERROR(size_str.Set(size_str8));
			
			// Create update file
			OpFile update_file;
			RETURN_IF_ERROR(update_file.Construct(m_file_name));
			RETURN_IF_ERROR(update_file.Open(OPFILE_WRITE));
			
			OP_STATUS ret = OpStatus::OK;
			
			// Write header
			ret |= update_file.Write(AU_DATAFILE_HEADER, uni_strlen(AU_DATAFILE_HEADER)*sizeof(uni_char));
			
			// Write version
			ret |= update_file.Write(AU_DATAFILE_VERSION, uni_strlen(AU_DATAFILE_VERSION)*sizeof(uni_char));
			
			// Write executable
			ret |= update_file.Write(AU_DATAFILE_EXECUTABLE, uni_strlen(AU_DATAFILE_EXECUTABLE)*sizeof(uni_char));
			ret |= update_file.Write(m_install_path.CStr(), m_install_path.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write file name
			ret |= update_file.Write(AU_DATAFILE_FILE, uni_strlen(AU_DATAFILE_FILE)*sizeof(uni_char));
			ret |= update_file.Write(m_update_file.CStr(), m_update_file.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write type
			ret |= update_file.Write(AU_DATAFILE_TYPE, uni_strlen(AU_DATAFILE_TYPE)*sizeof(uni_char));
			if(m_type == Full)
			{
				ret |= update_file.Write(AU_DATAFILE_TYPE_FULL, uni_strlen(AU_DATAFILE_TYPE_FULL)*sizeof(uni_char));
			}
			else
			{
				if(m_type == Patch)
				{
					ret |= update_file.Write(AU_DATAFILE_TYPE_PATCH, uni_strlen(AU_DATAFILE_TYPE_PATCH)*sizeof(uni_char));
				}
				else
				{
					ret |= OpStatus::ERR;
				}
			}
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
						
			// Write size
			ret |= update_file.Write(AU_DATAFILE_SIZE, uni_strlen(AU_DATAFILE_SIZE)*sizeof(uni_char));
			ret |= update_file.Write(size_str.CStr(), size_str.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write version
			ret |= update_file.Write(AU_DATAFILE_VERSION_NUM, uni_strlen(AU_DATAFILE_VERSION_NUM)*sizeof(uni_char));
			ret |= update_file.Write(m_version.CStr(), m_version.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));

			// Write buildnum
			ret |= update_file.Write(AU_DATAFILE_BUILD_NUM, uni_strlen(AU_DATAFILE_BUILD_NUM)*sizeof(uni_char));
			ret |= update_file.Write(m_buildnum.CStr(), m_buildnum.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write dialog caption
			OpString caption;
			ret |= g_languageManager->GetString(Str::D_UPDATE_WARNING_DIALOG_CAPTION, caption);
			ret |= update_file.Write(AU_DATAFILE_DIALOG_CAPTION, uni_strlen(AU_DATAFILE_DIALOG_CAPTION)*sizeof(uni_char));
			ret |= update_file.Write(caption.CStr(), caption.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));

			// Write dialog message
			OpString message;
			ret |= g_languageManager->GetString(Str::D_UPDATE_WARNING_DIALOG_MESSAGE, message);
			ret |= update_file.Write(AU_DATAFILE_DIALOG_MESSAGE, uni_strlen(AU_DATAFILE_DIALOG_MESSAGE)*sizeof(uni_char));
			ret |= update_file.Write(message.CStr(), message.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write dialog ok text
			OpString ok;
			ret |= g_languageManager->GetString(Str::D_UPDATE_WARNING_DIALOG_OK_TEXT, ok);
			ret |= update_file.Write(AU_DATAFILE_DIALOG_OK, uni_strlen(AU_DATAFILE_DIALOG_OK)*sizeof(uni_char));
			ret |= update_file.Write(ok.CStr(), ok.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));

			// Write dialog cancel text
			OpString cancel;
			ret |= g_languageManager->GetString(Str::D_UPDATE_WARNING_DIALOG_CANCEL_TEXT, cancel);
			ret |= update_file.Write(AU_DATAFILE_DIALOG_CANCEL, uni_strlen(AU_DATAFILE_DIALOG_CANCEL)*sizeof(uni_char));
			ret |= update_file.Write(cancel.CStr(), cancel.Length()*sizeof(uni_char));
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));
			
			// Write if information about update should be shown (see comment for value m_show_information in header file).
			ret |= update_file.Write(AU_DATAFILE_SHOW_INFORMATION, uni_strlen(AU_DATAFILE_SHOW_INFORMATION)*sizeof(uni_char));
			if(m_show_information)
			{
				ret |= update_file.Write("1", sizeof(uni_char));
			}
			else
			{
				ret |= update_file.Write("0", sizeof(uni_char));
			}
			ret |= update_file.Write(UNI_L("\n"), uni_strlen(UNI_L("\n"))*sizeof(uni_char));

			ret |= update_file.Close();
			
			if(OpStatus::IsError(ret))
			{
				// Delete file if an error occured
				update_file.Delete(FALSE);
				return ret;
			}
			
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR_NO_MEMORY;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS AUDataFile::Delete()
{
	OpFile f;
	RETURN_IF_ERROR(f.Construct(m_file_name));
	return f.Delete(FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

