/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <sys/stat.h>

#include "platforms/mac/File/FileUtils_Mac.h"		// knows what a FileKind is.
#include "platforms/mac/folderdefines.h"
#include "platforms/mac/util/macutils.h"				// My_p2c, SetTypeAndCreator
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/CocoaPlatformUtils.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopresources.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"

extern OSType	gFileKinds[];					// 4 characters
extern OpString g_pref_folder_suffix;

// --------------------------------------------------------------------------------------------------------------
//	ConvertFSRefToURL
// --------------------------------------------------------------------------------------------------------------
// Create a readable URL string from an FSRef

Boolean OpFileUtils::ConvertFSRefToURL(const FSRef *fsref, uni_char *url, size_t buflen)
{
	CFURLRef cfurl = NULL;
	CFStringRef urlStr = NULL;
	Boolean result = false;

	if(fsref)
	{
		cfurl = CFURLCreateFromFSRef(NULL, fsref);
		if(cfurl)
		{
			urlStr = CFURLGetString(cfurl);

			if(urlStr)
			{
				CFRetain(urlStr);

				CFIndex stringLength = CFStringGetLength(urlStr);

				if ((unsigned long)stringLength >= buflen)
					stringLength = buflen - 1;

				CFStringGetCharacters(urlStr, CFRangeMake(0, stringLength), (UniChar*)url);

				CFRelease(urlStr);

				url[stringLength] = '\0';

				result = true;
			}

			CFRelease(cfurl);
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////

Boolean OpFileUtils::GetFriendlyAppName(const uni_char *full_app_name, OpString &nice_app_name)
{
	uni_char *p;

	// Reverse find a "/" and then copy everything after that into the opstring
	// If no '/' is found then just copy over the whole app name
	if ((p = uni_strrchr(full_app_name, '/')) != 0)
		nice_app_name.Set(p + 1);
	else
		nice_app_name.Set(full_app_name);

	// Now get rid of an ".app" ending
	if (uni_stricmp((nice_app_name.CStr() + (nice_app_name.Length() - 4)), UNI_L(".app")) == 0)
		nice_app_name.Delete((nice_app_name.Length() - 4));

	// Remove any leading or trailing spaces
	nice_app_name.Strip();

	return true;
}


/**
* Converts an FSRef to a file system path.
* @param fsref The FSRef to convert
* @param outPath Path will be returned here
* @return OP_STATUS If the FSRef was valid and outPath was properly allocated then OpStatus::OK is returned.
*/
OP_STATUS OpFileUtils::ConvertFSRefToUniPath(const FSRef *fsref, OpString *outPath)
{
	OP_STATUS status = OpStatus::ERR;

	if(!outPath)
		return OpStatus::ERR;
	if(!fsref) {
		return outPath->Set("");
	}

	UInt8 path8[MAX_PATH+1];
	OSStatus ret = FSRefMakePath(fsref, path8, MAX_PATH+1);
	if(ret == noErr)
	{
		outPath->SetFromUTF8((const char *)path8);
		status =  OpStatus::OK;
	}
	return status;
}

Boolean OpFileUtils::ConvertUniPathToFileSpec(const uni_char *pathname, FSSpec &fss)
{
	FSRef fsref;

	if(!ConvertUniPathToFSRef(pathname, fsref))
	{
		return false;
	}

	OSErr err = FSGetCatalogInfo(&fsref, kFSCatInfoNone, NULL, NULL, &fss, NULL);

	return (err == noErr);
}

Boolean OpFileUtils::ConvertUniPathToFSRef(const uni_char *pathname, FSRef &fsref)
{
	if (pathname == NULL)
		return false;

	OSStatus 	err = -1;
	char *		utf8str;
	OpString	path;

	if(OpStatus::IsSuccess(path.Set(pathname)))
	{
		if(OpStatus::IsSuccess(path.UTF8Alloc(&utf8str)))
		{
			err = FSPathMakeRef((UInt8*)utf8str, &fsref, NULL);

			delete[] utf8str;
	  	}
	}

  	return (noErr == err);
}

Boolean OpFileUtils::ConvertURLToUniPath(const uni_char *url, OpString *path)
{
	CFURLRef urlRef;
	FSRef fsRef;
	Boolean result = false;

	urlRef = CFURLCreateWithBytes(NULL, (const UInt8*)url, 2*uni_strlen(url), kCFStringEncodingUnicode, NULL);

	if(urlRef)
	{
		if(CFURLGetFSRef(urlRef,&fsRef))
		{
			if(OpStatus::IsSuccess(ConvertFSRefToUniPath(&fsRef, path)))
			{
				result = true;
			}
		}

		CFRelease(urlRef);
	}

	return result;
}

// --------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------

OpStringC OpFileUtils::GetPathRelativeToBundleResource(const OpStringC &path)
{
	OpStringC result;
	OpString tmp_storage;
	OpStringC resourcePath = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_RESOURCES_FOLDER, tmp_storage);
	uni_char *p = uni_strstr(path.CStr(), resourcePath.CStr());
	if(p == path.CStr())
	{
		result = path.CStr() + resourcePath.Length();
	}
	else
	{
		result = path;
	}
	return result;
}

Boolean	OpFileUtils::ConvertUniPathToVolumeAndId(const uni_char *pathname, short &volume, long &id)
{
	FSRef fsref;

	if(ConvertUniPathToFSRef(pathname,fsref))
	{
		FSCatalogInfo catInfo;

		OSErr err = FSGetCatalogInfo(&fsref, kFSCatInfoVolume | kFSCatInfoNodeID, &catInfo, NULL, NULL, NULL);
		if(err == noErr)
		{
			volume = catInfo.volume;
			id = catInfo.nodeID;
			return true;
		}
	}

	return false;
}

Boolean OpFileUtils::FindFolder(OSType osSpecificSpecialFolder, OpString &path, Boolean createFolder, short vRefNum)
{
	OSErr	err;
	short	volume;
	FSRef	fsref;
	OSType  osSpecificSpecialFolder_original = osSpecificSpecialFolder;

    if (vRefNum == -1) // -1 is default, not used value
    { 	
		// Check if we have got path in hash table.
		OpString *s;
		if(sPaths.GetData(osSpecificSpecialFolder, &s) == OpStatus::OK)
		{
			path.Set(*s);
			return true;
		}

		// Set the volume.
		switch (osSpecificSpecialFolder)
		{
			case kInternetPlugInFolderType:
				volume = kLocalDomain;
				break;

			case kTemporaryFolderType:
				volume = kOnAppropriateDisk;
				break;

			case kApplicationsFolderType:	
				volume = kOnSystemDisk;
				break;

			default:
				volume = kUserDomain;
				break;
		}
	}
	else
	{
		volume = vRefNum;
	}
	
	if (osSpecificSpecialFolder == kSystemPreferencesFolderType)
	{
		osSpecificSpecialFolder = kPreferencesFolderType;
		volume = kLocalDomain;
		createFolder = false;
	}

	err = ::FSFindFolder(volume, osSpecificSpecialFolder, createFolder ? kCreateFolder : kDontCreateFolder, &fsref);

	if (err != noErr && err != fnfErr)
	{
		return false;
	}

	if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref,&path)))
	{
		if((path.Length() > 0) && (path[path.Length() - 1] != PATHSEPCHAR))
		{
			path.Append(PATHSEP);
		}

		// Add path to hash table
		if (vRefNum == -1) {
			OpString *s = new OpString();
			s->Set(path);
			sPaths.Add(osSpecificSpecialFolder_original, s);
		}
		return true;
	}

	return false;
}

