// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __OPERAACCOUNT_MANAGER_H__
#define __OPERAACCOUNT_MANAGER_H__

// Opera Account defines
#define MAX_NUMBER_OF_SIGNUPS_PER_DAY   5

// Sync defines
#ifdef SUPPORT_DATA_SYNC
 #define MYOPERA_PROVIDER UNI_L("myopera")
#endif // SUPPORT_DATA_SYNC

#define g_desktop_account_manager (OperaAccountManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Error masks for combining error enums from the sync and webserver managers
//
// Holds where the error has come from
#define OAM_ERROR							 0
#define OAM_SYNC_ERROR						(1 << 30)
#define OAM_WEBSERVER_ERROR					(2 << 30)
#define OAM_WEBSERVER_UPLOAD_ERROR			(3 << 30)
#define OAM_ERROR_MASK						0xC0000000

// Holds the type of error.
#define OAM_ERROR_TYPE_OTHER				 0
#define OAM_ERROR_TYPE_NETWORK				(1 << 28)
#define OAM_ERROR_TYPE_AUTH					(2 << 28)
#define OAM_ERROR_TYPE_MASK					0x30000000

// Mask to read the error code itself
#define OAM_ERROR_CODE_MASK					0x0FFFFFFF

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "modules/opera_auth/opera_auth_base.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "modules/forms/formmanager.h"

/**
 * @brief Interface for FeatureDialog.
 *
 * Interface used by FeatureDialog (FeatureSetupWizard.h), implemented 
 * by OperaAccountManager.
 */
class OperaAccountController
{
public:

	virtual ~OperaAccountController() {}

	class OAC_Listener
	{
	public:

		virtual ~OAC_Listener(){}

		/** @see OperaAuthentication::Listener::OnAuthCreateAccount */
		virtual void OnOperaAccountCreate(OperaAuthError error, OperaRegistrationInformation& reg_info) {}

		/** @see OperaAuthentication::Listener::OnAuthAuthenticate */
		virtual void OnOperaAccountAuth(OperaAuthError error, const OpStringC& shared_secret) {}

		/** @see OperaAuthentication::Listener::OnAuthReleaseDevice */
		virtual void OnOperaAccountReleaseDevice(OperaAuthError error) {}

		/** @see OperaAuthentication::Listener::OnAuthCreateDevice */
		virtual void OnOperaAccountDeviceCreate(OperaAuthError error, const OpStringC& shared_secret, 
				const OpStringC& server_message) {}

		/**
		 * Called when a request couldn't be sent to auth because the password wasn't retrieved.
		 */
		virtual void OnPasswordMissing() {}

		/** @see OperaAuthentication::Listener::OnAuthPasswordChanged */
		virtual void OnOperaAccountPasswordChanged(OperaAuthError err) {}
	};

	virtual OP_STATUS AddListener(OAC_Listener* listener) = 0;
	virtual OP_STATUS RemoveListener(OAC_Listener* listener) = 0;

	virtual void SetLoggedIn(BOOL logged_in) = 0;
	virtual void StopServices() = 0;

	virtual void SetUsernameAndPassword(const uni_char *username, const uni_char *password) = 0;
	virtual OP_STATUS SaveCredentials(const OpStringC & username, const OpStringC & password) = 0;

	virtual OP_STATUS CreateAccount(OperaRegistrationInformation& reg_info) = 0;
	virtual OP_STATUS Authenticate(const OpStringC& username, const OpStringC& password) = 0;
	virtual OP_STATUS RegisterDevice(const OpStringC& device_name, BOOL force = FALSE) = 0;
	virtual OP_STATUS ReleaseDevice(const OpStringC& devicename) = 0;
	virtual OP_STATUS ChangePassword(const OpStringC& username, const OpStringC& old_password,
			const OpStringC& new_password) = 0;
};

/**
 * @class OperaAccountContext
 * @brief The context of an Opera account, ie. username, password, email, and logged-in state.
 */
class OperaAccountContext
{
public:
	OperaAccountContext() : m_logged_in(FALSE), m_account_used(TRUE)
	{}

	virtual ~OperaAccountContext() {}

	static INT32		GetUsernameMaxLength() { return 30; }
	static INT32		GetPasswordMinLength() { return 5; }

	// getters for member variables
	BOOL				IsLoggedIn() const			{ return m_logged_in; }
	BOOL				IsAccountUsed() const	{ return m_account_used; }
	const OpStringC &	GetUsername() const		{ return m_username; }
	const OpStringC &	GetPassword() const		{ return m_password; }
	const OpStringC &	GetEmailAddress() const	{ return m_email_address; }
	const OpStringC &	GetDeviceName() const	{ return m_device_name; }

