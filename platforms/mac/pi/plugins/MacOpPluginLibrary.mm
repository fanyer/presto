/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/mac/pi/plugins/MacOpPluginLibrary.h"
#include "platforms/mac/util/macutils_mincore.h"
#include "platforms/mac/util/CTextConverter.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"

#define BOOL NSBOOL
#import <Foundation/NSRange.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSBundle.h>
#include <CoreFoundation/CFURL.h>
#undef BOOL

#if defined(_PLUGIN_SUPPORT_)

//////////////////////////////////////////////////////////////////////

OP_STATUS OpPluginLibrary::EnumerateLibraries(OtlList<LibraryPath>* out_library_paths,	const UniString& suggested_paths)
{
	// Set the Process name to be "Opera Plug-in Detector"
	UniString process_name;
	process_name.AppendConstData(UNI_L("Opera Plug-in Detector"));
	SetProcessName(process_name);
	
	// Note: the spaces around the inner template is required due to:
	// warning: ‘>>’ operator will be treated as two right angle brackets in C++0x [-Wc++0x-compat]
	OpAutoPtr< OtlCountedList<UniString> > plugin_path_list (suggested_paths.Split(':'));
	if (!plugin_path_list.get())
		return OpStatus::ERR_NO_MEMORY;
	
	for (OtlCountedList<UniString>::ConstRange plugin_path = plugin_path_list->ConstAll(); plugin_path; ++plugin_path)
	{
		MacOpPluginLibrary::ScanFolderForPlugins(*plugin_path, out_library_paths); 
	}
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS OpPluginLibrary::Create(OpPluginLibrary** out_library, const UniString& path)
{
	*out_library = OP_NEW(MacOpPluginLibrary, (path));
	if (*out_library == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = static_cast<MacOpPluginLibrary*>(*out_library)->Init(path);
	if (OpStatus::IsError(err))
		OP_DELETE(*out_library);
	
	return err;
}

//////////////////////////////////////////////////////////////////////

MacOpPluginLibrary::MacOpPluginLibrary(const UniString& path) : 
	m_mime_types_dic(NULL) 
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	, m_old_resource_read(FALSE) 
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
{
	op_memset(&m_functions, 0, sizeof(m_functions));
}

//////////////////////////////////////////////////////////////////////

MacOpPluginLibrary::~MacOpPluginLibrary() 
{
	// Unload the library
	if (m_plugin_bundle)
		CFRelease(m_plugin_bundle);
}



//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::Init(const UniString& path)
{
	m_plugin_path = path;
	m_plugin_bundle = MakePluginPackageBundle(path);
	if (!m_plugin_bundle)
		return OpStatus::ERR_NOT_SUPPORTED;
	m_plugin_nsbundle = (void *)[NSBundle bundleWithPath:[NSString stringWithCharacters:(const unichar*) m_plugin_path.Data(TRUE) length:m_plugin_path.Length()]];
	if (!m_plugin_nsbundle)
		return OpStatus::ERR_NOT_SUPPORTED;
	m_mime_types_dic = NULL;

	// Check if this plugin contains the new "WebPlugin" style resource information, if so read it.
	NSString* plugin_name = (NSString*)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"WebPluginName"];
	if (plugin_name && [plugin_name length] > 0)
	{
		SetUniString(m_name, (void *)plugin_name);
	
		// Check if this is a WebPluginMIMETypes or WebPluginMIMETypesFilename plugin
		NSDictionary *mime_types = (NSDictionary *)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"WebPluginMIMETypes"];
		if (mime_types)
			m_mime_types_dic = (void *)mime_types;
		else 
		{
			NSString *mime_types_file = (NSString *)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"WebPluginMIMETypesFilename"];
			if (mime_types_file && [mime_types_file length] > 0)
			{
				// Check the entry
				// Find /User/user-name/Library
				NSArray *library_folders;
				library_folders = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,  NSUserDomainMask, YES);
				if ([library_folders count] > 0)
				{
					NSString *plugin_mimetypes_filename = [library_folders objectAtIndex: 0];
					// Add on Preferences
					plugin_mimetypes_filename = [plugin_mimetypes_filename stringByAppendingPathComponent:@"Preferences"];
					
					// Add the name of the file
					plugin_mimetypes_filename = [plugin_mimetypes_filename stringByAppendingPathComponent:mime_types_file];
				
					// Check if the file exists
					if (![[NSFileManager defaultManager] fileExistsAtPath:plugin_mimetypes_filename])
					{
						// If not find the pointer to the create function inside the plugin itself and call it to create the file
						typedef void (*CreatePluginMIMETypesPreferences)(void);
						CreatePluginMIMETypesPreferences create_plugin_mime_type_preferences = (CreatePluginMIMETypesPreferences)CFBundleGetFunctionPointerForName(m_plugin_bundle, CFSTR("BP_CreatePluginMIMETypesPreferences"));
						if (create_plugin_mime_type_preferences)
							create_plugin_mime_type_preferences();
					}
						  
					// Now check a file was created or previously existed
					if ([[NSFileManager defaultManager] fileExistsAtPath:plugin_mimetypes_filename])
					{
						NSDictionary *mime_types_file = [NSDictionary dictionaryWithContentsOfFile:plugin_mimetypes_filename];
						if (mime_types_file)
						{
							NSDictionary *mime_types_in_file = (NSDictionary *)[mime_types_file objectForKey:@"WebPluginMIMETypes"];
							if (mime_types_in_file)
								m_mime_types_dic = (void *)mime_types_in_file;
						}
					}
				}
			}
		}
	}

