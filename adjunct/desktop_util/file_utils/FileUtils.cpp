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

#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/resources/ResourceUtils.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/windowcommander/src/TransferManager.h"

#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	DeleteFolderFiles
**
***********************************************************************************/
OP_STATUS FileUtils::DeleteFolderFiles(OpFileFolder folder_id)
{
#ifdef EXTERNAL_APPLICATIONS_SUPPORT
	OpFile dir;
	RETURN_IF_ERROR(dir.Construct(NULL, folder_id));
	RETURN_IF_ERROR(dir.Delete(TRUE));
	RETURN_IF_ERROR(dir.MakeDirectory());
#endif // EXTERNAL_APPLICATIONS_SUPPORT

    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS FileUtils::SetTurboSettingsPath()
{
	// Construct path to turbosettings file
	OpString path;
	OpStringC au_country_code = g_pcui->GetStringPref(PrefsCollectionUI::AuCountryCode);
	// If country code has been sent from autoupdate then build the path based on that
	if (au_country_code.HasContent())
	{
		// Get the region root folder
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_REGION_ROOT_FOLDER, path));

		// Convert country code to region and append it to the root
		OpString region, ignored;
		RETURN_IF_ERROR(ResourceUtils::CountryCodeToRegion(au_country_code, region, ignored));
		RETURN_IF_ERROR(path.Append(region));
		RETURN_IF_ERROR(path.Append(PATHSEP));
	}
	else
	{
		// No preference so just spit out the current region folder
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_REGION_FOLDER, path));
	}

	RETURN_IF_ERROR(path.Append(DESKTOP_RES_PACKAGE_TURBOSETTINGS));

	// Write it out as a serialised filename
	OpFile		turbo_file_serialised;
	RETURN_IF_ERROR(turbo_file_serialised.Construct(path, OPFILE_ABSOLUTE_FOLDER));
	TRAPD(status, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::WebTurboConfigFile, &turbo_file_serialised));
	return status;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace FileUtils
{
// Returns true if name is the last component of path.
// Path may contain PATHSEPCHARS, name must not.
bool PathEndsWith(OpStringC path, OpStringC name)
{
	OP_ASSERT(name.FindFirstOf(PATHSEPCHAR) == KNotFound);
	int path_length = path.Length();
	if (path_length > 0 && path[path_length - 1] == PATHSEPCHAR)
		-- path_length;
	int name_length = name.Length();
	if (name_length == 0)
		return path_length == 0;
	else
		return name_length <= path_length &&
			(path_length == name_length || path[path_length - name_length - 1] == PATHSEPCHAR) &&
			name.Compare(path.CStr() + path_length - name_length, name_length) == 0;
}
}