void OpFileUtils::AppendFolder(OpString &path, const OpStringC& customFolder, const uni_char* folder_suffix)
{
	if((path.Length() > 0) && (path[path.Length() - 1] != PATHSEPCHAR))
	{
		path.Append(PATHSEP);
	}

	path.Append(customFolder);

	// Append the suffix if it exists
	// This is for both "guest" mode and the multi prefs options
	if (folder_suffix && uni_strlen(folder_suffix))
	{
		path.Append(UNI_L(" "));
		path.Append(folder_suffix);
	}

	if((path.Length() > 0) && (path[path.Length() - 1] != PATHSEPCHAR))
	{
		path.Append(PATHSEP);
	}
}

extern long gVendorDataID;
extern OpString gOperaPrefsLocation;
void RandomCharFill(void *dest, int size);
Boolean	OpFileUtils::homeValid = false;
FSRef	OpFileUtils::contentsFSRef;
OpString OpFileUtils::sResourcesPath;
OpAutoINT32HashTable<OpString> OpFileUtils::sPaths;
OpString OpFileUtils::m_profile_name;
OpString OpFileUtils::m_profile_fullpath;
Boolean OpFileUtils::m_single_profle_checked = false;
Boolean OpFileUtils::m_next_checked = false;
CFURLRef OpFileUtils::sOperaLoc = NULL;

void OpFileUtils::InitHomeDir(const char* arg0)
{
	// Opera needs to use the argv stuff to restart correctly
	OpString	path;
	FSRef		bundleFSRef;

	path.SetFromUTF8(arg0);
	// Remove the exec name
	*(path.CStr() + path.FindLastOf(PATHSEPCHAR) + 1) = 0;

	if (OpFileUtils::ConvertUniPathToFSRef(path.CStr(), bundleFSRef))
	{
		FSCatalogInfoBitmap 	whichInfo = kFSCatInfoNone;

		if(noErr == FSGetCatalogInfo(&bundleFSRef, whichInfo, NULL, NULL, NULL, &contentsFSRef))	// Out of "MacOS"
		{
			homeValid = true;
		}
	}
}

