/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef ST_WEBSERVER_MANAGER_H
#define ST_WEBSERVER_MANAGER_H

#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "modules/opera_auth/opera_auth_myopera.h"
#include "adjunct/quick/hotlist/HotlistGenericModel.h"

/***********************************************************************************
**  @class	FakeFeatureControllerListener
**	@brief	
************************************************************************************/
class FakeFeatureControllerListener
		: public FeatureControllerListener, public OpTimerListener
{
public:
	enum PassingCriteria
	{
		None,
		FeatureEnabled,
		FeatureSettingsChanged
	};

	FakeFeatureControllerListener() : m_passing_criteria(None)
	{
		m_timer.SetTimerListener(this);
	}

	virtual void	OnFeatureEnablingSucceeded()
	{
		if (m_passing_criteria == FeatureEnabled)
		{
			ST_passed();
		}
		else
		{
			ST_failed("Unexpected OnFeatureEnablingSucceeded");
		}
		m_timer.Stop();
	}
	virtual void	OnFeatureSettingsChangeSucceeded()
	{
		m_timer.Stop();
		if (m_passing_criteria == FeatureSettingsChanged)
		{
			ST_passed();
		}
		else
		{
			ST_failed("Unexpected OnFeatureSettingsChangeSucceeded");
		}
	}
	
	//=== helper functions ===
	void SetPassingCriteria(PassingCriteria passing_criteria, UINT32 time_out)
	{
		m_passing_criteria = passing_criteria;
		m_timer.Start(time_out);
	}

	// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer)
	{
		if (timer == &m_timer)
		{
			ST_failed("Timed out");
		}
	}

private:
	PassingCriteria m_passing_criteria;
	OpTimer m_timer;
};


/***********************************************************************************
**  @class	FakeWebServerConnector
**	@brief	
************************************************************************************/
class FakeWebServerConnector : public WebServerConnector
{

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual OP_STATUS Start(WebserverListeningMode mode, const char *shared_secret = NULL)
	{ return OpStatus::OK; } // todo: stub
#else
	virtual OP_STATUS Start(WebserverListeningMode mode)
	{ return OpStatus::OK; } // todo: stub
#endif

	virtual OP_STATUS Stop(WebserverStatus status = WEBSERVER_OK)
	{ return OpStatus::OK; } // todo: stub

	virtual OP_STATUS AddWebserverListener(WebserverEventListener *listener)
	{ return OpStatus::OK; } // todo: stub

	virtual void RemoveWebserverListener(WebserverEventListener *listener)
	{} // todo: stub

#ifdef WEB_UPLOAD_SERVICE_LIST
	virtual OP_STATUS StartServiceDiscovery(const OpStringC &sentral_server_uri, const OpStringC8 &session_token)
	{ return OpStatus::OK; } // todo: stub

	virtual void StopServiceDiscovery()
	{} // todo: stub
#endif //WEB_UPLOAD_SERVICE_LIST

#ifdef TRANSFERS_TYPE
	virtual TRANSFERS_TYPE GetBytesUploaded()
	{ return 0; } // todo: stub

	virtual TRANSFERS_TYPE GetBytesDownloaded()
	{ return 0; } // todo: stub
#endif
	virtual UINT32 GetLastUsersCount(UINT32 seconds)
	{ return 0; } // todo: stub
};



/***********************************************************************************
**  @class	FakeWebServerPrefsConnector
**	@brief	
************************************************************************************/
class FakeWebServerPrefsConnector : public WebServerPrefsConnector
{
public:
	FakeWebServerPrefsConnector() :
		WebServerPrefsConnector(),
		m_device_name(),
		m_empty_str(),
		m_webserver_enabled(FALSE),
		m_use_opera_account(TRUE)
	{}

	FakeWebServerPrefsConnector(const OpStringC & device_name, BOOL webserver_enabled, BOOL use_opera_account) :
		WebServerPrefsConnector(),
		m_device_name(),
		m_empty_str(),
		m_webserver_enabled(webserver_enabled),
		m_use_opera_account(use_opera_account)
	{}

	virtual BOOL ResetStringL(PrefsCollectionWebserver::stringpref which)
	{ return TRUE; } // todo: stub

	virtual void GetStringPrefL(PrefsCollectionWebserver::stringpref which, OpString &result) const
	{
		switch(which)
		{
		case PrefsCollectionWebserver::WebserverDevice:
			{
				result.Set(m_device_name.CStr());
				break;
			}
		default:
			result.Empty();
		}
	}

	virtual const OpStringC GetStringPref(PrefsCollectionWebserver::stringpref which) const
	{
		switch(which)
		{
		case PrefsCollectionWebserver::WebserverDevice:
			{
				return m_device_name;
				break;
			}
		default:
			return m_empty_str;
		}
	}

