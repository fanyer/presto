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

#ifdef USE_COMMON_RESOURCES

#include "adjunct/desktop_util/resources/ResourceFolders.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

#include "adjunct/quick/managers/CommandLineManager.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opfolder.h"

// This is the biggest hack of all time but I need the language name to
// be available outside of this class and there isn't anywhere else for this unless
// OperaInitInfo changes to an OpString and you know how hard that is in core
OpString g_def_lang;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceFolders::SetFolders(OperaInitInfo *info, OpDesktopResources* desktop_resources, const uni_char *profile_name)
{
	// Make sure we have an OperaInitInfo object
	if (!info)
		return OpStatus::ERR;

	// Autopointer is only used if desktop_resources object is not provided by the caller
	OpAutoPtr<OpDesktopResources> dr_ap;
	if (!desktop_resources)
	{
		RETURN_IF_ERROR(OpDesktopResources::Create(&desktop_resources));
		dr_ap.reset(desktop_resources);
	}

	// Set all to something safe
	for (int i = 0; i < OPFILE_FOLDER_COUNT; i++)
		info->default_folder_parents[i] = OPFILE_ABSOLUTE_FOLDER;

	// Grab all the default prefs ones
	OpString folder_tmp;

	// These are all the parent folders that are different for each platform
	// These are the only ones that should be absolute paths
	RETURN_IF_ERROR(desktop_resources->GetResourceFolder(folder_tmp));
	info->default_folders[OPFILE_RESOURCES_FOLDER].TakeOver(folder_tmp);	// can't fail
	RETURN_IF_ERROR(desktop_resources->GetBinaryResourceFolder(folder_tmp));
	info->default_folders[OPFILE_BINARY_RESOURCES_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetFixedPrefFolder(folder_tmp));
	info->default_folders[OPFILE_FIXEDPREFS_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetGlobalPrefFolder(folder_tmp));
	info->default_folders[OPFILE_GLOBALPREFS_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetLargePrefFolder(folder_tmp, profile_name));
	info->default_folders[OPFILE_LOCAL_HOME_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetSmallPrefFolder(folder_tmp, profile_name));
	info->default_folders[OPFILE_HOME_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetTempPrefFolder(folder_tmp, profile_name));
	info->default_folders[OPFILE_CACHE_HOME_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetTempFolder(folder_tmp));
	info->default_folders[OPFILE_TEMP_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetDesktopFolder(folder_tmp));
	info->default_folders[OPFILE_DESKTOP_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetDocumentsFolder(folder_tmp));
	info->default_folders[OPFILE_DOCUMENTS_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetDownloadsFolder(folder_tmp));
	info->default_folders[OPFILE_DOWNLOAD_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetPicturesFolder(folder_tmp));
	info->default_folders[OPFILE_PICTURES_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetVideosFolder(folder_tmp));
	info->default_folders[OPFILE_VIDEOS_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetMusicFolder(folder_tmp));
	info->default_folders[OPFILE_MUSIC_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetPublicFolder(folder_tmp));
	info->default_folders[OPFILE_PUBLIC_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetDefaultShareFolder(folder_tmp));
	info->default_folders[OPFILE_DEFAULT_SHARED_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetLocaleFolder(folder_tmp));
	info->default_folders[OPFILE_LOCALE_FOLDER].TakeOver(folder_tmp);
#ifdef WIDGET_RUNTIME_SUPPORT
	// default path for runtime gadget manager to look for installed gadgets
	RETURN_IF_ERROR(desktop_resources->GetGadgetsFolder(folder_tmp));
	info->default_folders[OPFILE_DEFAULT_GADGET_FOLDER].TakeOver(folder_tmp);
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
#ifdef FOLDER_PARENT_SUPPORT
    info->default_locale_folders_parent = OPFILE_LOCALE_FOLDER;
#endif // FOLDER_PARENT_SUPPORT
    // Get the locale folders (used by for example) spellchecker
	OpFolderLister* locale_folders = desktop_resources->GetLocaleFolders(info->default_folders[OPFILE_LOCALE_FOLDER]);
	
	if (locale_folders)
	{
		while (locale_folders->Next())
		{
			if (locale_folders->IsFolder())
			{
				OpStringC file_name(locale_folders->GetFileName());
				if (file_name.HasContent() && file_name.Compare(".") != 0 && file_name.Compare("..") != 0)
				{
					RETURN_IF_ERROR(info->AddDefaultLocaleFolders(file_name));
				}
			}
		}

		OP_DELETE(locale_folders);
	}
#endif // FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT

#if defined(SKIN_USE_CUSTOM_FOLDERS)
	// Setup the two base custom folders, currently there is no support for multiple parents
	// so we have to do it like this

	// This call looks weird but guarantees it ends with a slash
	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_RESOURCES_FOLDER, info, folder_tmp));
	RETURN_IF_ERROR(folder_tmp.Append("custom"));
	info->default_folders[OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER].TakeOver(folder_tmp);

	RETURN_IF_ERROR(ResourceFolders::GetFolder(OPFILE_LOCAL_HOME_FOLDER, info, folder_tmp));
	RETURN_IF_ERROR(folder_tmp.Append("custom"));
	info->default_folders[OPFILE_RESOURCES_CUSTOM_FOLDER].TakeOver(folder_tmp);
