/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOW_COMMANDER_MANAGER_H
#define WINDOW_COMMANDER_MANAGER_H

#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/idle/idle_detector.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class WindowCommanderManager
	: public OpWindowCommanderManager
	, public OpActivityListener
{
public:
	WindowCommanderManager();
	~WindowCommanderManager();

	OP_STATUS GetWindowCommander(OpWindowCommander** windowCommander);
	void      ReleaseWindowCommander(OpWindowCommander* windowCommander);
	void      SetUiWindowListener(OpUiWindowListener* listener);
	OpUiWindowListener* GetUiWindowListener() { return m_uiWindowListener; }
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	void SetSSLListener(OpSSLListener* listener);
	OpSSLListener* GetSSLListener() { return m_sslListener; }
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef APPLICATION_CACHE_SUPPORT
	virtual OP_STATUS GetAllApplicationCacheEntries(OpVector<OpApplicationCacheEntry>& all_app_caches);

	virtual OP_STATUS DeleteApplicationCache(const uni_char *manifest_url);

	virtual OP_STATUS DeleteAllApplicationCaches();

#endif // APPLICATION_CACHE_SUPPORT
#ifdef WINDOW_COMMANDER_TRANSFER
	OpTransferManager* GetTransferManager();
#endif // WINDOW_COMMANDER_TRANSFER
	void SetAuthenticationListener(OpAuthenticationListener* listener);
	/**
		Authenticate the specified pending authentication.
		@param user_name the username
		@param password the password
		@param authid the authentication id you got in the listener
	*/
	void Authenticate(const uni_char* user_name, const uni_char* password, URL_ID authid);
	/**
		Cancel the specified pending authentication.
		@param authid the authentication id you got in the listener
	*/
	void CancelAuthentication(URL_ID authid);
	OpAuthenticationListener* GetAuthenticationListener() { return m_authenticationListener; }

	void SetIdleStateChangedListener(OpIdleStateChangedListener* listener);
	OpIdleStateChangedListener* GetIdleStateChangedListener() { return m_idleStateChangedListener; }

	// OpActivityListener
	virtual OP_STATUS OnActivityStateChanged(OpActivityState state);

	void SetOomListener(OpOomListener* listener);
	OpOomListener* GetOomListener() { return m_oomListener; }

#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
	void SetDataSyncListener(OpDataSyncListener* listener);
	OpDataSyncListener* GetDataSyncListener() { return m_dataSyncListener; }
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#ifdef WEB_TURBO_MODE
	void SetWebTurboUsageListener(OpWebTurboUsageListener* listener);
		OpWebTurboUsageListener* GetWebTurboUsageListener() { return m_webTurboUsageListener; }
#endif // WEB_TURBO_MODE

#ifdef PI_SENSOR
	virtual void SetSensorCalibrationListener(OpSensorCalibrationListener* listener);
	virtual OpSensorCalibrationListener* GetSensorCalibrationListener() { return m_sensorCalibrationListener; }
#endif //PI_SENSOR

#ifdef GADGET_SUPPORT
	void SetGadgetListener(OpGadgetListener* listener);
	OpGadgetListener* GetGadgetListener() { return m_gadgetListener; }
#endif // GADGET_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	virtual void SetSearchProviderListener(OpSearchProviderListener* listener);
	virtual OpSearchProviderListener* GetSearchProviderListener(){ return m_searchProviderListener; }
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

#ifdef WIC_TAB_API_SUPPORT
	virtual void SetTabAPIListener(OpTabAPIListener* listener);
	virtual OpTabAPIListener* GetTabAPIListener() { return m_tab_api_listener; }
#endif // WIC_TAB_API_SUPPORT

private:
	OpUiWindowListener* m_uiWindowListener;
#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	OpSSLListener* m_sslListener;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

#ifdef WINDOW_COMMANDER_TRANSFER
	OpTransferManager* m_transferManager; // should be moved to the "core object" later.
#endif // WINDOW_COMMANDER_TRANSFER

	class NullUiWindowListener : public OpUiWindowListener
	{
	public:
	OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, OpWindowCommander* opener, UINT32 width, UINT32 height, UINT32 flags) { return OpStatus::ERR; }
#ifdef WIC_ADDITIONAL_WINDOW_CREATION_ARGS
	OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, const CreateUiWindowArgs &args) { return OpStatus::ERR; }
#endif // WIC_ADDITIONAL_WINDOW_CREATION_ARGS
#ifdef WIC_CREATEDIALOGWINDOW
		OP_STATUS CreateDialogWindow(OpWindowCommander* wic, OpWindowCommander* opener, unsigned int width, unsigned int height, BOOL modal) { return OpStatus::ERR; }