	virtual OP_STATUS WriteStringL(PrefsCollectionWebserver::stringpref which, const OpStringC &value)
	{
		switch(which)
		{
		case PrefsCollectionWebserver::WebserverDevice:
			{
				return m_device_name.Set(value.CStr());
			}
		default:
			return OpStatus::ERR;
		}
	}

	virtual int GetIntegerPref(PrefsCollectionWebserver::integerpref which) const
	{
		switch(which)
		{
		case PrefsCollectionWebserver::WebserverEnable:
			{
				return m_webserver_enabled;
			}
		// FIXME: tkupczyk - merging Extensions Phase 2
		case PrefsCollectionWebserver::UseOperaAccount:
			{
				return m_use_opera_account;
			}
		default:
			return 0;
		}
	}

	virtual OP_STATUS WriteIntegerL(PrefsCollectionWebserver::integerpref which, int value)
	{ return OpStatus::OK; } // todo: stub

	virtual void CommitL()
	{} // todo: stub

private:
	OpString	m_device_name;
	OpString	m_empty_str;
	BOOL		m_webserver_enabled;
	BOOL		m_use_opera_account;
};

/***********************************************************************************
**  @class	FakeWebServerUIConnector
**	@brief	
************************************************************************************/
class FakeWebServerUIConnector : public WebServerUIConnector
{
public:
	FakeWebServerUIConnector() :
	  m_wants_takeover(TRUE),
	  m_wants_reconnect(TRUE)
	{}
	
	// blocking if webserver_enabled != NULL
	virtual OP_STATUS	ShowSetupWizard(WebServerManager * manager, BOOL * webserver_enabled,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context)
	{
		// TODO: stub
		return OpStatus::OK;
	}

	virtual OP_STATUS	ShowSettingsDialog(WebServerManager * manager,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context)
	{
		// TODO: stub
		return OpStatus::OK;
	}

	virtual OP_STATUS	ShowLoginDialog(WebServerManager * manager,
		const WebServerSettingsContext & settings_context,
		OperaAccountController * account_controller,
		OperaAccountContext* account_context, BOOL * logged_in)
	{
		// TODO: stub
		return OpStatus::OK;
	}

	virtual OP_STATUS	ShowStatus(WebServerManager * manager, OperaAccountManager::OAM_Status status)
	{
		// TODO: stub
		return OpStatus::OK;
	}

	virtual BOOL		UserWantsFeatureEnabling(DesktopWindow * parent_window = NULL)
	{
		return m_wants_enabling;
	}

	virtual BOOL		UserWantsInvalidUsernamesChange(OperaAccountContext * account_context, DesktopWindow * parent_window = NULL)
	{
		return m_wants_username_change;
	}

	virtual BOOL		UserWantsDeviceTakeover(const OpStringC & device_name, DesktopWindow * parent_window = NULL)
	{
		return m_wants_takeover;
	}

	virtual BOOL		UserWantsReconnectAfterTakeover(DesktopWindow * parent_window = NULL)
	{
		return m_wants_reconnect;
	}

	virtual void		OnWebServerIsOutdated(DesktopWindow * parent_window = NULL)
	{
	}

	virtual OP_STATUS	HandleErrorInDialog(OperaAuthError error)
	{
		return OpStatus::OK;
	}

	virtual void		LogError(WebserverStatus status)
	{ /* stub */ }

	virtual void		LogError(UploadServiceStatus status)
	{ /* stub */ }

	//=== helper functions ==============
	void		SetWantsEnabling(BOOL enabling)		{ m_wants_enabling = enabling; }
	void		SetWantsUsernameChange(BOOL change)	{ m_wants_username_change = change; }
	void		SetWantsTakeover(BOOL takeover)		{ m_wants_takeover = takeover; }
	void		SetWantsReconnect(BOOL reconnect)	{ m_wants_reconnect = reconnect; }

private:
	BOOL		m_wants_enabling;
	BOOL		m_wants_username_change;
	BOOL		m_wants_takeover;
	BOOL		m_wants_reconnect;
};


/***********************************************************************************
**  @class	FakeOperaAccountManager
**	@brief	
************************************************************************************/
class FakeOperaAccountManager : public OperaAccountManager
{
public:
	virtual void		SetLoggedIn(BOOL logged_in)
	{ m_account_context.SetLoggedIn(TRUE); }

	virtual BOOL		GetLoggedIn()
	{ return m_account_context.IsLoggedIn(); }

	virtual void		StopServices()
	{} // todo: stub

	virtual OperaAccountContext*	GetAccountContext()
	{ return &m_account_context; }
	
	virtual void SetUsernameAndPassword(const uni_char *username, const uni_char *password)
	{
		m_account_context.SetUsername(username);
		m_account_context.SetPassword(password);
	}

	virtual const uni_char*			GetUsername()
	{ return m_account_context.GetUsername().CStr(); } 
	
