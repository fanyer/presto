/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __MACOPPLUGINLIBRARY_H__
#define __MACOPPLUGINLIBRARY_H__

#if defined(_PLUGIN_SUPPORT_)

#include "modules/pi/OpPluginLibrary.h"
#define BOOL NSBOOL
#import <Foundation/NSString.h>
#undef BOOL

// Remove this define and the code inside it to get rid of
// the old-school plugin loading from the resource
// Currently this is only here for the VLC plugin, everything else
// loads with the new Info.plist code
#define MAC_OLD_RESOURCE_LOADING_SUPPORT

// Architecture defines
#define MAC_INTEL_32_ARCH  0x01
#define MAC_INTEL_64_ARCH  0x02

/**
 * A platform interface for loading, probing and initializing libraries
 * conforming to the Netscape Plug-in API (NPAPI).
 */
class MacOpPluginLibrary : public OpPluginLibrary
{
public:
	static OP_STATUS	EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths);

	static OP_STATUS	Create(OpPluginLibrary** out_library, const UniString& path);

	MacOpPluginLibrary(const UniString& path);
	~MacOpPluginLibrary();

	OP_STATUS			Init(const UniString& path);
	virtual OP_STATUS	GetName(UniString* out_name);
	virtual OP_STATUS	GetDescription(UniString* out_description);
	virtual OP_STATUS	GetVersion(UniString* out_version);
	virtual bool		GetEnabled() { return true; }
	virtual OP_STATUS	GetContentTypes(OtlList<UniString>* out_content_types);
	virtual OP_STATUS	GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type);
	virtual OP_STATUS	GetContentTypeDescription(UniString* out_contenttype_description, const UniString& content_type);
	virtual OP_STATUS	Load();
	virtual OP_STATUS	Unload();
	virtual NPError		Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs);
	virtual void		Shutdown();

	static OP_STATUS	ScanFolderForPlugins(const UniString &importFolder, OtlList<LibraryPath>* out_library_paths);

private:
	struct NPPFunctions
	{
		int  (*NP_InitializeP)(void*, void*);
		int  (*NP_GetEntryPointsP)(void*);
		void (*NP_ShutdownP)();
	};

	CFBundleRef			m_plugin_bundle;
	void				*m_plugin_nsbundle; // NSBundle to the plugin, easier for some queries
	UniString			m_plugin_path;
	
	UniString			m_name;
	UniString			m_desc;
	void				*m_mime_types_dic;		// Mime type dictionary either in the plugin Info.plist or com.apple... file for web plugins
												// This is also the flag used to say that the plugin name/desc/version/mime types were all found
	NPPFunctions		m_functions;			// Plugin function pointers
	
#ifdef MAC_OLD_RESOURCE_LOADING_SUPPORT
	BOOL				m_old_resource_read;	// Set once the old plugin data is read
	OtlList<UniString>	m_mime_content_types;
	OtlList<UniString>	m_mime_extensions;
	OP_STATUS			ReadOldPluginResource();
#endif // MAC_OLD_RESOURCE_LOADING_SUPPORT

	template<class T> inline BOOL GetFunctionPointer(T*& function, CFStringRef function_name);

	static CFBundleRef	MakePluginPackageBundle(const UniString& path);
	static Boolean		IsPluginPackage(const UniString& path, UINT32 &archs);
	static bool			IsBadJavaPlugin();
	/**
	 *  Function for resolving Mac Aliases
	 *  @param filePath path to resolve
	 *  @param isAlias will be set to TRUE if filePath was alias and target is dir, FALSE otherwise
	 *  @return resolved path
	 */
	static NSString*    ResolveAlias(NSString* filePath, Boolean* isAlias);
};

#endif // defined(_PLUGIN_SUPPORT_)

#endif // __MACOPPLUGINLIBRARY_H__