Boolean OpFileUtils::SetToMacFolder(MacOpFileFolder mac_folder, OpString &folder, const uni_char *profile_name)
{
	OpString path;
	
	// First we try and use the single profile folder
	if (!m_single_profle_checked)
	{
		OpString profile_fullpath, userpath, apppath;
		ProcessSerialNumber psn;
		FSRef				ref_app;

		// Mark this check as done as to not make calls to this function slow!
		m_single_profle_checked = true;
		
		// Build the path to the automated profile folder

		// Get path to /User path
		OpFileUtils::FindFolder(kCurrentUserFolderType, userpath, false);

		// The PersonalDirectory (-pd) option overrides all other checks
		if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory))
		{
			/*
			 Due to DSK-358498 we cannot set a nonexistent relative path as the location of the operaprefs.ini file, since that will make the
			 PrefsFile to mark the operaprefs.ini location as not writable and in consequence any future attempt to commit (i.e. save) the
			 file will fail.
			 We can use a nonexistent absolute path since the IsWritable() check should pass on the parent folders, which is not the case
			 when we pass a relative path, since the PrefsFile won't know what the parent folders are.
			 The trick therefore is to make sure the folder read from the -pd argument value is absolute, prefixing it with the current dir
			 if necessary.
			*/

			OpString absolute_pd_path;
			if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory)->m_string_value.FindFirstOf(UNI_L("/")) != 0)
			{
				// A relative directory, since no slash at the beginning, go ahead and fetch the current working dir
				char cur_dir[MAX_PATH + 1];
				RETURN_VALUE_IF_NULL(getcwd(cur_dir, MAX_PATH), false);
				// Prefix the path found in the -pd argument value with the current absolute path value
				RETURN_VALUE_IF_ERROR(absolute_pd_path.Set(cur_dir), false);
				RETURN_VALUE_IF_ERROR(absolute_pd_path.AppendFormat("/%s", CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory)->m_string_value.CStr()), false);
			}
			else {
				// The -pd argument value contains what we believe to be an absolute directory, just carry on.
				RETURN_VALUE_IF_ERROR(absolute_pd_path.Set(CommandLineManager::GetInstance()->GetArgument(CommandLineManager::PersonalDirectory)->m_string_value), false);
			}

			RETURN_VALUE_IF_ERROR(m_profile_fullpath.Set(absolute_pd_path), false);
		}
		// Get path to this running copy of Opera
		else if (GetCurrentProcess(&psn) == noErr)
		{
			// Get the path to this process
			if (GetProcessBundleLocation(&psn, &ref_app) == noErr)
			{
				if (OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&ref_app, &apppath)))
				{
					short	user_vol = 0, app_vol = 0;
					long	id;
					
					// Get the volume id's of the user and app paths
					if (OpStatus::IsSuccess(OpFileUtils::ConvertUniPathToVolumeAndId(userpath.CStr(), user_vol, id)) &&
						OpStatus::IsSuccess(OpFileUtils::ConvertUniPathToVolumeAndId(apppath.CStr(), app_vol, id)))
					{
						// Figure out if we are on the same volume as the /Users folder
						if (user_vol == app_vol)
						{
							// Use the home folder as the location of the prefs folder (i.e. /Users/username/
							profile_fullpath.Set(userpath);
						}
						else
						{
							// Root of the Volume
							FSRef fsRef;
							CFURLRef serverLocation;
							OSStatus result = FSCopyURLForVolume(app_vol, &serverLocation);
							OP_ASSERT(result == noErr);
							if (result == noErr)
							{
								if (CFURLGetFSRef(serverLocation, &fsRef))
									ConvertFSRefToUniPath(&fsRef, &profile_fullpath);

								CFRelease(serverLocation);
							}

							// Just in case
							if (profile_fullpath.IsEmpty())
								profile_fullpath.Set(userpath);
						}
						
						// Get the name of the app from the app path
						INT32 dot_pos = apppath.FindI(".app");
						if (dot_pos != KNotFound)
						{
							INT32 slash_pos = apppath.FindLastOf(PATHSEPCHAR);
							if (slash_pos != KNotFound && slash_pos < dot_pos)
							{
								OpString appname;
								appname.Set(apppath.SubString(slash_pos + 1, dot_pos - slash_pos - 1).CStr());
								OpString suffix;
								
								
								// Check if a -ps command line arg was supplied
								if (g_pref_folder_suffix.HasContent())
									suffix.Set(g_pref_folder_suffix.CStr());
								else
								{
									// Check if it starts with Opera
									if (appname.Length() > 5 && appname.FindI("Opera") == 0)
									{
										INT32 suffix_pos = 5;
										
										// Skip any spaces
										while (suffix_pos < appname.Length() && appname[suffix_pos] == ' ')
											suffix_pos++;
										
										// Make a suffix for the folder that matches the suffix on the App name
										if (suffix_pos < appname.Length())
											suffix.Set(appname.SubString(suffix_pos));
									}
								}
								// Append the profile folder with the optional suffix
								if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
									AppendFolder(profile_fullpath, OPERA_SINGLE_PROFILE_AUTOTEST_FOLDER_NAME, suffix.CStr());
								else
									AppendFolder(profile_fullpath, OPERA_SINGLE_PROFILE_FOLDER_NAME, suffix.CStr());

								// If the option to create the single profile is on (or watir test) then we just go and make
								// the folder
								if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::CreateSingleProfile) ||
									CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
								{
									// Create the folder
									if (OpStatus::IsSuccess(OpFileUtils::MakeDir(profile_fullpath)))
										m_profile_fullpath.Set(profile_fullpath);
								}
								else
								{
									// Check if the folder exists and has an operaprefs.ini inside it
									OpString	prefs_file_path;
									OpFile		prefs_file;
									BOOL		exists = FALSE;
									
									prefs_file_path.Set(profile_fullpath);
									
									// Check it has a slash on the end before adding the filename
									if((prefs_file_path.Length() > 0) && (prefs_file_path[prefs_file_path.Length() - 1] != PATHSEPCHAR))
										prefs_file_path.Append(PATHSEP);
									
									prefs_file_path.Append(DESKTOP_RES_OPERA_PREFS);
									
									// If the file exists on disk then just load up the prefs!
									if (OpStatus::IsSuccess(prefs_file.Construct(prefs_file_path.CStr())))
									{
										if (OpStatus::IsSuccess(prefs_file.Exists(exists)) && exists)
											m_profile_fullpath.Set(profile_fullpath);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	// Set this to true if the profile folder is needed
	// This is saved on the first call to this fuction then used forever more for this run of Opera
	if (profile_name && uni_strlen(profile_name) && m_profile_name.IsEmpty())
		m_profile_name.Set(profile_name);
	
	// Send back the full profile if it was set
	if (m_profile_name.IsEmpty() && m_profile_fullpath.HasContent())
	{
		folder.Set(m_profile_fullpath);
		return true;
	}

	if (!m_next_checked)
	{
		// Don't do this more than once
		m_next_checked = true;
		
		// This doesn't override the command line arg
		if (g_pref_folder_suffix.IsEmpty())
		{
			// DSK-347480: g_desktop_product may not be initialized yet (i.e. return default values),
			// so use GetProductInfo directly (it is used also to initialize g_desktop_product)
			OpDesktopProduct::ProductInfo product_info;
			if (OpStatus::IsSuccess(OpDesktopProduct::GetProductInfo(NULL, NULL, product_info)) &&
				product_info.m_product_type != PRODUCT_TYPE_OPERA)
			{
				// Use the CFBundleName minus "Opera";
				OpStringC product_name = GetCFBundleName();
				
				// Make sure it starts with "Opera" and then it has a space
				if (product_name.FindFirstOf(UNI_L("Opera ")) == 0)
				{
					// Get the Labs name and append it too
					OpStringC labs_name = product_info.m_labs_product_name;

					// Skip over "Opera "
					g_pref_folder_suffix.Set(product_name.CStr() + 6);
					if (labs_name.HasContent())
					{
						// Add the labs name
						g_pref_folder_suffix.Append(" ");
						g_pref_folder_suffix.Append(labs_name);
					}
				}
			}
		}
	}
	
	switch (mac_folder)
	{
		case MAC_OPFOLDER_APPLICATION:
				FindFolder(kApplicationSupportFolderType, path);

				// Add the Application Support folder name + a suffix if -pd was set
				if (m_profile_name.HasContent())
					AppendFolder(path, profile_name, g_pref_folder_suffix.CStr());
				else
					AppendFolder(path, OPERA_APP_SUPPORT_FOLDER_NAME, g_pref_folder_suffix.CStr());
			break;

		case MAC_OPFOLDER_CACHES:
				FindFolder(kCachedDataFolderType, path);

				// Add the Cache folder name + a suffix if -pd was set
				if (m_profile_name.HasContent())
					AppendFolder(path, profile_name, g_pref_folder_suffix.CStr());
				else
					AppendFolder(path, OPERA_CACHE_FOLDER_NAME, g_pref_folder_suffix.CStr());
			break;

		case MAC_OPFOLDER_PREFERENCES:
				// This writeLocation is used by embedded versions of Opera (i.e. Adobe) to put the preferences
				// for Opera where they want to. This should only be used from embedded products
				// Embedded products also don't support guest mode
				if (gOperaPrefsLocation.HasContent())
				{
					AppendFolder(path, gOperaPrefsLocation);
				}
				else
				{
					FindFolder(kDomainLibraryFolderType, path);

					// Add the Preferences folder name + a suffix if -pd was set
					if (m_profile_name.HasContent())
						AppendFolder(path, profile_name, g_pref_folder_suffix.CStr());
					else
						AppendFolder(path, OPERA_PREFS_FOLDER_NAME, g_pref_folder_suffix.CStr());
				}
			break;
		case MAC_OPFOLDER_LIBRARY:
			FindFolder(kDomainLibraryFolderType, path);
			break;
	}

	if(path.Length() > 0)
	{
		folder.Set(path);
		return true;
	}

	return false;
}

#ifndef USE_COMMON_RESOURCES

Boolean OpFileUtils::SetToSpecialFolder(OpFileFolder specialFolder, OpString &folder)
{
	FSRef	fsref;
	OpString path;

	switch(specialFolder)
	{
	  //case kDefaultsFolder:
	  case OPFILE_SECURE_FOLDER:		// This was added for symbian and we just user the user prefs folder we have nothing specific for this
	  case OPFILE_USERPREFS_FOLDER:
	  case OPFILE_HOME_FOLDER:
#ifdef UTIL_CAP_LOCAL_HOME_FOLDER
	  case OPFILE_LOCAL_HOME_FOLDER:
#endif // UTIL_CAP_LOCAL_HOME_FOLDER
		SetToMacFolder(MAC_OPFOLDER_PREFERENCES, path);
		break;

	  case OPFILE_GLOBALPREFS_FOLDER:
		if(FindFolder(kSystemPreferencesFolderType, path))
		{
			AppendFolder(path, OPERA_PREFS_FOLDER_NAME);
		}
		break;

	  case OPFILE_FIXEDPREFS_FOLDER:
		if(FindFolder(kSystemPreferencesFolderType, path))
		{
			AppendFolder(path, OPERA_PREFS_FOLDER_NAME);
			AppendFolder(path, UNI_L("Fixed"));					// inside that one we place the "Fixed" folder
		}
		break;

	  case OPFILE_TEMP_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_CACHES, path);
		AppendFolder(path, UNI_L("Temp"));
		break;

	  case OPFILE_THUMBNAIL_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_APPLICATION, path);
		AppendFolder(path, UNI_L("Thumbnails"));
		break;

	  case OPFILE_DESKTOP_FOLDER:
	  case OPFILE_UPLOAD_FOLDER:
		FindFolder(kDesktopFolderType, path);
		break;

	  case OPFILE_INI_FOLDER:
		SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path);
  		AppendFolder(path, UNI_L("defaults"));
	  break;
	  case OPFILE_RESOURCES_FOLDER:
//	  case kOperaBundleResourcesFolder:
	  	fsref = contentsFSRef;
		if(!sResourcesPath.HasContent())
		{
			if(OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &sResourcesPath)))
			{
				AppendFolder(sResourcesPath, UNI_L("Resources"));
			}
	  	}
		path.Set(sResourcesPath);
	  	break;
	  case OPFILE_LANGUAGE_FOLDER:
//	  case kOperaBundleLanguageFolder:
	  	{
	  		OpString langCode;
		  	if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		  	{
#if defined(WIDGET_COMMON_PREFS_INTERFACE)
				uni_char buf[16];
				buf[0] = 0;
				GET_PREF(kWidgetPrefOperaLanguage, buf);
				RETURN_VALUE_IF_ERROR(langCode.Set(buf), false);
#else
//				if (g_languageManager)
//			  		langCode.Set(g_languageManager->GetLanguage());
//				else
				RETURN_VALUE_IF_ERROR(langCode.Set(GetDefaultLanguage()), false);
#endif
				RETURN_VALUE_IF_ERROR(langCode.Append(UNI_L(".lproj")), false);
				AppendFolder(path, langCode.CStr());
			}
		}
		break;
#ifdef _FILE_UPLOAD_SUPPORT_
	  case OPFILE_OPEN_FOLDER:
	  case OPFILE_SAVE_FOLDER:
		FindFolder(kDesktopFolderType, path);
		break;											// Normally you should use prefsManager->GetDownloadFolderLocation.
#endif // _FILE_UPLOAD_SUPPORT_

	  case OPFILE_DOWNLOAD_FOLDER:								// This is the system's download folder
		{
			// First try to get Safari's DL location:
			CFPropertyListRef safari_dl = CFPreferencesCopyAppValue(CFSTR("DownloadsPath"), CFSTR("com.apple.Safari"));
			if (safari_dl)
			{
				if (CFGetTypeID(safari_dl) == CFStringGetTypeID())
				{
					CFStringRef safari_download = (CFStringRef) safari_dl;
					CFIndex length = CFStringGetLength(safari_download);
					UniChar first;
					CFRange range = {0, 1};
					if (length)
					{
						CFStringGetCharacters(safari_download, range, &first);
						if (first == '~')
						{
							OpFileUtils::FindFolder(kCurrentUserFolderType, path);
							path.Reserve(length + path.Length());				// FIXME:OOM
							uni_char* append = path.CStr() + path.Length();
							if (*(append-1) == '/')
							{
								append--;
							}
							range.location = 1;
							range.length = length-1;
							CFStringGetCharacters(safari_download, range, (UniChar*)append);
							append[length-1] = 0;
						}
						else
						{
							SetOpString(path, safari_download);
						}
						CFRelease(safari_download);
						break;
					}
				}
				CFRelease(safari_dl);
			}
#ifdef USE_IC_FOR_DOWNLOAD_FOLDER
			// Since that failed, ask internet config
			ICInstance ic = NULL;
			OSStatus err = ICStart(&ic, 'OPRA');
			if (err == noErr)
			{
				unsigned char buf[sizeof(ICFileSpec)+1024];	// also need room for alias record
				ICFileSpec* spec = (ICFileSpec*)buf;
				ICAttr attr = 0;
				long size = sizeof(buf);
				err = ICGetPref(ic, kICDownloadFolder, &attr, spec, &size);
				if (err == noErr)
				{
					FSRef fsref;
					FSpMakeFSRef(&spec->fss, &fsref);
					OpFileUtils::ConvertFSRefToUniPath(&fsref, &path);
					ICStop(ic);
					break;
				}
				ICStop(ic);
			}
#endif // USE_IC_FOR_DOWNLOAD_FOLDER
			// fallback to desktop folder
			FindFolder(kDesktopFolderType, path);
		}
		break;											// Normally you should use prefsManager->GetDownloadFolderLocation.

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	  case OPFILE_OCACHE_FOLDER:
		  	GetOperaCacheFolderName(path, TRUE);
		break;
#endif
	  case OPFILE_CACHE_FOLDER:
		  	GetOperaCacheFolderName(path, FALSE);
		break;

#ifdef UTIL_VPS_FOLDER
	  case OPFILE_VPS_FOLDER:
		  	GetOperaCacheFolderName(path, FALSE);
			AppendFolder(path, UNI_L("Visited Pages Index"));
		break;
#endif // UTIL_VPS_FOLDER

	  case OPFILE_SESSION_FOLDER:
		if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Sessions"));
		}
		break;

