/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MACGOGI
#include "platforms/macgogi/pi/MacOpSystemInfo.h"
#include "platforms/macgogi/pi/ui/MacOpUiInfo.h"
#include "platforms/macgogi/util/utils.h"
#include "platforms/macgogi/util/OpFileUtils.h"
#else // MACGOGI
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/pi/ui/MacOpUiInfo.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/MacIcons.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/util/macutils_mincore.h"
#include "platforms/mac/bundledefines.h"
#endif

#include "modules/util/opfile/opfile.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include <SystemConfiguration/SystemConfiguration.h>

#ifdef PI_CAP_SYSTEM_IP
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>

extern ThreadID gMainThreadID;
extern BOOL IsSandboxed();

OP_STATUS OpSystemInfo::Create(OpSystemInfo **newObj)
{
	MacOpSystemInfo* sysinfo = new MacOpSystemInfo();
	OP_STATUS err = OpStatus::ERR_NO_MEMORY;
	if (sysinfo) {
		err = sysinfo->Construct();
		if (OpStatus::IsError(err)) {
			delete sysinfo;
		} else {
			*newObj = sysinfo;
		}
	}
	return err;
}

MacOpSystemInfo::MacOpSystemInfo()
	:m_use_event_shiftkey_state(FALSE)
	,m_screen_reader_running(FALSE)
{
}

#ifndef POSIX_OK_CLOCK
void MacOpSystemInfo::GetWallClock(unsigned long& seconds, unsigned long& milliseconds)
{
	UnsignedWide ticks;
	Microseconds(&ticks);
	unsigned long long microsecs = ticks.hi;
	microsecs <<= 32;
	microsecs |= ticks.lo;
	seconds = microsecs / 1000000;
	milliseconds = microsecs % 1000000;
	milliseconds /= 1000;
}

double MacOpSystemInfo::GetWallClockResolution()
{
	return 0.001;
}

double MacOpSystemInfo::GetTimeUTC()
{
	struct timeval tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);

    return ((double) tp.tv_sec) * 1000.0 + ((double) tp.tv_usec) / 1000.0;
}

unsigned int MacOpSystemInfo::GetRuntimeTickMS()
{
	unsigned long long usecs;
	Microseconds((UnsignedWide*)&usecs);
	return (unsigned int)((usecs / (unsigned long long)1000) % UINT_MAX);
}

double MacOpSystemInfo::GetRuntimeMS()
{
	return GetMonotonicMS();
}
#endif // POSIX_OK_CLOCK

#ifndef POSIX_OK_TIME_ZONE
long MacOpSystemInfo::GetTimezone()
{
	MachineLocation theLocation;
	int internalGMTDelta;

	ReadLocation(&theLocation);								// Find out where user lives (includes position on globe).

	internalGMTDelta = (int)theLocation.u.gmtDelta & 0x00ffffff;	// gmtDelta is only 24 bits
	if ((internalGMTDelta >> 23) & 1)						// sign bit for 24 bit value.
		internalGMTDelta = internalGMTDelta | 0xff000000;	// convert to 32 bit signed

	internalGMTDelta = -internalGMTDelta;

	return internalGMTDelta;
}

double MacOpSystemInfo::DaylightSavingsTimeAdjustmentMS(double t)
{
	time_t sec_time = (time_t)(t/1000);
	struct tm* t1 = localtime(&sec_time);

	return (t1 && t1->tm_isdst) ? 3600000.0 : 0.0;
}
#endif // POSIX_OK_TIME_ZONE

UINT32 MacOpSystemInfo::GetPhysicalMemorySizeMB()
{
	SInt32 size = 0;

	if(noErr == Gestalt(gestaltPhysicalRAMSize, &size))
	{
		size /= (1024 * 1024);
		return size;
	}

	return 128;
}

void MacOpSystemInfo::GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver)
{
	CFStringRef use_proxy_key = NULL, proxy_server_key = NULL, proxy_port_key = NULL;
	enabled = false;
	proxyserver.Set("");

	if (uni_str_eq(protocol, UNI_L("http"))) {
		use_proxy_key = kSCPropNetProxiesHTTPEnable;
		proxy_server_key = kSCPropNetProxiesHTTPProxy;
		proxy_port_key = kSCPropNetProxiesHTTPPort;
	} else if (uni_str_eq(protocol, UNI_L("https"))) {
		use_proxy_key = CFSTR("HTTPSEnable")/*kSCPropNetProxiesHTTPSEnable*/;
		proxy_server_key = CFSTR("HTTPSProxy")/*kSCPropNetProxiesHTTPSProxy*/;
		proxy_port_key = CFSTR("HTTPSPort")/*kSCPropNetProxiesHTTPSPort*/;
	} else if (uni_str_eq(protocol, UNI_L("ftp"))) {
		use_proxy_key = kSCPropNetProxiesFTPEnable;
		proxy_server_key = kSCPropNetProxiesFTPProxy;
		proxy_port_key = kSCPropNetProxiesFTPPort;
	} else if (uni_str_eq(protocol, UNI_L("gopher"))) {
		use_proxy_key = kSCPropNetProxiesGopherEnable;
		proxy_server_key = kSCPropNetProxiesGopherProxy;
		proxy_port_key = kSCPropNetProxiesGopherPort;
	} else if (uni_str_eq(protocol, UNI_L("wais"))) {
		// no wais support
	} else {
		// don't know that protocal
	}
	if (use_proxy_key && proxy_server_key)
	{
		CFDictionaryRef cfDictionary = SCDynamicStoreCopyProxies(NULL);
		if(!cfDictionary)
			return;

		CFNumberRef cfNumber;
		int numBuf;

		if (!CFDictionaryGetValueIfPresent (cfDictionary, use_proxy_key, (const void **)&cfNumber))
		{
			CFRelease(cfDictionary);
			return;
		}

		if (!CFNumberGetValue(cfNumber, kCFNumberIntType, &numBuf))
		{
			CFRelease(cfDictionary);
			return;
		}

		CFStringRef cfString;
		if (!CFDictionaryGetValueIfPresent(cfDictionary, proxy_server_key, (const void **)&cfString))
		{
			CFRelease(cfDictionary);
			return;
		}

		enabled = (numBuf != 0);
		int length = CFStringGetLength(cfString);
		proxyserver.Reserve(length+1);
		UniChar* proxyserver_str = (UniChar*)proxyserver.Reserve(length+1);

		CFStringGetCharacters(cfString, CFRangeMake(0, length), proxyserver_str);
		proxyserver_str[length] = 0;

		if (proxy_port_key && CFDictionaryGetValueIfPresent(cfDictionary, proxy_port_key, (const void **)&cfNumber))
		{
        	if (CFNumberGetValue (cfNumber, kCFNumberIntType, &numBuf))
        	{
        		uni_char portNumber[10];
        		uni_snprintf(portNumber, 10, UNI_L(":%u"), numBuf);
        		proxyserver.Append(portNumber);
            }
		}

		CFRelease(cfDictionary);
	}
}

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
void MacOpSystemInfo::GetAutoProxyURLL(OpString *url, BOOL *enabled)
{
	*enabled = FALSE;
	CFDictionaryRef cfDictionary = SCDynamicStoreCopyProxies(NULL);
	if(!cfDictionary)
		return;

	CFNumberRef cfNumber;
	int numBuf;

	if (!CFDictionaryGetValueIfPresent (cfDictionary, kSCPropNetProxiesProxyAutoConfigEnable, (const void **)&cfNumber))
	{
		CFRelease(cfDictionary);
		return;
	}

	if (!CFNumberGetValue(cfNumber, kCFNumberIntType, &numBuf))
	{
		CFRelease(cfDictionary);
		return;
	}

	CFStringRef cfString;
	if (!CFDictionaryGetValueIfPresent(cfDictionary, kSCPropNetProxiesProxyAutoConfigURLString, (const void **)&cfString))
	{
		CFRelease(cfDictionary);
		return;
	}
	if (SetOpString(*url, cfString))
	{
		*enabled = numBuf;
	}
	CFRelease(cfDictionary);
}
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