	virtual const uni_char*			GetPassword()
	{ return m_account_context.GetPassword().CStr(); } 

	virtual const char*				GetToken()
	{ return "token"; } // todo: stub
	
	virtual void		GetInstallID(OpString &install_id)
	{ install_id.Set("install_id"); } // todo: stub

	virtual OP_STATUS Authenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force = FALSE)
	{
		if (install_id.HasContent() && device_name.HasContent())
		{
			if (!force && m_current_device.HasContent())
			{
				BroadcastCreateDeviceEvent(AUTH_ACCOUNT_CREATE_DEVICE_EXISTS, m_shared_secret, m_message);
			}
			else
			{
				SetCurrentDevice(device_name);
				m_shared_secret.Set("secret");
				BroadcastCreateDeviceEvent(AUTH_OK, m_shared_secret, m_message);
			}
		}
		else
		{
			BroadcastAuthEvent(AUTH_OK, m_shared_secret);
		}
		return OpStatus::OK;
	}

	virtual OP_STATUS ReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename)
	{
		if (m_current_device.Compare(devicename) == 0)
		{
			m_current_device.Empty();
			BroadcastReleaseDeviceEvent(AUTH_OK);
		}
		else
		{
			BroadcastCreateDeviceEvent(AUTH_ACCOUNT_AUTH_INVALID_KEY, m_shared_secret, m_message);
		}
		return OpStatus::OK;
	}

	virtual void		LogError(INT32 error_code, const OpStringC& context, const OpStringC& message)
	{} // todo: stub

	// helper
	OP_STATUS			SetCurrentDevice(const OpStringC & current_device)
	{
		return m_current_device.Set(current_device.CStr());
	}

private:
	OperaAccountContext		m_account_context;
	OpString				m_shared_secret;
	OpString				m_message;
	OpString				m_current_device;
};


/***********************************************************************************
**  @class	FakeDesktopGadgetManager
**	@brief	
************************************************************************************/
class FakeDesktopGadgetManager : public DesktopGadgetManager
{
public:
	virtual INT32		GetRunningServicesCount()
	{ return 0; } // todo: stub

	virtual OP_STATUS	OpenRootService(BOOL open_home_page)
	{ return OpStatus::OK; } // todo: stub

	virtual void		CloseRootServiceWindow(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE)
	{} // todo: stub

	virtual BOOL		IsGadgetInstalled(const OpStringC &service_path)
	{ return FALSE; } // todo: stub

};



/***********************************************************************************
**  @class	FakeHotlistManager
**	@brief	
************************************************************************************/
class FakeHotlistManager : public HotlistManager
{
public:
	FakeHotlistManager() : m_model(HotlistModel::UniteServicesRoot, TRUE) {}

	virtual HotlistGenericModel*   GetUniteServicesModel()
	{
		return &m_model;
	}

#ifdef WEBSERVER_SUPPORT
	virtual BOOL			NewUniteService(const OpStringC& address, 
			 const OpStringC& orig_url, 
			 INT32* got_id, 
			 BOOL clean_uninstall, 
			 BOOL launch_after_install,
			 const OpStringC& force_widget_name, 
			 const OpStringC& force_service_name,
			 const OpStringC& shared_path)
	{
		OP_STATUS status;
		HotlistModelItem* item = m_model.AddItem(OpTreeModelItem::UNITE_SERVICE_TYPE, status, FALSE);
		if (OpStatus::IsError(status))
		{
			return FALSE;
		}
		item->SetName(force_widget_name.CStr()); // todo: stub
		item->SetUrl(orig_url.CStr());
		return TRUE;
	}
#endif //WEBSERVER_SUPPORT

	HotlistModelItem *	GetItemByID( INT32 id )
	{
		return m_model.GetItemByID(id);
	}

private:
	HotlistGenericModel	m_model;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
class ST_WebServerManager : public WebServerManager
{
public:
	virtual OP_STATUS Init(OperaAccountManager * opera_account_manager, DesktopGadgetManager * desktop_gadget_manager, HotlistManager * hotlist_manager)
	{
		// deleted by WebServerManager
		SetWebServerConnector(OP_NEW(FakeWebServerConnector, ()));
		SetWebServerPrefsConnector(OP_NEW(FakeWebServerPrefsConnector, ()));
		SetWebServerUIConnector(OP_NEW(FakeWebServerUIConnector, ()));

		return WebServerManager::Init(opera_account_manager, desktop_gadget_manager, hotlist_manager);
	}

	DesktopGadgetManager *	GetDesktopGadgetManager()	{ return m_desktop_gadget_manager; }
	OperaAccountManager	*	GetOperaAccountManager()	{ return m_opera_account_manager; }
	HotlistManager *		GetHotlistManager()			{ return m_hotlist_manager; }
};

#endif // ST_WEBSERVER_MANAGER_H
