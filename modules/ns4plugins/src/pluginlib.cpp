/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/ns4plugins/src/pluginlibhandler.h"
#include "modules/ns4plugins/src/pluginglobals.h"
#include "modules/ns4plugins/src/pluginhandler.h"

#ifndef HAS_COMPLEX_GLOBALS
#include "modules/ns4plugins/ns4plugins_module.h"
#endif // !HAS_COMPLEX_GLOBALS


#include "modules/util/str.h"
#include "modules/viewers/viewers.h"

#ifdef ANDROID
#include "platforms/android/common/native/include/jni_env_helper.h"
#endif

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/hardcore/component/Messages.h"
#include "modules/ns4plugins/src/pluginfunctions.h"
#include "modules/ns4plugins/src/plugininstance.h"
#include "modules/opdata/UniStringUTF8.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"

PluginLib::PluginLib()
	: m_reference_count(0)
	, m_last_access(0)
	, m_saved_data(NULL)
	, m_component(NULL)
	, m_initialized(false)
{
}

PluginLib::~PluginLib()
{
	if (m_component)
	{
		if (Initialized())
		{
			m_component->SendMessage(OpPluginShutdownMessage::Create());
			m_initialized = false;
		}

		g_pluginhandler->DelistChannel(m_component);
		OP_DELETE(m_component);
	}

	/* There are some plugins which prevent deleting a cache file after the window
	 * is closed as they retain lock on the file for X duration even after a call
	 * to NPP_Destroy(). To tackle this issue, unloading of a URL is defered until
	 * the in-use plugin lib is unloaded and that way it can be ensured the file
	 * in unlocked. */
	for (unsigned int i = 0; i < m_urls_to_unload.GetCount(); i++)
		m_urls_to_unload.Get(i)->GetURL().Unload();
}

OP_STATUS PluginLib::InitLibFromDll(OpDLL*, const uni_char*, BOOL)
{
	OP_ASSERT(!"Component plug-ins cannot be loaded from OpDLL objects.");
	return OpStatus::ERR;
}

OP_STATUS PluginLib::LoadLib(const uni_char* path, OpComponentType component_type)
{
	/* Store path and type. */
	RETURN_IF_ERROR(m_path.SetCopyData(path));
	m_component_type = component_type;

	/* Create plug-in component. */
	RETURN_IF_ERROR(g_opera->RequestComponent(&m_component, m_component_type));
	RETURN_IF_ERROR(m_component->AddMessageListener(this));
	RETURN_IF_ERROR(g_pluginhandler->WhitelistChannel(m_component));

	/* And return immediately, as the actual load is postponed until Init(). One
	 * reason for this is that platform implementations already fold Load and Init
	 * into one operation, so the next iteration of the PI will probably do the
	 * same. More importantly, this method is called during Reflow, while Init()
	 * isn't, so we're better off waiting until we sit on a clean, sturdy stack
	 * (CT-1710). */
	return OpStatus::OK;
}

OP_STATUS PluginLib::Load()
{
	if (!ok())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	/* Perform synchronous load request with a timeout. */
	OpSyncMessenger sync(m_component);
	OP_STATUS s = sync.SendMessage(OpPluginLoadMessage::Create(m_path), false, g_pcapp->GetIntegerPref(PrefsCollectionApp::PluginSyncTimeout) * 1000);
	m_component = sync.TakePeer();

	if (OpStatus::IsSuccess(s))
		s = OpPluginLoadResponseMessage::Cast(sync.GetResponse())->GetStatus();

	/* If loading failed, we either got an error back or the component died.
	   In the former case we need to delist the channel, otherwise it has
	   already been done by ProcessMessages(). */
	if (OpStatus::IsError(s) && m_component)
	{
		g_pluginhandler->DelistChannel(m_component);
		OP_DELETE(m_component);
		m_component = NULL;
	}

	return s;
}