#endif // defined(SKIN_USE_CUSTOM_FOLDERS)

	// Set all the folders that are the same as another folder.
	// No need to set the folder, just set the parent then.
	info->default_folder_parents[OPFILE_SECURE_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SECURE_FOLDER].Set(""));
	info->default_folder_parents[OPFILE_USERPREFS_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_USERPREFS_FOLDER].Set(""));

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
	info->default_folder_parents[OPFILE_TRUSTED_CERTS_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_TRUSTED_CERTS_FOLDER].Set(""));
	info->default_folder_parents[OPFILE_UNTRUSTED_CERTS_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UNTRUSTED_CERTS_FOLDER].Set(""));
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

	info->default_folder_parents[OPFILE_COOKIE_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_COOKIE_FOLDER].Set(""));

	// Setup all the other folders from these parents
	// All paths below here should be relative with a parent from above

	// Default ini file stuff that's not UI
	info->default_folder_parents[OPFILE_INI_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_INI_FOLDER].Set("defaults"));

	// Default UI stuff
	info->default_folder_parents[OPFILE_UI_INI_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UI_INI_FOLDER].Set("ui"));

	// Junk that doesn't fit anywhere
	info->default_folder_parents[OPFILE_AUX_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_AUX_FOLDER].Set("extra"));
	
	// Styles/User styles
	info->default_folder_parents[OPFILE_STYLE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_STYLE_FOLDER].Set("styles"));
	info->default_folder_parents[OPFILE_USERSTYLE_FOLDER] = OPFILE_STYLE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_USERSTYLE_FOLDER].Set("user"));

#ifdef OPFILE_HAVE_SCRIPT_FOLDER
	// Script files location
	info->default_folder_parents[OPFILE_SCRIPT_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SCRIPT_FOLDER].Set("scripts"));
#endif

	// Userstyles in the preferences
#ifdef PREFS_USE_CSS_FOLDER_SCAN
	info->default_folder_parents[OPFILE_USERPREFSSTYLE_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_USERPREFSSTYLE_FOLDER].AppendFormat(UNI_L("%s%s%s"), UNI_L("styles"), UNI_L(PATHSEP), UNI_L("user")));
#endif // PREFS_USE_CSS_FOLDER_SCAN

	// Sessions
#ifdef SESSION_SUPPORT
	info->default_folder_parents[OPFILE_SESSION_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SESSION_FOLDER].Set("sessions"));