#ifdef M2_SUPPORT
	  case OPFILE_MAIL_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_APPLICATION, path);
		AppendFolder(path, UNI_L("Mail"));
		break;
#endif // M2_SUPPORT

#ifdef JS_PLUGIN_SUPPORT
	  case OPFILE_JSPLUGIN_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_APPLICATION, path);
		AppendFolder(path, UNI_L("JSPlugins"));
		break;
#endif // JS_PLUGIN_SUPPORT

	  case OPFILE_STYLE_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Styles"));
		}
		break;

	  case OPFILE_USERSTYLE_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Styles"));
			AppendFolder(path, UNI_L("user"));
		}
		break;

	  case OPFILE_USERPREFSSTYLE_FOLDER:
		if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Styles"));
			AppendFolder(path, UNI_L("user"));
		}
		break;

	  case OPFILE_IMAGE_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Images"));
		}
		break;

	  case OPFILE_BUTTON_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Buttons"));
		}
		break;

	  case OPFILE_SKIN_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Skin"));
		}
	  	break;

	  case OPFILE_DEFAULT_SKIN_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Skin"));
		}
	  	break;

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	  case OPFILE_DICTIONARY_FOLDER:
		if (SetToMacFolder(MAC_OPFOLDER_APPLICATION, path))
	  	{
	  		AppendFolder(path, UNI_L("Dictionaries"));
		}
	  	break;
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef WEBSERVER_SUPPORT
	  case OPFILE_WEBSERVER_FOLDER:
		if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Webserver"));
		}
	  	break;
