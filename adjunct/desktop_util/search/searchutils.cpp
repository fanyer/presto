/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifndef NO_SEARCH_ENGINES
#include "searchutils.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefsfile/prefsfile.h"


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL CreateFileBackup( OpFile& src, INT32 *error )
{
	OpString backup_filename;
	RETURN_VALUE_IF_ERROR(backup_filename.Set( src.GetFullPath()), FALSE);

	for( int i=-1; i<1000; i++ )
	{
		OpString filename;
		OP_STATUS err = filename.Set( backup_filename );
		if( i < 0 )
		{
			filename.Append( UNI_L(".bak") );
		}
		else
		{
			/* assert: 0 <= i < 1000 so we use at most 5 bytes of buf: the '.',
			 * three digits and a closing '\0'. */
			char buf[10]; // ARRAY OK
			sprintf( buf, ".%03d", i );
			filename.Append(buf);
		}

		OpFile file;
		err = file.Construct(filename.CStr());
		BOOL exists = FALSE;
		err = file.Exists(exists);
		if( !exists || i == 999 )
		{
			return file.CopyContents(&src, FALSE);
		}
	}

	if( error )
	{
		*error = 0;
	}
	return FALSE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
static int GetVersionFromResourceFile( const uni_char* filename )
{
	int version = -1;

	OpFile file;
	if( OpStatus::IsSuccess(file.Construct(filename) ) )
	{
		PrefsFile prefs(PREFS_STD);
		TRAPD(err, prefs.ConstructL());
		TRAP(err, prefs.SetFileL(&file));
		version = prefs.ReadIntL(UNI_L("Version"), UNI_L("File Version"), 0);
	}

	return version;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
OpFile* UpdateResourceFileL(const uni_char* filename, BOOL test_language)
{
#define PERSONAL_FOLDER_LOCALIZED_SEARCH_INI_PERMITTED // TODO: Make this a feature setting

	// Set location of the shared resource file

	OpString shared_folder; ANCHOR(OpString, shared_folder);
#if defined(PERSONAL_FOLDER_LOCALIZED_SEARCH_INI_PERMITTED)
	if( test_language )
	{
		OpFile file;
		BOOL exists = FALSE;

		// Mac's OPFILE_LANGUAGE_FOLDER is wrong and returns the complete path so we
		// will have to cheat slightly here, but fix it properly in peregrine - adamm 27-02-2007
#ifdef _MACINTOSH_
		// Mac is also going to be based on the filename, and I won't change the other
		// platforms so I won't break them at this late stage - adamm 27-02-2007
		OP_STATUS rc = file.Construct(filename, OPFILE_LANGUAGE_FOLDER);
#else
		OP_STATUS rc = file.Construct(g_languageManager->GetLanguage().CStr(), OPFILE_LANGUAGE_FOLDER);
#endif
		if (OpStatus::IsSuccess(rc))
		{
			rc = file.Exists(exists);
		}
#ifndef _MACINTOSH_
		OpFileInfo::Mode mode;
#endif
		if( OpStatus::IsSuccess(rc) && exists
#ifndef _MACINTOSH_
			&& OpStatus::IsSuccess(file.GetMode(mode)) && mode == OpFileInfo::DIRECTORY
#endif
			)

		{
			shared_folder.SetL(file.GetFullPath());
#ifndef _MACINTOSH_
			shared_folder.AppendL(PATHSEP);
#endif
		}
		else
		{
			test_language = FALSE;
			LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, shared_folder));
#ifdef _MACINTOSH_
			shared_folder.AppendL(filename);
#endif
		}
	}
	else
#endif
	{
		test_language = FALSE;
		LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, shared_folder));
#ifdef _MACINTOSH_
		shared_folder.AppendL(filename);
#endif
	}

	// This will work by accident since on Mac OPFILE_HOME_FOLDER and OPFILE_USERPREFS_FOLDER
	// are in the same location but I don't think that this is right - adamm 27-02-2007
	OpString personal_folder; ANCHOR(OpString, personal_folder);
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, personal_folder));

#ifndef _MACINTOSH_
	shared_folder.AppendL(filename);
#endif
	personal_folder.AppendL(filename);

	int shared_resource_version   = GetVersionFromResourceFile(shared_folder.CStr());
	int personal_resource_version = GetVersionFromResourceFile(personal_folder.CStr());


	BOOL copyfile   = FALSE;
	BOOL backupfile = FALSE;

	if( personal_resource_version <= 0 || shared_resource_version > personal_resource_version )
	{
		copyfile   = TRUE;
		backupfile = personal_resource_version > 0;
	}

	OpStackAutoPtr<OpFile> file(NULL);

	OpFile shared_resource_file; ANCHOR(OpFile, shared_resource_file);
	LEAVE_IF_ERROR(shared_resource_file.Construct(shared_folder.CStr()));

	OpFile personal_resource_file; ANCHOR(OpFile, personal_resource_file);
	LEAVE_IF_ERROR(personal_resource_file.Construct(personal_folder.CStr()));

	if( copyfile )
	{
		file.reset(OP_NEW_L(OpFile, ()));
		LEAVE_IF_ERROR(file->Copy(&personal_resource_file));

		if( backupfile )
		{
			CreateFileBackup( *file, 0 );
		}

		BOOL exists = FALSE;
		if (OpStatus::IsSuccess(shared_resource_file.Exists(exists)) && exists)
		{
			LEAVE_IF_ERROR(file->CopyContents(&shared_resource_file, FALSE));
		}
		else
		{
			file.reset();
		}
	}

	if( !file.get() )
	{
		if( test_language)
		{
			file.reset(UpdateResourceFileL(filename, FALSE));
		}
		else
		{
			file.reset(OP_NEW_L(OpFile, ()));

			BOOL exists = FALSE;
			if (OpStatus::IsSuccess(personal_resource_file.Exists(exists)) && exists)
				LEAVE_IF_ERROR(file->Copy(&personal_resource_file));
			else if (OpStatus::IsSuccess(shared_resource_file.Exists(exists)) && exists)
				LEAVE_IF_ERROR(file->Copy(&personal_resource_file));
		}
	}

	return file.release();
}

#endif // NO_SEARCH_ENGINES
