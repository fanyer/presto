/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifndef NS4P_COMPONENT_PLUGINS

#include "modules/pi/OpPluginDetectionListener.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"

#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/macutils.h"

// MacHacksIncluded: Mac Hacks

#ifdef _PLUGIN_SUPPORT_

CFBundleRef CreatePluginPackageBundle(const FSRef *inFile)
{
	CFURLRef		pluginURLRef = NULL;
	CFBundleRef		pluginBundleRef = NULL;

	pluginURLRef = CFURLCreateFromFSRef(NULL, inFile);
	if (pluginURLRef != NULL)
	{
		pluginBundleRef = CFBundleCreate(NULL, pluginURLRef);
		CFRelease(pluginURLRef);
	}

	return pluginBundleRef;
}

Boolean IsPluginPackage(const FSRef *inFile)
{
	UInt8 path8[MAX_PATH+1];
	OSStatus ret = FSRefMakePath(inFile, path8, MAX_PATH+1);
	if (ret != noErr)
		return false;

	CFBundleRef pluginBundle = CreatePluginPackageBundle(inFile);
	if(pluginBundle != NULL)
	{
		OSType type;
		OSType creator;

		CFBundleGetPackageInfo(pluginBundle, &type, &creator);

		CFRelease(pluginBundle);

		if(type == 'BRPL')
		{
			return true;
		}
	}

	return false;
}

class PluginCallbackContext
{
public:
	PluginCallbackContext(OpPluginDetectionListener* listener, uni_char* name, uni_char* location)
	: m_listener(listener),
	  m_name(name),
	  m_location(location),
	  m_token(NULL)
	{
	}

public:
	OpPluginDetectionListener*	m_listener;
	uni_char*					m_name;
	uni_char*					m_location;
	void*						m_token;
};

void InsertPluginDictionary(const void *key, const void *value, void *context)
{
	CFStringRef mime = (CFStringRef) key;

	if(CFGetTypeID(mime) != CFStringGetTypeID())
		return;

	CFDictionaryRef data = (CFDictionaryRef) value;

	if(CFGetTypeID(data) != CFDictionaryGetTypeID())
		return;

	CFArrayRef exts = (CFArrayRef) CFDictionaryGetValue(data, CFSTR("WebPluginExtensions"));

	if(exts && CFGetTypeID(exts) != CFArrayGetTypeID())
		return;

	CFStringRef desc = (CFStringRef) CFDictionaryGetValue(data, CFSTR("WebPluginTypeDescription"));

	if(desc && CFGetTypeID(desc) != CFStringGetTypeID())
		return;

	CFBooleanRef enable = (CFBooleanRef) CFDictionaryGetValue(data, CFSTR("WebPluginTypeEnabled"));

	if(enable && CFGetTypeID(enable) != CFBooleanGetTypeID())
		return;

	PluginCallbackContext *ctxt = (PluginCallbackContext *)context;
	if (!enable || CFBooleanGetValue(enable))
	{
		OpString mime_type;
		OpString extension, extensions, description;
		int i, count;
		if(exts)
		{
			count = CFArrayGetCount(exts);
			for(i=0; i<count; i++)
			{
				CFStringRef ext = (CFStringRef)CFArrayGetValueAtIndex(exts, i);

				if(CFGetTypeID(ext) != CFStringGetTypeID())
					continue;

				SetOpString(extension, ext);
				if (i)
					extensions.Append(UNI_L(","));
				extensions.Append(extension);
			}
		}
		if (desc)
		{
			SetOpString(description, desc);
		}
		if (SetOpString(mime_type, mime))
		{
			if(ctxt->m_token)
			{
				if (OpStatus::IsSuccess(ctxt->m_listener->OnAddContentType(ctxt->m_token, mime_type)))
				{
					OpStatus::Ignore(ctxt->m_listener->OnAddExtensions(ctxt->m_token, mime_type, extensions, description));
				}
			}
		}
	}
}

