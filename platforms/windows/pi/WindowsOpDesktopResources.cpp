
#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpDesktopResources.h"

#include "adjunct/desktop_util/resources/resourcedefines.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/windows_ui/Registry.h"
#include "platforms/windows/utils/authorization.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfile.h"

#define KF_FLAG_CREATE	0x00008000
#define KF_FLAG_INIT	0x00000800

OP_STATUS OpDesktopResources::Create(OpDesktopResources **newObj)
{
	OP_ASSERT(newObj != NULL);
	*newObj = OP_NEW(WindowsOpDesktopResources, ());
	
	if (*newObj == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}



WindowsOpDesktopResources::WindowsOpDesktopResources()
{
	GetExePath(m_opera_directory);
	int lastsep = m_opera_directory.FindLastOf(UNI_L(PATHSEPCHAR));
	m_opera_directory.Delete(lastsep);

	lastsep = m_opera_directory.FindLastOf(UNI_L(PATHSEPCHAR));
	if (lastsep != KNotFound)
		m_opera_last_path_component.Set(m_opera_directory.SubString(lastsep+1));
	else
		m_opera_last_path_component.Set("Opera");

	m_opera_directory.Append(UNI_L(PATHSEP));

	m_checker_path.AppendFormat(UNI_L("%supdatechecker%sopera_autoupdate.exe"), m_opera_directory.CStr(), PATHSEP);

	//If something goes horribly wrong when trying to get the multiuserprofile value, it's best to have it set to false.
	m_usermultiuserprofiles = FALSE;

	OpString globalsettings;
	globalsettings.AppendFormat(UNI_L("%s")UNI_L(PATHSEP)UNI_L("%s"), m_opera_directory.CStr(), DESKTOP_RES_OPERA_PREFS_GLOBAL);

	OpFile globalsettingfile;
	RETURN_VOID_IF_ERROR(globalsettingfile.Construct(globalsettings));

	PrefsFile global_prefsfile(PREFS_STD);
#if defined(_DEBUG) || defined(_VTUNE)
	TRAPD(status, global_prefsfile.ConstructL(); \
		global_prefsfile.SetFileL(&globalsettingfile); \
		m_usermultiuserprofiles = global_prefsfile.ReadIntL("System", "Multi User", 0); \
		global_prefsfile.ReadStringL("User Prefs", "Language Files Directory", m_language_directory, OpString()));
#else
	TRAPD(status, global_prefsfile.ConstructL(); \
		global_prefsfile.SetFileL(&globalsettingfile); \
		m_usermultiuserprofiles = global_prefsfile.ReadIntL("System", "Multi User", 1); \
		global_prefsfile.ReadStringL("User Prefs", "Language Files Directory", m_language_directory, OpString()));
#endif

	if (!m_usermultiuserprofiles && IsSystemWinVista())
	{
		DWORD UAC_state;
		if (OpRegReadDWORDValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"), UNI_L("EnableLUA"), &UAC_state) == ERROR_SUCCESS && UAC_state == 1)
			MergeVirtualStoreProfile();
	}
	
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::OWIInstall) || CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
		m_usermultiuserprofiles = FALSE;
}

void WindowsOpDesktopResources::MergeVirtualStoreProfile()
{
	OpString profile;
	OpString virtual_store_profile;
	OpFile profile_folder;
	OpFile virtual_store_profile_folder;

	RETURN_VOID_IF_ERROR(profile.Set(m_opera_directory));
	RETURN_VOID_IF_ERROR(profile.Append(UNI_L("profile") UNI_L(PATHSEP)));
	RETURN_VOID_IF_ERROR(profile_folder.Construct(profile));

	RETURN_VOID_IF_ERROR(GetFolderPath(FOLDERID_LocalAppData, CSIDL_LOCAL_APPDATA , virtual_store_profile));
	RETURN_VOID_IF_ERROR(virtual_store_profile.Append(UNI_L(PATHSEP) UNI_L("VirtualStore")));
	RETURN_VOID_IF_ERROR(virtual_store_profile.Append(m_opera_directory.SubString(m_opera_directory.FindFirstOf(PATHSEPCHAR))));
	RETURN_VOID_IF_ERROR(virtual_store_profile.Append(UNI_L("profile") UNI_L(PATHSEP)));
	RETURN_VOID_IF_ERROR(virtual_store_profile_folder.Construct(virtual_store_profile));

	BOOL exists;

	if (OpStatus::IsError(virtual_store_profile_folder.Exists(exists)) || !exists)
		return;

	if (OpStatus::IsError(profile_folder.Exists(exists)) || !exists)
		profile_folder.MakeDirectory();

	const OpStringC THIS_FOLDER(UNI_L("."));
	const OpStringC PARENT_FOLDER(UNI_L(".."));

	OpAutoVector<OpFolderLister> lister_stack;
	OpFolderLister* folderlister;
	OpString subfolder;
	subfolder.Empty();

	RETURN_VOID_IF_ERROR(OpFolderLister::Create(&folderlister));
	RETURN_VOID_IF_ERROR(folderlister->Construct(virtual_store_profile.CStr(), UNI_L("*")));
	RETURN_VOID_IF_ERROR(lister_stack.Add(folderlister));

	while(lister_stack.GetCount() > 0)
	{
		folderlister = lister_stack.Get(lister_stack.GetCount() - 1);

		while (folderlister->Next())
		{
			if (THIS_FOLDER.Compare(folderlister->GetFileName()) == 0 ||
				PARENT_FOLDER.Compare(folderlister->GetFileName()) == 0)
					continue;
			if (folderlister->IsFolder())
			{
				OpString new_folder_path;
				OpFile new_folder;
				RETURN_VOID_IF_ERROR(new_folder_path.Set(profile));
				RETURN_VOID_IF_ERROR(new_folder_path.Append(subfolder));
				RETURN_VOID_IF_ERROR(new_folder_path.Append(folderlister->GetFileName()));
				RETURN_VOID_IF_ERROR(new_folder.Construct(new_folder_path));
				if (OpStatus::IsError(new_folder.Exists(exists)) || !exists)
					new_folder.MakeDirectory();

				RETURN_VOID_IF_ERROR(subfolder.Append(folderlister->GetFileName()));
				RETURN_VOID_IF_ERROR(subfolder.Append(PATHSEP));

				OpFolderLister* newfolderlister;
				RETURN_VOID_IF_ERROR(OpFolderLister::Create(&newfolderlister));
				RETURN_VOID_IF_ERROR(newfolderlister->Construct(folderlister->GetFullPath(), UNI_L("*")));
				RETURN_VOID_IF_ERROR(lister_stack.Add(newfolderlister));
				folderlister=newfolderlister;
			}
			else
			{
				OpString dst_file;
				RETURN_VOID_IF_ERROR(dst_file.Set(profile));
				RETURN_VOID_IF_ERROR(dst_file.Append(subfolder));
				RETURN_VOID_IF_ERROR(dst_file.Append(folderlister->GetFileName()));
				MoveFileEx(folderlister->GetFullPath(), dst_file.CStr(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
			}
		}
		OpString rm_path;
		RETURN_VOID_IF_ERROR(rm_path.Set(virtual_store_profile));
		RETURN_VOID_IF_ERROR(rm_path.Append(subfolder));
		RemoveDirectory(rm_path.CStr());

		subfolder.Delete(subfolder.Length() - 1);
		int last_sep = subfolder.FindLastOf(PATHSEPCHAR);
		if (last_sep == KNotFound)
			subfolder.Empty();
		else
			subfolder.Delete(last_sep + 1);
		lister_stack.Delete(folderlister);
	}
}

OP_STATUS WindowsOpDesktopResources::GetResourceFolder(OpString &folder)
{
	return (folder.Set(m_opera_directory));
}

OP_STATUS WindowsOpDesktopResources::GetFixedPrefFolder(OpString &folder)
{
	return GetFolderPath(FOLDERID_System, CSIDL_SYSTEM, folder);
}

OP_STATUS WindowsOpDesktopResources::GetGlobalPrefFolder(OpString &folder)
{
	return GetResourceFolder(folder);
}

OP_STATUS WindowsOpDesktopResources::GetLargePrefFolder(OpString &folder, const uni_char *profile_name)
{
	// Use the specified directory if running with -pd (personal directory) cmdline argument
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory))
	{
		return folder.Set(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory)->m_string_value);
	}

	// Use special test profile if running with -autotestmode cmdline argument
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
		RETURN_IF_ERROR(folder.Set(m_opera_directory));
		return folder.Append(UNI_L("profile-autotest") UNI_L(PATHSEP));
	}

	if (!m_usermultiuserprofiles && !profile_name)
	{
		RETURN_IF_ERROR(folder.Set(m_opera_directory));
		return folder.Append("profile" PATHSEP);
	}

	RETURN_IF_ERROR(GetFolderPath(FOLDERID_LocalAppData, CSIDL_LOCAL_APPDATA, folder));

	RETURN_IF_ERROR(folder.Append(PATHSEP "Opera" PATHSEP));

	if (profile_name && uni_strlen(profile_name))
		return folder.Append(profile_name);
	return folder.Append(m_opera_last_path_component);
}