#ifdef GADGET_SUPPORT
	  case OPFILE_UNITE_PACKAGE_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("unite"));
		}
		break;
#endif // GADGET_SUPPORT
#endif // WEBSERVER_SUPPORT

	  case OPFILE_MOUSESETUP_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Mouse"));
		}
	  	break;

	  case OPFILE_KEYBOARDSETUP_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Keyboard"));
		}
	  	break;

	  case OPFILE_TOOLBARSETUP_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Toolbars"));
		}
	  	break;

	  case OPFILE_MENUSETUP_FOLDER:
	  	if(SetToSpecialFolder(OPFILE_USERPREFS_FOLDER, path))
	  	{
	  		AppendFolder(path, UNI_L("Menu"));
		}
	  	break;

	  case OPFILE_ICON_FOLDER:
	  	GetOperaCacheFolderName(path, FALSE, TRUE);
		AppendFolder(path, UNI_L("Icons"));
		break;

#ifdef GADGET_SUPPORT
	  case OPFILE_GADGET_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_APPLICATION, path);
		AppendFolder(path, UNI_L("Widgets"));
		break;

	  case OPFILE_GADGET_PACKAGE_FOLDER:
		if(SetToSpecialFolder(OPFILE_RESOURCES_FOLDER, path))
		{
			AppendFolder(path, UNI_L("Widgets"));
		}
		break;
