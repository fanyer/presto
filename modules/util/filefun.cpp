/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/util/filefun.h"

#ifndef NO_EXTERNAL_APPLICATIONS
# include "modules/pi/OpSystemInfo.h"
#endif // !NO_EXTERNAL_APPLICATIONS

#ifndef NO_SEARCH_ENGINES
# include "modules/prefsfile/prefsfile.h"
#endif // !NO_SEARCH_ENGINES

#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef UTIL_SPLIT_FILENAME_INTO_COMPONENTS

/***********************************************************************************
**
**	SplitFilenameIntoComponents
**
***********************************************************************************/

void SplitFilenameIntoComponentsL(const OpStringC &candidate, OpString *path, OpString *filename, OpString *extension)
{
	int pos1 = candidate.FindLastOf(PATHSEPCHAR);
	int pos2 = candidate.FindLastOf('.');

	if( pos2 != KNotFound )
	{
		if( pos1 != KNotFound && pos1+1==pos2 || pos2 == 0 )
		{
			pos2 = KNotFound; // dotfile in unix
		}
	}

	if( pos1 != KNotFound && pos2 != KNotFound )
	{
		if( pos2 > pos1 )
		{
			if( path ) { path->SetL(candidate.CStr(),pos1+1); }
			if( filename ) { filename->SetL(candidate.SubString(pos1+1).CStr(),pos2-pos1-1); }
			if( extension ) { extension->SetL(candidate.SubString(pos2+1)); }
		}
		else
		{
			if( path ) { path->SetL(candidate.CStr(),pos1+1); }
			if( filename ) { filename->SetL(candidate.SubString(pos1+1).CStr()); }
		}
	}
	else if( pos1 != KNotFound )
	{
		if( path ) { path->SetL(candidate.CStr(),pos1+1); }
		if( filename ) { filename->SetL(candidate.SubString(pos1+1)); }
	}
	else if( pos2 != KNotFound )
	{
		if( filename ) { filename->SetL(candidate.CStr(),pos2); }
		if( extension ) { extension->SetL(candidate.SubString(pos2+1)); }
	}
	else
	{
		if( filename ) { filename->SetL(candidate); }
	}
}
#endif // UTIL_SPLIT_FILENAME_INTO_COMPONENTS

#ifndef NO_EXTERNAL_APPLICATIONS
OP_STATUS Execute(const uni_char* app, const uni_char* filename)
{
	return g_op_system_info->ExecuteApplication(app, filename);
}
#endif // ! NO_EXTERNAL_APPLICATIONS

#ifdef CREATE_UNIQUE_FILENAME
/***********************************************************************************
**
**	CreateUniqueFilename
**
**  will make the filename unique for the folder it specifies
***********************************************************************************/
OP_STATUS CreateUniqueFilename(OpString& filename)
{
	OpFile startfile;
	RETURN_IF_ERROR(startfile.Construct(filename.CStr()));
	BOOL exists;
	RETURN_IF_ERROR(startfile.Exists(exists));
	if (!exists) //there is no file named "filename"
	{
		return OpStatus::OK;
	}

	OpString only_filename;
	OpString only_path;
	OpString extension;

	int pos = filename.FindLastOf(PATHSEPCHAR);
	if(pos != KNotFound)
	{
		RETURN_IF_ERROR(only_filename.Set(filename.SubString(pos+1).CStr()));
		RETURN_IF_ERROR(only_path.Set(filename.CStr(), pos+1)); // Must include path separator character
	}
	else
	{
		RETURN_IF_ERROR(only_filename.Set(filename));
	}

	// extension bordercases: trailing ., leading ., and no .
	BOOL found_extension = FALSE;
	pos = only_filename.FindLastOf('.');
	if(pos != KNotFound && pos != 0)
	{
		RETURN_IF_ERROR(extension.Set(only_filename.SubString(pos+1).CStr()));
		RETURN_IF_ERROR(only_filename.Set(only_filename.CStr(), pos));
		found_extension = TRUE;
	}

	return CreateUniqueFilename(filename, only_path, only_filename, extension, found_extension); 
}

OP_STATUS CreateUniqueFilename(OpString& filename, OpFileFolder folder, const OpStringC& file_component)
{
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, filename));
	RETURN_IF_ERROR(filename.Append(file_component));
	return CreateUniqueFilename(filename);
}

OP_STATUS CreateUniqueFilename(OpString& filename, OpStringC& path_component, OpStringC& file_component, OpStringC& extension_component, BOOL found_extension)
{
	// Create the new file with a unique file name. We try atmost 1000 times.
	OpString new_filename;
	new_filename.Reserve(path_component.Length() + file_component.Length() + extension_component.Length() + 16);
	BOOL exists;
	for( int i=1; i<1000; i++ )
	{
		RETURN_IF_ERROR(new_filename.Set(path_component));
		RETURN_IF_ERROR(new_filename.Append(file_component));
# ifdef OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
		RETURN_IF_ERROR(new_filename.AppendFormat(g_op_system_info->GetUniqueFileNamePattern(), i));
# else	// !OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
		RETURN_IF_ERROR(new_filename.AppendFormat(UNI_L(" (%d)"), i));
# endif // !OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
		if (found_extension)
			RETURN_IF_ERROR(new_filename.Append("."));
		if (extension_component.HasContent())
			RETURN_IF_ERROR(new_filename.Append(extension_component.CStr()));
		
		OpFile file;
		RETURN_IF_ERROR(file.Construct(new_filename.CStr()));
		RETURN_IF_ERROR(file.Exists(exists));
		if (!exists)
		{
			return filename.Set(file.GetFullPath());
		}
	}
	return OpStatus::ERR;
}
#endif // CREATE_UNIQUE_FILENAME

const uni_char* FindRelativePath(const uni_char* full_path, const uni_char* root_path)
{
	uni_char trimmed_path[_MAX_PATH]; /* ARRAY OK 2011-05-11 msimonides */
	const size_t full_path_length = uni_strlen(full_path);
	OP_ASSERT(full_path_length < _MAX_PATH);

	uni_strncpy(trimmed_path, full_path, full_path_length);
	for (const uni_char* trimmed_path_end = full_path + full_path_length;
	     trimmed_path_end != NULL;
	     trimmed_path_end = GetLastPathSeparator(full_path, trimmed_path_end))
	{
		trimmed_path[trimmed_path_end - full_path] = '\0';
		BOOL equal = FALSE;
		OpStatus::Ignore(g_op_system_info->PathsEqual(trimmed_path, root_path, &equal));
		if (equal)
			return (trimmed_path_end < full_path + full_path_length) ? trimmed_path_end + 1 : trimmed_path_end;
	}
	return NULL;
}