NPError PluginLib::Init()
{
	if (OpStatus::IsError(Load()))
		return NPERR_GENERIC_ERROR;

	OP_ASSERT(!Initialized());

	PluginSyncMessenger sync(g_opera->CreateChannel(m_component->GetDestination()));
	OP_STATUS s = sync.SendMessage(OpPluginInitializeMessage::Create());

	NPError error = NPERR_GENERIC_ERROR;
	if (OpStatus::IsSuccess(s))
	{
		error = OpPluginInitializeResponseMessage::Cast(sync.GetResponse())->GetNpError();
		if (error == NPERR_NO_ERROR)
		{
			PluginFunctions::InitializeFunctionTable(&m_plugin_functions);
			PluginFunctions::InitializeClass(&m_plugin_object_class);
			m_initialized = true;
		}
	}

	return error;
}

OP_STATUS PluginLib::AddURLIntoCleanupTaskList(URL* url)
{
	for (unsigned int i = 0; i < m_urls_to_unload.GetCount(); i++)
		if (m_urls_to_unload.Get(i)->GetURL() == *url)
			return OpStatus::OK;

	if (URL_InUse *url_in_use = OP_NEW(URL_InUse, (*url)))
		return m_urls_to_unload.Add(url_in_use);

	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS PluginLib::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
		case OpPluginLoadResponseMessage::Type:
		case OpPluginInitializeResponseMessage::Type:
		case OpStatusMessage::Type:
			/* Messages handled by OpSyncMessenger in LoadLib() and Init(). */
			break;

		case OpPeerDisconnectedMessage::Type:
			if (Initialized())
				if (PluginLibHandler* library_handler = g_pluginhandler->GetPluginLibHandler())
				{
					/* The following call will delete 'this' .. */
					library_handler->DestroyLib(m_path.Data(TRUE), true, message->GetSrc());

					/* .. so return early to prevent attempts to access released memory. */
					return OpStatus::OK;
				}
			break;

		case OpPluginGetValueMessage::Type:
			return PluginInstance::HandlePluginGetValueMessage(NULL, OpPluginGetValueMessage::Cast(message));

		case OpPluginGetStringIdentifiersMessage::Type:
			return OnGetStringIdentifiersMessage(OpPluginGetStringIdentifiersMessage::Cast(message));

		case OpPluginGetIntIdentifierMessage::Type:
			return OnGetIntIdentifierMessage(OpPluginGetIntIdentifierMessage::Cast(message));

		case OpPluginLookupIdentifierMessage::Type:
			return OnLookupIdentifierMessage(OpPluginLookupIdentifierMessage::Cast(message));

		case OpPluginRetainObjectMessage::Type:
			return OnPluginRetainObjectMessage(OpPluginRetainObjectMessage::Cast(message));

		case OpPluginReleaseObjectMessage::Type:
			return OnPluginReleaseObjectMessage(OpPluginReleaseObjectMessage::Cast(message));

		case OpPluginUserAgentMessage::Type:
			return OnPluginUserAgentMessage(OpPluginUserAgentMessage::Cast(message));

		case OpPluginReloadMessage::Type:
			return OnPluginReloadMessage(OpPluginReloadMessage::Cast(message));

		case OpPluginErrorMessage::Type:
			break;

		case OpPluginExceptionMessage::Type:
			return OnPluginExceptionMessage(OpPluginExceptionMessage::Cast(message));

		default:
			OP_ASSERT(!"PluginLib received unknown message!");
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS
PluginLib::OnGetStringIdentifiersMessage(OpPluginGetStringIdentifiersMessage* message)
{
	OpAutoPtr<OpPluginIdentifiersMessage> response(OpPluginIdentifiersMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	for (unsigned int i = 0; i < message->GetNames().GetCount(); i++)
	{
		NPString nps;
		RETURN_IF_ERROR(PluginInstance::SetNPString(&nps, message->GetNames().Get(i)));

		NPIdentifier identifier = NPN_GetStringIdentifier(nps.UTF8Characters);
		NPN_MemFree(const_cast<NPUTF8*>(nps.UTF8Characters));

		typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;
		PluginIdentifier* id = OP_NEW(PluginIdentifier, ());
		RETURN_OOM_IF_NULL(id);

		id->SetIdentifier(reinterpret_cast<UINT64>(identifier));
		id->SetIsString(true);

		OP_STATUS status = response->GetIdentifiersRef().Add(id);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(id);
			return status;
		}
	}

	return message->Reply(response.release());
}

OP_STATUS
PluginLib::OnGetIntIdentifierMessage(OpPluginGetIntIdentifierMessage* message)
{
	OpPluginIdentifiersMessage* response = OpPluginIdentifiersMessage::Create();
	RETURN_OOM_IF_NULL(response);

	if (NPIdentifier identifier = NPN_GetIntIdentifier(message->GetNumber()))
	{
		typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;
		PluginIdentifier* id = OP_NEW(PluginIdentifier, ());
		if (!id)
		{
			OP_DELETE(response);
			return OpStatus::ERR_NO_MEMORY;
		}

		id->SetIdentifier(reinterpret_cast<UINT64>(identifier));
		id->SetIsString(false);
		id->SetIntValue(message->GetNumber());

		OP_STATUS status = response->GetIdentifiersRef().Add(id);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(id);
			return status;
		}
	}

	return message->Reply(response);
}