OP_STATUS WindowsOpDesktopResources::GetSmallPrefFolder(OpString &folder, const uni_char *profile_name)
{
	// Use the specified directory if running with -pd (personal directory) cmdline argument
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory))
	{
		return folder.Set(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory)->m_string_value);
	}

	// Use special test profile if running with -autotestmode cmdline argument
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
	         RETURN_IF_ERROR(folder.Set(m_opera_directory)); // Opera installation directory
		 return folder.Append(UNI_L("profile-autotest\\"));
	}

	if (!m_usermultiuserprofiles && !profile_name)
	{
		RETURN_IF_ERROR(folder.Set(m_opera_directory));
		return folder.Append("profile" PATHSEP);
	}

	RETURN_IF_ERROR(GetFolderPath(FOLDERID_RoamingAppData, CSIDL_APPDATA, folder));

	RETURN_IF_ERROR(folder.Append(PATHSEP "Opera" PATHSEP));

	if (profile_name && uni_strlen(profile_name))
		return folder.Append(profile_name);
	return folder.Append(m_opera_last_path_component);
}

OP_STATUS WindowsOpDesktopResources::GetTempPrefFolder(OpString &folder, const uni_char *profile_name)
{
	return GetLargePrefFolder(folder, profile_name);
}

OP_STATUS WindowsOpDesktopResources::GetTempFolder(OpString &folder)
{
	if (folder.Reserve(_MAX_PATH) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (GetTempPath(_MAX_PATH, folder.CStr()) == 0)
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS WindowsOpDesktopResources::GetDesktopFolder(OpString &folder)
{
	return GetFolderPath(FOLDERID_Desktop, CSIDL_DESKTOP, folder);
}

OP_STATUS WindowsOpDesktopResources::GetDocumentsFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Documents, CSIDL_PERSONAL, folder);

	// fails with SHELL32.DLL version 4.0
	if (OpStatus::IsError(err))
		return GetDesktopFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetDownloadsFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Downloads, CSIDL_PERSONAL, folder);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetPicturesFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Pictures, CSIDL_MYPICTURES, folder, FALSE);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetVideosFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Videos, CSIDL_MYVIDEO, folder, FALSE);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetMusicFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Music, CSIDL_MYMUSIC, folder, FALSE);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetPublicFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_PublicDocuments, CSIDL_COMMON_DOCUMENTS, folder);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetHomeFolder(OpString &folder)
{
	OP_STATUS err = GetFolderPath(FOLDERID_Profile, CSIDL_PROFILE, folder);

	if (OpStatus::IsError(err))
		return GetDocumentsFolder(folder);

	return err;
}

