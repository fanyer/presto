// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#include "adjunct/quick/managers/OperaAccountManager.h"

#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "modules/console/opconsoleengine.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/regexp/include/regexp_api.h" 
#include "modules/opera_auth/opera_auth_myopera.h"

#include "modules/widgets/OpEdit.h"

#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountContext::IsUsernameTooShort() const
{
	return m_username.Length() < 3;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountContext::IsValidUsername() const
{
	if (!IsUsernameTooShort() && m_username.Length() <= GetUsernameMaxLength() && m_username.Find("xn--") != 0)
	{
		OpStringC pattern(UNI_L("[+.@/?&_ ]"));
		OpRegExp *regex;

		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = FALSE;
		flags.ignore_whitespace = FALSE;

		if (OpStatus::IsError(OpRegExp::CreateRegExp(&regex, pattern.CStr(), &flags)))
		{
			OP_ASSERT(FALSE); // faulty regex?
			return FALSE;
		}
		
		OpREMatchLoc match_pos;
		OP_STATUS status = regex->Match(m_username.CStr(), &match_pos);
		OP_DELETE(regex);

		if (OpStatus::IsError(status))
		{
			OP_ASSERT(FALSE);
			return FALSE;
		}

		if (match_pos.matchloc == REGEXP_NO_MATCH)
		{	// check that the username can be used to create a servername (characters must be translatable into punycode)
			
			ServerName *server = 0; // server object shouldn't be deleted, according to GetServerName declaration comment

			OpString host;
			host.AppendFormat(UNI_L("%s.opera.com"), m_username.CStr()); // create dummy servername

#ifdef URL_CAP_ATTRIBUTE_API
			server = g_url_api->GetServerName(host, TRUE);
#else
			unsigned short port = 80;
			server = urlManager->GetServerName(host.CStr(), port, TRUE);
#endif

			return (server != 0); // if a domain could be created, the translation went fine
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountContext::IsPasswordTooShort() const
{
	return m_password.Length() < GetPasswordMinLength();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountContext::IsValidPassword() const
{
	return !IsPasswordTooShort();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountContext::IsValidEmailAddress() const 
{
	return m_email_address.HasContent() ? FormManager::IsValidEmailAddress(m_email_address.CStr()) : FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OperaAccountManager::OperaAccountManager() :
	m_current_task(CurrentTaskNone),
	m_inited(FALSE),
	m_last_signup_date(0),
	m_num_signups(0),
	m_offline(FALSE),
	m_auto_login(FALSE),
	m_enable_link(FALSE),
	m_account_context(),
	m_opera_authentication(NULL)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OperaAccountManager::~OperaAccountManager()
{
	if (IsInited())
	{
#ifdef WEBSERVER_SUPPORT
		if (g_pcwebserver) g_pcwebserver->UnregisterListener(this);
#endif // WEBSERVER_SUPPORT
	
#ifdef SUPPORT_DATA_SYNC
		if (g_pcsync) g_pcsync->UnregisterListener(this);
#endif // SUPPORT_DATA_SYNC
	
		if(m_opera_authentication)
		{
			m_opera_authentication->SetListener(NULL);
			OP_DELETE(m_opera_authentication);
		}
		PrivacyManager::GetInstance()->RemoveCallback(this);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::Init()
{
	// Earlier this object was created in core (in opera_auth module),
	// but since core doesn't need g_opera_authentication global variable
	// anymore they removed it, but we're heavy dependent on it because
	// of DSK-331178, so now we need to create our own instance.
	m_opera_authentication = OP_NEW(MyOperaAuthentication, ());
	RETURN_OOM_IF_NULL(m_opera_authentication);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is the real initialization trigger when the main browser window has been created.  Starting this machinery
// any earlier might cause side effects such as seen in DSK-333039
OP_STATUS OperaAccountManager::Start()
{
	m_enable_link = TRUE; 

	// Listen to the Auth responses
	if(m_opera_authentication)
		m_opera_authentication->SetListener(this);

	// Read the current username
	OpString username;
	GetUsername(username);

	if (username.HasContent() && IsServiceEnabled())
	{
		// Try and autologin the user
		m_current_task = CurrentTaskAuthenticate;
		OpStatus::Ignore(GetWandPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, this));
	}

#ifdef WEBSERVER_SUPPORT
	if (g_pcwebserver) g_pcwebserver->RegisterListenerL(this);
#endif // WEBSERVER_SUPPORT

#ifdef SUPPORT_DATA_SYNC
	if (g_pcsync) g_pcsync->RegisterListenerL(this);
#endif // SUPPORT_DATA_SYNC

	m_inited = TRUE;

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ void
OperaAccountManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
#ifdef WEBSERVER_SUPPORT
	if (id == OpPrefsCollection::Webserver && pref == PrefsCollectionWebserver::WebserverEnable && newvalue == FALSE)
	{
		LogoutIfNoServicesRunning();
	}
#endif // WEBSERVER_SUPPORT

#ifdef SUPPORT_DATA_SYNC
	if (id == OpPrefsCollection::Sync && pref == PrefsCollectionSync::SyncEnabled && newvalue == FALSE)
	{
		LogoutIfNoServicesRunning();
	}
#endif // SUPPORT_DATA_SYNC
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::SetLoggedIn(BOOL logged_in)
{
	if (m_account_context.IsLoggedIn() == logged_in)
		return;

	m_account_context.SetLoggedIn(logged_in);

	// If this is a logout kill the password from the wand!
	if (!logged_in)
	{
		// reset password. if logged out, the account manager should
		// forget about the password
		m_account_context.SetPassword(NULL);

		// don't wipe the password if user want to keep it 
#ifdef PREFS_CAP_ACCOUNT_SAVE_PASSWORD
		if(!g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword))
#endif // PREFS_CAP_ACCOUNT_SAVE_PASSWORD
		{
			OpString username;
			GetUsername(username);

			if (username.HasContent())
			{
				// Wipe the wand password
				PrivacyManager::GetInstance()->DeletePassword(WAND_OPERA_ACCOUNT, UNI_L(""), username.CStr());
			}
		}

#ifdef WEBSERVER_SUPPORT
		// Set the status to disabled
		m_status = OperaAccountManager::Disabled;
#endif // WEBSERVER_SUPPORT
	}
	else
	{
#ifndef WEBSERVER_SUPPORT
		// If this is the first use of the Opera Account then reload the toolbar so the Link icon will appear
		// only needed for the Link icon in non-Unite builds, which is not always visible. see also crash [DSK-260195]
		if (!g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::OperaAccountUsed))
			g_application->SettingsChanged(SETTINGS_TOOLBAR_CONTENTS);
#endif // WEBSERVER_SUPPORT
		TRAPD(err, g_pcopera_account->WriteIntegerL(PrefsCollectionOperaAccount::OperaAccountUsed, TRUE));
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OperaAccountManager::GetLoggedIn()
{
	return m_account_context.IsLoggedIn();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL
OperaAccountManager::IsServiceEnabled()
{
	BOOL service_enabled = FALSE;

#ifdef SUPPORT_DATA_SYNC
	// Opera Link
	if (g_pcsync && m_enable_link && g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncEnabled))
	{
		service_enabled = TRUE;
	}
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
	// Opera Unite
	if (g_pcwebserver && g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable))
	{
		service_enabled = TRUE;
	}
#endif // WEBSERVER_SUPPORT

	return service_enabled;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
OperaAccountManager::LogoutIfNoServicesRunning()
{
	if (!m_account_context.IsLoggedIn()) // already logged out
		return;

	BOOL service_enabled = IsServiceEnabled();
	if (!service_enabled)
	{
		SetLoggedIn(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::SetIsOffline(BOOL offline)
{
	m_offline = offline;

#ifdef SUPPORT_DATA_SYNC
		g_sync_manager->UpdateOffline(offline);
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
	g_webserver_manager->UpdateOffline(offline);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
OperaAccountManager::ClearUsernameAndPassword()
{
	OP_ASSERT(!GetLoggedIn());
	if (!GetLoggedIn())
	{
		m_account_context.SetUsername(NULL);
		m_account_context.SetPassword(NULL);
	}
	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::SetUsernameAndPassword(const uni_char *username, const uni_char *password)
{
	m_account_context.SetUsername(username);
	m_account_context.SetPassword(password);

#ifdef SUPPORT_DATA_SYNC
	// Send the username and password to the sync module
	OpString uname;
	uname.Set(username);
	OpString pword;
	pword.Set(password);

	g_sync_coordinator->SetLoginInformation(MYOPERA_PROVIDER, uname, pword);
#endif // SUPPORT_DATA_SYNC
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::SaveCredentials(const OpStringC & username, const OpStringC & password)
{
	if (username.HasContent())
	{
		// save username to prefs
		OP_STATUS rc;
		TRAP(rc, g_pcopera_account->WriteStringL(PrefsCollectionOperaAccount::Username, username));

#ifdef WEBSERVER_SUPPORT
		if (OpStatus::IsSuccess(rc))
		{
			OpString lower_username;
			
			// Save in lowercase for the webserver since it wants to use this name in domain names
			lower_username.Set(username);
			lower_username.MakeLower();

			TRAP(rc, g_pcwebserver->WriteStringL(PrefsCollectionWebserver::WebserverUser, lower_username));
		}
#endif // WEBSERVER_SUPPORT
		if (password.HasContent())
		{
			// save password to Wand
			RETURN_IF_ERROR(PrivacyManager::GetInstance()->SetPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, password));
		}
		else
		{
			RETURN_IF_ERROR(PrivacyManager::GetInstance()->DeletePassword(WAND_OPERA_ACCOUNT, UNI_L(""), username));
		}

		return rc;
	}
	return OpStatus::ERR;
}

OP_STATUS
OperaAccountManager::GetUsername(OpString & username) const
{
	RETURN_IF_ERROR(username.Set(m_account_context.GetUsername().CStr()));
	if (!username.HasContent())
	{
		RETURN_IF_ERROR(GetUserNameFromPrefs(username));
	}
	return OpStatus::OK;
} 

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
OperaAccountManager::CreateAccount(OperaRegistrationInformation& reg_info)
{
	return m_opera_authentication->CreateAccount(reg_info);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
OperaAccountManager::ChangePassword(const OpStringC& username, 
		const OpStringC& old_password, const OpStringC& new_password)
{
	RETURN_IF_ERROR(
			m_passwd_change_context.m_username.Set(username));

	if (g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword))
	{
		RETURN_IF_ERROR(
				m_passwd_change_context.m_old_password_from_wand.Set(m_account_context.GetPassword()));
	}

	RETURN_IF_ERROR(
			m_passwd_change_context.m_old_password_from_user.Set(old_password));
	RETURN_IF_ERROR(
			m_passwd_change_context.m_new_password.Set(new_password));
	m_passwd_change_context.m_valid = TRUE;
	
	g_sync_manager->SetLinkEnabled(FALSE, FALSE, FALSE);

	// If Unite is turned off we can start password change request,
	// otherwise we need to wait unitl server tells us that it is
	// completely turned off and start password change request right
	// after that (see OperaAccountManager::OnAuthReleaseDevice).

	if (!g_webserver_manager->IsFeatureEnabled())
	{
		OP_STATUS ret = m_opera_authentication->ChangePassword(username, 
				old_password, new_password);
		if (OpStatus::IsError(ret))
		{
			OnAuthPasswordChanged(AUTH_ERROR);

			return ret;
		}
	}
	else
	{
		// We need also to disable Unite, then we should wait
		// on OnAuthReleaseDevice before starting password change.

		g_webserver_manager->DisableFeature(FALSE, FALSE);
	}

	// Call to this kills the password in wand, but if user remembered
	// his password we have it in m_passwd_change_context.m_old_password_from_wand.
	// We'll use it in case something fails to login.

	SetLoggedIn(FALSE);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
OperaAccountManager::Authenticate(const OpStringC& username, const OpStringC& password)
{
	OpString device_name, install_id;
	return OAuthAuthenticate(username, password, device_name, install_id);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
OperaAccountManager::RegisterDevice(const OpStringC& device_name, BOOL force)
{
	OP_ASSERT(m_current_task == CurrentTaskNone);

	OpString username;
	GetUsername(username);
	if (username.HasContent())
	{
		RETURN_IF_ERROR(m_account_context.SetDeviceName(device_name));
		m_current_task = force ? CurrentTaskRegisterDeviceForce : CurrentTaskRegisterDevice;

		if (m_account_context.GetPassword().HasContent())
		{
			// no need to retrieve password, it's already there
			OnPasswordRetrieved(m_account_context.GetPassword());
			return OpStatus::OK;
		}
		else
		{
			return GetWandPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, this);
		}
	}

	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ OP_STATUS
OperaAccountManager::ReleaseDevice(const OpStringC& devicename)
{
	OP_ASSERT(m_current_task == CurrentTaskNone);

	OpString username;
	GetUsername(username);
	if (username.HasContent())
	{
		RETURN_IF_ERROR(m_account_context.SetDeviceName(devicename));
		m_current_task = CurrentTaskReleaseDevice;
		
		if (m_account_context.GetPassword().HasContent())
		{
			// no need to retrieve password, it's already there
			OnPasswordRetrieved(m_account_context.GetPassword());
			return OpStatus::OK;
		}
		else
		{
			return GetWandPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, this);
		}
	}
	return OpStatus::ERR;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::StartServices(BOOL auto_login)
{
	// Start up all of the services that are enabled

#ifdef SUPPORT_DATA_SYNC
	// Opera Link
	if (g_pcsync && g_sync_manager && m_enable_link)
	{
		if (g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncEnabled))
		{
			// Opera link is enabled so fire it up!
			g_sync_manager->SetLinkEnabled(TRUE, auto_login);
		}
	}
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
	// Alien
	if (g_pcwebserver && g_webserver_manager)
	{
		if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable))
		{
			// Start the server now
			StartupType::RunType type = g_run_type->m_type;

			// Only do the copying on an upgrade
			BOOL first = FALSE;
			if (type == StartupType::RUNTYPE_FIRST || 
				type == StartupType::RUNTYPE_FIRSTCLEAN ||
				type == StartupType::RUNTYPE_FIRST_NEW_BUILD_NUMBER) 
				first = TRUE;
			g_webserver_manager->EnableFeature();
		}
	}
#endif // WEBSERVER_SUPPORT

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::StopServices()
{
	// Stop all the services

#ifdef SUPPORT_DATA_SYNC
	if (g_sync_manager)
	{
		// Kill the Opera Link service, this resets the pref as well
		g_sync_manager->SetLinkEnabled(FALSE);
	}
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
	if (g_webserver_manager)
	{
		// Stop Unite
		g_webserver_manager->DisableFeature();
	}
#endif // WEBSERVER_SUPPORT

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastOperaAccountPasswordChanged(OperaAuthError err)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOperaAccountPasswordChanged(err);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastCreateAccountEvent(OperaAuthError error, OperaRegistrationInformation& reg_info)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOperaAccountCreate(error, reg_info);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastAuthEvent(OperaAuthError error, const OpStringC &shared_secret)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOperaAccountAuth(error, shared_secret);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastReleaseDeviceEvent(OperaAuthError error)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOperaAccountReleaseDevice(error);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastCreateDeviceEvent(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOperaAccountDeviceCreate(error, shared_secret, server_message);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::BroadcastOnPasswordMissing()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPasswordMissing();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnAuthCreateAccount(OperaAuthError error, OperaRegistrationInformation& reg_info)
{
	BroadcastCreateAccountEvent(error, reg_info);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnAuthPasswordChanged(OperaAuthError err)
{
	if (err == AUTH_OK && m_passwd_change_context.m_valid)
	{
		if (g_pcopera_account->GetIntegerPref(PrefsCollectionOperaAccount::SavePassword))
			SaveCredentials(m_passwd_change_context.m_username, 
					m_passwd_change_context.m_new_password);

		m_current_task = CurrentTaskAuthenticate;
		OnPasswordRetrieved(m_passwd_change_context.m_new_password);
	}
	else
	{
		// If there was an error from the server, we try to restart services with
		// old credentials but only when user remembered his password, otherwise
		// we don't restart any services and we don't trigger authentication, because
		// we simply can't.

		if (m_passwd_change_context.m_old_password_from_wand.HasContent())
		{
			m_current_task = CurrentTaskAuthenticate;
			OnPasswordRetrieved(m_passwd_change_context.m_old_password_from_wand);
		}
	}

	m_passwd_change_context.m_valid = FALSE;
	BroadcastOperaAccountPasswordChanged(err);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnAuthCreateDevice(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message)
{
	if(error == AUTH_OK)
	{
		// Set the status to OK, error's returned will correct this if it's wrong
		m_status = OK;

		// Make sure that the icon changes
		g_input_manager->UpdateAllInputStates();
	}
	else
	{
		// Log the error in the console
		LogError(error);
	}
	BroadcastCreateDeviceEvent(error, shared_secret, server_message);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void OperaAccountManager::OnAuthAuthenticate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message, const OpStringC& token)
{
	// Always hold onto the token here so it's up to date
	m_token.Set(token.CStr());

	// Set credentials regardless of auth result, this gives chance to
	// the services to notify the user if credentials are wrong.
	SetUsernameAndPassword(m_account_context.GetUsername().CStr(), 
			m_account_context.GetPassword().CStr());

	// AJMTODO hackish, rewrite properly but ok for labs
	if (m_auto_login)
	{
		if (error == AUTH_OK)
		{
			if (!GetLoggedIn()) // this might be a webserver device auth after we're already logged in
			{
				// Login to the Opera Account Manager
				SetLoggedIn(TRUE);

				// Set the status to OK, error's returned will correct this if it's wrong
				m_status = OK;

				// Make sure that the icon changes
				g_input_manager->UpdateAllInputStates();
			}
		}
		else
		{
			// Log the error in the console
			LogError(error);
		}

		// Start the services even though the authentication failed. They will 
		// handle the rest themselves (see DSK-269778).
		StartServices(TRUE);

		m_auto_login = FALSE;
	}
	else
	{
		// Authentication failed at this point we must stop all services
		// Device already exists should be ok, since it will be used for alot of testing
		if (error != AUTH_OK && error != AUTH_ACCOUNT_CREATE_DEVICE_EXISTS)
		{
			// Log the error in the console
			LogError(error);
		}
		else
		{
			// Otherwise the status must be ok
			m_status = OK;
		}
	}

	BroadcastAuthEvent(error, shared_secret);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnAuthReleaseDevice(OperaAuthError error)
{
	BroadcastReleaseDeviceEvent(error);

	if (error == AUTH_OK && m_passwd_change_context.m_valid)
	{
		OP_STATUS ret = m_opera_authentication->ChangePassword(
				m_passwd_change_context.m_username, m_passwd_change_context.m_old_password_from_user, 
					m_passwd_change_context.m_new_password);
		if (OpStatus::IsError(ret))
			OnAuthPasswordChanged(AUTH_ERROR);
	}
	else if (error != AUTH_OK && m_passwd_change_context.m_valid)
		OnAuthPasswordChanged(AUTH_ERROR);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::OAuthAuthenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force)
{
	return m_opera_authentication->Authenticate(username, password, device_name, install_id, force);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::OAuthReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename)
{
	return m_opera_authentication->ReleaseDevice(username, password, devicename);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::GetUserNameFromPrefs(OpString & username) const
{
	TRAPD(err, g_pcopera_account->GetStringPrefL(PrefsCollectionOperaAccount::Username, username));
	return err;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OperaAccountManager::GetWandPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, PrivacyManagerCallback* callback)
{
	return PrivacyManager::GetInstance()->GetPassword(WAND_OPERA_ACCOUNT, UNI_L(""), username, this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::LogError(OperaAuthError error)
{
	OpString context, message;
	INT32 error_code = OAM_ERROR;

	switch (error)
	{
		case AUTH_ERROR:
			context.Set(UNI_L("AUTH_ERROR"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR, message);
			error_code |= OAM_ERROR_TYPE_OTHER;
		break;

		case AUTH_ERROR_PARSER:
			context.Set(UNI_L("AUTH_ERROR_PARSER"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_PARSER, message);
			error_code |= OAM_ERROR_TYPE_OTHER;
		break;

		case AUTH_ERROR_MEMORY:
			context.Set(UNI_L("AUTH_ERROR_MEMORY"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_MEMORY, message);
			error_code |= OAM_ERROR_TYPE_OTHER;
		break;

		case AUTH_ERROR_SERVER:
			context.Set(UNI_L("AUTH_ERROR_SERVER"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_SERVER, message);
			error_code |= OAM_ERROR_TYPE_OTHER;
		break;

		case AUTH_ERROR_COMM_FAIL:
			context.Set(UNI_L("AUTH_ERROR_COMM_FAIL"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_COMM_FAIL, message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
		break;

		case AUTH_ERROR_COMM_ABORTED:
			context.Set(UNI_L("AUTH_ERROR_COMM_ABORTED"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_COMM_ABORTED, message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
		break;

		case AUTH_ERROR_COMM_TIMEOUT:
			context.Set(UNI_L("AUTH_ERROR_COMM_TIMEOUT"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_COMM_TIMEOUT, message);
			error_code |= OAM_ERROR_TYPE_NETWORK;
		break;

		case AUTH_ERROR_INVALID_REQUEST:
			context.Set(UNI_L("AUTH_ERROR_INVALID_REQUEST"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_INVALID_REQUEST, message);
			error_code |= OAM_ERROR_TYPE_OTHER;
		break;

// Dialog based so shouldn't happen here
//		case AUTH_ACCOUNT_CREATE_USER_EXISTS:
//		case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS:
//		case AUTH_ACCOUNT_CREATE_DEVICE_INVALID:
//		case AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR:
//		case AUTH_ACCOUNT_CREATE_USER_EMAIL_EXISTS:
//		case AUTH_ACCOUNT_CREATE_USER_TOO_SHORT:
//		case AUTH_ACCOUNT_CREATE_USER_TOO_LONG:
//		case AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS:

		case AUTH_ACCOUNT_AUTH_FAILURE:
			context.Set(UNI_L("AUTH_ACCOUNT_AUTH_FAILURE"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_AUTH_FAILURE, message);
			error_code |= OAM_ERROR_TYPE_AUTH;
		break;

		case AUTH_ACCOUNT_USER_UNAVAILABLE:
			context.Set(UNI_L("AUTH_ACCOUNT_USER_UNAVAILABLE"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_USER_UNAVAILABLE, message);
			error_code |= OAM_ERROR_TYPE_AUTH;
		break;

		case AUTH_ACCOUNT_AUTH_INVALID_KEY:
			context.Set(UNI_L("AUTH_ACCOUNT_AUTH_INVALID_KEY"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_AUTH_INVALID_KEY, message);
			error_code |= OAM_ERROR_TYPE_AUTH;
		break;

		case AUTH_ACCOUNT_USER_BANNED:
			context.Set(UNI_L("AUTH_ACCOUNT_USER_BANNED"));
			g_languageManager->GetString(Str::S_OPERA_ACCOUNT_ERROR_USER_BANNED, message);
			error_code |= OAM_ERROR_TYPE_AUTH;
		break;

		default:
			// Unhandled so do not log to console
			return;
		break;
	}

	// Convert from enum to error_code
	error_code |= (INT32)error;

	// Log the error to the console
	g_desktop_account_manager->LogError(error_code, context, message);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::LogError(INT32 error_code, const OpStringC& context, const OpStringC& message)
{
#ifdef OPERA_CONSOLE
	// Build a console error message
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Error);

	// Set the error information
	OpStatus::Ignore(log_message.context.Set(context));
	OpStatus::Ignore(log_message.message.Set(message));
	log_message.error_code = error_code;

	// Send the error to the console
	g_console->PostMessageL(&log_message);
#endif // OPERA_CONSOLE
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::LogWarning(INT32 error_code, const OpStringC& context, const OpStringC& message)
{
#ifdef OPERA_CONSOLE
	// Build a console warning message
	// OpConsoleEngine::Information == Str::D_CONSOLE_LEVEL_WARNING in MessageConsoleDialog
	OpConsoleEngine::Message log_message(OpConsoleEngine::OperaAccount, OpConsoleEngine::Information); 

	// Set the error information
	OpStatus::Ignore(log_message.context.Set(context));
	OpStatus::Ignore(log_message.message.Set(message));
	log_message.error_code = error_code;

	// Send the error to the console
	g_console->PostMessageL(&log_message);
#endif // OPERA_CONSOLE
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::GetInstallID(OpString &install_id)
{
	// Empty it first!
	install_id.Empty();

#ifdef WEBSERVER_SUPPORT
	if (g_pcopera_account)
	{
		// See if the preference holds an entry
		TRAPD(err, g_pcopera_account->GetStringPrefL(PrefsCollectionOperaAccount::ComputerID, install_id));

		// No install id so create one!
		if (install_id.IsEmpty())
		{
			StringUtils::GenerateClientID(install_id);

			// Now put it into the pref!
			TRAP(err, g_pcopera_account->WriteStringL(PrefsCollectionOperaAccount::ComputerID, install_id));
		}
	}
#endif // WEBSERVER_SUPPORT
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OperaAccountManager::OAM_Status OperaAccountManager::GetStatus()
{
	OAM_Status status = m_status;

#ifdef WEBSERVER_SUPPORT
#ifdef SUPPORT_DATA_SYNC
	// Get the lowest status of all the statuses
	status = min(g_sync_manager->GetStatus(), status);
#endif
	status = min(g_webserver_manager->GetStatus(), status);
	status = min(g_webserver_manager->GetUploadStatus(), status);
#endif // WEBSERVER_SUPPORT

	return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnPasswordRetrieved(const OpStringC& password)
{
	OP_ASSERT(m_current_task != CurrentTaskNone);

	OpString username;
	GetUsername(username);

	switch (m_current_task)
	{
	case CurrentTaskAuthenticate:
		{
			// Get this installation's ID
			OpString install_id;
			g_desktop_account_manager->GetInstallID(install_id);

			// Authenticate, this will retrieve the token
			if (OpStatus::IsSuccess(OAuthAuthenticate(username, password, UNI_L(""), install_id)))
			{
				// Hold onto the username and password used
				m_account_context.SetUsername(username);
				m_account_context.SetPassword(password);

				// Set the autologin flag so it grabs the response
				m_auto_login = TRUE;
			}
		}
		break;
	case CurrentTaskRegisterDevice:
	case CurrentTaskRegisterDeviceForce:
		{
			OpString install_id;
			GetInstallID(install_id);

			if (username.HasContent() &&
				password.HasContent() && 
				m_account_context.GetDeviceName().HasContent() &&
				install_id.HasContent())
			{
#ifdef OAUTH_CAP_HAS_FORCED_DEVICE_CREATION
				OAuthAuthenticate(username,
					password,
					m_account_context.GetDeviceName(),
					install_id,
					m_current_task == CurrentTaskRegisterDeviceForce);
#else
				OP_ASSERT(!force); // not really nice, but OAUTH_CAP_HAS_FORCED_DEVICE_CREATION should be on anyway
				return OAuthAuthenticate(username, m_account_context.GetPassword(), device_name, install_id);
#endif // OAUTH_CAP_HAS_FORCED_DEVICE_CREATION
			}
			else
			{
				OP_ASSERT(FALSE);
			}

			OpString empty;
			m_account_context.SetDeviceName(empty);
		}
		break;

	case CurrentTaskReleaseDevice:
		{
			if (username.HasContent() && password.HasContent() && m_account_context.GetDeviceName().HasContent())
			{
				OAuthReleaseDevice(username, password, m_account_context.GetDeviceName());
			}
			else
			{
				OP_ASSERT(FALSE);
			}

			OpString empty;
			m_account_context.SetDeviceName(empty);
		}
		break;
	}

	m_current_task = CurrentTaskNone;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OperaAccountManager::OnPasswordFailed()
{
	OP_ASSERT(m_current_task != CurrentTaskNone);
	if (m_current_task != CurrentTaskNone)
	{
		m_current_task = CurrentTaskNone;
		BroadcastOnPasswordMissing();
	}
}