OP_STATUS
PluginLib::OnLookupIdentifierMessage(OpPluginLookupIdentifierMessage* message)
{
	NPUTF8* s = NPN_UTF8FromIdentifier(reinterpret_cast<NPIdentifier>(message->GetIdentifier().GetIdentifier()));
	if (!s)
		return OpStatus::ERR;

	OpPluginLookupIdentifierResponseMessage* response = OpPluginLookupIdentifierResponseMessage::Create();
	RETURN_OOM_IF_NULL(response);

	int len = from_utf8(NULL, s, INT_MAX);
	if (len > 0)
	{
		UniString response_value;
		uni_char* us = response_value.GetAppendPtr(len);
		if (!us)
		{
			OP_DELETE(response);
			return OpStatus::ERR_NO_MEMORY;
		}

		from_utf8(us, s, len);
		response->SetStringValue(response_value);
	}

	return message->Reply(response);
}

OP_STATUS
PluginLib::OnPluginRetainObjectMessage(OpPluginRetainObjectMessage* message)
{
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());
	NPN_RetainObject(np_object);

	return OpStatus::OK;
}

OP_STATUS
PluginLib::OnPluginReleaseObjectMessage(OpPluginReleaseObjectMessage* message)
{
	OpPluginObjectStateMessage* response = OpPluginObjectStateMessage::Create();
	RETURN_OOM_IF_NULL(response);

	/* Set message values before calling NPN_ReleaseObject, as the object may
	 * be released by the call. */
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());
	response->GetObjectRef().SetObject(reinterpret_cast<UINT64>(np_object));
	response->GetObjectRef().SetReferenceCount(np_object->referenceCount - 1);

	NPN_ReleaseObject(np_object);

	return message->Reply(response);
}

OP_STATUS
PluginLib::OnPluginUserAgentMessage(OpPluginUserAgentMessage* message)
{
	const char* user_agent = NPN_UserAgent(NULL);
	RETURN_OOM_IF_NULL(user_agent);

	OpAutoPtr<OpPluginUserAgentResponseMessage> response(OpPluginUserAgentResponseMessage::Create());
	RETURN_OOM_IF_NULL(response.get());

	UniString ua;
	RETURN_IF_ERROR(UniString_UTF8::FromUTF8(ua, user_agent));
	RETURN_IF_ERROR(response->SetUserAgent(ua));

	return message->Reply(response.release());
}

OP_STATUS
PluginLib::OnPluginReloadMessage(OpPluginReloadMessage* message)
{
	NPN_ReloadPlugins(message->GetReloadPages());

	return message->Reply(OpPluginErrorMessage::Create(NPERR_NO_ERROR));
}