void InsertWebPluginPackage(OpPluginDetectionListener* listener, CFBundleRef bundle)
{
	OpString fileLocation;
	OpString pluginName;
	OpString pluginDesc;
	OpString pluginVersion;

	CFURLRef pluginURLRef = CFBundleCopyBundleURL(bundle);
	if(pluginURLRef)
	{
		CFStringRef str = CFURLCopyFileSystemPath(pluginURLRef, kCFURLPOSIXPathStyle);
		if(str)
		{
			SetOpString(fileLocation, str);
			CFRelease(str);
		}
		CFRelease(pluginURLRef);
	}


	CFURLRef supportFilesCFURLRef = CFBundleCopySupportFilesDirectoryURL(bundle);

	CFURLRef versionURLRef = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, supportFilesCFURLRef, CFSTR("Info.plist"),false);

	CFRelease(supportFilesCFURLRef);

	// Read the XML file.
	CFDataRef xmlCFDataRef;
	SInt32 errorCode;

	if(!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, versionURLRef, &xmlCFDataRef, NULL, NULL, &errorCode))
	{
		CFRelease(versionURLRef);
		return;
	}
	CFStringRef errorString;

	// Reconstitute the dictionary using the XML data.
	CFPropertyListRef myCFPropertyListRef = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, xmlCFDataRef, kCFPropertyListImmutable, &errorString);
	if(myCFPropertyListRef && CFDictionaryGetTypeID() == CFGetTypeID(myCFPropertyListRef))
	{
		CFStringRef plugin_name;

		if(CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("WebPluginName"), (const void**) &plugin_name))
			SetOpString(pluginName, plugin_name);

		CFStringRef plugin_description;

		if(CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("WebPluginDescription"), (const void**) &plugin_description))
			if(plugin_description && CFGetTypeID(plugin_description) == CFStringGetTypeID())
				SetOpString(pluginDesc, plugin_description);
	}

	CFStringRef plugin_version;
	if(CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("CFBundleShortVersionString"), (const void**) &plugin_version))
		if(plugin_version && CFGetTypeID(plugin_version) == CFStringGetTypeID())
			SetOpString(pluginVersion, plugin_version);

	CFDictionaryRef mime_list;
	if(myCFPropertyListRef && CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("WebPluginMIMETypes"), (const void**) &mime_list))
	{
		if (CFGetTypeID(mime_list) == CFDictionaryGetTypeID())
		{
			PluginCallbackContext context(listener, pluginName, fileLocation.CStr());

			if(OpStatus::IsSuccess(listener->OnPrepareNewPlugin(OpStringC(context.m_location), OpStringC(context.m_name), OpStringC(pluginDesc), OpStringC(pluginVersion), COMPONENT_SINGLETON, TRUE, context.m_token)))
			{
				CFDictionaryApplyFunction(mime_list, InsertPluginDictionary, &context);
				OpStatus::Ignore(listener->OnCommitPreparedPlugin(context.m_token));
			}
		}
	}
	if (myCFPropertyListRef)
		CFRelease(myCFPropertyListRef);
	if (errorString)
		CFRelease(errorString);
	CFRelease(xmlCFDataRef);
	CFRelease(versionURLRef);
}

bool IsBadJavaPlugin()
{
	bool bad_java_plugin = false;

	if(GetOSVersion() < 0x1070)
	{
		// This code is here to detect an early version of the Java Plugin 2 plugin, which didn't work well.
		// If this version is detected (JVMVersion 1.6.0_20 and less), we will avoid loading the plugin.
		// TODO: This code should probably be removed in the future.

		CFURLRef url_path = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Resources/Info.plist"), kCFURLPOSIXPathStyle, false);

		if(url_path)
		{
			// Read the XML file.
			CFDataRef xmlCFDataRef;
			SInt32 errorCode;

			if(CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, url_path, &xmlCFDataRef, NULL, NULL, &errorCode))
			{
				CFStringRef errorString;

				// Reconstitute the dictionary using the XML data.
				CFPropertyListRef myCFPropertyListRef = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, xmlCFDataRef, kCFPropertyListImmutable, &errorString);

				if(myCFPropertyListRef)
				{
					if(CFDictionaryGetTypeID() == CFGetTypeID(myCFPropertyListRef))
					{
						CFDictionaryRef java_vm;

						if(CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("JavaVM"), (const void**) &java_vm) && CFDictionaryGetTypeID() == CFGetTypeID(java_vm))
						{
							CFStringRef java_version;

							if(CFDictionaryGetValueIfPresent(java_vm, (const void*)CFSTR("JVMVersion"), (const void**) &java_version) && CFStringGetTypeID() == CFGetTypeID(java_version))
							{
								int len = CFStringGetLength(java_version);
								OpString ver;
								if(ver.Reserve(len + 1))
								{
									CFStringGetCharacters(java_version, CFRangeMake(0,len), (UniChar*)ver.CStr());
									ver[len] = 0;
									int minor_version = 0;
									if(uni_sscanf(ver.CStr(), UNI_L("1.6.0_%d"), &minor_version) == 1)
									{
										if(minor_version < 20)
										{
											bad_java_plugin = true;
										}
									}
								}
							}
						}
					}
					CFRelease(myCFPropertyListRef);
				}
				CFRelease(xmlCFDataRef);
			}
			CFRelease(url_path);
		}
	}
	return bad_java_plugin;
}

