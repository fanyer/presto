/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UNIX_OP_PLUGIN_LIBRARY_H
#define UNIX_OP_PLUGIN_LIBRARY_H

#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/pi/OpPluginLibrary.h"

class UnixOpPluginLibrary : public OpPluginLibrary
{
public:
	~UnixOpPluginLibrary();
	OP_STATUS Init(const char* path);

	// Implementing OpPluginLibrary
	static OP_STATUS Create(OpPluginLibrary** out_library, const UniString& path);
	static OP_STATUS EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths);

	virtual OP_STATUS GetName(UniString* out_name) { return GetStringValue(NPPVpluginNameString, *out_name); }
	virtual OP_STATUS GetDescription(UniString* out_description) { return GetStringValue(NPPVpluginDescriptionString, *out_description); }
	virtual OP_STATUS GetVersion(UniString* out_version) { out_version->Clear(); return OpStatus::OK; }
	virtual bool GetEnabled() { return true; }
	virtual OP_STATUS GetContentTypes(OtlList<UniString>* out_content_types);
	virtual OP_STATUS GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type);
	virtual OP_STATUS GetContentTypeDescription(UniString* out_contenttype_description, const UniString& content_type) { out_contenttype_description->Clear(); return OpStatus::OK; }
	virtual OP_STATUS Load() { return OpStatus::OK; /* loaded already in initialization */ }
	virtual NPError Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs);
	virtual void Shutdown();
	virtual int GetValue(void* instance, int variable, void* value);

private:
	static short WrapperForEventsToFlash(NPP instance, void* event);

	static OP_STATUS GetComponentType(const char* dirpath, const char* filename, OpComponentType& type);

	UnixOpPluginLibrary();
	template<class T> inline OP_STATUS LoadFunction(T*& function, const char* symbol);
	OP_STATUS GetStringValue(NPPVariable variable, UniString& value);
	OP_STATUS ParseMimeDescription();
	OP_STATUS ParseExtensions(OtlList<UniString>& extensions, const char* string, const char* string_end);

	struct NPPFunctions
	{
		NPError (*NP_GetValue)(void* instance, NPPVariable variable, void* value);
		const char* (*NP_GetMIMEDescription)();
		NPError (*NP_Initialize)(void* browserfunctions, void* pluginfunctions);
		void (*NP_Shutdown)();
	};

	struct ContentType
	{
		UniString content_type;
		UniString description;
		OtlList<UniString> extensions;
	};

	void* m_handle;
	NPPFunctions m_functions;
	OpStringHashTable<ContentType> m_content_types;
	static NPP_HandleEventProcPtr s_orig_plugin_funcs_event;
	bool m_is_flash;
};

#endif // UNIX_OP_PLUGIN_LIBRARY_H
