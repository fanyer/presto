// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __WEBSERVER_MANAGER_H__
#define __WEBSERVER_MANAGER_H__

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/Application.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/webserver/webserver-api.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/webserver/controller/WebServerWidgetStateModifier.h"

#include "adjunct/quick/controller/FeatureController.h"
#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"
#include "adjunct/quick/webserver/view/WebServerErrorDescr.h"

#ifdef UPNP_SUPPORT
#include "modules/upnp/upnp_upnp.h"
#endif // UPNP_SUPPORT

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/quick/managers/AutoUpdateManager.h"
#endif // AUTO_UPDATE_SUPPORT

class HotlistManager;
class DesktopWindow;
class WebServerManager;
class UniteServiceModelItem;

/***********************************************************************************
 ** @struct	WebServerStatusContext
 **	@brief	information needed in the status dialog
 ************************************************************************************/
struct WebServerStatusContext
{
public:
	WebServerErrorDescr::SuggestedAction	m_suggested_action;
	OpString8			m_status_icon_name;
	OpString			m_status_text;
	INT32				m_services_running;
#ifdef WEBSERVER_CAP_STATS_TRANSFER
	OpFileLength		m_size_uploaded;
	OpFileLength		m_size_downloaded;
#endif // WEBSERVER_CAP_STATS_TRANSFER
#ifdef WEBSERVER_CAP_STATS_NUM_USERS
	INT32				m_people_connected;
#endif // WEBSERVER_CAP_STATS_NUM_USERS
	OpString			m_direct_access_address;
};

/***********************************************************************************
 ** @class	WebServerController
 **	@brief	Feature controller for webserver. Facade for calls needed by webserver UI.
 ************************************************************************************/
class WebServerController : public FeatureController
{
public:
	virtual ~WebServerController() {}

	virtual void		EnableFeature(const FeatureSettingsContext* settings, BOOL force_device_release = TRUE) = 0;
	virtual OP_STATUS	ChangeAccount() = 0;
	virtual OP_STATUS	Restart() = 0;

	virtual OP_STATUS	GetURLForComputerName(OpString & url, const OpStringC & computername) = 0;
	virtual void		SetConfiguring(BOOL configuring) = 0;
	virtual BOOL		IsConfiguring() = 0;

	virtual BOOL		IsUserRequestingDeviceRelease(DesktopWindow * parent_window = NULL) = 0;

	virtual OP_STATUS	GetWebServerStatusContext(WebServerStatusContext & context) = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Opera Account defines
#define MAX_NUMBER_OF_SIGNUPS_PER_DAY   5

// Sync defines
#ifdef SUPPORT_DATA_SYNC
 #define MYOPERA_PROVIDER UNI_L("myopera")
#endif // SUPPORT_DATA_SYNC

#define g_webserver_manager (WebServerManager::GetInstance())

class DesktopGadgetManager;
class UPnPDevice;

/***************************************************************************************
 *
 * WebServerConnector - 
 *
 * needed to replace webserver with a fake webserver in selftests
 * (g_webserver has private constructors/destructors)
 *
 *
 ***************************************************************************************/
class WebServerConnector
{
public:
	virtual ~WebServerConnector() {}

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual OP_STATUS Start(WebserverListeningMode mode, const char *shared_secret = NULL) { return g_webserver ? g_webserver->Start(mode, g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort), shared_secret) : OpStatus::ERR; }
#else
	virtual OP_STATUS Start(WebserverListeningMode mode) { return g_webserver ? g_webserver->Start(mode) : OpStatus::ERR; }
#endif

	virtual OP_STATUS Stop(WebserverStatus status = WEBSERVER_OK)
	{ return g_webserver ? g_webserver->Stop(status) : OpStatus::ERR; }

	virtual OP_STATUS AddWebserverListener(WebserverEventListener *listener)
	{ return g_webserver ? g_webserver->AddWebserverListener(listener) : OpStatus::ERR; }

	virtual void RemoveWebserverListener(WebserverEventListener *listener)
	{ if (g_webserver) g_webserver->RemoveWebserverListener(listener); }

	virtual unsigned int GetLocalListeningPort()
	{ return g_webserver ? g_webserver->GetLocalListeningPort() : 0; }

#ifdef WEB_UPLOAD_SERVICE_LIST
	virtual OP_STATUS StartServiceDiscovery(const OpStringC &sentral_server_uri, const OpStringC8 &session_token)
	{ return g_webserver ? g_webserver->StartServiceDiscovery(sentral_server_uri, session_token) : OpStatus::ERR; }