void InsertPluginPackage(OpPluginDetectionListener* listener, CFBundleRef pluginBundle)
{
	short 		resFileID;

	int mimeCount;
	OpString fileLocation;
	uni_char fileName[256];
	uni_char plugName[256];
	uni_char desc[256];
	uni_char mime[256];
	uni_char exts[256];
	Handle buf;
	unsigned char *bufPtr = NULL;
	short strCount;

	CFURLRef pluginURLRef = CFBundleCopyBundleURL(pluginBundle);
	if(pluginURLRef)
	{
		CFStringRef str = CFURLCopyFileSystemPath(pluginURLRef, kCFURLPOSIXPathStyle);
		if(str)
		{
			SetOpString(fileLocation, str);
			CFRelease(str);
		}
		CFRelease(pluginURLRef);
	}

	BOOL java_plugin = fileLocation.HasContent() && uni_strstr(fileLocation.CStr(), UNI_L("JavaPlugin2_NPAPI.plugin"));
	if (java_plugin && IsBadJavaPlugin())
		return;

	OpString pluginVersion;
	if (CFStringRef plugin_version = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(pluginBundle, CFSTR("CFBundleShortVersionString")))
		if (plugin_version && CFGetTypeID(plugin_version) == CFStringGetTypeID())
			SetOpString(pluginVersion, plugin_version);

	resFileID = CFBundleOpenBundleResourceMap(pluginBundle);

	if (resFileID > 0)
	{
		short oldResFileID = CurResFile();

		UseResFile(resFileID);
		if (noErr == ResError())
		{
			//it's a resource file
			//if (IsGoodFragment(resFileID))
			{
				desc[0] = '\0';
				plugName[0] = '\0';
				buf = Get1Resource('STR#', 126);
				if (buf)
				{
					HLock(buf);
					strCount = *(short *) *buf;
					if (strCount >= 1)
					{
						bufPtr = (unsigned char *) *buf + 2;
						gTextConverter->ConvertStringFromMacP(bufPtr, desc, 256);
					}
					if (strCount >= 2)
					{
						bufPtr += bufPtr[0] + 1;
						gTextConverter->ConvertStringFromMacP(bufPtr, plugName, 256);
					}
					HUnlock(buf);
					ReleaseResource(buf);
				}

				buf = Get1Resource('STR#', 128);
				if (buf && *buf && *(short *) *buf)
				{
					BOOL mime_types_parsed = FALSE;
					// Quicktime has an extra function that hold extra mimetypes we need to look in
					// This should not exist for other plugins if NP_GetMIMEDescriptionUPP does exist we should get the
					// mime types from here rather than from the resource
					BOOL quicktime_plugin = uni_strcmp(fileLocation.CStr(), UNI_L("/Library/Internet Plug-Ins/QuickTime Plugin.plugin")) == 0;
					if(quicktime_plugin)
					{
						CFDictionaryRef mime_list = (CFDictionaryRef) CFPreferencesCopyAppValue(CFSTR("WebPluginMIMETypes"), CFSTR("com.apple.quicktime.plugin.preferences"));
						if (mime_list)
						{
							if (CFGetTypeID(mime_list) == CFDictionaryGetTypeID())
							{
								PluginCallbackContext context(listener, (*plugName) ? plugName : fileName, fileLocation.CStr());
								if(OpStatus::IsSuccess(listener->OnPrepareNewPlugin(OpStringC(context.m_location), OpStringC(context.m_name), OpStringC(NULL), OpStringC(pluginVersion), COMPONENT_SINGLETON, TRUE, context.m_token)))
								{
									CFDictionaryApplyFunction(mime_list, InsertPluginDictionary, &context);
									OpStatus::Ignore(listener->OnCommitPreparedPlugin(context.m_token));
								}
								mime_types_parsed = true;
							}
							CFRelease(mime_list);
						}
					}


					// If NP_GetMIMEDescriptionUPP failed to get the mimetypes parse the resource
					if (!mime_types_parsed)
					{
						mimeCount = *(short *) *buf >> 1;
						HLock(buf);
						bufPtr = (unsigned char *) *buf + 2;
						void *token;
						if(OpStatus::IsSuccess(listener->OnPrepareNewPlugin(OpStringC(fileLocation), OpStringC((*plugName) ? plugName : fileName), OpStringC(desc), OpStringC(pluginVersion), COMPONENT_SINGLETON, TRUE, token)))
						{
							while (mimeCount--)
							{
								gTextConverter->ConvertStringFromMacP(bufPtr, mime, 256);
								bufPtr += bufPtr[0] + 1;
								gTextConverter->ConvertStringFromMacP(bufPtr, exts, 256);
								bufPtr += bufPtr[0] + 1;
								if(OpStatus::IsSuccess(listener->OnAddContentType(token, mime)))
								{
									OpStatus::Ignore(listener->OnAddExtensions(token, mime, exts));
								}
							}
							OpStatus::Ignore(listener->OnCommitPreparedPlugin(token));
						}
					}

					HUnlock(buf);
					ReleaseResource(buf);
				}
			}

		}
		UseResFile(oldResFileID);
		CFBundleCloseBundleResourceMap(pluginBundle, resFileID);
	}
	else
		InsertWebPluginPackage(listener, pluginBundle);
}