OP_STATUS
PluginLib::OnPluginExceptionMessage(OpPluginExceptionMessage* message)
{
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	char* message_text;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&message_text, message->GetMessage()));

	NPN_SetException(np_object, message_text);

	OP_DELETEA(message_text);
	return OpStatus::OK;
}

NPError
PluginLib::ClearSiteData(const char *site, uint64_t flags, uint64_t max_age)
{
	OP_ASSERT(Initialized());

	UniString u_site;
	if (site)
		UniString_UTF8::FromUTF8(u_site, site);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_component->GetDestination()));
	OP_STATUS s = sync.SendMessage(OpPluginClearSiteDataMessage::Create(u_site, flags, max_age));

	return (OpStatus::IsError(s)) ? NPERR_GENERIC_ERROR : OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

char**
PluginLib::GetSitesWithData()
{
	OP_ASSERT(Initialized());

	PluginSyncMessenger sync(g_opera->CreateChannel(m_component->GetDestination()));
	OP_STATUS s = sync.SendMessage(OpPluginGetSitesWithDataMessage::Create());
	if (OpStatus::IsError(s))
		return NULL;

	OpPluginGetSitesWithDataResponseMessage* response = OpPluginGetSitesWithDataResponseMessage::Cast(sync.GetResponse());
	const OpProtobufValueVector<UniString>& sites = response->GetSites();
	char** sites_with_data = OP_NEWA(char*, sites.GetCount() + 1);
	op_memset(sites_with_data, 0, sites.GetCount() + 1);

	for (unsigned int i = 0; i < sites.GetCount(); ++i)
		if (OpStatus::IsError(UniString_UTF8::ToUTF8(sites_with_data + i, sites.Get(i))))
			break;
	return sites_with_data;
}

#elif defined(USE_OPDLL) || defined(STATIC_NS4PLUGINS)

#include "modules/ns4plugins/src/pluginexceptions.h"

PluginLib::PluginLib() :
#ifdef DYNAMIC_NS4PLUGINS
	dll(NULL),
#endif
	initialized(FALSE),
	plugin(NULL),
	saved(NULL),
	count(0),
	last_accessed(0),
	dll_init(NULL),
	dll_shutdown(NULL)
{
		op_memset(&pluginfuncs, 0, sizeof(pluginfuncs));
}

PluginLib::~PluginLib()
{
#ifdef DYNAMIC_NS4PLUGINS
	if (dll)
	{
		if (dll_shutdown && Initialized())
		{
			TRY
#ifdef NS4P_OPDLL_SELF_INIT
			dll_shutdown(dll);
#else
			dll_shutdown();
#endif // NS4P_OPDLL_SELF_INIT
			CATCH_IGNORE

			dll_shutdown = NULL;
		}

		if (owns_dll)
		{
			OpStatus::Ignore(dll->Unload());
			OP_DELETE(dll);
		}
		dll = NULL;
	}
#endif //DYNAMIC_NS4PLUGINS

	/*
	There are some plugins which prevent deleting a cache file after the window
	is closed as they retain lock on the file for X duration even after a call
	to NPP_Destroy().

	To tackle this issue, unloading of a URL is defered until the in-use
	plugin lib is unloaded and that way it can be ensured the file in unlocked.
	*/
	int i, count = url_cleanup_tasklist.GetCount();
	for (i=0; i < count; i++)
		url_cleanup_tasklist.Get(i)->GetURL().Unload();

	OP_DELETEA(plugin);
	plugin = NULL;

	OP_DELETE(saved);
	saved = NULL;
}