	// setters for member variables
	virtual void		SetLoggedIn(BOOL logged_in) { m_logged_in = logged_in; }
	void				SetAccountUsed(BOOL account_used) { m_account_used = account_used; }
	OP_STATUS			SetUsername(const OpStringC & username)	{ return m_username.Set(username.CStr()); }
	OP_STATUS			SetPassword(const OpStringC & password)	{ return m_password.Set(password.CStr()); }
	OP_STATUS			SetEmailAddress(const OpStringC & email_address) { return m_email_address.Set(email_address.CStr()); }
	OP_STATUS			SetDeviceName(const OpStringC & device_name) { return m_device_name.Set(device_name.CStr()); }

	// other convenience functions
	BOOL				IsCredentialsSet() const	{ return (!m_username.IsEmpty() && !m_password.IsEmpty()); }
	
	// validity check
	BOOL				IsUsernameTooShort() const;
	BOOL				IsValidUsername() const;
	BOOL				IsPasswordTooShort() const;
	BOOL				IsValidPassword() const;
	BOOL				IsValidCredentials() const { return (IsValidUsername() && IsValidPassword()); }
	BOOL				IsValidEmailAddress() const;

private:
	BOOL				m_logged_in;
	BOOL				m_account_used;
	OpString			m_username;
	OpString			m_password;
	OpString			m_email_address;
	OpString			m_device_name;
};

/**
 * @class OperaAccountPasswordChangeContext
 * @brief The context for an Opera Account password change operation.
 */
class OperaAccountPasswordChangeContext
{
public:

	OperaAccountPasswordChangeContext() : m_valid(FALSE) {}

	OpString m_username;
	OpString m_old_password_from_user;
	OpString m_old_password_from_wand;
	OpString m_new_password;
	BOOL m_valid;
};

/**
 * @class OperaAccountManager
 * @brief Manages accounts used by Opera.
 *
 * Used to for example trigger MyOpera authentication and
 * registration of Unite devices, handles logged in state,
 * broadcasts important network events.
 *
 * FIXME: Duplicates much functionality of CoreOperaAccountManager,
 * and probably should be replaced by it, see DSK-331178 for details.
 */
