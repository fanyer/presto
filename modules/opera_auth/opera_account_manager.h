/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef OPERA_ACCOUNT_MANAGER_H
#define OPERA_ACCOUNT_MANAGER_H

#include "modules/wand/wandmanager.h"

/**
 * This class handles MyOpera account creation, authentication and Unite device
 * registration against the auth server.
 */
class CoreOperaAccountManager
{
public:
	/**
	 * Create a CoreOperaAccountManager object, or LEAVE.
	 * @param operaAccountManager will be set to the newly created CoreOperaAccountManager
	 */
	static void CreateL(CoreOperaAccountManager*& operaAccountManager);

	virtual ~CoreOperaAccountManager() {}

	/**
	 * Error codes returned from LastError
	 */
	enum OperaAccountError
	{
		OPERA_ACCOUNT_SUCCESS								= 0,
		OPERA_ACCOUNT_ALREADY_LOGGED_IN						= 1,
		OPERA_ACCOUNT_NOT_LOGGED_IN							= 2,

		OPERA_ACCOUNT_INVALID_USERNAME_PASSWORD				= 10,
		OPERA_ACCOUNT_USER_BANNED							= 11,
		OPERA_ACCOUNT_USER_ALREADY_EXISTS					= 12,
		OPERA_ACCOUNT_INVALID_ENCRYPTION_KEY_USED			= 13,

		OPERA_ACCOUNT_REQUEST_ABORTED						= 20,
		OPERA_ACCOUNT_REQUEST_ERROR							= 21,
		OPERA_ACCOUNT_REQUEST_TIMEOUT						= 22,
		OPERA_ACCOUNT_REQUEST_ALREADY_IN_PROGRESS			= 23,
		OPERA_ACCOUNT_INTERNAL_SERVER_ERROR					= 24,
		OPERA_ACCOUNT_INTERNAL_CLIENT_ERROR					= 25,
		OPERA_ACCOUNT_BAD_REQUEST							= 26,

		OPERA_ACCOUNT_OUT_OF_MEMORY							= 30,
		OPERA_ACCOUNT_PARSING_ERROR							= 31,

		OPERA_ACCOUNT_USERNAME_TOO_SHORT					= 40,
		OPERA_ACCOUNT_USERNAME_TOO_LONG						= 41,
		OPERA_ACCOUNT_USERNAME_CONTAINS_INVALID_CHARACTERS	= 42,
		OPERA_ACCOUNT_PASSWORD_TOO_SHORT					= 43,
		OPERA_ACCOUNT_PASSWORD_TOO_LONG						= 44,
		OPERA_ACCOUNT_EMAIL_ADDRESS_INVALID					= 45,
		OPERA_ACCOUNT_EMAIL_ADDRESS_ALREADY_IN_USE			= 46,
		OPERA_ACCOUNT_DEVICE_NAME_INVALID					= 47,
		OPERA_ACCOUNT_DEVICE_NAME_ALREADY_IN_USE			= 48,
		OPERA_ACCOUNT_TO_MANY_CHANGES						= 49
	};

	/**
	 * Listener class for callbacks on account state changes
	 */
	class Listener
	{
	public:
		virtual ~Listener() {}

		/** Called when IsLoggedIn() changes or is about to change value, or there is an
		 * error related to logging in.
		 */
		virtual void OnAccountLoginStateChange() { }

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		/** Called when IsDeviceRegistered() changes or is about to change value, or there is an
		 * error related to registering a device. Notice that on trying to register a device using
		 * the password saved from the last session, a failure to retrieve the password will
		 * cause the current session to be logged out, and the OnAccountLoginStateChange() callback
		 * will be called in addition to this one. This might happen e.g. if the master password
		 * is not correctly entered.
		 */
		virtual void OnAccountDeviceRegisteredChange() { }
#endif
	};

	/**
	 * Get current username
	 * @return current username
	 */
	virtual OpStringC& GetUsername() = 0;

	/**
	 * Query login state
	 * @return TRUE if currently logged in
	 */
	virtual BOOL IsLoggedIn() = 0;

	/**
	 * Query current "save password" setting
	 * @return the "save password" setting
	 */
	virtual BOOL GetSavePasswordSetting() = 0;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	/**
	 * Query current or previously set device name used by Unite
	 * @return device name
	 */
	virtual OpStringC& GetDevice() = 0;

	/**
	 * Query device registration state
	 * @return TRUE if a device is registered
	 */
	virtual BOOL IsDeviceRegistered() = 0;

	/**
	 * The shared secret to use with the unite proxy, available if the device is registered
	 * @return the shared secret
	 */
	virtual OpStringC& GetSharedSecret() = 0;

	/**
	 * Set the PrefsCollectionWebserver::WebserverUser preference from the
	 * GetUsername() value under the following conditions:
	 * <br> 1) IsLoggedIn() is TRUE
	 * <br> 2) PrefsCollectionWebserver::Enable is TRUE
	 * <br> 3) PrefsCollectionWebserver::WebserverUser is empty
	 */
	virtual OP_STATUS ConditionallySetWebserverUser() = 0;
#endif

	/**
	 * Query whether there is currently a request in progress.
	 * If TRUE, the request may be waiting for a response from the auth server,
	 * or waiting for the user to provide the master password to unlock the saved password.
	 * A new request will not be initiated while another request is in progress.
	 *
	 * @return TRUE if a request is in progress
	 */
	virtual BOOL IsRequestInProgress() = 0;