void MacOpSystemInfo::GetProxyExceptionsL(OpString *exceptions, BOOL *enabled)
{
}

#ifdef _PLUGIN_SUPPORT_
void MacOpSystemInfo::GetPluginPathL(const OpStringC &dfpath, OpString &newpath)
{
	FSRef plugin_folder;
	// Search the system internet plugin folder
	if (noErr == ::FSFindFolder(kLocalDomain, kInternetPlugInFolderType, kCreateFolder, &plugin_folder))
	{
		OpString path;
		if (OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&plugin_folder, &path)))
		{
			newpath.Set(path);
		}
	}
	
	// Search the users internet plugin folder
	if (noErr == ::FSFindFolder(kUserDomain, kInternetPlugInFolderType, kCreateFolder, &plugin_folder))
	{
		OpString path;
		if (OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&plugin_folder, &path)))
		{
			newpath.AppendFormat(":%s", path.CStr());
		}
	}
}

#if defined(PI_PLUGIN_DETECT) && !defined(NS4P_COMPONENT_PLUGINS)
OP_STATUS MacOpSystemInfo::DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener)
{
	OP_STATUS DetectMacPluginViewers(const OpStringC& suggested_plugin_paths, OpPluginDetectionListener* listener); // platform/mac/util/mac_plugins.cpp
	return DetectMacPluginViewers(suggested_plugin_paths, listener);
}
#endif
#endif // _PLUGIN_SUPPORT_

#if 0
#ifdef _APPLET_2_EMBED_
void MacOpSystemInfo::GetDefaultJavaClassPathL(OpString &target)
{
	OpFile jarFile;
#ifdef USE_COMMON_RESOURCES	
	LEAVE_IF_ERROR(jarFile.Construct(UNI_L("opera.jar"), OPFILE_JAVA_FOLDER));
#else	
	LEAVE_IF_ERROR(jarFile.Construct(UNI_L("opera.jar"), OPFILE_RESOURCES_FOLDER));
#endif // USE_COMMON_RESOURCES	
	target.SetL(jarFile.GetFullPath());
}

void MacOpSystemInfo::GetDefaultJavaPolicyFilenameL(OpString &target)
{
	OpFile policyFile;
#ifdef USE_COMMON_RESOURCES	
	LEAVE_IF_ERROR(policyFile.Construct(UNI_L("opera.policy"), OPFILE_JAVA_FOLDER));
#else	
	LEAVE_IF_ERROR(policyFile.Construct(UNI_L("opera.policy"), OPFILE_RESOURCES_FOLDER));
#endif // USE_COMMON_RESOURCES	
	target.SetL(policyFile.GetFullPath());
}
#endif // _APPLET_2_EMBED_
#endif // 0

#ifdef USE_OP_MAIN_THREAD

BOOL MacOpSystemInfo::IsInMainThread()
{
	ThreadID currThread = 0;
	GetCurrentThread(&currThread);
	return (gMainThreadID == currThread);
}

#endif

extern const uni_char* GetSystemAcceptLanguage();

const char *MacOpSystemInfo::GetSystemEncodingL()
{
	const char* preferedLanguage = NULL;
	OpString8 sys_8;
	if (g_pcnet)
		preferedLanguage = g_pcnet->GetAcceptLanguage();
	if (!preferedLanguage)
	{
		sys_8.Set(GetSystemAcceptLanguage());
		preferedLanguage = sys_8.CStr();
	}
	if (preferedLanguage)
	{
		unsigned short langCode = *(const unsigned short*) preferedLanguage;
		langCode = htons(langCode);
		switch (langCode)
		{
			case 0x656e: //'en':	// English
			case 0x6465: //'de':	// German
			case 0x6672: //'fr':	// French
			case 0x6e6c: //'nl':	// Duch
			case 0x6974: //'it':	// Italian
			case 0x6573: //'es':	// Spanish
			case 0x7074: //'pt':	// Portuguese
			case 0x7376: //'sv':	// Swedish
			case 0x6461: //'da':	// Danish
			case 0x6e6f: //'no':	// Norwegian
			case 0x6669: //'fi':	// Finnish
			case 0x6379: //'cy':	// Cymraeg/Welch
			case 0x656f: //'eo':	// Esperanto
			case 0x6575: //'eu':	// Euzkeraz
			case 0x6973: //'is':	// Icelandic
			case 0x6d74: //'mt':	// Maltese
				return "iso-8859-15";
			case 0x656c: //'el':	// Greek
				return "iso-8859-7";
			case 0x7372: //'sr':	// Serbian
			case 0x6267: //'bg':	// Bulgarian
			case 0x6d6b: //'mk':	// Macedonia
			case 0x7275: //'ru':	// Russian
			case 0x756B: //'uk':	// Ukrainian
				return "iso-8859-5";	//Cyrillic
			case 0x6761: //'ga':	// Gaeilge/Irish (Gaelic)
			case 0x6764: //'gd':	// Gaidhlig/Scottish Gaelic
				return "iso-8859-14";
			case 0x7969: //'yi':	// Yiddish
			case 0x6865: //'he':	// Hebrew
				return "iso-8859-8";
			case 0x6172: //'ar':	// Arabic
				return "iso-8859-6";
			case 0x6a61: //'ja':
				return "iso-2022-jp";	// This affects, default outgoing encoding. Default in chat/mail should be iso-2022-jp no matter what.
//				return "shift_jis";
			case 0x6b6f: //'ko':
				return "euc-kr";
			case 0x7a68: //'zh':
				short sub_code =  *(const unsigned short*) (preferedLanguage + 3);
				sub_code = htons(sub_code);
				if (sub_code == 0x434e)	//'CN', simplified
					return "gb2312";
				if ((sub_code == 0x5457) || (sub_code == 0x484b))	//'TW'+'HK', traditional
					return "big5";
				break;
		}
	}

	return "utf-8";	// LAST RESORT!
}