OP_STATUS FileUtils::UpdateDesktopLocaleFolders(bool force)
{
	OpStringC region = g_region_info->m_region;
	OpStringC language = g_languageManager->GetLanguage();
	if (language.HasContent() && region.HasContent())
	{
		bool update_region = force;
		if (!update_region)
		{
			// Check if current region is different than the region that was used to setup folders.
			OpString path;
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_REGION_FOLDER, path));
			update_region = !PathEndsWith(path, region);
		}

		// Actual folder name may be different than language code - we need OpDesktopResources to get it.
		OpDesktopResources* dr_temp;
		RETURN_IF_ERROR(OpDesktopResources::Create(&dr_temp));
		OpAutoPtr<OpDesktopResources> desktop_resources(dr_temp);

		bool update_language = update_region || force;
		if (!update_language)
		{
			// Check if current language is different than the language that was used to setup folders.
			// All desktop-specific language folders should use the same language code, so it is enough
			// to check one of them (OPFILE_REGION_LANGUAGE_FOLDER).

			OpString lang_folder;
			RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(lang_folder, OpDesktopResources::REGION, language));

			OpString path;
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_REGION_LANGUAGE_FOLDER, path));

			update_language = !PathEndsWith(path, lang_folder);
		}

		if (update_region)
		{
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_REGION_FOLDER, region, OPFILE_REGION_ROOT_FOLDER));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_REGION_CUSTOM_FOLDER, region, OPFILE_REGION_ROOT_CUSTOM_FOLDER));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_REGION_CUSTOM_PACKAGE_FOLDER, region, OPFILE_REGION_ROOT_CUSTOM_PACKAGE_FOLDER));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_DEFAULT_REGION_LANGUAGE_FOLDER, g_region_info->m_default_language, OPFILE_LOCALE_FOLDER));
		}
		if (update_language)
		{
			OpString lang_folder;
			RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(lang_folder, OpDesktopResources::REGION, language));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_REGION_LANGUAGE_FOLDER, lang_folder, OPFILE_REGION_FOLDER));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_REGION_LANGUAGE_CUSTOM_FOLDER, lang_folder, OPFILE_REGION_CUSTOM_FOLDER));
			RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(lang_folder, OpDesktopResources::CUSTOM_LOCALE, language));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_LANGUAGE_CUSTOM_FOLDER, lang_folder, OPFILE_LOCALE_CUSTOM_FOLDER));
			RETURN_IF_ERROR(g_folder_manager->SetFolderPath(OPFILE_LANGUAGE_CUSTOM_PACKAGE_FOLDER, lang_folder, OPFILE_LOCALE_CUSTOM_PACKAGE_FOLDER));
		}
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef USE_COMMON_RESOURCES