#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	// Unable to load the resources from the Info.plist so load
	// them old-school from the resource
	if (!m_mime_types_dic)
		RETURN_IF_ERROR(ReadOldPluginResource());
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
	
	// Set all the function pointers (It's only written like this to make it easier to debug)
	if (!GetFunctionPointer(m_functions.NP_InitializeP, CFSTR("NP_Initialize")))
		return OpStatus::ERR_NOT_SUPPORTED;
	if (!GetFunctionPointer(m_functions.NP_GetEntryPointsP, CFSTR("NP_GetEntryPoints")))
		return OpStatus::ERR_NOT_SUPPORTED;
	if (!GetFunctionPointer(m_functions.NP_ShutdownP, CFSTR("NP_Shutdown")))
		return OpStatus::ERR_NOT_SUPPORTED;
	
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	return OpStatus::OK;
#else	
	return m_mime_types_dic ? OpStatus::OK : OpStatus::ERR_NOT_SUPPORTED;
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
}

//////////////////////////////////////////////////////////////////////

template<class T> BOOL MacOpPluginLibrary::GetFunctionPointer(T*& function, CFStringRef function_name)
{
	function = (T*)CFBundleGetFunctionPointerForName(m_plugin_bundle, function_name);
	return function ? TRUE : FALSE;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetName(UniString* out_name)
{
	*out_name = m_name;
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetDescription(UniString* out_description)
{
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	if (m_mime_types_dic)
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
	{
		NSString* plugin_desc = (NSString*)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"WebPluginDescription"];
		if (plugin_desc)
			SetUniString(m_desc, (void *)plugin_desc);
	}
	
	*out_description = m_desc;
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetVersion(UniString* out_version)
{
	NSString* plugin_version = (NSString *)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

	// Fallback version I found in the Real Player since it didn't have CFBundleShortVersionString
	if (!plugin_version)
		plugin_version = (NSString *)[(NSBundle *)m_plugin_nsbundle objectForInfoDictionaryKey:@"CFBundleVersion"];
	
	if (plugin_version)
	{
		SetUniString(*out_version, (void *)plugin_version);
		
		return OpStatus::OK;
	}
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetContentTypes(OtlList<UniString>* out_content_types)
{
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	if (!m_mime_types_dic)
	{
		if (!m_mime_content_types.IsEmpty())
		{
			for (OtlCountedList<UniString>::ConstRange mime_content_type = m_mime_content_types.ConstAll(); mime_content_type; ++mime_content_type)
				out_content_types->Append(*mime_content_type);
			return OpStatus::OK;
		}
	}
	else
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
	{
		NSString *mime_type;
		NSEnumerator *dic_enum = [(NSDictionary *)m_mime_types_dic keyEnumerator];
		while(mime_type = [dic_enum nextObject])
		{
			// DSK-362859: Block QuickTime plug-in from handling PDFs
			if (m_name.Equals(UNI_L("QuickTime Plug-in"),17) && [mime_type compare:@"application/pdf"] == NSOrderedSame)
				continue;

			UniString mime_type_unistring;
			SetUniString(mime_type_unistring, (void *)mime_type);
			out_content_types->Append(mime_type_unistring);
		}

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type)
{	
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	if (!m_mime_types_dic)
	{
		if (!m_mime_extensions.IsEmpty())
		{
			// Go though the two lists at the same time to find the matching content type and get its extensions.
			// This isn't the most optimal way to do this but who cares if only for old plugins of which there are basically none.
			OtlCountedList<UniString>::ConstRange mime_extension = m_mime_extensions.ConstAll();
			for (OtlCountedList<UniString>::ConstRange mime_content_type = m_mime_content_types.ConstAll(); mime_content_type; ++mime_content_type)
			{
				// Find the matching content type
				if (content_type == (*mime_content_type).Data(TRUE))
				{
					// Split the comma separated list into its sections and add them one at a time to the returned list
					OpAutoPtr< OtlCountedList<UniString> > extension_list ((*mime_extension).Split(','));
					if (!extension_list.get())
						return OpStatus::ERR_NO_MEMORY;

					for (OtlCountedList<UniString>::ConstRange extension = extension_list->ConstAll(); extension; ++extension)
					{
						out_extensions->Append(*extension);
					}
					
					return OpStatus::OK;
				}
				++mime_extension;
			}
		}
	}
	else
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
	{
		NSString *content;
		SetNSString((void **)&content, content_type);
		
		NSDictionary *ext_dic = (NSDictionary *)[(NSDictionary *)m_mime_types_dic objectForKey:content];
		if (ext_dic)
		{
			NSArray *ext_array = (NSArray *)[ext_dic objectForKey:@"WebPluginExtensions"];
			NSString *ext_info;
			NSEnumerator *dic_enum = [ext_array objectEnumerator];
			while(ext_info = [dic_enum nextObject])
			{
				UniString ext_unistring;
				SetUniString(ext_unistring, (void *)ext_info);
				out_extensions->Append(ext_unistring);
			}

			return OpStatus::OK;
		}
	}
	
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::GetContentTypeDescription(UniString* out_contenttype_description, const UniString& content_type)
{
	out_contenttype_description->Clear();
	
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	if (!m_mime_types_dic)
	{
		// There are no descriptions in the old resource mime type
	}
	else
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT
	{
		NSString *content;
		SetNSString((void **)&content, content_type);
		
		NSDictionary *ext_dic = (NSDictionary *)[(NSDictionary *)m_mime_types_dic objectForKey:content];
		if (ext_dic)
		{
			NSString *desc = (NSString *)[ext_dic objectForKey:@"WebPluginTypeDescription"];
			SetUniString(*out_contenttype_description, (void *)desc);
		}
	}
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::Load()
{
	// Load is automatically done by the first call to 
	// CFBundleGetFunctionPointerForName
	
	// Set the Process name to be "Opera Plug-in (<Plugin Name>)"
	UniString name, process_name;
	GetName(&name);
	process_name.AppendConstData(UNI_L("Opera Plug-in ("));
	process_name.Append(name);
	process_name.AppendConstData(UNI_L(")"));
	SetProcessName(process_name);
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::Unload()
{
	// Unloading is handled in the destructor by calling 
	// CFRelease on m_plugin_bundle

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

NPError MacOpPluginLibrary::Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs)
{
	OP_ASSERT(m_plugin_bundle);
	
	int error = m_functions.NP_InitializeP(browser_funcs, plugin_funcs);
	if (error == NPERR_NO_ERROR)
		error = m_functions.NP_GetEntryPointsP(plugin_funcs);

	return error;
}

//////////////////////////////////////////////////////////////////////

void MacOpPluginLibrary::Shutdown()
{
	OP_ASSERT(m_plugin_bundle);
	
	m_functions.NP_ShutdownP();
}

//////////////////////////////////////////////////////////////////////

OP_STATUS MacOpPluginLibrary::ScanFolderForPlugins(const UniString& importFolder, OtlList<LibraryPath>* out_library_paths) 
{
	NSString	*folder;
    id			importFile;
	
	SetNSString((void **)&folder, importFolder);
    NSDirectoryEnumerator *dir_enum =[[NSFileManager defaultManager] enumeratorAtPath:folder];
    while (importFile = [dir_enum nextObject])
    {
		NSString *fullPath = [folder stringByAppendingPathComponent:importFile];
		NSBOOL isDir = NO;
		Boolean isAlias;
		NSString* resolvedPath = ResolveAlias(fullPath, &isAlias);
		if (isAlias) fullPath = resolvedPath;

		// All plugins are folders so we can make this faster
        if ([[NSFileManager defaultManager] fileExistsAtPath:fullPath isDirectory:&isDir] == YES && isDir == YES)
		{
			UniString	plugin_file;
			UINT32		archs = 0;
			SetUniString(plugin_file, fullPath);

			// Check if it's a plugin
			if (MacOpPluginLibrary::IsPluginPackage(plugin_file, archs))
			{
				// Since plugins are folders skip all decendents now!
				[dir_enum skipDescendents];
				
				// Check for misbehaving plugins
				if ([[fullPath lastPathComponent] caseInsensitiveCompare:@"JavaPlugin2_NPAPI.plugin"] == NSOrderedSame && MacOpPluginLibrary::IsBadJavaPlugin())
					continue;
				// Adobe never behaves it seems
				if ([[fullPath lastPathComponent] caseInsensitiveCompare:@"AdobePDFViewerNPAPI.plugin"] == NSOrderedSame)
					continue;
				
				LibraryPath lib_path;
				lib_path.path = plugin_file;
#ifdef SIXTY_FOUR_BIT
				if ((archs & MAC_INTEL_64_ARCH) == MAC_INTEL_64_ARCH)
					lib_path.type = COMPONENT_PLUGIN;
				else if ((archs & MAC_INTEL_32_ARCH) == MAC_INTEL_32_ARCH)
					lib_path.type = COMPONENT_PLUGIN_MAC_32;
#else // 32 bit				
				if ((archs & MAC_INTEL_32_ARCH) == MAC_INTEL_32_ARCH)
					lib_path.type = COMPONENT_PLUGIN;
#endif //SIXTY_FOUR_BIT
				else
				{
					// No 32-bit or 64-bit archs in the plugin so just continue
					continue;
				}
				
				// Add to the list
				out_library_paths->Append(lib_path);
			}
		}
    }
	
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////

#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT

OP_STATUS MacOpPluginLibrary::ReadOldPluginResource()
{
	if (!m_old_resource_read)
	{
		// Reset the name
		m_name.Clear();

		short resFileID = CFBundleOpenBundleResourceMap(m_plugin_bundle);
		unsigned char *bufPtr = NULL;
		
		// Old resources
		short oldResFileID = CurResFile();
		
		UseResFile(resFileID);
		if (noErr == ResError())
		{
			// Resource 126 is the name and description
			Handle buf = Get1Resource('STR#', 126);
			if (buf)
			{
				HLock(buf);
				short strCount = *(short *) *buf;
				if (strCount >= 1)
				{
					bufPtr = (unsigned char *) *buf + 2;
					m_desc.Clear();
					UInt32 dest_len = gTextConverter->ConvertStringFromMacP(bufPtr, m_desc.GetAppendPtr(256), 256);
					m_desc.Trunc(dest_len);
				}
				if (strCount >= 2)
				{
					bufPtr += bufPtr[0] + 1;
					m_name.Clear();
					UInt32 dest_len = gTextConverter->ConvertStringFromMacP(bufPtr, m_name.GetAppendPtr(256), 256);
					m_name.Trunc(dest_len);
				}
				HUnlock(buf);
				ReleaseResource(buf);
			}

			// Resource 128 is the MIME Types
			buf = Get1Resource('STR#', 128);
			if (buf && *buf && *(short *) *buf)
			{
				int mimeCount = *(short *) *buf >> 1;
				HLock(buf);
				bufPtr = (unsigned char *) *buf + 2;
				UniString mime, exts;

				while (mimeCount--)
				{
					mime.Clear();
					UInt32 dest_len = gTextConverter->ConvertStringFromMacP(bufPtr, mime.GetAppendPtr(256), 256);
					mime.Trunc(dest_len);
					bufPtr += bufPtr[0] + 1;
					exts.Clear();
					dest_len = gTextConverter->ConvertStringFromMacP(bufPtr, exts.GetAppendPtr(256), 256);
					exts.Trunc(dest_len);
					bufPtr += bufPtr[0] + 1;
					
					// Add to the internal list
					m_mime_content_types.Append(mime);
					m_mime_extensions.Append(exts);
				}
				HUnlock(buf);
				ReleaseResource(buf);
			}
		}
		UseResFile(oldResFileID);
		CFBundleCloseBundleResourceMap(m_plugin_bundle, resFileID);

		m_old_resource_read = TRUE;
		
		// Make sure we got a name and some mime types or this plugin is broken somehow
		if (m_name.IsEmpty() || m_mime_content_types.IsEmpty())
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	
	return OpStatus::OK;
}

#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT

//////////////////////////////////////////////////////////////////////

CFBundleRef MacOpPluginLibrary::MakePluginPackageBundle(const UniString& path)
{
	CFStringRef		pluginURLString = NULL;
	CFURLRef		pluginURLRef = NULL;
	CFBundleRef		pluginBundleRef = NULL;
	UniString		local_unistring(path);
	
	pluginURLString = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)local_unistring.Data(TRUE), local_unistring.Length());
	if (pluginURLString)
	{
		pluginURLRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pluginURLString, kCFURLPOSIXPathStyle, false);
		if (pluginURLRef != NULL)
		{
			pluginBundleRef = CFBundleCreate(NULL, pluginURLRef);
			CFRelease(pluginURLRef);
		}
		CFRelease(pluginURLString);
	}
	
	return pluginBundleRef;
}

//////////////////////////////////////////////////////////////////////

Boolean MacOpPluginLibrary::IsPluginPackage(const UniString& path, UINT32 &archs)
{
	UniString local_unistring(path);
	
	CFBundleRef pluginBundle = MacOpPluginLibrary::MakePluginPackageBundle(local_unistring);
	if (pluginBundle != NULL)
	{
		OSType type;
		OSType creator;
		
		CFBundleGetPackageInfo(pluginBundle, &type, &creator);
		
		if (type == 'BRPL')
		{
			archs = 0;

			// Get tha architecture(s) this plugin supports
			CFArrayRef arch_list = CFBundleCopyExecutableArchitectures(pluginBundle);
			if (!arch_list) 
			{
				CFRelease(pluginBundle);
				return false;
			}
			
			for (CFIndex i = 0; i < CFArrayGetCount(arch_list); i++) 
			{
				CFNumberRef cfnumber_arch = static_cast<CFNumberRef>(CFArrayGetValueAtIndex(arch_list, i));
				
				int arch = 0;
				if (!CFNumberGetValue(cfnumber_arch, kCFNumberIntType, &arch)) 
				{
					CFRelease(arch_list);
					CFRelease(pluginBundle);
					return false;
				}
				
				if (arch == kCFBundleExecutableArchitectureI386)
					archs |= MAC_INTEL_32_ARCH;
				else if (arch == kCFBundleExecutableArchitectureX86_64)
					archs |= MAC_INTEL_64_ARCH;
			}
			CFRelease(arch_list);
			CFRelease(pluginBundle);
			
			return true;
		}
		CFRelease(pluginBundle);
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////

bool MacOpPluginLibrary::IsBadJavaPlugin()
{
	bool bad_java_plugin = false;
	SInt32 version = 0;

	Gestalt(gestaltSystemVersion, &version);
	if (version < 0x1070)
	{
		// This code is here to detect an early version of the Java Plugin 2 plugin, which didn't work well.
		// If this version is detected (JVMVersion 1.6.0_20 and less), we will avoid loading the plugin.
		// TODO: This code should probably be removed in the future.
		
		CFURLRef url_path = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Resources/Info.plist"), kCFURLPOSIXPathStyle, false);
		
		if (url_path)
		{
			// Read the XML file.
			CFDataRef xmlCFDataRef;
			SInt32 errorCode;
			
			if (CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, url_path, &xmlCFDataRef, NULL, NULL, &errorCode))
			{
				CFStringRef errorString;
				
				// Reconstitute the dictionary using the XML data.
				CFPropertyListRef myCFPropertyListRef = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, xmlCFDataRef, kCFPropertyListImmutable, &errorString);
				
				if (myCFPropertyListRef)
				{
					if (CFDictionaryGetTypeID() == CFGetTypeID(myCFPropertyListRef))
					{
						CFDictionaryRef java_vm;
						
						if (CFDictionaryGetValueIfPresent((CFDictionaryRef)myCFPropertyListRef, (const void*)CFSTR("JavaVM"), (const void**) &java_vm) && CFDictionaryGetTypeID() == CFGetTypeID(java_vm))
						{
							CFStringRef java_version;
							
							if (CFDictionaryGetValueIfPresent(java_vm, (const void*)CFSTR("JVMVersion"), (const void**) &java_version) && CFStringGetTypeID() == CFGetTypeID(java_version))
							{
								int len = CFStringGetLength(java_version);
								UniString ver;
								
								CFStringGetCharacters(java_version, CFRangeMake(0,len), (UniChar*)ver.GetAppendPtr(len));
								int minor_version = 0;
								if (uni_sscanf(ver.Data(TRUE), UNI_L("1.6.0_%d"), &minor_version) == 1)
								{
									if (minor_version < 20)
									{
										bad_java_plugin = true;
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

//////////////////////////////////////////////////////////////////////

NSString* MacOpPluginLibrary::ResolveAlias(NSString* filePath, Boolean* isAlias)
{
	NSString* resolvedPath = nil;
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)filePath, kCFURLPOSIXPathStyle, NO);
	*isAlias = FALSE;

	if (url != NULL)
	{
		FSRef fsRef;
		if (CFURLGetFSRef(url, &fsRef))
		{
			Boolean isFolder;

			// we are interested only in directories
			if (noErr == FSResolveAliasFile(&fsRef, YES, &isFolder, isAlias) && isAlias && isFolder)
			{
				CFURLRef resolvedURL = CFURLCreateFromFSRef(kCFAllocatorDefault, &fsRef);
				if (resolvedURL != NULL)
				{
					resolvedPath = (NSString*)CFURLCopyFileSystemPath(resolvedURL, kCFURLPOSIXPathStyle);
					CFRelease(resolvedURL);
				}
				else
				{
					*isAlias = FALSE;
				}
			}
		}
	}
	if (resolvedPath == nil) return nil;
	return [resolvedPath autorelease];
}

#endif // defined(_PLUGIN_SUPPORT_)