OP_STATUS MacOpSystemInfo::ExpandSystemVariablesInString(const uni_char* in, OpString* out)
{
	// FIXME: OpString implementation should be used throughout.
	if (!out) return OpStatus::ERR;
	if (!out->CStr())
		RETURN_IF_LEAVE(out->ReserveL(MAX_PATH));
	return ExpandSystemVariablesInString(in, out->CStr(), out->Capacity());
}

OP_STATUS MacOpSystemInfo::ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString tmp_storage1;
	OpString tmp_storage2;
	OpString tmp_storage3;
	return OpFileUtils::ExpandSystemVariablesInString(in, out, out_max_len, g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_RESOURCES_FOLDER, tmp_storage1),
														g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_LOCAL_HOME_FOLDER, tmp_storage2), g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage3));
}

const uni_char* MacOpSystemInfo::GetDefaultTextEditorL()
{
	CFURLRef 	appURL;
	static OpString	appPathOpString;
	FSRef 		fsref;

	if (noErr == LSGetApplicationForInfo('TEXT', kLSUnknownCreator, CFSTR("txt"), kLSRolesAll, NULL, &appURL))
	{
		if (CFURLGetFSRef(appURL, &fsref))
			OpFileUtils::ConvertFSRefToUniPath(&fsref, &appPathOpString);
	}
	return appPathOpString.CStr();
}

#ifdef OPSYSTEMINFO_GETFILETYPENAME
OP_STATUS MacOpSystemInfo::GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len)
{
//	OP_ASSERT(!"Implement MacOpSystemInfo::GetFileTypeName()");
	if (out)
		*out = 0;
	return OpStatus::OK;
}
#endif // OPSYSTEMINFO_GETFILETYPENAME

#ifdef QUICK
OP_STATUS MacOpSystemInfo::GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler)
{
	OpFile 			theFile;
	OSType 			fileCreator;
	OSType 			fileType;
	Boolean 		unknownFileTypeAndCreator = true;
	CFStringRef 	extension = 0;
	CFURLRef 		appURL;
	OP_STATUS		status = OpStatus::ERR;
	Boolean			fileIsFolder = false;
	if(!filename)
		return OpStatus::ERR;

	OpStringC cstr(filename->CStr());
	int offset = 0;
	int length = 0;
	offset = cstr.FindLastOf('.');
	if (offset == KNotFound) {
		offset = 0;
	} else {
		offset += 1;
	}

	length = (cstr.Length() - offset);

	extension = CFStringCreateWithCharacters(NULL, (UniChar*)cstr.CStr() + offset, length);

	if(unknownFileTypeAndCreator)
	{
		fileCreator = kLSUnknownCreator;
		fileType = kLSUnknownType;
	}

	if(noErr == LSGetApplicationForInfo(fileType, fileCreator, extension, kLSRolesAll, NULL, &appURL))
	{
		CFStringRef appPath = CFURLCopyFileSystemPath(appURL, kCFURLPOSIXPathStyle);

		if(appPath)
		{
			size_t pathLen = CFStringGetLength(appPath);

			if(handler.Reserve(pathLen+1) != NULL)
			{
				uni_char *dataPtr = handler.DataPtr();
				CFStringGetCharacters(appPath, CFRangeMake(0, pathLen), (UniChar*)dataPtr);
				dataPtr[pathLen] = 0;

				status = OpStatus::OK;
			}

			CFRelease(appPath);
		}

		CFRelease(appURL);
	}
	else
	{
		if (filename->CStr()[filename->Length() - 1] == '/')
			fileIsFolder = true;
		else
		{
			FSRef docFSRef;
			if (OpFileUtils::ConvertUniPathToFSRef(filename->CStr(), docFSRef))
			{
				FSCatalogInfoBitmap what = kFSCatInfoNodeFlags;
				FSCatalogInfo folder_info;
				FSSpec folder_fss;
				FSGetCatalogInfo(&docFSRef, what, &folder_info, NULL, &folder_fss, NULL);
				if (folder_info.nodeFlags & (1 << 4))
				{
					fileIsFolder = true;
				}
			}
		}
		if (fileIsFolder)
			handler.Set(UNI_L("FINDER"));	// Dummy String.
		else
			handler.Set(UNI_L(""));
		status = OpStatus::OK;
	}

	if(extension)
		CFRelease(extension);

	return status;
}

