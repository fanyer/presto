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

#ifdef USE_COMMON_RESOURCES

#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/WebServerManager.h"

#include "modules/util/opfile/opfile.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_protection.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceSetup::CopyDefaultFile(const OpStringC &filename_source, const OpStringC &filename_dest, OperaInitInfo *pinfo)
{
	OpString	filename_temp;
	OpFile		userprefs_file;
	BOOL		exists = FALSE;
	
	// Build the user preferences folder
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_USERPREFS_FOLDER, pinfo, filename_temp));
	
	// Build the path to the destination file
	RETURN_IF_ERROR(filename_temp.Append(filename_dest));
	RETURN_IF_ERROR(userprefs_file.Construct(filename_temp.CStr()));
	
	// Check if the destination file exists
	if (OpStatus::IsSuccess(userprefs_file.Exists(exists)) && !exists)
	{
		OpFile	default_file;

		OP_STATUS status = GetPackageFile(filename_source, default_file, pinfo);
		if (OpStatus::IsSuccess(status))
		{
			// Default file exists - copy the content
			RETURN_IF_ERROR(userprefs_file.CopyContents(&default_file, FALSE));
		}
		else if (status != OpStatus::ERR_FILE_NOT_FOUND)
		{
			return status;
		}
	}

    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ResourceSetup
{
	struct FolderInfo
	{
		OpFileFolder folder;
		bool package_only;  // true if this is only for GetPackageFile
		bool is_custom;     // true if folder is OPFILE_*_CUSTOM_FOLDER
	};

	// possible locations of customization files (operaprefs.ini, search.ini, etc.) in the order
	// they should be checked when looking for a file
	const FolderInfo g_prefs_folders[] = {
		{ OPFILE_REGION_LANGUAGE_CUSTOM_FOLDER, false, true },
		{ OPFILE_REGION_CUSTOM_FOLDER, false, true },
		{ OPFILE_LANGUAGE_CUSTOM_FOLDER, false, true },
		{ OPFILE_INI_CUSTOM_FOLDER, false, true },
		{ OPFILE_REGION_LANGUAGE_CUSTOM_PACKAGE_FOLDER, true, true },
		{ OPFILE_REGION_CUSTOM_PACKAGE_FOLDER, true, true },
		{ OPFILE_LANGUAGE_CUSTOM_PACKAGE_FOLDER, true, true },
		{ OPFILE_INI_CUSTOM_PACKAGE_FOLDER, true, true },
		{ OPFILE_REGION_LANGUAGE_FOLDER, false, false },
		{ OPFILE_REGION_FOLDER, false, false },
		{ OPFILE_DEFAULT_REGION_LANGUAGE_FOLDER, false, false }, // DSK-352090
		{ OPFILE_LANGUAGE_FOLDER, false, false },
		{ OPFILE_INI_FOLDER, false, false }
	};
}

OP_STATUS ResourceSetup::GetPackageFile(const OpStringC &filename_source, OpFile& default_file, OperaInitInfo *pinfo)
{
	if (!pinfo)
		pinfo = g_desktop_bootstrap->GetInitInfo();

	if (!pinfo)
		return OpStatus::ERR;

	for (size_t i = 0; i < ARRAY_SIZE(g_prefs_folders); ++i)
	{
		OpString filename_temp;
		RETURN_IF_ERROR(ResourceFolders::GetFolder(g_prefs_folders[i].folder, pinfo, filename_temp));
		RETURN_IF_ERROR(filename_temp.Append(filename_source));

		OpFile file_temp;
		RETURN_IF_ERROR(file_temp.Construct(filename_temp.CStr()));

		BOOL exists = FALSE;
		if (OpStatus::IsSuccess(file_temp.Exists(exists)) && exists)
		{
			return default_file.Construct(filename_temp.CStr());
		}
	}

	return OpStatus::ERR_FILE_NOT_FOUND;
}

