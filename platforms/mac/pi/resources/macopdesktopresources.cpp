/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#include "core/pch.h"

#include "platforms/mac/pi/resources/macopdesktopresources.h"
#include "platforms/mac/folderdefines.h"
#include "platforms/mac/File/FileUtils_Mac.h"	
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefsfile/prefsfile.h"
#include "adjunct/desktop_util/version/operaversion.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

#include <pwd.h>

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/GadgetStartup.h"
#include "platforms/mac/util/CocoaPlatformUtils.h"
#endif // WIDGET_RUNTIME_SUPPORT

/////////////////////////////////////////////////////////////////////////////

OP_STATUS OpDesktopResources::Create(OpDesktopResources **newObj)
{
	*newObj = new MacOpDesktopResources();
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetResourceFolder(OpString &folder)
{
    FSRef fsref;
    BOOL contentsAdded = TRUE;
    // This !defined(_XCODE_) is for a reason. While debugging in XCode we want to see and use the resources 
    // we've actually just built not the ones we can find in /Applications/Opera.app
#ifdef WIDGET_RUNTIME_SUPPORT
    if (GadgetStartup::IsGadgetStartupRequested())
	{
	    if (OpFileUtils::GetOperaBundle())
		{
			CFURLGetFSRef(OpFileUtils::GetOperaBundle(), &fsref);
			contentsAdded = FALSE;
		}
	}
    else
#endif 
    {
		fsref = OpFileUtils::contentsFSRef;
	}
	RETURN_IF_ERROR(OpFileUtils::ConvertFSRefToUniPath(&fsref, &folder));
	
	if (!contentsAdded)
		OpFileUtils::AppendFolder(folder, UNI_L("Contents"));
	OpFileUtils::AppendFolder(folder, UNI_L("Resources"));
	
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetUpdateCheckerPath(OpString &checker_path)
{
	FSRef fsref;
	fsref = OpFileUtils::contentsFSRef;
	RETURN_IF_ERROR(OpFileUtils::ConvertFSRefToUniPath(&fsref, &checker_path));
	RETURN_IF_ERROR(checker_path.Append("/MacOS/opera_autoupdate"));

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetFixedPrefFolder(OpString &folder)
{
	return GetGlobalPrefFolder(folder);
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetGlobalPrefFolder(OpString &folder)
{
	if (OpFileUtils::FindFolder(kSystemPreferencesFolderType, folder))
	{
		OpFileUtils::AppendFolder(folder, OPERA_PREFS_FOLDER_NAME);

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetLargePrefFolder(OpString &folder, const uni_char *profile_name)
{
	return OpFileUtils::SetToMacFolder(MAC_OPFOLDER_APPLICATION, folder, profile_name) ? OpStatus::OK : OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetSmallPrefFolder(OpString &folder, const uni_char *profile_name)
{
	return OpFileUtils::SetToMacFolder(MAC_OPFOLDER_PREFERENCES, folder, profile_name) ? OpStatus::OK : OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetTempPrefFolder(OpString &folder, const uni_char *profile_name)
{
	return OpFileUtils::SetToMacFolder(MAC_OPFOLDER_CACHES, folder, profile_name) ? OpStatus::OK : OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetTempFolder(OpString &folder)
{
	if (OpFileUtils::SetToMacFolder(MAC_OPFOLDER_CACHES, folder))
	{
		OpFileUtils::AppendFolder(folder, UNI_L("Temp"));

		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetDesktopFolder(OpString &folder)
{
	return OpFileUtils::FindFolder(kDesktopFolderType, folder) ? OpStatus::OK : OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetDocumentsFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kDocumentsFolderType, folder))
	{
		return GetDesktopFolder(folder);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetDownloadsFolder(OpString &folder)
{
	OSType folder_type = 'down';	// Download folder
		
	return OpFileUtils::FindFolder(folder_type, folder) ? OpStatus::OK : OpStatus::ERR;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetPicturesFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kPictureDocumentsFolderType, folder))
	{
		return GetDesktopFolder(folder);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetVideosFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kMovieDocumentsFolderType, folder))
	{
		return GetDesktopFolder(folder);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetMusicFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kMusicDocumentsFolderType, folder))
	{
		return GetDesktopFolder(folder);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetPublicFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kPublicFolderType, folder))
	{
		return GetDesktopFolder(folder);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetDefaultShareFolder(OpString &folder)
{
	if(!OpFileUtils::FindFolder(kPublicFolderType, folder))
	{
		return GetDesktopFolder(folder);		
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetLocaleFolder(OpString &folder)
{
	// On Mac the languages are in the resource folder
	return MacOpDesktopResources::GetResourceFolder(folder);
}

/////////////////////////////////////////////////////////////////////////////

OpFolderLister* MacOpDesktopResources::GetLocaleFolders(const uni_char* locale_root)
{
	OpFolderLister* lister;
	if (OpStatus::IsError(OpFolderLister::Create(&lister)) ||
		OpStatus::IsError(lister->Construct(locale_root, UNI_L("*.lproj"))))
	{
		OP_DELETE(lister);
		return NULL;
	}
	return lister;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code)
{
	if (lang_code.HasContent())
	{
		RETURN_IF_ERROR(folder_name.Set(lang_code));
	}
	else
	{
		if (m_language_code.IsEmpty())
		{
			RETURN_IF_ERROR(m_language_code.Set(GetDefaultLanguage()));
		}
		RETURN_IF_ERROR(folder_name.Set(m_language_code));
	}
	if (type == LOCALE)
	{
		RETURN_IF_ERROR(folder_name.Append(UNI_L(".lproj")));
	}
	else if (type == REGION)
	{
		// strip territory/script/variant code - only language code is used
		// inside region folder (DSK-346940)
		int pos = folder_name.FindFirstOf(UNI_L('-'));
		if (pos == KNotFound)
			pos = folder_name.FindFirstOf(UNI_L('_'));
		if (pos > 0)
			folder_name.Delete(pos);
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetLanguageFileName(OpString& file_name, OpStringC lang_code)
{
	if (lang_code.HasContent())
	{
		RETURN_IF_ERROR(file_name.Set(lang_code));
	}
	else
	{
		if (m_language_code.IsEmpty())
		{
			RETURN_IF_ERROR(m_language_code.Set(GetDefaultLanguage()));
		}
		RETURN_IF_ERROR(file_name.Set(m_language_code));
	}
	return file_name.Append(UNI_L(".lng"));
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetCountryCode(OpString& country_code)
{
	country_code.Empty();

	CFLocaleRef userLocaleRef;
	CFStringRef stringRef;

	userLocaleRef = CFLocaleCopyCurrent();
	stringRef = (CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleCountryCode);
	int len = 0;
	if (stringRef) {
		len = CFStringGetLength(stringRef);
		RETURN_OOM_IF_NULL(country_code.Reserve(len+1));
		CFStringGetCharacters(stringRef, CFRangeMake(0,len), (UniChar *)country_code.CStr());
		country_code[len] = 0;
	}
	CFRelease(userLocaleRef);

	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::EnsureCacheFolderIsAvailable(OpString &folder)
{
	CheckAndFixCacheFolderName(folder);
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString resource_dir, largepref_dir, smallpref_dir;
	
	RETURN_IF_ERROR(GetResourceFolder(resource_dir));
	RETURN_IF_ERROR(GetLargePrefFolder(largepref_dir));
	RETURN_IF_ERROR(GetSmallPrefFolder(smallpref_dir));

	return OpFileUtils::ExpandSystemVariablesInString(in, out, out_max_len, resource_dir, largepref_dir, smallpref_dir);
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString resource_dir, largepref_dir, smallpref_dir;
	
	RETURN_IF_ERROR(GetResourceFolder(resource_dir));
	RETURN_IF_ERROR(GetLargePrefFolder(largepref_dir));
	RETURN_IF_ERROR(GetSmallPrefFolder(smallpref_dir));

	return OpFileUtils::SerializeFileName(in, out, out_max_len, resource_dir, largepref_dir, smallpref_dir);
}

/////////////////////////////////////////////////////////////////////////////

BOOL MacOpDesktopResources::IsSameVolume(const uni_char* path1, const uni_char* path2)
{
	const uni_char * voff1 = uni_strstr(path1, UNI_L("/Volumes/"));
	const uni_char * voff2 = uni_strstr(path2, UNI_L("/Volumes/"));
	if (voff1 != path1 && voff2 != path2) {
		// Both on boot volume
		return TRUE;
	}
	if (voff1 == path1 && voff2 == path2) {
		// Both on non-boot
		voff1 = uni_strchr(path1+9, UNI_L('/'));
		voff2 = uni_strchr(path2+9, UNI_L('/'));
		if (voff1 && voff2) {
			int vlen1=(voff1-path1);
			int vlen2=(voff2-path2);
			if (vlen1==vlen2 && uni_strncmp(path1,path2,vlen1)==0)
				return TRUE;
		}
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpDesktopResources::GetOldProfileLocation(OpString& old_profile_path)
{
	old_profile_path.Empty();
	
	return OpStatus::OK;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef WIDGET_RUNTIME_SUPPORT
OP_STATUS MacOpDesktopResources::GetGadgetsFolder(OpString &folder)
{
	return GetGadgetsFolder(folder, FALSE);
}

OP_STATUS MacOpDesktopResources::GetGadgetsFolder(OpString &folder, BOOL create)
{
    BOOL has_admin_access;
    RETURN_IF_ERROR(CocoaPlatformUtils::HasAdminAccess(has_admin_access));
     
    if (has_admin_access)
    {	
		if (OpFileUtils::FindFolder(kApplicationsFolderType, folder))
		{		
			return OpStatus::OK;
		}
	}
	else
	{
        // if we can't write to global applications folder
        // we will store widgets file in ~/applications folder 
        if (OpFileUtils::FindFolder(kApplicationsFolderType, folder, create, kUserDomain))
        {       
             return OpStatus::OK;
        }		
	}
	return OpStatus::ERR;	
}
#endif // WIDGET_RUNTIME_SUPPORT
/////////////////////////////////////////////////////////////////////////////

void MacOpDesktopResources::FixBrokenSandboxing()
{
	if (VER_NUM_MAJOR < 12 && SandboxPrefsExistWithIsNewestVersionLowerThan12())
	{
		// Only move the prefs from the sandboxed location to the real location if no 
		// real prefs already exists
		if (!PrefsInCommonLocation())
		{
			// version is candidate for repairing
			OpString user_home;
			OpString user_lib;
			OpString container_path;
			
			RETURN_VOID_IF_ERROR(user_home.Set(getpwuid(getuid())->pw_dir));
			const char* opera_container_path = "/Library/Containers/com.operasoftware.Opera";
			const char* container_data = "/Data/Library";
			const char* lib_path = "/Library";
			RETURN_VOID_IF_ERROR(user_lib.Set(user_home));
			RETURN_VOID_IF_ERROR(user_lib.Append(lib_path));
			RETURN_VOID_IF_ERROR(container_path.Set(user_home));
			RETURN_VOID_IF_ERROR(container_path.Append(opera_container_path));
			RETURN_VOID_IF_ERROR(container_path.Append(container_data));
			
			uni_char** paths_to_move = new uni_char*[3];
			paths_to_move[0] = UNI_L("/Opera/");
			paths_to_move[1] = UNI_L("/Caches/Opera/");
			paths_to_move[2] = UNI_L("/Application Support/Opera/");
			
			for (int i = 0; i < 3; ++i)
			{
				OpString move_src;
				OpString move_dst;
				
				move_src.Set(container_path);
				move_src.Append(paths_to_move[i]);
				move_dst.Set(user_lib);
				move_dst.Append(paths_to_move[i]);
				OpFile src_file, dst_file;
				src_file.Construct(move_src);
				dst_file.Construct(move_dst);
				DesktopOpFileUtils::Move(&src_file, &dst_file);
			}
		}

		// Always kill the dodgey sandboxed prefs
		// It's done this was since it was the quickest and easiet to kill as many folders
		// we have permission to kill without stopping on the first error!
		system("rm -Rf ~/Library/Containers/com.operasoftware.Opera");
		system("rm -Rf ~/Library/Containers/com.yourcompany.Opera/");
	}
}

bool MacOpDesktopResources::SandboxPrefsExistWithIsNewestVersionLowerThan12()
{
	OpString prefs_file;
	RETURN_VALUE_IF_ERROR(prefs_file.Set(getpwuid(getuid())->pw_dir), false);
	const char* opera_prefs_file = "/Library/Containers/com.operasoftware.Opera/Data/Library/Opera/operaprefs.ini";
	RETURN_VALUE_IF_ERROR(prefs_file.Append(opera_prefs_file), false);
	OpFile operaprefs;
	RETURN_VALUE_IF_ERROR(operaprefs.Construct(prefs_file.CStr()), false);
	RETURN_VALUE_IF_ERROR(operaprefs.Open(OPFILE_READ), false);
	PrefsFile pf(PREFS_INI, 0, 0);
	{
		RETURN_IF_LEAVE(pf.ConstructL(); \
						pf.SetFileL(&operaprefs); \
						pf.LoadAllL()
						);
	}
	OpString newest_opera_version;
	RETURN_IF_LEAVE(pf.ReadStringL(UNI_L("Install"), UNI_L("Newest Used Version"), newest_opera_version));

	// couldn't find the entry so just leave the prefs alone
	if (!newest_opera_version.HasContent())
		return false;
	
	OperaVersion version, ov1200, ov1100;
	RETURN_VALUE_IF_ERROR(version.Set(newest_opera_version.CStr()), false);
	RETURN_VALUE_IF_ERROR(ov1100.Set(UNI_L("11.00.00")), false);
	RETURN_VALUE_IF_ERROR(ov1200.Set(UNI_L("12.00.00")), false);

	// Only return true if there is a version between 11.00 and 11.99 just so
	// we don't ever kill someones prefs
	return (version >= ov1100 && version < ov1200);
}

bool MacOpDesktopResources::PrefsInCommonLocation()
{
	OpString prefs_file;
	RETURN_VALUE_IF_ERROR(prefs_file.Set(getpwuid(getuid())->pw_dir), true);
	const char* prefs_in_lib = "/Library/Opera/operaprefs.ini";
	RETURN_VALUE_IF_ERROR(prefs_file.Append(prefs_in_lib), true);
	OpFile file;
	file.Construct(prefs_file);
	BOOL exists;
	RETURN_VALUE_IF_ERROR(file.Exists(exists), true);
	file.Close();
	RETURN_VALUE_IF_ERROR(prefs_file.Set(getpwuid(getuid())->pw_dir), true);
	// cleanup for ::Move
	if (!exists)
	{
		RETURN_VALUE_IF_ERROR(prefs_file.Set(getpwuid(getuid())->pw_dir), true);
		uni_char** paths_to_del = new uni_char*[3];
		paths_to_del[0] = UNI_L("/Library/Opera");
		paths_to_del[1] = UNI_L("/Library/Caches/Opera");
		paths_to_del[2] = UNI_L("/Library/Application Support/Opera");
		for (int i = 0; i < 3; ++i)
		{
			OpString rmpath;
			
			rmpath.Set(prefs_file);
			rmpath.Append(paths_to_del[i]);
			OpFile rm_file;
			rm_file.Construct(rmpath);
			rm_file.Delete();
		}
	}
	return exists;
}