#endif // SESSION_SUPPORT

	info->default_folder_parents[OPFILE_CACHE_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_CACHE_FOLDER].Set("cache"));

	info->default_folder_parents[OPFILE_APP_CACHE_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_APP_CACHE_FOLDER].Set("application_cache"));

	// On some platforms a new cache folder is assigned if this cache folder is in use
	RETURN_IF_ERROR(desktop_resources->EnsureCacheFolderIsAvailable(info->default_folders[OPFILE_CACHE_FOLDER]));

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	info->default_folder_parents[OPFILE_OCACHE_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_OCACHE_FOLDER].Set("opcache"));

	// On some platforms a new cache folder is assigned if this cache folder is in use
	RETURN_IF_ERROR(desktop_resources->EnsureCacheFolderIsAvailable(info->default_folders[OPFILE_OCACHE_FOLDER]));
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT __OEM_OPERATOR_CACHE_MANAGEMENT

	// VPS index
	info->default_folder_parents[OPFILE_VPS_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_VPS_FOLDER].Set("vps"));

	// Regions
	info->default_folder_parents[OPFILE_REGION_ROOT_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_REGION_ROOT_FOLDER].Set("region"));

	// Languages
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(folder_tmp, OpDesktopResources::LOCALE));
	info->default_folder_parents[OPFILE_LANGUAGE_FOLDER] = OPFILE_LOCALE_FOLDER;
	info->default_folders[OPFILE_LANGUAGE_FOLDER].TakeOver(folder_tmp);
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(folder_tmp, OpDesktopResources::LOCALE));
	info->default_folder_parents[OPFILE_DEFAULT_LANGUAGE_FOLDER] = OPFILE_LOCALE_FOLDER;
	info->default_folders[OPFILE_DEFAULT_LANGUAGE_FOLDER].TakeOver(folder_tmp);

	RETURN_IF_ERROR(desktop_resources->GetLanguageFileName(g_def_lang));

	// Default/fallback languages
	info->def_lang_file_folder = OPFILE_DEFAULT_LANGUAGE_FOLDER;
	info->def_lang_file_name = g_def_lang.CStr();

	// For us images are auxilary files (i.e should really be somewhere else)
	info->default_folder_parents[OPFILE_IMAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_IMAGE_FOLDER].Set("extra"));

	// Set upload to the desktop for now
#ifdef _FILE_UPLOAD_SUPPORT_
	info->default_folder_parents[OPFILE_UPLOAD_FOLDER] = OPFILE_DESKTOP_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UPLOAD_FOLDER].Set(""));
#endif // _FILE_UPLOAD_SUPPORT_

	// Skins
#ifdef SKIN_SUPPORT
	info->default_folder_parents[OPFILE_SKIN_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SKIN_FOLDER].Set("skin"));

	info->default_folder_parents[OPFILE_DEFAULT_SKIN_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_DEFAULT_SKIN_FOLDER].Set("skin"));
#endif // SKIN_SUPPORT

    // Persistent storage
    info->default_folder_parents[OPFILE_PERSISTENT_STORAGE_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
    RETURN_IF_ERROR(info->default_folders[OPFILE_PERSISTENT_STORAGE_FOLDER].Set(""));

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	info->default_folder_parents[OPFILE_DICTIONARY_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_DICTIONARY_FOLDER].Set("dictionaries"));
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WEBSERVER_SUPPORT
	info->default_folder_parents[OPFILE_WEBSERVER_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_WEBSERVER_FOLDER].Set("webserver"));

#ifdef GADGET_SUPPORT
	info->default_folder_parents[OPFILE_UNITE_PACKAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UNITE_PACKAGE_FOLDER].Set("unite"));
#endif // GADGET_SUPPORT
#endif // WEBSERVER_SUPPORT

	// Open and close defaults to documents
#ifdef _FILE_UPLOAD_SUPPORT_
	info->default_folder_parents[OPFILE_OPEN_FOLDER] = OPFILE_DOCUMENTS_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_OPEN_FOLDER].Set(""));
	info->default_folder_parents[OPFILE_SAVE_FOLDER] = OPFILE_DOCUMENTS_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SAVE_FOLDER].Set(""));
#endif // _FILE_UPLOAD_SUPPORT_

	info->default_folder_parents[OPFILE_BUTTON_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_BUTTON_FOLDER].Set("buttons"));

	info->default_folder_parents[OPFILE_ICON_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_ICON_FOLDER].Set("icons"));

# if defined SUPPORT_GENERATE_THUMBNAILS && defined SUPPORT_SAVE_THUMBNAILS
	info->default_folder_parents[OPFILE_THUMBNAIL_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_THUMBNAIL_FOLDER].Set("thumbnails"));
# endif // defined SUPPORT_GENERATE_THUMBNAILS && defined SUPPORT_SAVE_THUMBNAILS

	info->default_folder_parents[OPFILE_MENUSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_MENUSETUP_FOLDER].Set("menu"));

	info->default_folder_parents[OPFILE_TOOLBARSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_TOOLBARSETUP_FOLDER].Set("toolbar"));

	info->default_folder_parents[OPFILE_MOUSESETUP_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_MOUSESETUP_FOLDER].Set("mouse"));

	info->default_folder_parents[OPFILE_KEYBOARDSETUP_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_KEYBOARDSETUP_FOLDER].Set("keyboard"));

