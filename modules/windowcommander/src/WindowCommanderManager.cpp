/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/windowcommander/src/WindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/TransferManager.h"

#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/media/media_module.h"
#include "modules/url/url_man.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/win.h"
#include "modules/dom/domenvironment.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#ifdef SUPPORT_GENERATE_THUMBNAILS
# include "modules/thumbnails/thumbnailmanager.h"
#endif // SUPPORT_GENERATE_THUMBNAILS

#ifdef HISTORY_SUPPORT
# include "modules/history/OpHistoryModel.h"
#endif // HISTORY_SUPPORT

#ifdef DIRECT_HISTORY_SUPPORT
# include "modules/history/direct_history.h"
#endif //DIRECT_HISTORY_SUPPORT

#ifndef HAS_COMPLEX_GLOBALS
extern void init_wic_charsets();
#endif // !HAS_COMPLEX_GLOBALS

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef PREFS_GETOVERRIDDENHOSTS
# include "modules/util/opstrlst.h"
#endif // PREFS_GETOVERRIDDENHOSTS

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

#ifdef _PLUGIN_SUPPORT_
# include "modules/ns4plugins/src/pluginhandler.h"
# include "modules/ns4plugins/src/pluginlibhandler.h"
#endif // _PLUGIN_SUPPORT_

WindowCommanderManager::WindowCommanderManager()
	: m_uiWindowListener(&m_nullUiWindowListener),
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	m_sslListener(&m_nullSslListener),
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER
#ifdef WINDOW_COMMANDER_TRANSFER
	m_transferManager(NULL),
#endif // WINDOW_COMMANDER_TRANSFER
	m_authenticationListener(&m_nullAuthenticationListener),
#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
	m_dataSyncListener(&m_nullDataSyncListener),
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT
#ifdef WEB_TURBO_MODE
	m_webTurboUsageListener(&m_nullWebTurboUsageListener),
#endif // WEB_TURBO_MODE
#ifdef PI_SENSOR
	m_sensorCalibrationListener(&m_nullSensorCalibrationListener),
#endif // PI_SENSOR
#ifdef GADGET_SUPPORT
	m_gadgetListener(&m_nullGadgetListener),
#endif
#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	m_searchProviderListener(&m_nullSearchProviderListener),
#endif //SEARCH_PROVIDER_QUERY_SUPPORT
	m_idleStateChangedListener(&m_nullIdleStateChangedListener),
#ifdef WIC_TAB_API_SUPPORT
	m_tab_api_listener(&m_null_tab_api_listener),
#endif // WIC_TAB_API_SUPPORT
	m_oomListener(&m_nullOomListener)
{
	CONST_ARRAY_INIT(wic_charsets);
}

WindowCommanderManager::~WindowCommanderManager()
{
#ifdef WINDOW_COMMANDER_TRANSFER
	OP_DELETE(m_transferManager);
#endif // WINDOW_COMMANDER_TRANSFER

	g_idle_detector->RemoveListener(this);
}