OP_STATUS MacOpSystemInfo::GetFileHandlers(const OpString& filename,
										  const OpString &content_type,
										  OpVector<OpString>& handlers,
										  OpVector<OpString>& handler_names,
										  OpVector<OpBitmap>& handler_icons,
										  URLType type,
										  UINT32 icon_size)
{
#ifndef MACGOGI
	MacFileHandlerCache	*cache_entry;
	OpString			contentType, handler, local_file, default_app, ext;
	UINT32				index;

	// First we need to grab the default application so that it can appear at the
	// the top of the menu

	// The type coming in can be Local or Remote file.
	// We will handle these in different ways
	if (type == URL_FILE)
	{
		// The full path is supposed to be come through with local files so
		// we can just go straight for them
		local_file.Set(filename);
	}
	else
	{
		// Remote files we can just assume we want to open it in a webbrowser
		// so make a dummy htm link and get the list of apps that can open it.
		local_file.Set("dummy.htm");
	}

	// Grab just the extension
	int offset = local_file.FindLastOf('.');
	if (offset != KNotFound)
		ext.Set(local_file.CStr() + offset);
	else
		return OpStatus::ERR;

	// Try and retrieve a cached entry
	if (OpStatus::IsSuccess(m_file_handlers.GetData(ext, &cache_entry)))
	{
		RETURN_IF_ERROR(cache_entry->CopyInto(handlers, handler_names, handler_icons, icon_size));

		return OpStatus::OK;
	}
	else
	{
		// Call GetFileHandler which finds the default one and add it to the list straight away
		// Make sure we get a default handler or there really is no point going on
		if (OpStatus::IsSuccess(GetFileHandler(&local_file, contentType, handler)) && !handler.IsEmpty())
		{
			OpString 				default_text;
			FSRef			 		fsref;
			OpAutoVector<OpString> 	signatures;
			OpAutoVector<OpString> 	short_versions;

			// Create the new cache entry and add it to the list
			cache_entry = new MacFileHandlerCache();
			if (!cache_entry)
				return OpStatus::ERR_NO_MEMORY;

			// Set the extension
			cache_entry->m_ext.Set(ext);
			// Save into the list
			m_file_handlers.Add(cache_entry->m_ext.CStr(), cache_entry);

			OpString *app_path = new OpString();
			app_path->Set(handler.CStr());
			// Save the default app so we don't add it a second time below
			default_app.Set(*app_path);

			OpString *app_name = new OpString();
			OpFileUtils::GetFriendlyAppName(handler.CStr(), *app_name);

			// Create the strings, we need them even if they are blank do the arrays match
			OpString *signature_def = new OpString();
			OpString *short_version_def = new OpString();

			// Convert the default app to a URL
			CFURLRef defaultURLRef = NULL;
			if(OpFileUtils::ConvertUniPathToFSRef(default_app.CStr(), fsref))
			{
				defaultURLRef = CFURLCreateFromFSRef(NULL, &fsref);
			}
			if (defaultURLRef != NULL && !IsSandboxed())
				GetSignatureAndVersionFromBundle(defaultURLRef, signature_def, short_version_def);

			// Add the strings to the array even if blank
			signatures.Add(signature_def);
			short_versions.Add(short_version_def);

			// Add (default) to the end of the friendly name
			TRAPD(rc, g_languageManager->GetStringL(Str::S_CONFIG_DEFAULT, default_text));
			if (OpStatus::IsSuccess(rc)) {
				default_text.MakeLower();
				app_name->AppendFormat(UNI_L(" (%s)"), default_text.CStr());
			}

			OpBitmap *app_icon;
			app_icon = GetAttachmentIconOpBitmap(handler.CStr(), FALSE, (UINT32)icon_size);

			// Add the default item to the list
			cache_entry->m_handlers.Add(app_path);
			cache_entry->m_handler_names.Add(app_name);
			cache_entry->m_handler_icons.Add(app_icon);

				CFURLRef inURL = NULL;
				OpFile	 file;
				BOOL	 cleanup = FALSE;

				// The type coming in can be Local or Remote file.
				// We will handle these in different ways
				if (type == URL_FILE)
				{
					// The full path is supposed to be come through with local files so
					// we can just go straight for them, if the file doesn't exist on
					// disk just make it and let Launch Services do it's magic with 
					// the extension
					if (!OpFileUtils::ConvertUniPathToFSRef(filename.CStr(), fsref))
					{
						// Just to be super-safe look for a backslash
						uni_char *just_filename = uni_strrchr(filename.CStr(), PATHSEPCHAR);
						if(!just_filename)
							just_filename = (uni_char *)filename.CStr();
							
						// Dodge-o-rama but just make a file on disk so this thing will work
						if (OpStatus::IsSuccess(file.Construct(just_filename, OPFILE_TEMP_FOLDER)))
						{
							OpStatus::Ignore(file.Open(OPFILE_WRITE));
							file.Close();
							OpFileUtils::ConvertUniPathToFSRef(file.GetFullPath(), fsref);
							cleanup = TRUE;
						}
					}
					inURL = CFURLCreateFromFSRef(NULL, &fsref);
				}
				else
				{
					// Remote files we can just assume we want to open it in a webbrowser
					// so make a dummy htm link and get the list of apps that can open it.
					const uni_char * foo = UNI_L("http://www.opera.com/");
					inURL = CFURLCreateWithBytes(kCFAllocatorDefault, (const UInt8*)foo, uni_strlen(foo), kCFStringEncodingUnicode, NULL);
				}

				CFArrayRef apps = LSCopyApplicationURLsForURL(inURL, kLSRolesAll);

				// Clean up if the temp file was required
				if (cleanup)
					file.Delete();
				
				CFIndex appCount = 0;
				if (apps == NULL || (appCount = CFArrayGetCount(apps)) == 0)
					return OpStatus::ERR;

				CFIndex 	appIndex;
				CFURLRef 	appURL;
				OpString 	path;
				UINT32 		add_index;

				for (appIndex = 0 ; appIndex < appCount ; appIndex++) {
					appURL = (const CFURLRef) CFArrayGetValueAtIndex(apps, appIndex);

				if(CFURLGetFSRef(appURL, &fsref))
				{
					OpFileUtils::ConvertFSRefToUniPath(&fsref, &path);

					// Don't add the default again
					if (path == default_app)
						continue;

					// Create the strings, we need them even if they are blank do the arrays match
					OpString *signature = new OpString();
					OpString *short_version = new OpString();

					// We need to grab the signature and short version from the bundle
					if (!IsSandboxed())
						GetSignatureAndVersionFromBundle(appURL, signature, short_version);

					// Add the signature and version at the bottom so that the lists mirror each
					// other and are in the correct order.

					OpString *app_path = new OpString();
					app_path->Set(path.CStr());

					OpString *app_name = new OpString();
					OpFileUtils::GetFriendlyAppName(path.CStr(), *app_name);

					OpBitmap *app_icon;
					app_icon = GetAttachmentIconOpBitmap(path.CStr(), FALSE, (UINT32)icon_size);

					// Start the add index at the bottom of the list so it only changes if
					// we find a valid insertion point
					add_index = cache_entry->m_handlers.GetCount();

					bool version_done = false;

					// Find where to add the item into the list so that it is sorted alphabetically
					for (index = 0; index < cache_entry->m_handlers.GetCount(); index++)
					{
						// Check if the signature of what you are adding is
						// the same as this item in the handlers list
						if (!signature->IsEmpty() && !signatures.Get(index)->IsEmpty() && !signature->Compare(*signatures.Get(index)))
						{
							// Has the matching item had version info added before (If not then add it!)
							// It will have an extra space on the end if it did
							if (cache_entry->m_handler_names.Get(index)->CStr()[cache_entry->m_handler_names.Get(index)->Length() - 1] != UNI_L(' '))
							{
								// Add version information to the friendly name of the matching item
								// Also append a space to say that this has been done!
								cache_entry->m_handler_names.Get(index)->AppendFormat(UNI_L(" (%s) "), short_versions.Get(index)->CStr());
							}

							// Only add the version once!
							if (!version_done)
							{
								// Add version info to the friendly name of the Item being added
								// Also append a space to say that this has been done!
								app_name->AppendFormat(UNI_L(" (%s) "), short_version->CStr());
								version_done = true;
							}
						}

						// Don't do a comparison test on the default item or if it has already been set
						if (index && (add_index == cache_entry->m_handlers.GetCount()) &&
							(app_name->Compare(*cache_entry->m_handler_names.Get(index)) < 0))
							add_index = index;
					}

					// Add the item to the list
					cache_entry->m_handlers.Insert(add_index, app_path);
					cache_entry->m_handler_names.Insert(add_index, app_name);
					cache_entry->m_handler_icons.Insert(add_index, app_icon);

					// Add the strings to the array even if blank
					signatures.Insert(add_index, signature);
					short_versions.Insert(add_index, short_version);
				}
			}

			// Strip the extra space that marked an item as already having the version added
			for (index = 0; index < cache_entry->m_handler_names.GetCount(); index++)
				cache_entry->m_handler_names.Get(index)->Strip();

			CFRelease(apps);

			// Copy the cache_entries data into the returned vectors
			RETURN_IF_ERROR(cache_entry->CopyInto(handlers, handler_names, handler_icons, icon_size));

			return OpStatus::OK;
		}
	}
#endif // MACGOGI

	return OpStatus::ERR;
}