class OperaAccountManager : 
	public DesktopManager<OperaAccountManager>,
	public OperaAuthentication::Listener,
	public PrivacyManagerCallback,
	public OpPrefsListener,
	public OperaAccountController
{
public:
	// Status is held in an order of importance.
	// This status is used by all the services, and in a GetStatus call the lowest status of
	// the services wins
	enum OAM_Status
	{
		Offline,
		Error,
		Busy,
		OK,
		Disabled
	};

	OperaAccountManager();
	~OperaAccountManager();

	// Initialise all the things the Opera Account Manager needs
	OP_STATUS Init(); 
	BOOL	  IsInited() { return m_inited; }

	// OpPrefsListener interface
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	// Setting and retrieving the logged in state of the account
	virtual void SetLoggedIn(BOOL logged_in);
	virtual BOOL GetLoggedIn();
	BOOL IsServiceEnabled();
	void LogoutIfNoServicesRunning();

	void SetEnableLink(BOOL enable) { m_enable_link = enable;}

	// Very hackish but needed to stop gadgets showing the login dialog on auto login
	BOOL IsAutoLoginRequest() { return m_auto_login; }

	void SetIsOffline(BOOL offline);
	BOOL GetIsOffline() { return m_offline; }

	// Sets the username and password into the members and module
	OP_STATUS				 ClearUsernameAndPassword();
	virtual void			 SetUsernameAndPassword(const uni_char *username, const uni_char *password);
	virtual OP_STATUS		 SaveCredentials(const OpStringC & username, const OpStringC & password);
	virtual OP_STATUS		 GetUsername(OpString & username) const;
	virtual const OpStringC& GetPassword() const { return m_account_context.GetPassword(); }
	virtual const char*      GetToken() { return m_token.CStr(); } 

	virtual OP_STATUS CreateAccount(OperaRegistrationInformation& reg_info);
	virtual OP_STATUS Authenticate(const OpStringC& username, const OpStringC& password);
	virtual OP_STATUS RegisterDevice(const OpStringC& device_name, BOOL force = FALSE);
	virtual OP_STATUS ReleaseDevice(const OpStringC& devicename);

	// We need to delay our Init until the main browser window is created to avoid auth triggering
	// sync too early. 
	OP_STATUS Start();

	/**
	 * We need to stop Link and Unite before password change, after successfull
	 * or unsuccessfull operation we bring services to their original state.
	 * Just after successfull send of request to the server user is logged out also.
	 * If password change was successfull we login the user with new credentials,
	 * else we login the user with old credentials as nothing has happened.
	 * @see OperaAccountPasswordChangeContext
	 *
	 * @param username Username.
	 * @param old_password Old password.
	 * @param new_password New password.
	 * @return Status.
	 */
	virtual OP_STATUS ChangePassword(const OpStringC& username, const OpStringC& old_password,
			const OpStringC& new_password);

	// Starts up the services that are enabled
	void StartServices(BOOL auto_login = FALSE);
	virtual void StopServices();

	// OperaAuthentication::Listener implementation
	virtual void OnAuthCreateAccount(OperaAuthError error, OperaRegistrationInformation& reg_info);
	virtual void OnAuthAuthenticate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message, const OpStringC& token);
	virtual void OnAuthPasswordChanged(OperaAuthError error);
	virtual void OnAuthReleaseDevice(OperaAuthError error);
	virtual void OnAuthCreateDevice(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message);

	// Methods for broadcasting events to the listeners
	void BroadcastCreateAccountEvent(OperaAuthError error, OperaRegistrationInformation& reg_info);
	void BroadcastAuthEvent(OperaAuthError error, const OpStringC &shared_secret);
	void BroadcastReleaseDeviceEvent(OperaAuthError error);
	void BroadcastCreateDeviceEvent(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message);
	void BroadcastOnPasswordMissing();
	void BroadcastOperaAccountPasswordChanged(OperaAuthError err);

	// Listener functions for this classes internal listener
	virtual OP_STATUS AddListener(OAC_Listener* listener) { return m_listeners.Add(listener); }
	virtual OP_STATUS RemoveListener(OAC_Listener* listener) { return m_listeners.Remove(listener); }

	// Sends an error to the console
	void LogError(INT32 error_code, const OpStringC& context, const OpStringC& message);
	void LogWarning(INT32 error_code, const OpStringC& context, const OpStringC& message);

	// Gets the Install ID of this build
	virtual void GetInstallID(OpString &install_id);

	// State functions
	void		SetStatus(OAM_Status status) { m_status = status; };
	OAM_Status	GetStatus();

	// PrivacyManagerCallback implementation
	virtual void OnPasswordRetrieved(const OpStringC& password);
	virtual void OnPasswordFailed();

	virtual OperaAccountContext*	GetAccountContext()	{ return &m_account_context; }

protected:
	enum CurrentTask
	{
		CurrentTaskNone,
		CurrentTaskAuthenticate,
		CurrentTaskRegisterDevice,
		CurrentTaskRegisterDeviceForce,
		CurrentTaskReleaseDevice
	}	m_current_task;

	// wrapping calls to static instances, so they can be overridden by selftest subclasses
	virtual OP_STATUS OAuthAuthenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force = FALSE);
	virtual OP_STATUS OAuthReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename);
	virtual OP_STATUS GetUserNameFromPrefs(OpString & username) const;
	virtual OP_STATUS GetWandPassword(const OpStringC8 &protocol, const OpStringC &server, const OpStringC &username, PrivacyManagerCallback* callback);


private:

	/**
	 * Takes the error returned from the listeners and converts it to our format 
	 * ready for the console.
	 */
	void LogError(OperaAuthError error);

	OpListeners<OAC_Listener> m_listeners; /**< Listeners for auth notifications. */

	BOOL m_inited; /**< Set to TRUE after a successful Init() call. */

	/** 
	 * The following signup dates are read only on demand, not on startup
	 */
	time_t m_last_signup_date; /**< Time of last (from 1 to MAX_NUMBER_OF_SIGNUPS_PER_DAY) of signups. */
	time_t m_num_signups; /**< Number of signups on last_signup_date. */

	OpString8	m_token; /**< Token which represents this user for this session. */
	BOOL		m_offline;
	BOOL		m_auto_login; /**< FIXME: Hackish remove ASAP. */
	
	/**
	 * They will track all of the status to make everything simpler.
	 */
	OAM_Status	m_status; /**< Current status of the Opera Account. */ 
	BOOL        m_enable_link;

	OperaAccountContext	m_account_context;
	OperaAccountPasswordChangeContext m_passwd_change_context;
	MyOperaAuthentication* m_opera_authentication;
};

#endif // __OPERAACCOUNT_MANAGER_H__