OP_STATUS PluginLib::InitLibFromDll(OpDLL* plugin_dll, const uni_char *lib_name, BOOL owns_plugin_dll/*=TRUE*/)
{
#ifdef DYNAMIC_NS4PLUGINS
	if (!plugin)
	{
		plugin = uni_stripdup(lib_name);
		if (plugin == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	dll = plugin_dll;
	owns_dll = owns_plugin_dll;

	// Initialize saved data
	saved = OP_NEW(NPSavedData, ());
	if (saved == NULL)
		return OpStatus::ERR_NO_MEMORY;
	saved->len = 0;
	saved->buf = NULL;

	op_memset(&pluginfuncs, 0, sizeof(NPPluginFuncs));

	pluginfuncs.size = sizeof(NPPluginFuncs);
	pluginfuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

	// Find addresses for init and shutdown

#ifdef DLL_NAME_LOOKUP_SUPPORT
	dll_init = (NP_InitializeP)dll->GetSymbolAddress("NP_Initialize");
	if (dll_init == NULL)
	{
		dll_init = (NP_InitializeP)dll->GetSymbolAddress("NP_PluginInit");
	}
	dll_shutdown = (NP_ShutdownP)dll->GetSymbolAddress("NP_Shutdown");
	if (dll_shutdown == NULL)
	{
		dll_shutdown = (NP_ShutdownP)dll->GetSymbolAddress("NP_PluginShutdown");
	}
#else // DLL_NAME_LOOKUP_SUPPORT
	dll_init = (NP_InitializeP)dll->GetSymbolAddress(3);
	dll_shutdown = (NP_ShutdownP)dll->GetSymbolAddress(4);
#endif // DLL_NAME_LOOKUP_SUPPORT

	// If init or shutdown is not found, return error.
	// On MSWIN: Find address for NP_GetEntryPoints

#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI) || defined(_MACINTOSH_)
	dll_get_entry_points = (NP_GetEntryPointsP)dll->GetSymbolAddress("NP_GetEntryPoints");

	if (dll_init == NULL || dll_shutdown == NULL || dll_get_entry_points == NULL)
#else
	if (dll_init == NULL || dll_shutdown == NULL)
#endif // MSWIN
	{
		return OpStatus::ERR;
	}

#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI)
	NPError ret = dll_get_entry_points(&pluginfuncs);
	if (ret != NPERR_NO_ERROR)
	{
		if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			return OpStatus::ERR_NO_MEMORY;
		else
			return OpStatus::ERR;
	}
#endif // MSWIN


	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // DYNAMIC_NS4PLUGINS
}

OP_STATUS PluginLib::LoadLib(const uni_char* plugin_dll)
{
#ifdef DYNAMIC_NS4PLUGINS
	// Store the name of the dll
	plugin = uni_stripdup(plugin_dll);
	if (plugin == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Create OpDLL
	OP_STATUS status = OpDLL::Create(&dll);
	if (OpStatus::IsError(status))
		return status;

	// Load the DLL into memory
	status = dll->Load(plugin);
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(dll->Unload());
		OP_DELETE(dll);
		dll = NULL;
		return status;
	}

	status = InitLibFromDll(dll, plugin);
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(dll->Unload());
		OP_DELETE(dll);
		dll = NULL;
		return status;
	}

	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // DYNAMIC_NS4PLUGINS
}

NPError PluginLib::Init()
{
	NPError ret = NPERR_NO_ERROR;
	NPNetscapeFuncs* ofuncs = (NPNetscapeFuncs*)&g_operafuncs;

	// If you don't have a dll_init at this point - something is seriously wrong
	OP_ASSERT(dll_init);
	if(dll_init == 0)
		return NPERR_GENERIC_ERROR;

	TRY
#ifdef NS4P_OPDLL_SELF_INIT
	ret = dll_init(dll, ofuncs, &pluginfuncs);
#else
#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI)
	ret = dll_init(ofuncs);
#elif defined(_MACINTOSH_)
	/* On Mac, NP_Initialize is called before getting pointers to the plugin functions. */
	ret = dll_init(ofuncs);

	if (ret == NPERR_NO_ERROR)
		ret = dll_get_entry_points(&pluginfuncs);
#elif defined(ANDROID)
	ret = dll_init(ofuncs, &pluginfuncs, jni::getEnv());
#else
	ret = dll_init(ofuncs, &pluginfuncs);
#endif // MSWIN
#endif // NS4P_OPDLL_SELF_INIT
	CATCH_PLUGIN_EXCEPTION(ret=NPERR_GENERIC_ERROR;)

	if (ret == NPERR_NO_ERROR)
		initialized = TRUE;

	return ret;
}

OP_STATUS PluginLib::LoadStaticLib(const uni_char* plugin_dll, NP_InitializeP init_function, NP_ShutdownP shutdown_function)
{
#ifdef STATIC_NS4PLUGINS

	plugin = uni_stripdup(plugin_dll);
	if (plugin == NULL)
		return OpStatus::ERR_NO_MEMORY;

	// Initialize saved data
	saved = OP_NEW(NPSavedData, ());
	if (saved == NULL)
		return OpStatus::ERR_NO_MEMORY;
	saved->len = 0;
	saved->buf = NULL;

	dll_init = init_function;
	dll_shutdown = shutdown_function;

	pluginfuncs.size = sizeof(NPPluginFuncs);
	pluginfuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

#if defined(MSWIN) || defined(_WINDOWS) || defined(WIN_CE) || defined(WINGOGI)
	NPError ret = dll_init((NPNetscapeFuncs*)&g_operafuncs);
#else
	NPError ret = dll_init((NPNetscapeFuncs*)&g_operafuncs, &pluginfuncs);
#endif

	if (ret != NPERR_NO_ERROR)
	{
		if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			return OpStatus::ERR_NO_MEMORY;
		else
			return OpStatus::ERR;
	}

	initialized = TRUE;
	return OpStatus::OK;
#else
	OP_ASSERT(!"Error: You must enable static nsplugins to use this function!");
	return OpStatus::ERR;
#endif //STATIC_NS4PLUGINS
}

OP_STATUS PluginLib::AddURLIntoCleanupTaskList(URL* url)
{
	//Return if URL is already present in the cleanup list
	int i, count = url_cleanup_tasklist.GetCount();
	for (i=0; i < count; i++)
	{
		if (url_cleanup_tasklist.Get(i)->GetURL() == *url)
			return OpStatus::OK;
	}

	URL_InUse *url_in_use = OP_NEW(URL_InUse, (*url));
	if (url_in_use)
		return url_cleanup_tasklist.Add(url_in_use);
	return OpStatus::ERR_NO_MEMORY;
}

BOOL PluginLib::ok() const
{
	return initialized
# ifdef DYNAMIC_NS4PLUGINS
		|| (dll && dll->IsLoaded())
# endif
		;
}

/**
 * This function returns true if this class is the library
 * which is specified by const char *lib
 */
BOOL PluginLib::isThisLibrary(const uni_char *lib)
{
	return lib && plugin && !uni_stricmp(lib, plugin);
}

NPError PluginLib::ClearSiteData(const char *site, uint64_t flags, uint64_t max_age)
{
	if (!pluginfuncs.clearsitedata)
		return NPERR_GENERIC_ERROR;

	return pluginfuncs.clearsitedata(site, flags, max_age);
}

char** PluginLib::GetSitesWithData()
{
	if (!pluginfuncs.getsiteswithdata)
		return NULL;

	return pluginfuncs.getsiteswithdata();
}

#else //USE_OPDLL || STATIC_NS4PLUGINS

PluginLib::PluginLib()
    : plugin(0)
    , saved(0)
    , count(0)
    , last_accessed(0)
{
#if defined(MSWIN) || defined(WINGOGI)
    PluginDLL = 0;
#endif
    pluginfuncs.javaClass = NULL;
}

/**
 * This function returns true if this class is the library
 * which is specified by const char *lib
 */
BOOL PluginLib::isThisLibrary(const uni_char *lib)
{
	if (lib && plugin && !uni_stricmp(lib, plugin))
		return TRUE;
	return FALSE;
}

#endif // USE_OPDLL

#endif // _PLUGIN_SUPPORT