OP_STATUS MacOpSystemInfo::OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files)
{
	OpFileInfo::Mode	mode;
	OpFile				file;
	BOOL				exists = FALSE;
	
	// File may exist so check this first
	RETURN_IF_ERROR(OpStatus::IsSuccess(file.Construct(file_path.CStr())));
	
	// Make sure that the file exists
	RETURN_IF_ERROR(OpStatus::IsSuccess(file.Exists(exists)));
	if (!exists)
		return OpStatus::ERR;

	// Get the file type
	RETURN_IF_ERROR(OpStatus::IsSuccess(file.GetMode(mode)));

	// If treat_folders_as_files is true and this is a folder open the parent
	OpString full_path;
	full_path.Set(file_path.CStr());
	if (mode == OpFileInfo::DIRECTORY && treat_folders_as_files)
	{
		// Kill the slash on the end if it's there
		if (full_path.HasContent() && full_path[full_path.Length() - 1] == PATHSEPCHAR)
			full_path[full_path.Length() - 1] = '\0';
		
		// Look for the next slash to move to the parent folder
		int slash_pos;
		if ((slash_pos = full_path.FindLastOf(PATHSEPCHAR)) > KNotFound)
			full_path[slash_pos] = '\0';
		
		// Check the parent folder exists on disk
		// Make sure that the file exists
		exists = FALSE;
		RETURN_IF_ERROR(OpStatus::IsSuccess(file.Exists(exists)));
		if (!exists)
			return OpStatus::ERR;
	}
	
	::OpenFileFolder(full_path);

	return OpStatus::OK;
}

BOOL MacOpSystemInfo::AllowExecuteDownloadedContent()
{
	return TRUE;
}

OP_STATUS MacOpSystemInfo::GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler)
{
	OpString appname;
	OpString protocolstring;
	protocolstring.Set(protocol.CStr());
	protocolstring.Append(UNI_L(":"));
	Boolean retval = MacGetDefaultApplicationPath(protocolstring.CStr(), appname, handler);

	/*
	 For some Apple-specific protocols, if the application is found, we can regard the handler as trusted. 
	 To put this logic here is a HACK, but will allow us to avoid the annoying Unknown Protocol Dialog,
	 without touching any core code or other platforms than Mac.
	 */
	if (retval && GetOSVersion() >= 0x1060)
	{
		OpString protocol_name;
		int colonindex = protocolstring.FindFirstOf(':');
		protocol_name.Set(protocolstring, colonindex);
		if (OpStatus::IsSuccess(protocol_name.Set(protocolstring, colonindex)) &&
			(protocol_name == UNI_L("macappstore") ||
			 protocol_name == UNI_L("macappstores") ||
			 protocol_name == UNI_L("itms") ||
			 protocol_name == UNI_L("itmss") ||
			 protocol_name == UNI_L("itunes")))
		{
			TrustedProtocolData data;
			if (g_pcdoc->GetTrustedProtocolInfo(protocol_name, data) == -1)
			{
				int num_trusted_protocols = g_pcdoc->GetNumberOfTrustedProtocols();

				data.protocol = protocol_name;
				data.filename = appname;
				data.viewer_mode = UseDefaultApplication;
				data.flags = TrustedProtocolData::TP_Protocol | TrustedProtocolData::TP_Filename | TrustedProtocolData::TP_ViewerMode;

				BOOL success = FALSE;
				TRAPD(err,success = g_pcdoc->SetTrustedProtocolInfoL(num_trusted_protocols, data));

				if (OpStatus::IsSuccess(err) && success)
				{
					num_trusted_protocols++;
					TRAP(err,g_pcdoc->WriteTrustedProtocolsL(num_trusted_protocols));
					OP_ASSERT(OpStatus::IsSuccess(err));
					TRAP(err, g_prefsManager->CommitL());
					OP_ASSERT(OpStatus::IsSuccess(err));
				}
			}
		}
	}

	return retval ? OpStatus::OK : OpStatus::ERR;
}

BOOL MacOpSystemInfo::GetIsExecutable(OpString* filename)
{
	if( !filename )
	{
		return FALSE;
	}
	else
	{
		BOOL retval = FALSE;
		CFStringRef filePath = CFStringCreateWithCharacters(NULL, (UniChar *)filename->CStr(), filename->Length());
		if(filePath)
		{
			CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, filePath, kCFURLPOSIXPathStyle, TRUE);
			if(bundleURL)
			{
				CFBundleRef ref = CFBundleCreate(NULL, bundleURL);
				if(ref)
				{
					CFRelease(ref);
					retval = TRUE;
				}
				CFRelease(bundleURL);
			}
			CFRelease(filePath);
		}
		return retval;
	}

}

OP_STATUS MacOpSystemInfo::GetIllegalFilenameCharacters(OpString* illegalchars)
{
	return illegalchars->Set(UNI_L(PATHSEP));
}

BOOL MacOpSystemInfo::RemoveIllegalPathCharacters(OpString& path, BOOL replace)
{
	path.Strip();
	return path.Length() > 0;
}

BOOL MacOpSystemInfo::RemoveIllegalFilenameCharacters(OpString& path, BOOL replace)
{
	if( replace )
	{
		int length = path.Length();
		for( int i=0; i<length; i++ )
		{
			if( path.CStr()[i] == '/' )
			{
				path.CStr()[i] = '_';
			}
		}
	}
	else
	{
		const size_t end = path.Length();
		size_t i = 0;
		while (i < end && path.CStr()[i] != PATHSEPCHAR)
			i++;
		size_t j = i++;
		while (i < end)
			if (path.CStr()[i] == PATHSEPCHAR)
				i++;
			else
				path.CStr()[j++] = path.CStr()[i++];
		path.CStr()[j] = 0;
	}

	path.Strip();
	return path.HasContent();
}

////////////////////////////////////////////////////////////////////////////////////