#endif // WIC_CREATEDIALOGWINDOW
		void CloseUiWindow(OpWindowCommander* windowCommander) {}
	} m_nullUiWindowListener;

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
	class NullSSLListener : public OpSSLListener
	{
	public:
		~NullSSLListener() {}

		void OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options)
			{ context->OnCertificateBrowsingDone(FALSE, SSL_CERT_OPTION_NONE); }

		void OnCertificateBrowsingCancel(OpWindowCommander* wic, SSLCertificateContext* context)
			{ }

		void OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
			{ callback->OnSecurityPasswordDone(FALSE, NULL, NULL); }

		void OnSecurityPasswordCancel(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
			{ }

	} m_nullSslListener;
#endif // _SSL_SUPPORT_ && _NATIVE_SSL_SUPPORT_ || WIC_USE_SSL_LISTENER

	class NullAuthenticationListener : public OpAuthenticationListener
	{
	public:
		void OnAuthenticationRequired(OpAuthenticationCallback* callback) {}
		void OnAuthenticationCancelled(const OpAuthenticationCallback* callback) {}

	} m_nullAuthenticationListener;

	OpAuthenticationListener* m_authenticationListener;


#if defined SUPPORT_DATA_SYNC && defined CORE_BOOKMARKS_SUPPORT
	class NullDataSyncListener : public OpDataSyncListener
	{
	public:
		virtual void OnSyncLogin(SyncCallback *cb) { if (cb) cb->OnSyncLoginCancel(); }
		virtual void OnSyncError(const uni_char *error_msg) {}
	} m_nullDataSyncListener;

	OpDataSyncListener* m_dataSyncListener;
#endif // SUPPORT_DATA_SYNC && CORE_BOOKMARKS_SUPPORT

#ifdef WEB_TURBO_MODE
	class NullWebTurboUsageListener : public OpWebTurboUsageListener
	{
	public:
		virtual void OnUsageChanged(TurboUsage status) {}
	} m_nullWebTurboUsageListener;

	OpWebTurboUsageListener* m_webTurboUsageListener;
#endif // WEB_TURBO_MODE

#ifdef PI_SENSOR
	class NullSensorCalibrationListener : public OpSensorCalibrationListener
	{
	public:
		virtual void OnSensorCalibrationRequest(OpSensor *sensor) {}
	} m_nullSensorCalibrationListener;

	OpSensorCalibrationListener* m_sensorCalibrationListener;
#endif // PI_SENSOR

#ifdef GADGET_SUPPORT
	class NullGadgetListener : public OpGadgetListener
	{
	public:
		void OnGadgetUpdateReady(const OpGadgetUpdateData& data) {}
		void OnGadgetUpdateError(const OpGadgetErrorData& data) {}
		void OnGadgetDownloadPermissionNeeded(const OpGadgetDownloadData& data, GadgetDownloadPermissionCallback *callback) { callback->OnGadgetDownloadPermission(TRUE); }
		void OnGadgetDownloaded(const OpGadgetDownloadData& data) { }
		void OnRequestRunGadget(const OpGadgetDownloadData& data) { }
		void OnGadgetDownloadError(const OpGadgetErrorData& data) { }
		void OnGadgetInstalled(const OpGadgetInstallRemoveData& data) { }
		void OnGadgetRemoved(const OpGadgetInstallRemoveData& data) { }
		void OnGadgetUpgraded(const OpGadgetStartStopUpgradeData& data) { }
		void OnGadgetSignatureVerified(const OpGadgetSignatureVerifiedData& data) { }
		void OnGadgetStarted(const OpGadgetStartStopUpgradeData& data) { }
		void OnGadgetStartFailed(const OpGadgetStartFailedData& data) { }
		void OnGadgetStopped(const OpGadgetStartStopUpgradeData& data) { }
		void OnGadgetInstanceCreated(const OpGadgetInstanceData& data) { };
		void OnGadgetInstanceRemoved(const OpGadgetInstanceData& data) { };
	} m_nullGadgetListener;

	OpGadgetListener* m_gadgetListener;
#endif // GADGET_SUPPORT

#ifdef SEARCH_PROVIDER_QUERY_SUPPORT
	class NullSearchProviderListener : public OpSearchProviderListener
	{
	public:
		virtual SearchProviderInfo* OnRequestSearchProviderInfo(RequestReason reason){ return NULL; }
	} m_nullSearchProviderListener;

	OpSearchProviderListener* m_searchProviderListener;