	virtual void StopServiceDiscovery()
	{ if (g_webserver) g_webserver->StopServiceDiscovery(); }
#endif //WEB_UPLOAD_SERVICE_LIST

#ifdef TRANSFERS_TYPE
	virtual TRANSFERS_TYPE GetBytesUploaded()
	{ return g_webserver ? g_webserver->GetBytesUploaded() : 0; }

	virtual TRANSFERS_TYPE GetBytesDownloaded()
	{ return g_webserver ? g_webserver->GetBytesDownloaded() : 0; }
#endif
#ifdef WEBSERVER_CAP_STATS_NUM_USERS
	virtual UINT32 GetLastUsersCount(UINT32 seconds)
	{ return g_webserver ? g_webserver->GetLastUsersCount(seconds) : 0; }
#endif // WEBSERVER_CAP_STATS_NUM_USERS
};


/***************************************************************************************
 *
 * WebServerPrefsConnector - 
 *
 * needed to replace g_pcwebserver with a fake prefs class in selftests
 * (g_pcwebserver can't be subclassed directly)
 *
 *
 ***************************************************************************************/
class WebServerPrefsConnector
{
public:
	virtual ~WebServerPrefsConnector() {}

	virtual BOOL ResetStringL(PrefsCollectionWebserver::stringpref which)
	{ return g_pcwebserver->ResetStringL(which); }

	virtual void GetStringPrefL(PrefsCollectionWebserver::stringpref which, OpString &result) const
	{ g_pcwebserver->GetStringPrefL(which, result); }

	virtual const OpStringC GetStringPref(PrefsCollectionWebserver::stringpref which) const
	{ return g_pcwebserver->GetStringPref(which); }

	virtual OP_STATUS WriteStringL(PrefsCollectionWebserver::stringpref which, const OpStringC &value)
	{ return g_pcwebserver->WriteStringL(which, value); }

	virtual int GetIntegerPref(PrefsCollectionWebserver::integerpref which) const
	{ return g_pcwebserver->GetIntegerPref(which); }

	virtual OP_STATUS WriteIntegerL(PrefsCollectionWebserver::integerpref which, int value)
	{ return g_pcwebserver->WriteIntegerL(which, value); }

	virtual void CommitL()
	{ return g_prefsManager->CommitL(); }
};

/***************************************************************************************
 *
 * WebServerUIConnector - 
 *
 * needed to replace all UI interaction in the manager with fake interaction in selftest
 *
 *
 ***************************************************************************************/
class WebServerUIConnector
#ifdef AUTO_UPDATE_SUPPORT
	: public AutoUpdateManager::AUM_Listener
#endif // AUTO_UPDATE_SUPPORT
{
public:
	WebServerUIConnector() : 	m_configure_style(NotConfiguring), m_configure_dialog(NULL), m_current_error_descr(NULL) {}
	virtual ~WebServerUIConnector() {}

	virtual void		SetConfiguring(BOOL configuring);
	virtual BOOL		IsConfiguring();

	 // blocking if webserver_enabled != NULL
	virtual OP_STATUS	ShowSetupWizard(WebServerManager * manager, BOOL * webserver_enabled,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context);

	virtual OP_STATUS	ShowSettingsDialog(WebServerManager * manager,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context);

	virtual OP_STATUS	ShowLoginDialog(WebServerManager * manager,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context, BOOL * logged_in);

	virtual OP_STATUS	ShowStatus(WebServerManager * manager);

	virtual BOOL		UserWantsFeatureEnabling(DesktopWindow * parent_window = NULL);
	virtual BOOL		UserWantsInvalidUsernamesChange(OperaAccountContext * account_context, DesktopWindow * parent_window = NULL);
	virtual BOOL		UserWantsDeviceTakeover(const OpStringC & device_name, DesktopWindow * parent_window = NULL);
	virtual BOOL		UserWantsReconnectAfterTakeover(DesktopWindow * parent_window = NULL);

	virtual void		OnWebServerIsInsecure(DesktopWindow * parent_window = NULL);
	virtual void		OnWebServerIsOutdated(DesktopWindow * parent_window = NULL);

	virtual OP_STATUS	HandleErrorInDialog(OperaAuthError error);

	virtual void		LogError(WebserverStatus status, BOOL show_as_warning = FALSE);
	virtual void		LogError(UploadServiceStatus status, BOOL show_as_warning = FALSE);

	const WebServerErrorDescr *	GetCurrentErrorDescr() const { return m_current_error_descr; }

protected:
#ifdef AUTO_UPDATE_SUPPORT
		//==== AUM_Listener implementations ===
		virtual void OnUpToDate();
		virtual void OnUpdateAvailable(AvailableUpdateContext* context);
		virtual void OnError(UpdateErrorContext* context);

		// unused virtual functions (AUM_Listener)
		virtual void OnChecking() {}
		virtual void OnDownloading(UpdateProgressContext* context) {}
		virtual void OnDownloadingDone(UpdateProgressContext* context) {}
		virtual void OnDownloadingFailed() {}
		virtual void OnPreparing(UpdateProgressContext* context) {}
		virtual void OnFinishedPreparing() {}
		virtual void OnReadyToInstall(UpdateProgressContext* context) {}
		virtual void OnMinimizedStateChanged(BOOL minimized) {}
#endif // AUTO_UPDATE_SUPPORT

	enum WebServerConfigureStyle {
		NotConfiguring = 0,
		SetupWizard,
		SettingsDialog,
		LoginDialog
	};

private:
	BOOL				DialogResult(const OpStringC& title, const OpStringC& message, DesktopWindow* parent_window);

	WebServerConfigureStyle			m_configure_style;
	class WebServerDialog*			m_configure_dialog;

	const WebServerErrorDescr *		m_current_error_descr;
};