OpString MacOpSystemInfo::GetLanguageFolder(const OpStringC &lang_code)
{
	OpString ret; 
	
	ret.AppendFormat(UNI_L("%s.lproj"), lang_code.CStr()); 
	
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpSystemInfo::GetNewlineString(OpString* newline_string)
{
	return newline_string->Set(UNI_L("\r"));
}

INT32 MacOpSystemInfo::GetShiftKeyState()
{
	ShiftKeyState state = SHIFTKEY_NONE;
	if (GetEventShiftKeyState(state))
		return state;
#ifdef NO_CARBON
	CGEventFlags mods = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
	if (mods & kCGEventFlagMaskShift)
		state |= SHIFTKEY_SHIFT;
	if (mods & kCGEventFlagMaskAlternate)
		state |= SHIFTKEY_ALT;
	if (mods & kCGEventFlagMaskCommand)
		state |= SHIFTKEY_CTRL;
#else
	UInt32 mods = GetCurrentKeyModifiers();
	if (mods & shiftKey)
		state |= SHIFTKEY_SHIFT;
	if (mods & optionKey)
		state |= SHIFTKEY_ALT;
	if (mods & cmdKey)
		state |= SHIFTKEY_CTRL;
#endif
	return state;
}
#endif // QUICK

#ifdef EMBROWSER_SUPPORT
OP_STATUS MacOpSystemInfo::SetCustomHighlightColor(BOOL use_custom, COLORREF new_color)
{
	return ((MacOpUiInfo*)g_op_ui_info)->SetCustomHighlightColor(use_custom, new_color);
}
#endif //EMBROWSER_SUPPORT

#ifndef NO_EXTERNAL_APPLICATIONS
# ifdef DU_REMOTE_URL_HANDLER
OP_STATUS MacOpSystemInfo::PlatformExecuteApplication(const uni_char* application,
													  const uni_char* args,
													  BOOL)
# else
OP_STATUS MacOpSystemInfo::ExecuteApplication(const uni_char* application,
											  const uni_char* args,
											  BOOL)
# endif
{
#ifndef MACGOGI
	if (MacExecuteProgram(application, args))
	{
		return OpStatus::OK;
	}
#endif
	return OpStatus::ERR;
}
#endif // !NO_EXTERNAL_APPLICATIONS

#ifdef EXTERNAL_APPLICATIONS_SUPPORT 
OP_STATUS MacOpSystemInfo::OpenURLInExternalApp(const URL& url) 
{
	OpAutoPtr<uni_char> urlname(StrFromLocaleEncoding(url.Name(PASSWORD_NOTHING))); 
	Boolean result = MacOpenURLInDefaultApplication(urlname.get()); 
	return result ? OpStatus::OK : OpStatus::ERR;
}
#endif // EXTERNAL_APPLICATIONS_SUPPORT

#if defined(_CHECK_SERIAL_) || defined(M2_SUPPORT) || defined(DPI_CAP_DESKTOP_SYSTEMINFO)
OP_STATUS MacOpSystemInfo::GetSystemIp(OpString& ip)
{
	char hname[1024];
	struct hostent	*host;
	int err = gethostname(hname, 1024);
	if (err == 0)
	{
		host = gethostbyname(hname);
		if (host && host->h_addr_list[0] && host->h_length == 4)
		{
			const char* localIP = inet_ntoa(*(struct in_addr *)*host->h_addr_list);
			if (localIP)
			{
				ip.Set(localIP);
				return OpStatus::OK;
			}
		}
	}
	return OpStatus::ERR;
}
#endif // _CHECK_SERIAL_ || M2_SUPPORT || DPI_CAP_DESKTOP_SYSTEMINFO

const char *MacOpSystemInfo::GetOSStr(UA_BaseStringId ua)
{
	// 10.4.1: For some reason, Safari on Intel still returns "PPC" here.
	// Most likely to remain compatiple with as many pages as possible.
	// For the time being, let's do the same.

	// 10.4.3: OK, now Safari reports "Intel" as platform. Apparently it was just a bug. Updating ours to comply.

	if (ua == UA_MSIE)
	{
		// MSIE is strange. Note that IE used "Mac_PowerPC" in navigator.userAgent but "Macintosh" in navigator.appVersion
		// Since these strings are normally the same in Opera, we have a workaround in navigat.cpp to get around this.

		// We really ought to work out what to report here, so our processor-IFF system on for instance the bug reporter, detect us correctly.
		// OTOH, since IE for Intel Macs will probably never exist, this is most likely the most "compatible" string.
		return "Mac_PowerPC";
	}
	else if (ua == UA_MSIE_Only)
	{
		// bug 211012:
		// Mask as IE should spoof IE6 on Windows XP perfectly.
		return "Windows NT 5.1";
	}
	else
	{
		int major, minor, bugfix;
		GetDetailedOSVersion(major, minor, bugfix);
		static char osstr[64];
		static char osform[] = "Macintosh; Intel Mac OS X %d.%d.%d";
		sprintf(osstr, osform, major, minor, bugfix);
		return osstr;
	}
}

const uni_char *MacOpSystemInfo::GetPlatformStr()
{
	// from javascript:alert(navigator.platform)
	// 68k Macs return "Macintosh". PPC return "MacPPC"
	return UNI_L("MacIntel");
}

#ifndef POSIX_OK_NATIVE
OP_STATUS MacOpSystemInfo::OpFileLengthToString(OpFileLength length, OpString8* result)
{
	char buf[24];
#ifdef MAC_64BIT_FILES
	sprintf(buf, "%lli", length);
#else
	sprintf(buf, "%i", length);
#endif
	return result->Set(buf);
}

OP_STATUS MacOpSystemInfo::StringToOpFileLength(const char* length, OpFileLength* result)
{
	long long answer; // gcc wants %lld to see a long long ...
	if (strchr(length, '-') || // See bug 205033
		sscanf( length, "%lld", &answer ) != 1)
	{
		return OpStatus::ERR;
	}
	*result = answer;
	return OpStatus::OK;
}
#endif

#ifdef SYNCHRONOUS_HOST_RESOLVING
BOOL MacOpSystemInfo::HasSyncLookup()
{
	return TRUE;
}
#endif // SYNCHRONOUS_HOST_RESOLVING

SystemLanguage GetSystemLanguage()
{
	static SystemLanguage gSysLang;
	static Boolean gSysLangInitialized = false;
	if (!gSysLangInitialized)
	{
		const uni_char* lang = GetSystemAcceptLanguage();
		if (!lang)
			gSysLang = kSystemLanguageUnknown;
		else if (0 == uni_strnicmp(lang, UNI_L("ja"), 2))
			gSysLang = kSystemLanguageJapanese;
		else if (0 == uni_strnicmp(lang, UNI_L("zh_CN"), 5))
			gSysLang = kSystemLanguageSimplifiedChinese;
		else if ((0 == uni_strnicmp(lang, UNI_L("zh_TW"), 5)) || (0 == uni_strnicmp(lang, UNI_L("zh_HK"), 5)))
			gSysLang = kSystemLanguageTraditionalChinese;
		else if (0 == uni_strnicmp(lang, UNI_L("ko"), 2))
			gSysLang = kSystemLanguageKorean;
		else
			gSysLang = kSystemLanguageUnknown;
		gSysLangInitialized = true;
	}
	return gSysLang;
}

OP_STATUS MacOpSystemInfo::GetUserLanguages(OpString *result)
{
	return result->Set(GetSystemAcceptLanguage());
}

void MacOpSystemInfo::GetUserCountry(uni_char result[3])
{
	CFLocaleRef userLocaleRef;
	CFStringRef stringRef;

	userLocaleRef = CFLocaleCopyCurrent();
	stringRef = (CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleCountryCode);
	int len = 0;
	if (stringRef) {
		len = CFStringGetLength(stringRef);
		CFStringGetCharacters(stringRef, CFRangeMake(0,len), (UniChar *)result);
	}
	result[len] = 0;
	CFRelease(userLocaleRef);
}

#ifdef GUID_GENERATE_SUPPORT
OP_STATUS MacOpSystemInfo::GenerateGuid(OpGuid &guid)
{
	CFUUIDRef   uuid = 0;
	CFUUIDBytes bytes;

	// Generate the UUID (GUID) and get the bytes
	uuid	= CFUUIDCreate(kCFAllocatorDefault);
	bytes	= CFUUIDGetUUIDBytes(uuid);

	if (uuid)
	{
		int i;
		int maximum = sizeof(OpGuid);

		// should be the same, 128-bit
		OP_ASSERT(sizeof(OpGuid) == sizeof(CFUUIDBytes));

		unsigned char *ptr = (unsigned char *)&bytes;
		for(i = 0; i < maximum; i++)
		{
			guid[i] = *(ptr++);
		}
		CFRelease(uuid);

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}
#endif // GUID_GENERATE_SUPPORT

#ifdef DPI_CAP_DESKTOP_SYSTEMINFO

#ifndef NO_CARBON
int MacOpSystemInfo::GetDoubleClickInterval()
{
	int tics = GetDblTime();	// 1 tick = 1/60 second.
	int ms = (int)((((double)tics) * 1000.0) / 60.0);
	return ms;
}
#endif // !NO_CARBON

OP_STATUS MacOpSystemInfo::GetFileTypeInfo(const OpStringC& filename,
									  const OpStringC& content_type,
									  OpString & content_type_name,
									  OpBitmap *& content_type_bitmap,
									  UINT32 content_type_bitmap_size)
{
	CFStringRef mime_type = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)content_type.CStr(), content_type.Length());

	// Get the File Kind based on the MIME type
	if (mime_type)
	{
		CFStringRef outKindString;

		if (LSCopyKindStringForMIMEType (mime_type,  &outKindString) == noErr && outKindString)
		{
			SetOpString(content_type_name, outKindString);
			CFRelease(outKindString);
		}

		CFRelease(mime_type);
	}

	// I can't see how to get the icon from the File Kind so I guess this will become the mess
	// that is create a dummy file on disk and then ask what icon it uses, then clean up the file.
	BOOL	exists = FALSE;
	BOOL	get_file_icon = FALSE;
	BOOL	dummy_file = FALSE;
	OpFile	file_on_disk;

	// File may exist so check this first
	if (OpStatus::IsSuccess(file_on_disk.Construct(filename.CStr())) &&
	    OpStatus::IsSuccess(file_on_disk.Exists(exists)) && exists)
	{
		get_file_icon = TRUE;
	}
	else {
		FSRef		fsref;
		OpString 	path;

		// Get temp folder
		if ((FSFindFolder(kUserDomain, kTemporaryFolderType, kCreateFolder, &fsref) == noErr) &&
			OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&fsref, &path)))
		{
			if((path.Length() > 0) && (path[path.Length() - 1] != PATHSEPCHAR))
			{
				path.Append(PATHSEP);
			}
			path.Append(filename.CStr());

			// Create a dummy file
			if (OpStatus::IsSuccess(file_on_disk.Construct(path)) &&
				OpStatus::IsSuccess(file_on_disk.Open(OPFILE_WRITE)))
			{
				get_file_icon =	dummy_file = TRUE;
				file_on_disk.Close();
			}
		}
	}

	// Do we have a file we can get an icon for now?
	if (get_file_icon)
	{
		content_type_bitmap = GetAttachmentIconOpBitmap(file_on_disk.GetFullPath(), FALSE, (UINT32)content_type_bitmap_size);
	}

	// Clean up temp file if it exists
	if (dummy_file)
	{
		file_on_disk.Delete();
	}

	return OpStatus::ERR;
}
#endif // DPI_CAP_DESKTOP_SYSTEMINFO