OP_STATUS WindowCommanderManager::GetWindowCommander(OpWindowCommander** windowCommander)
{
	WindowCommander* wc = OP_NEW(WindowCommander, ());

	if (wc == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = wc->Init();

	if (OpStatus::IsError(status))
	{
		if (status == OpStatus::ERR_NO_MEMORY)
		{
			g_memory_manager->RaiseCondition(status);
		}

		OP_DELETE(wc);
		return status;
	}

	*windowCommander = (OpWindowCommander*)wc;

	return OpStatus::OK;
}

void WindowCommanderManager::ReleaseWindowCommander(OpWindowCommander* windowCommander)
{
	OP_DELETE(windowCommander);
}

void WindowCommanderManager::SetUiWindowListener(OpUiWindowListener* listener)
{
	if (listener == NULL)
	{
		m_uiWindowListener = &m_nullUiWindowListener;
	}
	else
	{
		m_uiWindowListener = listener;
	}
}

#ifdef APPLICATION_CACHE_SUPPORT
OP_STATUS WindowCommanderManager::GetAllApplicationCacheEntries(OpVector<OpApplicationCacheEntry>& all_app_caches)
{
	if (g_application_cache_manager)
		return g_application_cache_manager->GetAllApplicationCacheEntries(all_app_caches);

	return OpStatus::OK;
}

OP_STATUS WindowCommanderManager::DeleteApplicationCache(const uni_char *manifest_url)
{
	if (g_application_cache_manager)
		return g_application_cache_manager->DeleteApplicationCacheGroup(manifest_url);

	return OpStatus::OK;
}

OP_STATUS WindowCommanderManager::DeleteAllApplicationCaches()
{
	if (g_application_cache_manager)
		return g_application_cache_manager->DeleteAllApplicationCacheGroups();

	return OpStatus::OK;
}
#endif // APPLICATION_CACHE_SUPPORT

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
void WindowCommanderManager::SetSSLListener(OpSSLListener* listener)
{
	if (listener == NULL)
	{
		m_sslListener = &m_nullSslListener;
	}
	else
	{
		m_sslListener = listener;
	}

	// Set the listener for all windows
	for(Window* win = g_windowManager->FirstWindow(); win; win = win->Suc())
	{
		if (win->GetWindowCommander())
		{
			win->GetWindowCommander()->SetSSLListener(m_sslListener);
		}
	}
}
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef WINDOW_COMMANDER_TRANSFER
OpTransferManager* WindowCommanderManager::GetTransferManager()
{
	if (m_transferManager == NULL)
	{
		m_transferManager = OP_NEW(TransferManager, ());
		if (m_transferManager == NULL)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
	}
	return m_transferManager;
}
#endif // WINDOW_COMMANDER_TRANSFER

void WindowCommanderManager::SetAuthenticationListener(OpAuthenticationListener* listener)
{
	m_authenticationListener = listener;

	if (m_authenticationListener == NULL)
	{
		m_authenticationListener = &m_nullAuthenticationListener;
	}
}

void WindowCommanderManager::SetIdleStateChangedListener(OpIdleStateChangedListener* listener)
{
	g_idle_detector->RemoveListener(this);

	if (listener == NULL)
	{
		m_idleStateChangedListener = &m_nullIdleStateChangedListener;
	}
	else
	{
		m_idleStateChangedListener = listener;
		g_idle_detector->AddListener(this);
	}
}

/* virtual */ OP_STATUS
WindowCommanderManager::OnActivityStateChanged(OpActivityState state)
{
	if (state == ACTIVITY_STATE_IDLE)
		m_idleStateChangedListener->OnIdle();
	else if (state == ACTIVITY_STATE_BUSY)
		m_idleStateChangedListener->OnNotIdle();

	return OpStatus::OK;
}

void WindowCommanderManager::SetOomListener(OpOomListener* listener)
{
	if (listener == NULL)
	{
		m_oomListener = &m_nullOomListener;
	}
	else
	{
		m_oomListener = listener;
	}
}

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
void WindowCommanderManager::SetDataSyncListener(OpDataSyncListener* listener)
{
	m_dataSyncListener = listener ? listener : &m_nullDataSyncListener;
}
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#ifdef WEB_TURBO_MODE
void WindowCommanderManager::SetWebTurboUsageListener(OpWebTurboUsageListener* listener)
{
	m_webTurboUsageListener = listener ? listener : &m_nullWebTurboUsageListener;
}
#endif // WEB_TURBO_MODE

#ifdef PI_SENSOR
void WindowCommanderManager::SetSensorCalibrationListener(OpSensorCalibrationListener *listener)
{
	m_sensorCalibrationListener = listener ? listener : &m_nullSensorCalibrationListener;
}
#endif //PI_SENSOR

#ifdef GADGET_SUPPORT
void WindowCommanderManager::SetGadgetListener(OpGadgetListener* listener)
{
	if (m_gadgetListener && m_gadgetListener != &m_nullGadgetListener)
		g_gadget_manager->DetachListener(m_gadgetListener);
	m_gadgetListener = listener ? listener : &m_nullGadgetListener;
	if (listener)
		OpStatus::Ignore(g_gadget_manager->AttachListener(listener));
}
#endif // GADGET_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
void WindowCommanderManager::SetSearchProviderListener(OpSearchProviderListener* listener)
{
	m_searchProviderListener = listener ? listener : &m_nullSearchProviderListener;
}
#endif // SEARCH_PROVIDER_QUERY_SUPPORT

#ifdef WIC_TAB_API_SUPPORT

/* virtual */ void
WindowCommanderManager::SetTabAPIListener(OpTabAPIListener* listener)
{
	m_tab_api_listener = listener ? listener : &m_null_tab_api_listener;
}
#endif // WIC_TAB_API_SUPPORT

/* static */
OpWindowCommanderManager* OpWindowCommanderManager::CreateL()
{
	return OP_NEW_L(WindowCommanderManager, ());
}

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/database/ps_commander.h"

/*virtual*/PersistentStorageCommander*
WindowCommanderManager::GetPersistentStorageCommander()
{
	return PersistentStorageCommander::GetInstance();
}
#endif //DATABASE_MODULE_MANAGER_SUPPORT

/*virtual*/
void WindowCommanderManager::ClearPrivateData(int option_flags)
{
	if (option_flags & WIC_OPERA_CLEAR_MEMORY_CACHE)
	{
		extern void NondestructiveOutOfMemoryFlush();
		NondestructiveOutOfMemoryFlush();
	}

	if (option_flags & WIC_OPERA_CLEAR_IMAGE_CACHE && imgManager && g_memory_manager)
	{
		// Clear the image cache by setting the cache size to 0 and
		// back again.
		// FIXME: We need a proper api for this.
		imgManager->SetCacheSize(0, IMAGE_CACHE_POLICY_SOFT);
		imgManager->SetCacheSize(g_memory_manager->GetMaxImgMemory(), IMAGE_CACHE_POLICY_SOFT);
	}

#ifdef SUPPORT_GENERATE_THUMBNAILS
	if (option_flags & WIC_OPERA_CLEAR_THUMBNAILS)
		OpStatus::Ignore(g_thumbnail_manager->Purge());
#endif // SUPPORT_GENERATE_THUMBNAILS

	BOOL visited_links  = option_flags & WIC_OPERA_CLEAR_VISITED_LINKS  ?TRUE:FALSE;
	BOOL disk_cache     = option_flags & WIC_OPERA_CLEAR_DISK_CACHE     ?TRUE:FALSE;
	BOOL sensitive_data = option_flags & WIC_OPERA_CLEAR_SENSITIVE_DATA ?TRUE:FALSE;
	BOOL cookies        = option_flags & WIC_OPERA_CLEAR_ALL_COOKIES    ?TRUE:FALSE;
	BOOL session_cookies= option_flags & WIC_OPERA_CLEAR_SESSION_COOKIES?TRUE:FALSE;
	BOOL memory_cache   = option_flags & WIC_OPERA_CLEAR_MEMORY_CACHE   ?TRUE:FALSE;
	if (option_flags & (WIC_OPERA_CLEAR_VISITED_LINKS
						| WIC_OPERA_CLEAR_DISK_CACHE
						| WIC_OPERA_CLEAR_SENSITIVE_DATA
						| WIC_OPERA_CLEAR_ALL_COOKIES
						| WIC_OPERA_CLEAR_SESSION_COOKIES)
		&& g_url_api)
	{
		if (visited_links || disk_cache)
			g_url_api->CleanUp(); // close all connections

		g_url_api->PurgeData(visited_links,
							 disk_cache,
							 sensitive_data,
							 session_cookies,
							 cookies,
							 FALSE, /* certificate */
							 memory_cache);
	}

	if (option_flags & WIC_OPERA_CLEAR_GLOBAL_HISTORY)
	{
#ifdef HISTORY_SUPPORT
		g_globalHistory->DeleteAllItems();
#endif
#ifdef DIRECT_HISTORY_SUPPORT
		g_directHistory->DeleteAllItems();
#endif
	}

#ifdef OPERA_CONSOLE
	if (option_flags & WIC_OPERA_CLEAR_CONSOLE)
		g_console->Clear();
#endif

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	if (GetPersistentStorageCommander())
	{
		if (option_flags & WIC_OPERA_CLEAR_WEBDATABASES)
			GetPersistentStorageCommander()->DeleteWebDatabases(PersistentStorageCommander::ALL_CONTEXT_IDS);
		if (option_flags & WIC_OPERA_CLEAR_WEBSTORAGE)
			GetPersistentStorageCommander()->DeleteWebStorage(PersistentStorageCommander::ALL_CONTEXT_IDS);
		if (option_flags & WIC_OPERA_CLEAR_EXTENSION_STORAGE)
			GetPersistentStorageCommander()->DeleteExtensionData(PersistentStorageCommander::ALL_CONTEXT_IDS);
	}
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	if (option_flags & WIC_OPERA_CLEAR_APPCACHE && g_application_cache_manager)
	{
		OpStatus::Ignore(g_application_cache_manager->DeleteAllApplicationCacheGroups());
	}
#endif // APPLICATION_CACHE_SUPPORT

#ifdef GEOLOCATION_SUPPORT
	if (option_flags & WIC_OPERA_CLEAR_GEOLOCATION_PERMISSIONS)
	{
		g_secman_instance->ClearUserConsentPermissions(OpSecurityManager::PERMISSION_TYPE_GEOLOCATION);
	}
#endif // GEOLOCATION_SUPPORT

#ifdef DOM_STREAM_API_SUPPORT
	if (option_flags & WIC_OPERA_CLEAR_CAMERA_PERMISSIONS)
	{
		g_secman_instance->ClearUserConsentPermissions(OpSecurityManager::PERMISSION_TYPE_CAMERA);
	}
#endif // DOM_STREAM_API_SUPPORT

#if defined(PREFS_GETOVERRIDDENHOSTS) && defined(PREFS_WRITE)
	if (option_flags & WIC_OPERA_CLEAR_SITE_PREFS)
	{
		TRAPD(err, g_prefsManager->RemoveOverridesAllHostsL(TRUE));
	}
#endif // defined(PREFS_GETOVERRIDDENHOSTS) && defined(PREFS_WRITE)

#ifdef _PLUGIN_SUPPORT_
	if (option_flags & WIC_OPERA_CLEAR_PLUGIN_DATA)
		OpStatus::Ignore(g_pluginhandler->GetPluginLibHandler()->ClearAllSiteData());
#endif // _PLUGIN_SUPPORT_

#ifdef CORS_SUPPORT
	if (sensitive_data || (option_flags & WIC_OPERA_CLEAR_CORS_PREFLIGHT))
		g_secman_instance->ClearCrossOriginPreflightCache();
#endif // CORS_SUPPORT

}

#ifdef MEDIA_PLAYER_SUPPORT
	BOOL WindowCommanderManager::IsAssociatedWithVideo(OpMediaHandle handle)
	{
		return g_media_module.IsAssociatedWithVideo(handle);
	}
#endif //MEDIA_PLAYER_SUPPORT