#ifdef UPNP_SUPPORT

struct KnownDeviceMenuEntry
{
	friend class DesktopUPnPListener;
public:
	OpString	m_description_string;
	OpString	m_url;

private:
	UPnPDevice* m_device;
};

class DesktopUPnPListener: public UPnPLogic
{
public:
	DesktopUPnPListener(WebServerManager * webserver_manager, UPnP *upnp_father);

	// === UPnPLogic implementations ===
	virtual OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex);
	virtual void OnNewDevice(UPnPDevice *device);
	virtual void OnRemoveDevice(UPnPDevice *device);

	// === other ===
	const OpVector<KnownDeviceMenuEntry> &	GetKnownDeviceList() const	{ return m_known_devices; }
	void ClearDevicesList() { m_known_devices.Clear(); }
	
private:
	WebServerManager * m_webserver_manager;
	OpAutoVector<KnownDeviceMenuEntry>	m_known_devices;
};

#endif // UPNP_SUPPORT
/***************************************************************************************
 *
 * WebServerManager - 
 *
 * Handles starting and stopping the WebServer and correspondingly the root/home service
 * Holds a list of running services
 * Functions to show the unite settings and setup dialogs
 * WebserverEventListener
 *
 *
 *
 ***************************************************************************************/
class WebServerManager
	: public DesktopManager<WebServerManager>
	, public OpPrefsListener
	, public WebserverEventListener
	, public MessageObject
	, public WebServerController
	, public OperaAccountController::OAC_Listener
{
public:
	WebServerManager();
	~WebServerManager();

	// Initialise all the things the Opera Account Manager needs
	OP_STATUS	Init();
	OP_STATUS	Init(OperaAccountManager * opera_account_manager, DesktopGadgetManager * desktop_gadget_manager, HotlistManager * hotlist_manager);
	BOOL		IsInited() { return m_inited; }

	OP_STATUS	SetWebServerConnector(WebServerConnector * webserver_connector);
	OP_STATUS	SetWebServerPrefsConnector(WebServerPrefsConnector * prefs_connector);
	OP_STATUS	SetWebServerUIConnector(WebServerUIConnector * ui_connector);

	//=== MessageObject implementation ===========
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// NOTICE that has_started equal TRUE only implies that service is enabled or enabling.
	OP_STATUS	EnableIfRequired(BOOL& has_started, BOOL always_ask, BOOL delay_for_service);
	OP_STATUS	OpenAndEnableIfRequired(BOOL& has_started, BOOL always_ask, BOOL delay_for_service);

	OP_STATUS	ChangeAccount();

	// Takes you to the root service page
	void			OpenRootService();
	const uni_char	*GetRootServiceAddress(BOOL admin_url = FALSE);

	OP_STATUS LoadCustomDefaultServices();
	OP_STATUS LoadDefaultServices();

	BOOL		URLNeedsDelayForWebServer(const OpStringC & url_str);
	OP_STATUS	AddAutoLoadWindow(unsigned long window_id, const OpStringC & url_str);

	BOOL        HasUsedWebServer();

	WebServerWidgetStateModifier*		GetWidgetStateModifier()	{ return &m_state_modifier; }

	const WebServerSettingsContext &	GetWebserverSettings() { return GetWebserverSettings(FALSE); }
	const WebServerSettingsContext &	GetWebserverSettings(BOOL force_enable);

	void        UpdateOffline(BOOL offline);
	virtual OP_STATUS	GetURLForComputerName(OpString & url, const OpStringC & computername) { return GetURLForComputerName(url, computername, FALSE); } 
	virtual OP_STATUS	GetURLForComputerName(OpString & url, const OpStringC & computername, BOOL admin_url);

	virtual void SetConfiguring(BOOL configuring) { m_webserver_ui->SetConfiguring(configuring); }
	virtual BOOL IsConfiguring() { return m_webserver_ui->IsConfiguring(); }

	// OpPrefsListener interface
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);
	
	OperaAccountManager::OAM_Status GetStatus() { return m_status; }
	OperaAccountManager::OAM_Status GetUploadStatus() { return m_upload_status; }

	BOOL				IsFeatureAllowed() const;
	// FeatureController implementation
	virtual BOOL		IsFeatureEnabled() const;
	BOOL				IsFeatureEnabling() const { return m_enabling; }
	void				EnableFeature() { EnableFeature(NULL); } // convenience function
	void				EnableFeature(const FeatureSettingsContext* settings);
	virtual void		EnableFeature(const FeatureSettingsContext* settings, BOOL force_device_release);
	void				DisableFeature(BOOL write_pref, BOOL shutdown);
	virtual void		DisableFeature() { DisableFeature(TRUE, FALSE); }
	void				Shutdown(); // disabling feature without setting pref to disabled
	virtual OP_STATUS	Restart();

	virtual OP_STATUS	GetWebServerStatusContext(WebServerStatusContext & context);

	virtual void	SetFeatureSettings(const FeatureSettingsContext* settings);
	virtual void	InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2);
	void			CloseAllServices();

	/**
	* If TRUE, the service wants the home page opening to be delayed until the service is started.
	*/
	BOOL			IsHomeServicePageDelayedForService() { return m_delay_home_page != NoDelay; }

	// WebserverEventListener interface 
	virtual void	OnWebserverStopped(WebserverStatus status);
	virtual void	OnWebserverServiceStarted(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE);
	virtual void	OnWebserverServiceStopped(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE);
	virtual void	OnWebserverListenLocalStarted(unsigned int port);
	virtual void	OnWebserverListenLocalStopped();
	virtual void	OnWebserverListenLocalFailed(WebserverStatus status);