#if !defined(POSIX_OK_PATH)
# ifdef PI_CAP_PATHSEQUAL
OP_STATUS MacOpSystemInfo::PathsEqual(const uni_char* p0, const uni_char* p1, BOOL* equal)
{
	size_t i0 = uni_strlen(p0), i1 = uni_strlen(p1), s0 = i0, s1 = i1;
	while (i0 > 0 && i1 > 0)
		if (p0[--i0] != p1[--i1])
			break;
		else if (p0[i0] == PATHSEPCHAR)
		{
			s0 = i0;
			s1 = i1;
		}
	
	char norm0[PATH_MAX + 1], norm1[PATH_MAX + 1];
	const char	*lp0 = ResolvePath(p0, s0, norm0),
				*lp1 = ResolvePath(p1, s1, norm1);
	
	*equal= (lp0 && lp1 && op_strcmp(lp0, lp1) == 0);
	return OpStatus::OK;
}
# endif // PI_CAP_PATHSEQUAL

# if defined(OPSYSTEMINFO_PATHSEQUAL) || defined(PI_CAP_PATHSEQUAL)
const char *MacOpSystemInfo::ResolvePath(const uni_char *path, size_t stop, char *buffer) const
{
	// buffer's size is PATH_MAX + 1
	OpString8 loc;
	loc.SetUTF8FromUTF16(path, stop);
	if (loc.CStr() == 0)
		return NULL;

	errno = 0;
	const char *ret = realpath( loc.CStr(), buffer );
	if (ret) // the easy case: path exists
	{
		OP_ASSERT(ret == buffer);
		return ret;
	}

	/* We now need to resolve path into a non-existent suffix on an extant
	 * prefix, resolve the prefix to canonical form and append the suffix to it.
	 * For these purposes, things we can't read (whether due to access
	 * restriction or due to excessive symlink recursion) are deemed to not
	 * exist.
	 */
	char *local = loc.CStr();
	size_t cut = strlen(local);
	buffer[0] = '\0';
	while (cut > 0)
	{
		switch (errno) // from realpath
		{
		case EINVAL: case ENAMETOOLONG:
			return NULL;
		case ENOMEM:
			return NULL;
		}
		while (cut-- > 0 && local[cut] != PATHSEPCHAR)
			/* skip */;

		if (cut > 0)
		{
			local[cut] = '\0';
			ret = realpath(local, buffer);
			local[cut] = PATHSEPCHAR;
			if (ret)
			{
				OP_ASSERT(ret == buffer);
				break;
			}
			buffer[0] = '\0';
			cut--; // skip the PATHSEPCHAR
		}
	}
	/* Now buffer[:strlen(buffer)] is the canonical form of local[:cut]; append
	 * local[cut:] to it.  Eddy/2007/July/27/17:10 doesn't think it makes sense
	 * to try to prune uses of ../ from the non-existent tail portion.
	 */

	if (op_strlcat(buffer, local + cut, PATH_MAX + 1) > PATH_MAX + 1)
		return NULL;

	return buffer;
}