class ScanFolderForPlugins {
	OpPluginDetectionListener* listener;
	Boolean m_recurse;
public:
	explicit ScanFolderForPlugins(OpPluginDetectionListener* pl, Boolean recurse = true)
	: listener(pl), m_recurse(recurse)
	{ }
	void operator()(ItemCount count, const FSCatalogInfo* inCatInfo, const FSRef* ref) {
		Boolean isFolder, wasAlias;
		if (noErr == FSResolveAliasFile(const_cast<FSRef*>(ref), NULL, &isFolder, &wasAlias))
		{
			BOOL ignore_plugin = FALSE;
			OpString name;
			OpFileUtils::ConvertFSRefToUniPath(ref, &name);
			if(g_pcapp->IsPluginToBeIgnored(name))
			{
				ignore_plugin = TRUE;
			}
			else
			{
				// Also check name without suffix.
				int index = name.FindLastOf('.');
				if(index >= 0)
				{
					name[index] = 0;
					if(g_pcapp->IsPluginToBeIgnored(name))
					{
						ignore_plugin = TRUE;
					}
				}
			}

			if(!ignore_plugin)
			{
				//We found another thing in the Plugins folder
				if (isFolder)
				{
					// This "folder" might just be a package...
					if(IsPluginPackage(ref))
					{
						BOOL universal_plugin = TRUE;
/*
						MacHack: Ignore Flash Player Enabler plugin on Intel Macs

						Ignores the Flash Player Enabler plugin on Intel Macs.

						We need to skip this plugin on Intel Mac's because it has been built for PPC only and thus
						when it is picked up the real flash player is ignored and doesn't run. It also seems to serve no
						purpose as using the Flash Player plugin directly works fine.
*/

						if (uni_strstr(uni_strupr(name), UNI_L("FLASH PLAYER ENABLER.PLUGIN")))
						{
							universal_plugin = FALSE;
						}
						if (universal_plugin)
						{
							CFBundleRef plugin_bundle = CreatePluginPackageBundle(ref);
							if(plugin_bundle)
							{
								InsertPluginPackage(listener, plugin_bundle);
								CFRelease(plugin_bundle);
							}
						}
					}
					else
					{
						//It's a folder...
						if (m_recurse)
						{
							MacForEachItemInFolder(*ref, ScanFolderForPlugins(listener, false));
						}
					}
				}
				else
				{
					//It's a file... Not supported!
				}
			}
		}
	}
};

OP_STATUS DetectMacPluginViewers(const OpStringC& suggested_plugin_paths, OpPluginDetectionListener* listener)
{
	FSRef plugin_folder;
	// Apple TN 2020 says we should use kLocalDomain under OS X.
	if (noErr == ::FSFindFolder( kLocalDomain, kInternetPlugInFolderType, kCreateFolder, &plugin_folder))
	{
		MacForEachItemInFolder(plugin_folder, ScanFolderForPlugins(listener));
	}

	// Bug 187806 says we should also use user domain
	if (noErr == ::FSFindFolder( kUserDomain, kInternetPlugInFolderType, kCreateFolder, &plugin_folder))
	{
		MacForEachItemInFolder(plugin_folder, ScanFolderForPlugins(listener));
	}

	return OpStatus::OK;
}

#endif //_PLUGIN_SUPPORT_
#endif // NS4P_COMPONENT_PLUGINS
