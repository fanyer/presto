/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef _OPERA_AUTH_H_INCLUDED_
#define _OPERA_AUTH_H_INCLUDED_

#define AUTH_LOADING_TIMEOUT (10*60)	// 10 minutes
#include "modules/opera_auth/opera_account_manager.h"

enum OperaRegistrationType
{
	OperaRegNormal,			// regular registration
	OperaRegWebserver		// webserver device registration
};

/*
** This structure is used to pass state information between the listener and the sync module
** about the status of a user creation attempt
*/
struct OperaRegistrationInformation
{
	OperaRegistrationInformation(OperaRegistrationType registration_type = OperaRegNormal)
		: m_force_webserver_name(FALSE)
		, m_registration_type(registration_type)
	{
	}

	OP_STATUS Set(const OperaRegistrationInformation& src)
	{
		RETURN_IF_ERROR(m_username.Set(src.m_username));
		RETURN_IF_ERROR(m_password.Set(src.m_password));
		RETURN_IF_ERROR(m_email.Set(src.m_email));
		RETURN_IF_ERROR(m_webserver_name.Set(src.m_webserver_name));
		RETURN_IF_ERROR(m_install_id.Set(src.m_install_id));
		m_force_webserver_name = src.m_force_webserver_name;
		m_registration_type = src.m_registration_type;
		return OpStatus::OK;
	}

	OpString	m_username;			// Username to register with
	OpString	m_password;			// Password to register with
	OpString	m_email;			// email address to register with
	OpString	m_webserver_name;	// suggested webserver device name the user wants
	OpString	m_install_id;		// installation id used on device create requests
	BOOL		m_force_webserver_name;	// for webserver device name registration, force name if already taken
	OperaRegistrationType m_registration_type;	// What type of registration
};

enum OperaAuthError
{
	AUTH_OK = 0,
	AUTH_ERROR,					// unknown error
	AUTH_ERROR_PARSER,			// error while parsing
	AUTH_ERROR_MEMORY,			// out of memory
	AUTH_ERROR_SERVER,			// unhandled server error
	AUTH_ERROR_COMM_FAIL,		// communication error
	AUTH_ERROR_COMM_ABORTED,	// communication aborted
	AUTH_ERROR_COMM_TIMEOUT,	// timeout while accessing the server
	AUTH_ERROR_INVALID_REQUEST,	// invalid request - empty request, no POST data, invalid request URI

	AUTH_ACCOUNT_CREATE_USER_EXISTS,	// user already exists (during account creation)
	AUTH_ACCOUNT_CREATE_DEVICE_EXISTS,	// device already exists (during device name creation for webserver)
	AUTH_ACCOUNT_CREATE_DEVICE_INVALID,	// invalid device name
	AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR,	// email address is invalid
	AUTH_ACCOUNT_CREATE_USER_EMAIL_EXISTS,	// email address already in use
	AUTH_ACCOUNT_CREATE_USER_TOO_SHORT,		// username too short
	AUTH_ACCOUNT_CREATE_USER_TOO_LONG,		// username too long
	AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS,	// username contains invalid characters
	AUTH_ACCOUNT_AUTH_FAILURE,		// invalid username/password
	AUTH_ACCOUNT_USER_UNAVAILABLE,	// user temporary unavailable - the user data has been locked for internal processing and will be available shortly
	AUTH_ACCOUNT_AUTH_INVALID_KEY,	// invalid encryption key used
	AUTH_ACCOUNT_USER_BANNED,	//	User is banned
	AUTH_ACCOUNT_CREATE_PASSWORD_TOO_SHORT,	// password too short
	AUTH_ACCOUNT_CREATE_PASSWORD_TOO_LONG,	// password too long
	AUTH_ACCOUNT_CHANGE_PASSWORD_TO_MANY_CHANGES // Too many password changes pr minute
};

// all timeouts are in seconds
class OpAuthTimeout : public MessageObject
{
public:
	OpAuthTimeout(OpMessage message)
		: m_timeout(AUTH_LOADING_TIMEOUT)
		, m_message(message)
	{
	}

	virtual ~OpAuthTimeout();

	UINT32 SetTimeout(UINT32 timeout)
	{
		UINT32 old_timeout = m_timeout;

		m_timeout = timeout;

		return old_timeout;
	}
	/// override function from MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		OnTimeout(msg, par1, par2);
	}

	// needs to be implemented in subclasses
	virtual void OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) = 0;

	// uses a custom interval
	OP_STATUS StartTimeout(UINT32 timeout, BOOL short_interval);

	// uses the set time interval
	OP_STATUS RestartTimeout();

	void StopTimeout();

private:
	UINT32 m_timeout;	// timeout in seconds
	OpMessage m_message;
};



class OperaAuthentication : public OpAuthTimeout
{
public:
	OperaAuthentication();
	virtual ~OperaAuthentication();

	class Listener
	{
	public:
		virtual ~Listener() {}

		// Called when a account creation has been attempted.  Returns AUTH_OK on success,
		// AUTH_ACCOUNT_CREATE_USER_EXISTS if the user already exists and any other
		// AUTH_ error for other errors
		virtual void OnAuthCreateAccount(OperaAuthError error, OperaRegistrationInformation& reg_info) = 0;

		// Called when authentication has been completed. AUTH_ACCOUNT_AUTH_FAILURE
		// on error with username/password, AUTH_OK on success
		virtual void OnAuthAuthenticate(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message, const OpStringC& token) = 0;

		virtual void OnAuthPasswordChanged(OperaAuthError error) = 0;

		// Called when creating a device has been completed. AUTH_ACCOUNT_AUTH_FAILURE
		// on error with username/password, AUTH_OK on success
		virtual void OnAuthCreateDevice(OperaAuthError error, const OpStringC& shared_secret, const OpStringC& server_message) = 0;

		// Called when releasing a device has completed.
		virtual void OnAuthReleaseDevice(OperaAuthError error) = 0;
	};

	/**
	 * Authenticates with the given username/password combination. When the authentication
	 * has been completed, the listener's OnAuthAuthenticate() method will be called
	 * with the result
	 *
	 * @param username Username
	 * @param password Password
	 * @param devicename Device name to associate with the authentication, if applicable
	 * @return OK or ERR_MEM_ERROR
	 */
	virtual OP_STATUS Authenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force = FALSE) = 0;

	/**
	* Creates a new account for the user with the information given in the reg_info
	* argument.  If the type given is OpSyncRegWebserver then this call might
	* be used for creating a web server device for an existing user.
	* The reg_info structure will be copied for use internally, so the callee
	* does not need to keep it valid for the lifetime of the creation process.
	*
	* @param reg_info OperaRegistrationInformation instance
	* @param password Password
	* @return OK or ERR_MEM_ERROR
	*/
	virtual OP_STATUS CreateAccount(OperaRegistrationInformation& reg_info) = 0;

	virtual OP_STATUS ReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename) = 0;

	virtual void Abort(BOOL userAction=FALSE) = 0;

	void SetListener(OperaAuthentication::Listener *listener) { m_listener = listener; }

	// timeout for authentication, should be overridden in derived classes
	virtual void OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) {}

protected:
	Listener* m_listener;
};

#endif //_OPERA_AUTH_H_INCLUDED_