#ifdef WEB_UPLOAD_SERVICE_LIST	
	virtual void	OnWebserverUploadServiceStatus(UploadServiceStatus status);
#endif // WEB_UPLOAD_SERVICE_LIST	
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

	virtual void	OnProxyConnected();
	virtual void	OnProxyConnectionFailed(WebserverStatus status, BOOL retry);
	virtual void	OnProxyConnectionProblem(WebserverStatus status,  BOOL retry);
	virtual void	OnProxyDisconnected(WebserverStatus status, BOOL retry, int code = 0);
#endif
	virtual void	OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address);
	virtual void	OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path = NULL);
	virtual void	OnPortOpened(UPnPDevice *device, UINT16 internal_port, UINT16 external_port) {}
	virtual void	OnWebserverUPnPPortsClosed(UINT16 port) {}

	//== OAC_Listener implementation
	virtual void	OnOperaAccountCreate(OperaAuthError error, OperaRegistrationInformation& reg_info) {} // not needed
	virtual void	OnOperaAccountAuth(OperaAuthError error, const OpStringC& shared_secret);
	virtual void	OnOperaAccountReleaseDevice(OperaAuthError error);
	virtual void	OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message);
	virtual void	OnPasswordMissing();

#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
	// handling of local service discovery UI
	BOOL			IsLocalDiscoveryNotificationsEnabled();
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT

	// UI calls
	virtual OP_STATUS	ShowSetupWizard()	// not blocking
			{ return ShowSetupWizard(NULL); }

	virtual OP_STATUS	ShowSetupWizard(BOOL * webserver_enabled);
	virtual OP_STATUS	ShowSettingsDialog();

	virtual OP_STATUS	ShowStatus()
			{ return m_webserver_ui->ShowStatus(this); }

	virtual INT32		GetRunningServicesCount();

#ifdef TRANSFERS_TYPE
	virtual TRANSFERS_TYPE GetBytesUploaded()
	{ return m_webserver->GetBytesUploaded(); }

	virtual TRANSFERS_TYPE GetBytesDownloaded()
	{ return m_webserver->GetBytesDownloaded(); }
#endif
#ifdef WEBSERVER_CAP_STATS_NUM_USERS
	virtual UINT32 GetLastUsersCount(UINT32 seconds)
	{ return m_webserver->GetLastUsersCount(seconds); }