OP_STATUS ResourceSetup::GetDefaultPrefsFile(const OpStringC& filename, OpFile& file, PrefsFileChecker* checker /*= NULL*/, bool* is_custom /*= NULL*/)
{
	for (size_t i = 0; i < ARRAY_SIZE(g_prefs_folders); ++i)
	{
		if (!g_prefs_folders[i].package_only)
		{
			BOOL is_valid;
			OP_STATUS status;
			if (checker)
			{
				status = checker->CheckFile(g_prefs_folders[i].folder, filename, is_valid);
			}
			else
			{
				OpFile temp;
				RETURN_IF_ERROR(temp.Construct(filename, g_prefs_folders[i].folder));
				status = temp.Exists(is_valid);
			}
			if (OpStatus::IsSuccess(status) && is_valid)
			{
				if (is_custom)
				{
					*is_custom = g_prefs_folders[i].is_custom;
				}
				return file.Construct(filename, g_prefs_folders[i].folder);
			}
		}
	}

	return OpStatus::ERR_FILE_NOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceSetup::CopyCustomFolders(OperaInitInfo *pinfo)
{
#if defined(SKIN_USE_CUSTOM_FOLDERS)
	OpFile		custom_package_opfile, custom_opfile;
	OpString	filename_temp;
	BOOL		src_exists = FALSE;
	BOOL		dst_exists = FALSE;

	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(custom_package_opfile.Construct(filename_temp.CStr()));

	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_RESOURCES_CUSTOM_FOLDER, pinfo, filename_temp));
	RETURN_IF_ERROR(custom_opfile.Construct(filename_temp.CStr()));
	RETURN_IF_ERROR(custom_opfile.Exists(dst_exists));

	if (OpStatus::IsSuccess(custom_package_opfile.Exists(src_exists)) && src_exists)
	{
		// do the copying on an upgrade or if custom folder does not exist in the preferences folder
		if (g_run_type->m_type == StartupType::RUNTYPE_FIRST ||
			g_run_type->m_type == StartupType::RUNTYPE_FIRSTCLEAN ||
			g_run_type->m_type == StartupType::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
			!dst_exists)
		{
			g_run_type->m_added_custom_folder = true;

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			// Clear current selection of hardcoded search engines, because it may
			// be wrong for search.ini from custom package (this will trigger new
			// selection when SearchEngineManager starts).
			SearchProtection::ClearIdsOfHardcodedSearches();
#endif // DESKTOP_UTIL_SEARCH_ENGINES

			// In the case we are an elevated process, we have already copied and started using the custom package.
			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall) && dst_exists)
				return OpStatus::OK;

			// If the folder exists kill it, if the custom package is included
			RETURN_IF_ERROR(custom_opfile.Delete(TRUE));

			RETURN_IF_ERROR(DesktopOpFileUtils::Copy(custom_opfile, custom_package_opfile, TRUE, FALSE));
		}
	}
#endif // defined(SKIN_USE_CUSTOM_FOLDERS)
    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef GADGET_SUPPORT
#ifndef WIDGET_RUNTIME_SUPPORT

OP_STATUS ResourceSetup::InstallGadgets()
{
	// Search the widgets folder under resources and add all the widgets in here
	OpFolderLister* lister = NULL;
	OpString 		gadget_path, gadget_file;
	OpFile 			gadget_file_src, gadget_file_dest;

	// Grab the gadget folder path to reuse
	OpString tmp_storage;
	gadget_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_GADGET_FOLDER, tmp_storage));

	// Get all the zip files in the package folder
	lister = OpFile::GetFolderLister(OPFILE_GADGET_PACKAGE_FOLDER, UNI_L("*.zip"));
	if (lister)
	{
		while (lister && lister->Next())
		{
			// Set the destination filename
			gadget_file.Set(gadget_path.CStr());
			gadget_file.Append(lister->GetFileName());

			// Check if the gadget is installed already
			if (g_desktop_gadget_manager->IsGadgetInstalled(gadget_file))
				continue;

			// Create the opfiles and copy the file over
			if (OpStatus::IsSuccess(gadget_file_src.Construct(lister->GetFullPath())) &&
				OpStatus::IsSuccess(gadget_file_dest.Construct(gadget_file.CStr())) &&
				OpStatus::IsSuccess(gadget_file_dest.CopyContents(&gadget_file_src, FALSE)))
			{
				// Add the gadget
				g_hotlist_manager->NewGadget(gadget_file, OpStringC(), NULL, FALSE, FALSE);
			}
		}

		OP_DELETE(lister);
	}

    return OpStatus::OK;
}
#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WEBSERVER_SUPPORT

OP_STATUS ResourceSetup::InstallUniteServices()
{
// Not forward ported yet up to the Unite team
//	g_webserver_manager->LoadCustomDefaultServices();
	
	return OpStatus::OK;
}

#endif // WEBSERVER_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // USE_COMMON_RESOURCES