OP_STATUS WindowsOpDesktopResources::GetDefaultShareFolder(OpString &folder)
{
	return GetDocumentsFolder(folder);
}

#ifdef WIDGET_RUNTIME_SUPPORT
OP_STATUS WindowsOpDesktopResources::GetGadgetsFolder(OpString &folder)
{
	return GetFolderPath(FOLDERID_LocalAppData, CSIDL_LOCAL_APPDATA, folder);
}
#endif // WIDGET_RUNTIME_SUPPORT

OP_STATUS WindowsOpDesktopResources::GetLocaleFolder(OpString &folder)
{
	RETURN_IF_ERROR(folder.Set(m_opera_directory));
	return folder.Append("locale" PATHSEP);
}

OpFolderLister* WindowsOpDesktopResources::GetLocaleFolders(const uni_char* locale_root)
{
	OpFolderLister* locale_folders;
	if (OpStatus::IsError(OpFolderLister::Create(&locale_folders)))
		return NULL;

	OpString locale_folder;
	locale_folder.Set(locale_root);

	locale_folders->Construct(locale_folder.CStr(), UNI_L("*"));

	return locale_folders;
}

OP_STATUS WindowsOpDesktopResources::GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code)
{
	if (lang_code.HasContent())
	{
		RETURN_IF_ERROR(folder_name.Set(lang_code));
	}
	else
	{
		if (m_language_code.IsEmpty())
		{
			OpString language_code;
			if (m_language_directory.HasContent())
				RETURN_IF_ERROR(language_code.Set(m_language_directory.SubString(m_language_directory.FindLastOf('\\')+1)));
			else
				RETURN_IF_ERROR(WindowsOpSystemInfo::GetSystemFallbackLanguage(&language_code));

			OpString language_file_path;
			RETURN_IF_ERROR(GetLocaleFolder(language_file_path));
			RETURN_IF_ERROR(language_file_path.Append(language_code));
			RETURN_IF_ERROR(language_file_path.Append(PATHSEP));
			RETURN_IF_ERROR(language_file_path.Append(language_code));
			RETURN_IF_ERROR(language_file_path.Append(".lng"));

			OpFile lang_file;
			RETURN_IF_ERROR(lang_file.Construct(language_file_path));

			BOOL exists;
			RETURN_IF_ERROR(lang_file.Exists(exists));

			if (exists)
				m_language_code.TakeOver(language_code);
			else
				RETURN_IF_ERROR(m_language_code.Set("en"));
		}
		RETURN_IF_ERROR(folder_name.Set(m_language_code));
	}
	if (type == REGION)
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

OP_STATUS WindowsOpDesktopResources::GetLanguageFileName(OpString& file_name, OpStringC lang_code)
{
	// file name = folder name + extension
	RETURN_IF_ERROR(GetLanguageFolderName(file_name, LOCALE, lang_code));
	return file_name.Append(".lng");
}

OP_STATUS WindowsOpDesktopResources::GetCountryCode(OpString& country_code)
{
	uni_char country[3];
	if (GetLocation(country))
	{
		return country_code.Set(country, 2);
	}
	country_code.Empty();
	return OpStatus::OK;
}

OP_STATUS WindowsOpDesktopResources::EnsureCacheFolderIsAvailable(OpString &folder)
{
	return OpStatus::OK;
}

OP_STATUS WindowsOpDesktopResources::ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString folder;
	OpString new_path;
	RETURN_IF_ERROR(new_path.Set(in));

	if (new_path.Compare(UNI_L("{LargePreferences}"), 18) == 0)
	{
		RETURN_IF_ERROR(GetLargePrefFolder(folder));
		new_path.Delete(0, 18);
		RETURN_IF_ERROR(new_path.Insert(0, folder));
	}
	if (new_path.Compare(UNI_L("{SmallPreferences}"), 18) == 0)
	{
		RETURN_IF_ERROR(GetSmallPrefFolder(folder));
		new_path.Delete(0, 18);
		RETURN_IF_ERROR(new_path.Insert(0, folder));
	}
	if (new_path.Compare(UNI_L("{Resources}"), 11) == 0)
	{
		RETURN_IF_ERROR(GetResourceFolder(folder));
		new_path.Delete(0, 11);
		RETURN_IF_ERROR(new_path.Insert(0, folder));
	}
	if (new_path.Compare(UNI_L("{Binaries}"), 10) == 0)
	{
		RETURN_IF_ERROR(GetResourceFolder(folder));
		new_path.Delete(0, 10);
		RETURN_IF_ERROR(new_path.Insert(0, folder));
	}
	if (new_path.Compare(UNI_L("{Home}"), 6) == 0)
	{
		RETURN_IF_ERROR(GetHomeFolder(folder));
		new_path.Delete(0, 6);
		RETURN_IF_ERROR(new_path.Insert(0, folder));
	}

	DWORD ret = ExpandEnvironmentStrings(new_path.CStr(), out, out_max_len);

	if (ret > (UINT)out_max_len || ret == 0)
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS WindowsOpDesktopResources::SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString serialized_path;
	OpString folder;
	serialized_path.Set(in);
	GetLargePrefFolder(folder);
	if (serialized_path.CompareI(folder, folder.Length()) == 0)
	{
		serialized_path.Delete(0, folder.Length());
		serialized_path.Insert(0, "{LargePreferences}");
	}
	GetSmallPrefFolder(folder);
	if (serialized_path.CompareI(folder, folder.Length()) == 0)
	{
		serialized_path.Delete(0, folder.Length());
		serialized_path.Insert(0, "{SmallPreferences}");
	}
	GetResourceFolder(folder);
	if (serialized_path.CompareI(folder, folder.Length()) == 0)
	{
		serialized_path.Delete(0, folder.Length());
		serialized_path.Insert(0, "{Resources}");
	}
	GetHomeFolder(folder);
	if (serialized_path.CompareI(folder, folder.Length()) == 0)
	{
		serialized_path.Delete(0, folder.Length());
		serialized_path.Insert(0, "{Home}");
	}

	uni_strlcpy(out, serialized_path.CStr(), out_max_len);

	return OpStatus::OK;
}