#endif //SEARCH_PROVIDER_QUERY_SUPPORT

	class NullIdleStateChangedListener : public OpIdleStateChangedListener
	{
	public:
		void OnIdle() {}
		void OnNotIdle() {}
	} m_nullIdleStateChangedListener;

	OpIdleStateChangedListener* m_idleStateChangedListener;

#ifdef WIC_TAB_API_SUPPORT

	class NullTabAPIListener : public OpTabAPIListener
	{
	public:
		virtual void OnQueryAllBrowserWindows(ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryAllBrowserWindows(OpStatus::ERR_NOT_SUPPORTED, NULL); }
		virtual void OnQueryAllTabGroups(ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryAllTabGroups(OpStatus::ERR_NOT_SUPPORTED, NULL); }
		virtual void OnQueryAllTabs(ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryAllTabs(OpStatus::ERR_NOT_SUPPORTED, NULL, NULL); }

		virtual void OnQueryBrowserWindow(TabAPIItemId window_id, BOOL include_contents, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryBrowserWindow(OpStatus::ERR_NOT_SUPPORTED, NULL, NULL, NULL, NULL); }
		virtual void OnQueryTabGroup(TabAPIItemId tab_group_id, BOOL include_contents, ExtensionWindowTabNotifications* callbacks)  { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryTabGroup(OpStatus::ERR_NOT_SUPPORTED, NULL, NULL, NULL); }
		virtual void OnQueryTab(TabAPIItemId tab_id, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryTab(OpStatus::ERR_NOT_SUPPORTED, NULL); }
		virtual void OnQueryTab(OpWindowCommander* target_window, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyQueryTab(OpStatus::ERR_NOT_SUPPORTED, NULL); }

		virtual void OnRequestBrowserWindowClose(TabAPIItemId window_id, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyBrowserWindowClose(OpStatus::ERR_NOT_SUPPORTED); }
		virtual void OnRequestBrowserWindowMoveResize(TabAPIItemId window_id, const ExtensionWindowMoveResize &window_info, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyBrowserWindowMoveResize(OpStatus::ERR_NOT_SUPPORTED); }

		virtual void OnRequestTabGroupClose(TabAPIItemId tab_group_id, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabGroupClose(OpStatus::ERR_NOT_SUPPORTED); }
		virtual void OnRequestTabGroupMove(TabAPIItemId tab_group_id, const WindowTabInsertTarget& data,	ExtensionWindowTabNotifications* callbacks){ OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabGroupMove(OpStatus::ERR_NOT_SUPPORTED, 0); }
		virtual void OnRequestTabGroupUpdate(TabAPIItemId tab_group_id, int collapsed, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabGroupUpdate(OpStatus::ERR_NOT_SUPPORTED); }

		virtual void OnRequestTabClose(TabAPIItemId tab_id, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabClose(OpStatus::ERR_NOT_SUPPORTED); }
		virtual void OnRequestTabMove(TabAPIItemId tab_id, const WindowTabInsertTarget& data, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabMove(OpStatus::ERR_NOT_SUPPORTED, 0, 0); }

		virtual void OnRequestTabUpdate(TabAPIItemId tab_id, BOOL set_focus, int set_pinned, const uni_char* set_url, const uni_char* set_title, ExtensionWindowTabNotifications* callbacks) { OP_ASSERT(!"Error: listener not set"); callbacks->NotifyTabUpdate(OpStatus::ERR_NOT_SUPPORTED); }

		virtual OP_STATUS OnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *l) { OP_ASSERT(!"Error: listener not set"); return OpStatus::ERR; }
		virtual OP_STATUS OnUnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *l) { return OpStatus::ERR; }
	} m_null_tab_api_listener;

	OpTabAPIListener* m_tab_api_listener;
#endif // WIC_TAB_API_SUPPORT

	class NullOomListener : public OpOomListener
	{
	public:
		void OnOomError() {}
		void OnSoftOomError() {}
		void OnOodError() {}
	} m_nullOomListener;

	OpOomListener* m_oomListener;

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	virtual PersistentStorageCommander* GetPersistentStorageCommander();
#endif //DATABASE_MODULE_MANAGER_SUPPORT

	virtual void ClearPrivateData(int option_flags);

#ifdef MEDIA_PLAYER_SUPPORT
	virtual BOOL IsAssociatedWithVideo(OpMediaHandle handle);
#endif //MEDIA_PLAYER_SUPPORT
};

#endif // WINDOW_COMMANDER_MANAGER_H