#endif

#ifdef _BITTORRENT_SUPPORT_
	  case OPFILE_BITTORRENT_METADATA_FOLDER:
		SetToMacFolder(MAC_OPFOLDER_APPLICATION, path);
		AppendFolder(path, UNI_L("BT Metadata"));
	  break;
#endif // _BITTORRENT_SUPPORT_

	  case OPFILE_TEMPDOWNLOAD_FOLDER:
	  	GetOperaCacheFolderName(path, FALSE, TRUE);
		AppendFolder(path, UNI_L("Temporary Downloads"));
		break;

#ifdef SELFTEST
	  case OPFILE_SELFTEST_DATA_FOLDER:
	  {
		CommandLineArgument *cl_arg = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::SourceRootDir);

		// This should be set by a command line argument, if not we will assume this is being compiled and
		// and use the __DIRECTORY__ to get the root based on the location of this file
		if (cl_arg)
			path.Set(cl_arg->m_string_value.CStr());
		else
		{
			// By doing this everything will work in debug without the need or a commandline arg

			// Set to the director that holds this folder
			path.Set(__FILE__);

			// Move back 4 folders to find the root source folder
			for (int i = 0; i < 4; i++)
			{
				int sep_pos = path.FindLastOf(PATHSEPCHAR);
				if (sep_pos != KNotFound)
					path.Set(path.SubString(0, sep_pos));
				else
					break;
			}
		}

	  }
	  break;