#endif // WEBSERVER_CAP_STATS_NUM_USERS

#ifdef UPNP_SUPPORT
	const OpVector<KnownDeviceMenuEntry> &	GetKnownDeviceList() const	{ return m_upnp_listener.GetKnownDeviceList(); }
#endif // UPNP_SUPPORT

#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
	void				SetHasDiscoveredServices(BOOL discovered_services);
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT

private:
	void				SetFeatureEnabling(BOOL enabling);
	OP_STATUS			SaveAdvancedSettings(const WebServerSettingsContext* webserver_settings, BOOL & needs_restart);
	/*
	** made private because it doesn't send Authenticate() and shouldn't be used by other classes
	** instead. use EnableFeature()/DisableFeature()
	*/
	virtual OP_STATUS	SetWebServerEnabled(BOOL enable, BOOL write_pref = TRUE, BOOL shutdown = FALSE);

	OP_STATUS   Start();
	void		Stop(BOOL shutdown = FALSE);

	// Adds all services that are in the opfolder except if opfolder is OPFILE_UNITE_PACKAGE_FOLDER it will skip the home service to the unite panel
	// They will not be installed per se because that requires setting up shared folders 
	// and this function is supposed to be called on upgrade
	OP_STATUS				LoadDefaultServicesFromFolder(OpFileFolder opfolder);

	// Gets the webserver listening mode based on preferences set
	WebserverListeningMode GetWebServerListeningMode();

	BOOL		IsUserRequestingDeviceRelease(DesktopWindow * parent_window = NULL);

	void		SetSharedSecret(const OpStringC& shared_secret);

	BOOL		IsValidDeviceName(const OpStringC & device_name);
	void		SaveDeviceName();
	OP_STATUS	ReleaseDevice(const OpStringC & device_name);
	OP_STATUS	RegisterDevice(const OpStringC & device_name, BOOL force = FALSE);

	void SetIsRunning(BOOL is_running); //< Changes running state and informs all the listeners about it.

	OP_STATUS	GetServiceAddressFromPath(const OpStringC & url_str, OpString & service_address);
	OP_STATUS	OpenCachedPagesForService(const OpStringC & service_name);

	void HandleWebserverError(WebserverStatus status);
	void HandleWebserverUploadServiceError(UploadServiceStatus status);

protected:
	// references to otherwise static managers etc.
	OperaAccountManager *		m_opera_account_manager;
	DesktopGadgetManager *		m_desktop_gadget_manager;
	HotlistManager *			m_hotlist_manager;
	WebServerConnector *		m_webserver;
	WebServerPrefsConnector *	m_webserver_prefs;
	WebServerUIConnector *		m_webserver_ui;

private:
	BOOL m_inited;						// Set to TRUE after a successful Init() call
	BOOL m_running;						// Set when the webserver is enabled and actually running
	BOOL m_enabling;					// Set when the webserver is currently being enabled

	OpString8 m_shared_secret;			// Shared secret for the webserver to start up with
	OpString  m_root_service_address;	// Holds the url of the root service
	BOOL      m_force_open_home_service;
	
	OperaAccountManager::OAM_Status m_status;			// Current status of the webserver service
	OperaAccountManager::OAM_Status m_upload_status;	// Current status of the webserver upload service

	// -------
	WebServerWidgetStateModifier	m_state_modifier;
	WebServerSettingsContext		m_settings_context;
	BOOL							m_schedule_feature_enabling;
	BOOL							m_schedule_feature_change;

	enum HomePageDelayStatus
	{
		NoDelay,			// if opening the home service is requested when starting Unite, the page is opened right away
		DelayRequested,		// delaying to open the home service page later was requested (in EnableIfRequired)
		DelayTriggered		// the delay was triggered, ie, the home service page would have been opened by now.
							// work-around to find out that opening the root service doesn't result in a 404 
	};
	HomePageDelayStatus				m_delay_home_page;
	UniteServiceModelItem *			m_service_to_open;

	MessageHandler*					m_msg_handler;
	INT32							m_user_wants_device_release;
	OpString						m_direct_access_address;
#ifdef UPNP_SUPPORT
	DesktopUPnPListener				m_upnp_listener;
#endif // UPNP_SUPPORT

	struct ServiceWindowMap
	{
		OpString service_id;
		OpINT32Vector window_ids;
	};
	OpStringHashTable<ServiceWindowMap>	m_auto_load_windows; // holds service address and an int32 vector with all window id's for that service to be opened
};

#endif // WEBSERVER_SUPPORT
#endif // __WEBSERVER_MANAGER_H__