	/**
	 * Query the result of a request. May be used when the listener is called to signal
	 * the end of a request.
	 *
	 * <p>Immediately after a request has been called, this function will return
	 * OPERA_ACCOUNT_SUCCESS, unless the request failed before it got started, e.g.
	 * due to out of memory or OPERA_ACCOUNT_REQUEST_ALREADY_IN_PROGRESS.
	 *
	 * @return The result of the previous request
	 */
	virtual OperaAccountError LastError() = 0;

	/**
	 * Start an account creation request. Whether successful or not, the listener's
	 * OnAccountLoginStateChange() will be called at first to signal that the request
	 * is in progress, and then a second time when the request completes. If the
	 * request succeeds, the account will be automatically logged in.
	 *
	 * @param username The username of the new account
	 * @param password The password of the new account
	 * @param email The email of the new account
	 * @param savePassword if TRUE, the password and login state will be saved for the next session
	 */
	virtual void CreateAccount(const OpStringC& username, const OpStringC& password, const OpStringC& email, BOOL savePassword) = 0;

	/**
	 * Start a login request. Whether successful or not, the listener's
	 * OnAccountLoginStateChange() will be called at first to signal that the request
	 * is in progress, and then a second time when the request completes.
	 *
	 * @param username The username
	 * @param password The password
	 * @param savePassword if TRUE, the password and login state will be saved for the next session
	 */
	virtual void Login(const OpStringC& username, const OpStringC& password, BOOL savePassword) = 0;

	/**
	 * Start a change password request. Whether successful or not, the listener's
	 * OnAccountLoginStateChange() will be called at first to signal that the request
	 * is in progress, and then a second time when the request completes.
	 *
	 * @param username	   The username
	 * @param old_password The old password
	 * @param new_password The new password
	 * @param savePassword if TRUE, the new password will be saved for the next session
	 */
	virtual void ChangePassword(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password, BOOL savePassword) = 0;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	/**
	 * Start a device registration request to register a new Unite device. The
	 * listener's OnAccountDeviceRegisteredChange() will be called when the request
	 * completes. The account must be logged in before calling this function.
	 *
	 * <p>It is possible to register a new device name even if a device
	 * is currently registered. In that case the old device name is automatically
	 * released.
	 *
	 * <p>The request may prompt the user for the master password, in case the
	 * session was logged in from a previous session. If the user refuses to provide
	 * the master password, or if the password is retrieved, but it turns out that
	 * the user has changed the password through other means since the account
	 * was logged in, it may be that this operation causes the account to be
	 * forcibly logged out. In that case the listener's OnAccountLoginStateChange()
	 * will be called in addition to OnAccountDeviceRegisteredChange().
	 *
	 * @param device Name of the new Unite device
	 * @param force if TRUE, force the device registration. If FALSE, the
	 *    registration may result in an OPERA_ACCOUNT_DEVICE_NAME_ALREADY_IN_USE error
	 */
	virtual void RegisterDevice(const OpStringC &device, BOOL force) = 0;

	/**
	 * Start a device release request to release a Unite device. The listener's
	 * OnAccountDeviceRegisteredChange() will be called when the request completes.
	 * The account must be logged in and the device must be registered before
	 * calling this function.
	 *
	 * <p>The request may prompt the user for the master password, in case the
	 * session was logged in from a previous session. If the user refuses to provide
	 * the master password, or if the password is retrieved, but it turns out that
	 * the user has changed the password through other means since the account
	 * was logged in, it may be that this operation causes the account to be
	 * forcibly logged out. In that case the listener's OnAccountLoginStateChange()
	 * will be called in addition to OnAccountDeviceRegisteredChange().
	 *
	 * @param userInitiated If TRUE, we make a proper effort to get the device
	 *     unregistered, possibly asking for the master password. If FALSE,
	 *     we assume that the request is the result of a failed authentication
	 *     with the proxy, so the password wouldn't work in any case. Then we
	 *     quickly and quietly just forget about the device without notifying
	 *     the server.
	 */
	virtual void ReleaseDevice(BOOL userInitiated = TRUE) = 0;
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	/**
	 * Abort a request in progress. According to the type of request, the
	 * listeners will be called with the appropriate callback.
	 */
	virtual void Abort() = 0;

	/**
	 * Logout from the account. The listener's OnAccountLoginStateChange() will be
	 * called when the request completes. The password will be cleared.
	 */
	virtual void Logout() = 0;

	/**
	 * Add a listener to get callbacks when a request finishes.
	 * @return OK if the listener could be added
	 */
	virtual OP_STATUS AddListener(Listener* listener) = 0;

	/**
	 * Remove a listener.
	 */
	virtual void RemoveListener(Listener* listener) = 0;

	/**
	 * Retrieve the password used for logging in. Since the password may be
	 * protected by the master password (from login during a previous session),
	 * it may require user interaction to retrieve it.
	 *
	 * @param[in] window The window that will act as parent to any dialogs.
	 * If NULL, any dialogs will be positioned randomly or not at all.
	 *
	 * @param callback The callback. It must be a live object until the
	 * callback is called. There is no way to unregister a callback once
	 * this method has returned. The callback will be called with the
	 * password either directly (while this method is called) or after a
	 * while. If the password couldn't be accessed, e.g. if IsLoggedIn()
	 * returns FALSE, callback->OnPasswordRetrievalFailed() will be called.
	 * The returned password string is owned by CoreOperaAccountManager
	 * and should be copied as soon as possible.
	 *
	 * @return An error code if the process couldn't be initiated,
	 * otherwise OK. If OK is returned, the callback is guaranteed to be
	 * called.
	 */
	virtual OP_STATUS GetLoginPassword(Window* window, WandLoginCallback* callback) = 0;
};

#endif //  OPERA_ACCOUNT_MANAGER_H