#ifdef M2_SUPPORT
	info->default_folder_parents[OPFILE_MAIL_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_MAIL_FOLDER].Set("mail"));
#endif // M2_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
	info->default_folder_parents[OPFILE_JSPLUGIN_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_JSPLUGIN_FOLDER].Set("jsplugins"));
#endif // JS_PLUGIN_SUPPORT

#ifdef GADGET_SUPPORT
	info->default_folder_parents[OPFILE_GADGET_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_GADGET_FOLDER].Set("widgets"));
	info->default_folder_parents[OPFILE_GADGET_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_GADGET_PACKAGE_FOLDER].Set("widgets"));
#endif // GADGET_SUPPORT

#ifdef _UNIX_DESKTOP_
#ifdef WIDGET_RUNTIME_SUPPORT
	// Packaging scripts for widgets on UNIX
	info->default_folder_parents[OPFILE_PACKAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_PACKAGE_FOLDER].Set("package"));
#endif // WIDGET_RUNTIME_SUPPORT
#endif // _UNIX_DESKTOP_

#if defined(MSWIN)
	info->default_folder_parents[OPFILE_JUMPLIST_CACHE_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_JUMPLIST_CACHE_FOLDER].Set("jumplist_icon_cache"));
#endif // defined(MSWIN)

	// Folder used to cache the icons used in the jump list on Windows 7+
	info->default_folder_parents[OPFILE_CUSTOM_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_CUSTOM_FOLDER].Set("custom"));
	info->default_folder_parents[OPFILE_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_CUSTOM_PACKAGE_FOLDER].Set("custom"));

#ifndef NO_EXTERNAL_APPLICATIONS
	info->default_folder_parents[OPFILE_TEMPDOWNLOAD_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_TEMPDOWNLOAD_FOLDER].Set("temporary_downloads"));
#endif // NO_EXTERNAL_APPLICATIONS

#ifdef _BITTORRENT_SUPPORT_
	info->default_folder_parents[OPFILE_BITTORRENT_METADATA_FOLDER] = OPFILE_LOCAL_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_BITTORRENT_METADATA_FOLDER].Set("bt_metadata"));
#endif // _BITTORRENT_SUPPORT_

#ifdef WEBFEEDS_BACKEND_SUPPORT
	info->default_folder_parents[OPFILE_WEBFEEDS_FOLDER] = OPFILE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_WEBFEEDS_FOLDER].Set("webfeeds"));
#endif // WEBFEEDS_BACKEND_SUPPORT

	// Custom folders
	info->default_folder_parents[OPFILE_INI_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_INI_CUSTOM_PACKAGE_FOLDER].Set("defaults"));
	info->default_folder_parents[OPFILE_INI_CUSTOM_FOLDER] = OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_INI_CUSTOM_FOLDER].Set("defaults"));

	info->default_folder_parents[OPFILE_LOCALE_CUSTOM_FOLDER] = OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_LOCALE_CUSTOM_FOLDER].Set("locale"));
	info->default_folder_parents[OPFILE_LANGUAGE_CUSTOM_FOLDER] = OPFILE_LOCALE_CUSTOM_FOLDER;
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(folder_tmp, OpDesktopResources::CUSTOM_LOCALE));
	info->default_folders[OPFILE_LANGUAGE_CUSTOM_FOLDER].TakeOver(folder_tmp);

	info->default_folder_parents[OPFILE_LOCALE_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_LOCALE_CUSTOM_PACKAGE_FOLDER].Set("locale"));
	info->default_folder_parents[OPFILE_LANGUAGE_CUSTOM_PACKAGE_FOLDER] = OPFILE_LOCALE_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(desktop_resources->GetLanguageFolderName(folder_tmp, OpDesktopResources::CUSTOM_LOCALE));
	info->default_folders[OPFILE_LANGUAGE_CUSTOM_PACKAGE_FOLDER].TakeOver(folder_tmp);

	info->default_folder_parents[OPFILE_UI_INI_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UI_INI_CUSTOM_PACKAGE_FOLDER].Set("ui"));
	info->default_folder_parents[OPFILE_UI_INI_CUSTOM_FOLDER] = OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UI_INI_CUSTOM_FOLDER].Set("ui"));

	info->default_folder_parents[OPFILE_REGION_ROOT_CUSTOM_FOLDER] = OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_REGION_ROOT_CUSTOM_FOLDER].Set("region"));

	info->default_folder_parents[OPFILE_REGION_ROOT_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_REGION_ROOT_CUSTOM_PACKAGE_FOLDER].Set("region"));