# endif // OPSYSTEMINFO_PATHSEQUAL
#endif // !POSIX_OK_PATH

#if defined(POSIX_SERIALIZE_FILENAME) && defined(POSIX_OK_FILE)

uni_char* MacOpSystemInfo::SerializeFileName(const uni_char *path)
{
	OpString respath, largeprefspath, smallprefspath;
	if (!path)
		return NULL;

	uni_char *filepath = new uni_char[uni_strlen(path)+16];
	if (!filepath)
		return NULL;

	// Set the resource path
	/* The return value of each of the calls to GetFolderPath below
	 * only needs to live until the surrounding Set() has returned.
	 * So it should be safe to reuse the same storage for all of them.
	 */
	OpString tmp_storage;
	respath.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_RESOURCES_FOLDER, tmp_storage));
	largeprefspath.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_LOCAL_HOME_FOLDER, tmp_storage));
	smallprefspath.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_HOME_FOLDER, tmp_storage));

	OpFileUtils::SerializeFileName(path, filepath, uni_strlen(path)+16, respath, largeprefspath, smallprefspath);
	return filepath;
}
#endif // POSIX_SERIALIZE_FILENAME && POSIX_OK_FILE

void MacOpSystemInfo::ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler)
{
	::ComposeExternalMail(to,cc,bcc,subject,message,raw_address,mailhandler);
}

BOOL MacOpSystemInfo::HasSystemTray()
{
	return FALSE;
}

#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
BOOL MacOpSystemInfo::ShallWeTryToSetOperaAsDefaultBrowser()
{
	CFStringRef product_bundle_id = GetCFBundleID();

	int hits = 0;
	CFStringRef handler = LSCopyDefaultRoleHandlerForContentType(CFSTR("public.html"), kLSRolesAll);
	if (handler)
	{
		if (CFStringCompare(handler, product_bundle_id, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			hits++;
		CFRelease(handler);
	}

	handler = LSCopyDefaultRoleHandlerForContentType(CFSTR("public.xhtml"), kLSRolesAll);
	if (handler) {
		if (CFStringCompare(handler, product_bundle_id, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			hits++;
		CFRelease(handler);
	}


	handler = LSCopyDefaultHandlerForURLScheme(CFSTR("http"));
	if (handler) {
		if (CFStringCompare(handler, product_bundle_id, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			hits++;
		CFRelease(handler);
	}

	handler = LSCopyDefaultHandlerForURLScheme(CFSTR("https"));
	if (handler)
	{
		if (CFStringCompare(handler, product_bundle_id, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			hits++;
		CFRelease(handler);
	}

	return (hits < 4);
}

OP_STATUS MacOpSystemInfo::GetBinaryPath(OpString *path)
{
	if (!path)
		return OpStatus::ERR_NULL_POINTER;

	ProcessSerialNumber psn;
	FSRef				ref_app;
	
	// Get this process info
	if (GetCurrentProcess(&psn) == noErr)
	{
		// Get the path to this process
		if (GetProcessBundleLocation(&psn, &ref_app) == noErr)
		{
			if (OpStatus::IsSuccess(OpFileUtils::ConvertFSRefToUniPath(&ref_app, path)))
			{
				// Append the path to the actual executable you can launch from on a command line
				return path->Append(UNI_L("/Contents/MacOS/Opera"));
			}
		}
	}
	
	return OpStatus::ERR;
}

OP_STATUS MacOpSystemInfo::SetAsDefaultBrowser()
{
	CFStringRef product_bundle_id = GetCFBundleID();

	LSSetDefaultRoleHandlerForContentType(CFSTR("public.html"), kLSRolesAll, product_bundle_id);
	LSSetDefaultRoleHandlerForContentType(CFSTR("public.xhtml"), kLSRolesAll, product_bundle_id);

	LSSetDefaultHandlerForURLScheme(CFSTR("http"), product_bundle_id);
	LSSetDefaultHandlerForURLScheme(CFSTR("https"), product_bundle_id);

	return OpStatus::OK;
}
#endif // QUICK_USE_DEFAULT_BROWSER_DIALOG

BOOL MacOpSystemInfo::IsPowerConnected()
{
	// FIXME: IMPLEMENTME
	return TRUE;
}

BYTE MacOpSystemInfo::GetBatteryCharge()
{
	// FIXME: IMPLEMENTME
	return 255;
}

BOOL MacOpSystemInfo::IsLowPowerState()
{
	// FIXME: IMPLEMENTME
	return FALSE;
}

void MacOpSystemInfo::SetEventShiftKeyState(BOOL inEvent, ShiftKeyState state)
{
	m_use_event_shiftkey_state = inEvent;
	m_event_shiftkey_state = state;
}

BOOL MacOpSystemInfo::GetEventShiftKeyState(ShiftKeyState &state)
{
	if (m_use_event_shiftkey_state)
		state = m_event_shiftkey_state;
	return m_use_event_shiftkey_state;
}

#ifdef OPSYSTEMINFO_CPU_FEATURES
unsigned int MacOpSystemInfo::GetCPUFeatures()
{
	unsigned int capabilities = CPU_FEATURES_NONE;
	unsigned int sse_capability = 0;
	size_t sse_size = sizeof(sse_capability);
	char* sse_opt[] = {"hw.optional.sse2", "hw.optional.sse3",
					   "hw.optional.supplementalsse3",
					   "hw.optional.sse4_1", "hw.optional.sse4_2"};
	int sse_flags[] = {CPU_FEATURES_IA32_SSE2, CPU_FEATURES_IA32_SSE3,
		               CPU_FEATURES_IA32_SSSE3, CPU_FEATURES_IA32_SSE4_1,
					   CPU_FEATURES_IA32_SSE4_2};

	for (unsigned int i = 0; i < sizeof(sse_flags)/sizeof(int); ++i)
	{
		sse_capability = 0;
		sse_size = sizeof(sse_capability);
		if (sysctlbyname(sse_opt[i], &sse_capability, &sse_size, NULL, 0) == 0)
		{
			capabilities |= sse_capability ? sse_flags[i] : CPU_FEATURES_NONE;
		}
	}

	return capabilities;
}
#endif