OP_STATUS FileUtils::TranslateIniFilenames(PrefsFile *pf, OperaInitInfo *pinfo)
{
	// We have renamed some files so we need to fix any preferences here in case they were
	// written out in a previous version

	// The five arrays listed here must match
	// These setup the old and new filename, the file location and the preferences to check
#ifdef _MACINTOSH_
	const uni_char *old_filenames[] = { UNI_L("Bookmarks"), UNI_L("Opera Direct History"), UNI_L("Opera Global History"),
									UNI_L("Site Preferences"), UNI_L("Site Preferences Downloaded"), UNI_L("")};
#else // WIN32 && Unix
	const uni_char *old_filenames[] = { UNI_L("opera6.adr"), UNI_L("opera.dir"), UNI_L("global.dat"),
									UNI_L("override.ini"), UNI_L("override_downloaded.ini"), UNI_L("")};
#endif
	const uni_char *new_filenames[] = { DEFAULT_BOOKMARKS_FILE, DEFAULT_DIRECTHISTORY_FILE, DEFAULT_GLOBALHISTORY_FILE,
									DEFAULT_OVERRIDES_FILE, DEFAULT_DL_OVERRIDES_FILE, UNI_L("")};

	OpFileFolder op_folder[] = { OPFILE_USERPREFS_FOLDER, OPFILE_USERPREFS_FOLDER, OPFILE_USERPREFS_FOLDER,
								 OPFILE_USERPREFS_FOLDER, OPFILE_USERPREFS_FOLDER, (OpFileFolder)0};

	const char *pref_section[] = { "User Prefs", "User Prefs", "User Prefs",
								"User Prefs", "User Prefs", ""};

	const char *pref_key[] = { "Hot List File Ver2", "Direct History File", "Global History File",
							"Overrides File", "Downloaded Overrides File", ""};


	// Loop for each entry that needs to be changed
	int i = 0;
	while (uni_strlen(old_filenames[i]))
	{
		OpString default_folder;

		// Get the folder name taking into account the parent folder stuff
		if (pinfo->default_folder_parents[op_folder[i]] != OPFILE_ABSOLUTE_FOLDER)
		{
			// Set the parent folder
			default_folder.Set(pinfo->default_folders[pinfo->default_folder_parents[op_folder[i]]]);

			// Make sure that we end with a slash
			if (default_folder[default_folder.Length() - 1] != PATHSEPCHAR)
				RETURN_IF_ERROR(default_folder.Append(PATHSEP));

			// Add on the actual path
			default_folder.Append(pinfo->default_folders[op_folder[i]]);
		}
		else
			default_folder.Set(pinfo->default_folders[op_folder[i]]);

		// Make sure that we end with a slash
		if (default_folder[default_folder.Length() - 1] != PATHSEPCHAR)
			RETURN_IF_ERROR(default_folder.Append(PATHSEP));

		// First try to translate the file, only if this works should we try and fix
		// any default preferences that may be in the pref file
		if (OpStatus::IsSuccess(FileUtils::TranslateIniFilename(default_folder.CStr(), old_filenames[i], new_filenames[i])) && pf)
		{
			// First check if there is an entry
			if (pf->IsKey(pref_section[i], pref_key[i]))
			{
				// Found a key so check if it is still pointing at the default value
				const OpStringC pref_filename = pf->ReadStringL(pref_section[i], pref_key[i]);

				// Looks for the last slash to split filename and path
				int last_slash = pref_filename.FindLastOf(PATHSEPCHAR);
				if (last_slash != KNotFound && (last_slash + 1) < pref_filename.Length())
				{
					OpString path_string, filename_string;

					// split into filename and path
					path_string.Set(pref_filename, last_slash + 1);
					filename_string.Set(pref_filename.CStr() + last_slash+1);

					// Check if the name is the old filename and it's in the default folder
					if (!filename_string.Compare(old_filenames[i]) && !path_string.Compare(default_folder.CStr()))
					{
						// If so KILL the pref finally
						RETURN_IF_LEAVE(pf->DeleteKeyL(pref_section[i], pref_key[i]));

						// Commit the file to disk and the Delete section loads everything
						RETURN_IF_LEAVE(pf->CommitL());
					}
				}
			}
		}
		i++;
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS FileUtils::TranslateIniFilename(const OpStringC& path, const OpStringC& old_filename, const OpStringC& new_filename)
{
	// Translate any of the old naming convention for some ini files to the new
	OpString	temp_filename;
	OpFile		new_ini_filename;
	BOOL        exists;

	RETURN_IF_ERROR(temp_filename.Set(path));
	int pos = temp_filename.FindLastOf(PATHSEPCHAR);
	if( pos == KNotFound || pos+1 != temp_filename.Length() )
		RETURN_IF_ERROR(temp_filename.Append(PATHSEP));
	RETURN_IF_ERROR(temp_filename.Append(new_filename));
	RETURN_IF_ERROR(new_ini_filename.Construct(temp_filename));

	// Only copy over the file if the new name doesn't exist yet
	if (OpStatus::IsSuccess(new_ini_filename.Exists(exists)) && !exists)
	{
		OpFile old_ini_filename;

		RETURN_IF_ERROR(temp_filename.Set(path));
		pos = temp_filename.FindLastOf(PATHSEPCHAR);
		if( pos == KNotFound || pos+1 != temp_filename.Length() )
			RETURN_IF_ERROR(temp_filename.Append(PATHSEP));
		RETURN_IF_ERROR(temp_filename.Append(old_filename));
		RETURN_IF_ERROR(old_ini_filename.Construct(temp_filename));

		// Only copy over the file if the old file actually exists
		if (OpStatus::IsSuccess(old_ini_filename.Exists(exists)) && exists)
			return DesktopOpFileUtils::Move(&old_ini_filename, &new_ini_filename);
	}

	return OpStatus::ERR;
}

#endif // USE_COMMON_RESOURCES

OP_STATUS FileUtils::IsDirectory(const OpStringC& path, BOOL& is_directory)
{
	is_directory = FALSE;
	OpFile file;
	OpFileInfo::Mode file_mode;

	RETURN_IF_ERROR(file.Construct(path));
	RETURN_IF_ERROR(file.GetMode(file_mode));

	is_directory = file_mode == OpFileInfo::DIRECTORY;

	return OpStatus::OK;
}