#ifdef WEBSERVER_SUPPORT
	info->default_folder_parents[OPFILE_UNITE_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_UNITE_CUSTOM_PACKAGE_FOLDER].Set("unite"));
#endif // WEBSERVER_SUPPORT
#ifdef WIDGET_RUNTIME_SUPPORT
	info->default_folder_parents[OPFILE_GADGETS_CUSTOM_FOLDER] =
			OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_GADGETS_CUSTOM_FOLDER].Set(
				"widgets"));
#endif // WIDGET_RUNTIME_SUPPORT

	info->default_folder_parents[OPFILE_EXTENSION_CUSTOM_PACKAGE_FOLDER] =
			OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_EXTENSION_CUSTOM_PACKAGE_FOLDER].Set(
				"extensions"));

	info->default_folder_parents[OPFILE_EXTENSION_CUSTOM_FOLDER] =
			OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_EXTENSION_CUSTOM_FOLDER].Set(
				"extensions"));

	// Region/language-based folders
	// Set to defaults for now - it will be called again once correct region and language are known.
	RETURN_IF_ERROR(SetRegionFolders(info, NULL, NULL, NULL));

	// Custom skin folders
#ifdef SKIN_USE_CUSTOM_FOLDERS
	info->default_folder_parents[OPFILE_SKIN_CUSTOM_PACKAGE_FOLDER] = OPFILE_RESOURCES_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SKIN_CUSTOM_PACKAGE_FOLDER].Set("skin"));
	info->default_folder_parents[OPFILE_SKIN_CUSTOM_FOLDER] = OPFILE_RESOURCES_CUSTOM_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SKIN_CUSTOM_FOLDER].Set("skin"));
#endif // SKIN_USE_CUSTOM_FOLDERS

	// GStreamer library/plugins folder
#ifdef MEDIA_BACKEND_GSTREAMER
	info->default_folder_parents[OPFILE_GSTREAMER_FOLDER] = OPFILE_BINARY_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_GSTREAMER_FOLDER].Set("gstreamer"));
#endif // MEDIA_BACKEND_GSTREAMER

	// not used in core yet, so just reusing the gstreamer folder
	info->default_folder_parents[OPFILE_VIDEO_FOLDER] = OPFILE_BINARY_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_VIDEO_FOLDER].Set("gstreamer"));

	// Read-Write: Location of the protocol handlers white or black list.
	info->default_folder_parents[OPFILE_HANDLERS_FOLDER] = OPFILE_INI_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_HANDLERS_FOLDER].Set(""));

	// Plugin wrapper binaries
#ifdef NS4P_COMPONENT_PLUGINS
	info->default_folder_parents[OPFILE_PLUGINWRAPPER_FOLDER] = OPFILE_BINARY_RESOURCES_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_PLUGINWRAPPER_FOLDER].Set("pluginwrapper"));
#endif // NS4P_COMPONENT_PLUGINS

	// Crash logs
	info->default_folder_parents[OPFILE_CRASHLOG_FOLDER] = OPFILE_CACHE_HOME_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_CRASHLOG_FOLDER].Set("logs"));

	// Selftest is special and is not a parent
#ifdef SELFTEST
	CommandLineArgument *cl_arg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::SourceRootDir);
	OpString self_test_path;

	// This should be set by a command line argument, if not we will assume this is being compiled
	// and use the __FILE__ to get the root based on the location of this file
	if (cl_arg)
		self_test_path.Set(cl_arg->m_string_value.CStr());
	else if (const char *env = op_getenv("SELFTEST_DATA_ROOT"))
		self_test_path.Set(env);
	else
	{
		// By doing this everything will work in debug without the need for a commandline arg

		// Set to the director that holds this folder
		self_test_path.Set(__FILE__);

		// Move back 4 folders to find the root source folder
		for (int i = 0; i < 4; i++)
		{
			int sep_pos = self_test_path.FindLastOf(PATHSEPCHAR);
			if (sep_pos != KNotFound)
				self_test_path.Set(self_test_path.CStr(), sep_pos);
			else
				break;
		}
	}

	// This is an absolute path
	info->default_folder_parents[OPFILE_SELFTEST_DATA_FOLDER] = OPFILE_ABSOLUTE_FOLDER;
	RETURN_IF_ERROR(info->default_folders[OPFILE_SELFTEST_DATA_FOLDER].Set(self_test_path));