BOOL WindowsOpDesktopResources::IsSameVolume(const uni_char* path1, const uni_char* path2)
{
	OP_ASSERT(path1 && path2);

	if(!path1 || !path2)
	{
		return FALSE;
	}
	OpString8 tmp1;
	OpString8 tmp2;

	tmp1.Set(path1);
	tmp2.Set(path2);

	char vol1[_MAX_PATH];
	char vol2[_MAX_PATH];

	if(GetVolumePathNameA(tmp1.CStr(), vol1, _MAX_PATH))
	{
		if(GetVolumePathNameA(tmp2.CStr(), vol2, _MAX_PATH))
		{
			return op_stricmp(vol1, vol2) == 0;
		}
	}
	return FALSE;
}

OP_STATUS WindowsOpDesktopResources::GetOldProfileLocation(OpString& old_profile_path)
{
	old_profile_path.Empty();

	if (m_usermultiuserprofiles)
		return old_profile_path.Set("profile" PATHSEP);

	return OpStatus::OK;
}

OP_STATUS WindowsOpDesktopResources::GetFolderPath(REFKNOWNFOLDERID Vista_known_folder, int XP_CLSID_folder, OpString& folder, BOOL create)
{

	if (IsSystemWinVista())
	{
		uni_char* path;
		OP_STATUS err;

		// Might fail if the user down't have the right permissions.
		HRESULT res = ::OPSHGetKnownFolderPath(Vista_known_folder, 0, NULL, &path);
	
		// If we failed, it is most likely because of no permissions to create/init that folder.
		if (FAILED(res) && create)
			res = ::OPSHGetKnownFolderPath(Vista_known_folder, KF_FLAG_CREATE | KF_FLAG_INIT, NULL, &path);

		if (FAILED(res))
			return OpStatus::ERR;

		err = folder.Set(path);
		CoTaskMemFree(path);
		return err;
	}
	else
	{
		RETURN_OOM_IF_NULL(folder.Reserve(_MAX_PATH));

		HRESULT res = SHGetFolderPath(NULL, XP_CLSID_folder | CSIDL_FLAG_DONT_UNEXPAND, NULL, SHGFP_TYPE_CURRENT, folder);
		if (FAILED(res) && res != E_INVALIDARG && create)
			res = SHGetFolderPath(NULL, XP_CLSID_folder | CSIDL_FLAG_DONT_UNEXPAND | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, folder);

		if (FAILED(res))
			return OpStatus::ERR;

		return OpStatus::OK;
	}
}

