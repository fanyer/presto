/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OP_PLUGIN_LIBRARY_H
#define WINDOWS_OP_PLUGIN_LIBRARY_H

#include "modules/pi/OpPluginLibrary.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"

typedef NPError (WINAPI* NP_GetEntryPoints_p)(void* pFuncs);
typedef NPError (WINAPI* NP_Initialize_p)(void* browserfunctions);
typedef void (WINAPI* NP_Shutdown_p)();

class WindowsOpPluginLibrary : public OpPluginLibrary
{
public:
	WindowsOpPluginLibrary() : m_dll(NULL) { };
	virtual ~WindowsOpPluginLibrary();

	// Implementing OpPluginLibrary
	static OP_STATUS	Create(OpPluginLibrary** out_library, const UniString& path);
	static OP_STATUS	EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths);

	virtual OP_STATUS	GetName(UniString* out_name) { *out_name = m_name; return OpStatus::OK; }
	virtual OP_STATUS	GetDescription(UniString* out_description) { *out_description = m_description; return OpStatus::OK; }
	virtual OP_STATUS	GetVersion(UniString* out_version) { *out_version = m_version; return OpStatus::OK; }
	virtual bool		GetEnabled() { return true; }
	virtual OP_STATUS	GetContentTypes(OtlList<UniString>* out_content_types);
	virtual OP_STATUS	GetContentTypeDescription(UniString* out_content_type_description, const UniString& content_type);
	virtual OP_STATUS	GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type);
	virtual OP_STATUS	Load();
	virtual NPError		Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs);
	virtual void		Shutdown();

private:
	enum PluginArch
	{
		ARCH_UNSUPPORTED,	///< plugin should be ignored
		ARCH_32,
		ARCH_64
	};

	static OP_STATUS	AddMozillaPlugins(HKEY root_key, OtlList<LibraryPath>* out_library_paths, REGSAM view_flags = 0);
	static OP_STATUS	ReadRegValue(HKEY key, const  uni_char* subkey_name, const uni_char* value_name, UniString &result, REGSAM view_flags = 0);
	static PluginArch	GetPluginArchitecture(const uni_char* plugin_path);
	static OP_STATUS	AddPluginToList(OtlList<LibraryPath>* out_library_paths, UniString& library_path);

	OP_STATUS			Init(const UniString& path);
	OP_STATUS			ParseContentType(UniString& mime_types, UniString& extensions, UniString& extension_descriptions);

	UniString			m_path;

	UniString			m_name;
	UniString			m_description;
	UniString			m_version;

	HMODULE				m_dll;

	struct NPPFunctions
	{
		NP_GetEntryPoints_p NP_GetEntryPoints;
		NP_Initialize_p NP_Initialize;
		NP_Shutdown_p NP_Shutdown;
	};

	struct ContentType
	{
		UniString content_type;
		UniString description;
		OtlList<UniString> extensions;
	};

	NPPFunctions		m_functions;
	OpStringHashTable<ContentType>
						m_content_types;
};

#endif //WINDOWS_OP_PLUGIN_LIBRARY_H