#endif // SELFTEST

    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceFolders::SetRegionFolders(OperaInitInfo* pinfo, OpStringC in_region, OpStringC in_language, OpStringC in_default_language)
{
	OpStringC language = in_language.HasContent() ? in_language : OpStringC(UNI_L("en"));
	OpStringC default_language = in_default_language.HasContent() ? in_default_language : language;
	OpStringC region = in_region.HasContent() ? in_region : OpStringC(UNI_L("undefined"));

	pinfo->default_folder_parents[OPFILE_REGION_FOLDER] = OPFILE_REGION_ROOT_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_FOLDER].Set(region));
	pinfo->default_folder_parents[OPFILE_REGION_LANGUAGE_FOLDER] = OPFILE_REGION_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_LANGUAGE_FOLDER].Set(language));

	pinfo->default_folder_parents[OPFILE_REGION_CUSTOM_FOLDER] = OPFILE_REGION_ROOT_CUSTOM_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_CUSTOM_FOLDER].Set(region));
	pinfo->default_folder_parents[OPFILE_REGION_LANGUAGE_CUSTOM_FOLDER] = OPFILE_REGION_CUSTOM_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_LANGUAGE_CUSTOM_FOLDER].Set(language));

	pinfo->default_folder_parents[OPFILE_REGION_CUSTOM_PACKAGE_FOLDER] = OPFILE_REGION_ROOT_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_CUSTOM_PACKAGE_FOLDER].Set(region));
	pinfo->default_folder_parents[OPFILE_REGION_LANGUAGE_CUSTOM_PACKAGE_FOLDER] = OPFILE_REGION_CUSTOM_PACKAGE_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_REGION_LANGUAGE_CUSTOM_PACKAGE_FOLDER].Set(language));

	pinfo->default_folder_parents[OPFILE_DEFAULT_REGION_LANGUAGE_FOLDER] = OPFILE_LOCALE_FOLDER;
	RETURN_IF_ERROR(pinfo->default_folders[OPFILE_DEFAULT_REGION_LANGUAGE_FOLDER].Set(default_language));

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceFolders::GetFolder(OpFileFolder opfilefolder, OperaInitInfo *pinfo, OpString &folder)
{
	// Make sure we have an OperaInitInfo object
	if (!pinfo)
		return OpStatus::ERR;

	RETURN_IF_ERROR(folder.Set(pinfo->default_folders[opfilefolder]));

	// Make sure that we end with a slash
	if (folder.HasContent() && folder[folder.Length() - 1] != PATHSEPCHAR)
		RETURN_IF_ERROR(folder.Append(PATHSEP));

	// Get the folder name taking into account the parent folder stuff
	while (pinfo->default_folder_parents[opfilefolder] != OPFILE_ABSOLUTE_FOLDER)
	{
		OpString subfolder;
		// Set the parent folder
		RETURN_IF_ERROR(subfolder.Set(pinfo->default_folders[pinfo->default_folder_parents[opfilefolder]]));

		// Make sure that we end with a slash
		if (subfolder.HasContent() && subfolder[subfolder.Length() - 1] != PATHSEPCHAR)
			RETURN_IF_ERROR(subfolder.Append(PATHSEP));

		RETURN_IF_ERROR(folder.Insert(0, subfolder));

		opfilefolder = pinfo->default_folder_parents[opfilefolder];
	}

    return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS ResourceFolders::ComparePaths(const OpStringC &path1, const OpStringC &path2, BOOL &identical)
{
	// Consider using OpSystemInfo::PathsEqualL() !
	int len1 = path1.Length();
	int len2 = path2.Length();
	if (!path1.HasContent() || !path2.HasContent())
		return OpStatus::ERR;
	while (path1[len1-1] == PATHSEPCHAR)
		--len1;
	while (path2[len2-1] == PATHSEPCHAR)
		--len2;
	if (len1 == len2)
#ifdef MSWIN
		identical = (path1.CompareI(path2.CStr(), len1) == 0);
#else
		identical = (path1.Compare(path2.CStr(), len1) == 0);
#endif
	else
		identical = FALSE;
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // USE_COMMON_RESOURCES