#endif // SELFTEST
	}

	if(path.Length() > 0)
	{
		folder.Set(path);
		return true;
	}

	return false;
}

#endif // !USE_COMMON_RESOURCES


OP_STATUS OpFileUtils::MakeDir(const uni_char* path)
{
	char *dirName = StrToLocaleEncoding(path);

	int ret = mkdir(dirName, 0777);

	delete [] dirName;

	if(ret == 0 || errno == EEXIST)
	{
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS OpFileUtils::MakePath(uni_char* path, uni_char* path_parent)
{
	OpString full_path;

	if (path_parent && uni_strlen(path_parent))
	{
		full_path.Set(path_parent);

		// Add a slash if required
		if (path_parent[uni_strlen(path_parent) - 1] != UNI_L(PATHSEPCHAR))
			full_path.Append(UNI_L(PATHSEP));

		// Append the path now
		full_path.Append(path);
	}
	else
	{
		full_path.Set(path);
	}

	int len = full_path.Length();
	for (int i=0; i < len; ++i)
	{
		if (full_path[i] == '/')
		{
			full_path[i] = 0;
			OP_STATUS res = MakeDir(full_path.CStr());
			full_path[i] = PATHSEPCHAR;
			if (OpStatus::IsMemoryError(res)) return res;
		}
	}

	return len ? MakeDir(full_path.CStr()) : OpStatus::OK;
}

OP_STATUS OpFileUtils::ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len, const OpStringC &resource_dir, const OpStringC &largepref_dir, const OpStringC &smallpref_dir)
{
	if (!in || !out)
		return OpStatus::OK;

	OpString userpath;

	// Set the user path
	OpFileUtils::FindFolder(kCurrentUserFolderType, userpath, false);

	if (uni_strncmp(in, UNI_L("{Resources}"), 11) == 0)
	{
		if (resource_dir.CStr() && out_max_len > resource_dir.Length())
		{
			uni_strcpy(out, resource_dir.CStr());
			out += resource_dir.Length();
			out_max_len -= resource_dir.Length();
			in += 11;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{LargePreferences}"), 18) == 0)
	{
		if (largepref_dir.CStr() && out_max_len > largepref_dir.Length())
		{
			uni_strcpy(out, largepref_dir.CStr());
			out += largepref_dir.Length();
			out_max_len -= largepref_dir.Length();
			in += 18;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{LargePrefs}"), 12) == 0)
	{
		if (largepref_dir.CStr() && out_max_len > largepref_dir.Length())
		{
			uni_strcpy(out, largepref_dir.CStr());
			out += largepref_dir.Length();
			out_max_len -= largepref_dir.Length();
			in += 12;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{SmallPreferences}"), 18) == 0)
	{
		if (smallpref_dir.CStr() && out_max_len > smallpref_dir.Length())
		{
			uni_strcpy(out, smallpref_dir.CStr());
			out += smallpref_dir.Length();
			out_max_len -= smallpref_dir.Length();
			in += 18;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{SmallPrefs}"), 12) == 0)
	{
		if (smallpref_dir.CStr() && out_max_len > smallpref_dir.Length())
		{
			uni_strcpy(out, smallpref_dir.CStr());
			out += smallpref_dir.Length();
			out_max_len -= smallpref_dir.Length();
			in += 12;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{Preferences}"), 13) == 0)
	{
		if (smallpref_dir.CStr() && out_max_len > smallpref_dir.Length())
		{
			uni_strcpy(out, smallpref_dir.CStr());
			out += smallpref_dir.Length();
			out_max_len -= smallpref_dir.Length();
			in += 13;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{Home}"), 6) == 0)
	{
		if (userpath.CStr() && out_max_len > userpath.Length())
		{
			uni_strcpy(out, userpath.CStr());
			out += userpath.Length();
			out_max_len -= userpath.Length();
			in += 6;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{Users}"), 7) == 0)
	{
		if (userpath.CStr() && out_max_len > userpath.Length())
		{
			uni_strcpy(out, userpath.CStr());
			out += userpath.Length();
			out_max_len -= userpath.Length();
			in += 7;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	else if (uni_strncmp(in, UNI_L("{Binaries}"), 10) == 0)
	{
		// This thing only makes sense on UNIX so I'm mapping it to the Resources folder // adamm
		if (resource_dir.CStr() && out_max_len > resource_dir.Length())
		{
			uni_strcpy(out, resource_dir.CStr());
			out += resource_dir.Length();
			out_max_len -= resource_dir.Length();
			in += 10;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	// Could theoretically just remove this entire block, if it isn't supposed to work
	else if (uni_strncmp(in, UNI_L("{Root}"), 6) == 0)
	{
		if (out_max_len > 1)
		{
			*out++ = L'/';
			*out = L'\0';
			out_max_len--;
			in += 6;
		}
		else
		{
			return OpStatus::ERR;
		}
	}
	
	uni_strlcpy(out, in, out_max_len);

	return OpStatus::OK;
}

OP_STATUS OpFileUtils::SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len, const OpStringC &resource_dir, const OpStringC &largepref_dir, const OpStringC &smallpref_dir)
{
	OpString userpath;
	OpFileUtils::FindFolder(kCurrentUserFolderType, userpath, false);
	size_t in_len = uni_strlen(in), resource_dir_len = resource_dir.Length(), largepref_dir_len = largepref_dir.Length(), smallpref_dir_len = smallpref_dir.Length(), userpath_len = userpath.Length();
	if ((in_len + 1) == resource_dir_len && resource_dir.CStr()[resource_dir_len - 1] == '/')
		resource_dir_len--;
	if ((in_len + 1) == largepref_dir_len && largepref_dir.CStr()[largepref_dir_len - 1] == '/')
		largepref_dir_len--;
	if ((in_len + 1) == smallpref_dir_len && smallpref_dir.CStr()[smallpref_dir_len - 1] == '/')
		smallpref_dir_len--;
	if ((in_len + 1) == userpath_len && userpath.CStr()[userpath_len - 1] == '/')
		userpath_len--;

	// If this file is in the resource path change it to a relative path
	if (resource_dir.CStr() && uni_strncmp(in, resource_dir.CStr(), resource_dir_len) == 0)
	{
		uni_strlcpy(out, UNI_L("{Resources}"), out_max_len);
		uni_strlcpy(out+11, in+resource_dir_len, out_max_len-11);
	}
	else if (largepref_dir.CStr() && uni_strncmp(in, largepref_dir.CStr(), largepref_dir_len) == 0)
	{
		uni_strlcpy(out, UNI_L("{LargePreferences}"), out_max_len);
		uni_strlcpy(out+18, in+largepref_dir_len, out_max_len-18);
	}
	else if (smallpref_dir.CStr() && uni_strncmp(in, smallpref_dir.CStr(), smallpref_dir_len) == 0)
	{
		uni_strlcpy(out, UNI_L("{SmallPreferences}"), out_max_len);
		uni_strlcpy(out+18, in+smallpref_dir_len, out_max_len-18);
	}
	else if (userpath.CStr() && uni_strncmp(in, userpath.CStr(), userpath_len) == 0)
	{
		uni_strlcpy(out, UNI_L("{Home}"), out_max_len);
		uni_strlcpy(out+6, in+userpath_len, out_max_len-6);
	}
	else
		uni_strlcpy(out, in, out_max_len);

	return OpStatus::OK;
}

OP_STATUS OpFileUtils::HasAdminAccess(BOOL &has_admin_access)
{
	return CocoaPlatformUtils::HasAdminAccess(has_admin_access);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Hack for DSK-334964 ///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

static OpString oldloc, newloc;

void OpFileUtils::UpgradePreferencesFolder()
{
	BOOL exists;
	OpFile oldfolder, newfolder;

	FindFolder(kPreferencesFolderType, oldloc);
	if (m_profile_name.HasContent())
		AppendFolder(oldloc, m_profile_name, g_pref_folder_suffix.CStr());
	else
#ifdef _MAC_DEBUG
		AppendFolder(oldloc, UNI_L("Opera Debug Preferences"), g_pref_folder_suffix.CStr());
#else
		AppendFolder(oldloc, UNI_L("Opera Preferences"), g_pref_folder_suffix.CStr());
#endif
	oldfolder.Construct(oldloc, OPFILE_ABSOLUTE_FOLDER);
	if (OpStatus::IsSuccess(oldfolder.Exists(exists)) && exists) {
		SetToMacFolder(MAC_OPFOLDER_PREFERENCES, newloc);
		newfolder.Construct(newloc, OPFILE_ABSOLUTE_FOLDER);
		if (OpStatus::IsSuccess(newfolder.Exists(exists)) && !exists) {
			DesktopOpFileUtils::Move(&oldfolder, &newfolder);
		}
	}
}

void OpFileUtils::LinkOldPreferencesFolder()
{
	if (oldloc.HasContent() && newloc.HasContent())
	{
		OpString8 old_loc8, new_loc8;
		old_loc8.SetUTF8FromUTF16(oldloc, oldloc.Length()-1);
		new_loc8.SetUTF8FromUTF16(newloc);
		symlink(new_loc8.CStr(), old_loc8.CStr());
	}
}
