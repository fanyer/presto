/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef _SYNC_COORDINATOR_H_INCLUDED_
#define _SYNC_COORDINATOR_H_INCLUDED_

#include "modules/sync/sync_datacollection.h"
#include "modules/sync/sync_dataitem.h"
#include "modules/sync/sync_dataqueue.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/sync/sync_state.h"
#include "modules/opera_auth/opera_auth_oauth.h"

#ifndef SYNC_MAX_SEND_ITEMS
# define SYNC_MAX_SEND_ITEMS 200
#endif // SYNC_MAX_SEND_ITEMS

#define SYNC_LOADING_TIMEOUT (1*60)		// 1 minute
#define SYNC_AUTH_TIMEOUT	 60			// 60 seconds

#ifdef _DEBUG
# define DEFAULT_SYNC_INTERVAL_LONG	(60 * 2)		// 2 minutes
#else
# define DEFAULT_SYNC_INTERVAL_LONG	(70 * 1)		// 1 minute
#endif // _DEBUG
#define DEFAULT_SYNC_INTERVAL_SHORT 10				// 10 seconds

class OpSyncFactory;
class OpSyncTransportProtocol;
class OpSyncParser;
class OpSyncDataItem;
class OpSyncItem;
class OpSyncDataQueue;
class OpSyncAllocator;
class OpInputAction;
class SyncEncryptionKeyManager;

/** These error codes are returned from the server to the client */
enum OpSyncError
{
	SYNC_OK = 0,
	SYNC_ERROR,						///< Unknown error.
	/** Missing or unsupported client build version. */
	SYNC_ERROR_CLIENT_VERSION,
	SYNC_ERROR_COMM_ABORTED,		///< Communication aborted.
	SYNC_ERROR_COMM_FAIL,			///< Communication error.
	SYNC_ERROR_COMM_TIMEOUT,		///< Timeout while accessing the server.
	SYNC_ERROR_INVALID_BOOKMARK,	///< Invalid bookmark element.
	SYNC_ERROR_INVALID_CONTACT,		///< Invalid contact element.
	SYNC_ERROR_INVALID_FEED,		///< Invalid feed element.
	SYNC_ERROR_INVALID_NOTE,		///< Invalid note element.
	/** Invalid request, e.g. empty request or no POST data or invalid request
	 * URI. */
	SYNC_ERROR_INVALID_REQUEST,
	SYNC_ERROR_INVALID_SEARCH,		///< Invalid search_engine element.
	SYNC_ERROR_INVALID_SPEEDDIAL,	///< Invalid speed dial element.
	SYNC_ERROR_INVALID_STATUS,		///< Invalid status attribute.
	SYNC_ERROR_INVALID_TYPED_HISTORY,///< Invalid typed_history element.
	SYNC_ERROR_INVALID_URLFILTER,	///< Invalid urlfilter element.
	SYNC_ERROR_MEMORY,				///< Out of memory.
	SYNC_ERROR_PARSER,				///< Error while parsing.
	SYNC_ERROR_PROTOCOL_VERSION,	///< Missing or unsupported protocol version
	SYNC_ERROR_SERVER,				///< Unhandled server error.
	/** Synchronisation has been disabled by the client. */
	SYNC_ERROR_SYNC_DISABLED,
	SYNC_ERROR_SYNC_IN_PROGRESS,	///< Synchronisation is already in progress.

	/**
	 * This error code is reported when the synchronisation of passwords
	 * (SYNC_SUPPORTS_PASSWORD_MANAGER, SYNC_SUPPORTS_ENCRYPTION) is currently
	 * stopped due to an unreachable encryption-key.
	 *
	 * This may happen when the user protects the wand-data with a
	 * master-password (so if the client needs to load the encryption-key from
	 * wand, the user may need to enter the master-password), and the user has
	 * changed the Opera Account password; and after starting sync and being
	 * presented with the master-password dialog, the user chooses to cancel
	 * that dialog.
	 *
	 *  The best way to resolve this problem is to display an error message like
	 * "Passwords synchronisation is temporarily off because the master-password
	 * wasn't entered, everything else works just fine. Press <here> to resume."
	 * And after the user pressed "resume", call
	 * OpSyncCoordinator::ResetSupportsState(SYNC_SUPPORTS_ENCRYPTION). That
	 * will restart the process to get the encryption-key.
	 * @note If the user chose to disable the master-password, sync does not
	 *  resume password syncing automatically, but after calling the reset
	 *  method, the user will not be presented with the master-password dialog.
	 */
	SYNC_PENDING_ENCRYPTION_KEY,

	SYNC_ACCOUNT_AUTH_FAILURE,		///< Invalid username/password.
	SYNC_ACCOUNT_AUTH_INVALID_KEY,	///< Invalid encryption key used.
	/** The Oauth token has expired and the client needs to request a new Oauth
	 * token and try again. */
	SYNC_ACCOUNT_OAUTH_EXPIRED,
	SYNC_ACCOUNT_USER_BANNED,		///< User is banned.
	/** The user is temporary unavailable - the user data has been locked for
	 * internal processing and will be available shortly. */
	SYNC_ACCOUNT_USER_UNAVAILABLE
};

#ifdef _DEBUG
inline Debug& operator<<(Debug& d, enum OpSyncError e)
{
# define CASE_ITEM_2_STRING(x) case x: return d << # x
	switch (e) {
		CASE_ITEM_2_STRING(SYNC_OK);
		CASE_ITEM_2_STRING(SYNC_ERROR);
		CASE_ITEM_2_STRING(SYNC_ERROR_CLIENT_VERSION);
		CASE_ITEM_2_STRING(SYNC_ERROR_COMM_ABORTED);
		CASE_ITEM_2_STRING(SYNC_ERROR_COMM_FAIL);
		CASE_ITEM_2_STRING(SYNC_ERROR_COMM_TIMEOUT);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_BOOKMARK);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_CONTACT);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_FEED);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_NOTE);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_REQUEST);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_SEARCH);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_SPEEDDIAL);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_STATUS);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_TYPED_HISTORY);
		CASE_ITEM_2_STRING(SYNC_ERROR_INVALID_URLFILTER);
		CASE_ITEM_2_STRING(SYNC_ERROR_MEMORY);
		CASE_ITEM_2_STRING(SYNC_ERROR_PARSER);
		CASE_ITEM_2_STRING(SYNC_ERROR_PROTOCOL_VERSION);
		CASE_ITEM_2_STRING(SYNC_ERROR_SERVER);
		CASE_ITEM_2_STRING(SYNC_ERROR_SYNC_DISABLED);
		CASE_ITEM_2_STRING(SYNC_ERROR_SYNC_IN_PROGRESS);
		CASE_ITEM_2_STRING(SYNC_ACCOUNT_AUTH_FAILURE);
		CASE_ITEM_2_STRING(SYNC_ACCOUNT_AUTH_INVALID_KEY);
		CASE_ITEM_2_STRING(SYNC_ACCOUNT_USER_BANNED);
		CASE_ITEM_2_STRING(SYNC_ACCOUNT_USER_UNAVAILABLE);
	default: return d << "OpSyncError(" << (int)e << ")";
	}
# undef CASE_ITEM_2_STRING
}
#endif // _DEBUG

/**
 * This enum defines the possible error codes that are returned from the
 * OpSyncDataClient to the OpSyncCoordinator on receiving the OpSyncItem
 * collections in OpSyncDataClient::SyncDataAvailable().
 */
enum OpSyncDataError
{
	SYNC_DATAERROR_NONE,			///< No error.

	/**
	 * The client has detected inconsistency in the data from the server. The
	 * sync module might initiate a dirty sync to resolve this inconsistency.
	 */
	SYNC_DATAERROR_INCONSISTENCY,

	/**
	 * The client needs some asynchronous action - e.g. open a dialog and wait
	 * for some user-input - before the client can continue to handle incoming
	 * data. If the implementation of OpSyncDataClient::SyncDataAvailable() sets
	 * the error status to this value, the OpSyncCoordinator keeps the
	 * collection of received OpSyncItem instances. The client can then handle
	 * the collection in the next call to OpSyncDataClient::SyncDataAvailable().
	 *
	 * OpSyncDataClient::SyncDataAvailable() is either called on the next
	 * sync-connection, or the client can request a call to that method by
	 * calling OpSyncCoordinator::ContinueSyncData().
	 *
	 * @note The sync-connections happen in regular intervals. All
	 *  OpSyncDataClient instances that are registered for one
	 *  OpSyncDataItem::DataItemType need to expect another call to
	 *  OpSyncDataClient::SyncDataAvailable() on the next sync-connection, if
	 *  one OpSyncDataClient instance sets this return value for that type.
	 *  If the asynchronous action is not yet finished, the client needs to set
	 *  this status again.
	 *  @see OpSyncCoordinator::SetSyncDataClient()
	 *
	 * @note The collection of OpSyncItem instances that is specified in
	 *  OpSyncDataClient::SyncDataAvailable() includes all received items. The
	 *  OpSyncDataClient instance can only decide to handle all items (by
	 *  setting the code to SYNC_DATAERROR_NONE) or to delay handling all items
	 *  (by setting the code to SYNC_DATAERROR_ASYNC).
	 */
	SYNC_DATAERROR_ASYNC
};

#ifdef _DEBUG
inline const char* OpSyncDataError2String(enum OpSyncDataError e)
{
# define CASE_ITEM_2_STRING(x) case x: return # x
	switch (e) {
		CASE_ITEM_2_STRING(SYNC_DATAERROR_NONE);
		CASE_ITEM_2_STRING(SYNC_DATAERROR_INCONSISTENCY);
		CASE_ITEM_2_STRING(SYNC_DATAERROR_ASYNC);
	default: return "OpSyncDataError(unknown)";
	}
# undef CASE_ITEM_2_STRING
}
#endif // _DEBUG

/**
 *
 */
class OpSyncTimeout : public MessageObject
{
public:
	/** Handles a timeout interval with the specified message. */
	OpSyncTimeout(OpMessage message);
	virtual ~OpSyncTimeout();

	/** Sets the default timeout interval to the specified value.
	 * @param timeout is the new default timeout interval in seconds. */
	unsigned int SetTimeout(unsigned int timeout);

	/**
	 * @name Implementation of MessageObject
	 * @{
	 */

	/**
	 * Handles the message that was specified in the constructor by calling
	 * OnTimeout(). If a class inherits from this class, then it can implement
	 * another version of HandleCallback() and pass all not handled messages to
	 * the base-class:
	 * @code
	 * class MyClass : public OpSyncTimeout {
	 *     virtual ~MyClass() {}
	 *     ...
	 *     virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	 *     {
	 *         switch (msg) {
	 *         case ...: ...; break;
	 *         ...
	 *         default: OpSyncTimeout::HandleCallback(msg, par1, par2);
	 *     }
	 * };
	 * @endcode
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** @} */ // Implementation of MessageObject

	/** Needs to be implemented in subclasses. */
	virtual void OnTimeout() = 0;

	/** Restart the timer interval with the specified interval. Unless the timer
	 * is stopped or restarted, OnTimeout() will be called after the specified
	 * interval.
	 * @param timeout in seconds */
	OP_STATUS StartTimeout(unsigned int timeout);

	/** Restart the timer with the default interval. Unless the timer is stopped
	 * or restarted, OnTimeout() will be called after the default interval.
	 * @note The default interval is initialised with SYNC_LOADING_TIMEOUT and
	 *  can be changed on calling SetTimeout(). */
	OP_STATUS RestartTimeout();

	void StopTimeout();

private:
	unsigned int m_timeout;	///< Timeout in seconds.
	OpMessage m_message;
};

class OpSyncServerInformation
{
public:
	OpSyncServerInformation()
		: m_sync_interval_long(DEFAULT_SYNC_INTERVAL_LONG)
		, m_sync_interval_short(DEFAULT_SYNC_INTERVAL_SHORT)
		, m_sync_max_items(SYNC_MAX_SEND_ITEMS)
		{}

	void Clear() {
		m_sync_interval_long = DEFAULT_SYNC_INTERVAL_LONG;
		m_sync_interval_short = DEFAULT_SYNC_INTERVAL_SHORT;
		m_sync_max_items = SYNC_MAX_SEND_ITEMS;
	}

	unsigned int GetSyncIntervalLong() const {
		OP_ASSERT(m_sync_interval_long != 0);
		return m_sync_interval_long ? m_sync_interval_long : DEFAULT_SYNC_INTERVAL_LONG;
	}

	unsigned int GetSyncIntervalShort() const { return m_sync_interval_short ? m_sync_interval_short : DEFAULT_SYNC_INTERVAL_SHORT; }
	UINT32 GetSyncMaxItems() const { return m_sync_max_items < SYNC_MAX_SEND_ITEMS ? SYNC_MAX_SEND_ITEMS : m_sync_max_items; }
	void SetSyncIntervalLong(unsigned int sync_interval_long) {
		m_sync_interval_long = sync_interval_long;
		OP_ASSERT(m_sync_interval_long != 0);
	}

	void SetSyncIntervalShort(unsigned int sync_interval_short) { m_sync_interval_short = sync_interval_short; }
	void SetSyncMaxItems(UINT32 max_items) { m_sync_max_items = max_items; }

	OpSyncServerInformation& operator=(const OpSyncServerInformation& src) {
		m_sync_interval_long = src.m_sync_interval_long;
		m_sync_interval_short = src.m_sync_interval_short;
		m_sync_max_items = src.m_sync_max_items;
		return *this;
	}

private:
	unsigned int m_sync_interval_long;
	unsigned int m_sync_interval_short;
	/** Maximum number of items the client is allowed to send to the server. */
	UINT32 m_sync_max_items;
};

#ifdef _DEBUG
inline Debug& operator<<(Debug& d, const OpSyncServerInformation& i)
{
	return d << "OpSyncServerInformation(interval="
			 << i.GetSyncIntervalLong() << ":" << i.GetSyncIntervalShort()
			 << ";items=" << i.GetSyncMaxItems() << ")";
}
#endif // _DEBUG

enum OpSyncSystemInformationType
{
	SYNC_SYSTEMINFO_SYSTEM = 1,		///< OS (linux, windows, ...).
	SYNC_SYSTEMINFO_SYSTEMVERSION,	///< Version of the OS (eg. 5.1 or 6.0).
	SYNC_SYSTEMINFO_BUILD,			///< Build number of the client.
	SYNC_SYSTEMINFO_PRODUCT,		///< The product, eg. "Opera Desktop 11.10"
	/** What targetted data is wanted ("mini" or not given). */
	SYNC_SYSTEMINFO_TARGET,
	SYNC_SYSTEMINFO_MAX
};

class OpSyncUIListener
{
public:
	virtual ~OpSyncUIListener() {}

	/** Called when a sync has been started.
	 *
	 * @param items_sending TRUE if actual items are being sent to the server.
	 */
	virtual void OnSyncStarted(BOOL items_sending) = 0;

	/**
	 * Called when a synchronisation was completed unsuccessfully. The error
	 * code given describes the problem. The listener might also be called if
	 * sync was disabled using SetSyncActive() and the client tries to do a
	 * synchronisation.
	 */
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_message) = 0;

	/**
	 * Called when a synchronisation was completed successfully and all data
	 * listeners have processed the data.
	 *
	 * @param sync_state Synchronization state required by the server and kept
	 *  on the client.
	 */
	virtual void OnSyncFinished(OpSyncState& sync_state) = 0;

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	/**
	 * This class is used as a context on the
	 * OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed() notification. On
	 * receiving that notification, the OpSyncUIListener can
	 *
	 * -# Ignore the event. In that case steps password sync will not continue
	 *    until this situation is resolved.
	 *
	 *    Note: It may be possible that the situation is resolved
	 *    automatically. If the user does not answer this notification, but
	 *    instead starts sync on a different client that has a local copy of the
	 *    encryption-key, then the encryption-key is re-encrypted with the new
	 *    Opera Account password on that client, uploaded to Link, downloaded
	 *    and decrypted by this client. And in that case
	 *    OpSyncUIListener::OnSyncReencryptEncryptionKeyCancel() is called.
	 * -# When the user enters the old Opera Account password, call
	 *    DecryptWithPassword() and then sync tries to decrypt the received
	 *    encryption-key. If that is successful, sync can resume normal sync
	 *    operations for passwords. If that fails (e.g. because the user entered
	 *    the wrong password), this method is called once again.
	 * -# Choose to create a new encryption-key. This means that the existing
	 *    encryption-key and all existing wand-data is deleted on the Link
	 *    server and on all Opera clients. In that case CreateNewEncryptionKey()
	 *    is called and after deleting all password entries sync can resume
	 *    normal sync operations for passwords.
	 */
	class ReencryptEncryptionKeyContext {
	public:
		virtual ~ReencryptEncryptionKeyContext() {}

		/**
		 * Try again to decrypt the encrypted encryption-key with the specified
		 * (old) Opera Account password.
		 *
		 * If encrypted encryption-key could be decrypted with the specified
		 * password, OpSyncUIListener::OnSyncReencryptEncryptionKeyCancel() is
		 * called (with argument context == 0) and sync resumes synchronising
		 * wand data.
		 *
		 * If the encrypted encryption-key cannot be decrypted with the
		 * specified password,
		 * OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed() is called once
		 * more.
		 *
		 * @note After calling this method, the context instance may no longer
		 *  be valid.
		 *
		 * @param password is the (old) Opera Account password which shall be
		 *  used to try to decrypt the encrypted encryption-key.
		 */
		virtual void DecryptWithPassword(const OpStringC& password) = 0;

		/**
		 * Don't try to decrypt the encrypted encryption-key. Instead create a
		 * new encryption-key and delete all existing wand data on the Link
		 * server and all other Opera clients.
		 */
		virtual void CreateNewEncryptionKey() = 0;
	};

	/**
	 * This method is called, when the sync module received an encrypted
	 * encryption-key from the Link server which cannot be decrypted. The
	 * encryption-key is encrypted with the current Opera Account password. So
	 * this method is called when the user changed the Opera Account password
	 * and starts sync for the first time on a client that does not have a local
	 * copy of the encryption-key.
	 *
	 * \msc
	 * User,Opera,Auth,Link;
	 * User=>Auth [label="1. Change Opera Account password"];
	 * User=>Opera [label="2. Start sync (username,password)"];
	 * Opera=>Auth [label="3. Get Oauth token (username,password)"];
	 * Opera<<Auth [label="Oauth token"];
	 * Opera=>Link [label="4. sync (using the Oauth token)"];
	 * Opera<<Link [label="5. encrypted encryption-key"];
	 * Opera->User [label="6. OnSyncReencryptEncryptionKeyFailed()"];
	 * \endmsc
	 *
	 * -# User changes the Opera Account password.
	 * -# User starts sync (entering the new Opera Account password).
	 * -# Opera uses the new Opera Account password to get an Oauth token from
	 *    Auth.
	 * -# Opera uses the Oauth token to request a sync-update.
	 * -# Opera receives from the Link server the encrypted encryption-key.
	 * -# Because the encryption-key was encrypted with the old Opera Account
	 *    password, Opera cannot decrypt it. And because the Opera client has no
	 *    local copy of the encryption-key it cannot re-encrypt the
	 *    encryption-key itself. But the encryption-key is needed to handle any
	 *    password data. So this method is called.
	 *
	 * Now the user has three options:
	 *
	 * -# Ignore this event. In that case steps 2. until 6. are repeated until
	 *    this situation is resolved.
	 *
	 *    Note: It may be possible that the situation is resolved
	 *    automatically. If the user does not answer this notification, but
	 *    instead starts sync on a different client that has a local copy of the
	 *    encryption-key, then the encryption-key is re-encrypted with the new
	 *    Opera Account password on that client, uploaded to Link, downloaded
	 *    and decrypted by this client. And in that case
	 *    OnSyncReencryptEncryptionKeyCancel() is called.
	 * -# Enter the old Opera Account password. In that case
	 *    ReencryptEncryptionKeyContext::DecryptWithPassword() is called and
	 *    sync tries to decrypt the received encryption-key. If that is
	 *    successful, sync can resume normal sync operations for passwords. If
	 *    that fails (e.g. because the user entered the wrong password), this
	 *    method is called once again.
	 * -# Choose to create a new encryption-key. This means that the existing
	 *    encryption-key and all existing wand-data is deleted on the Link
	 *    server and on all Opera clients. In that case
	 *    ReencryptEncryptionKeyContext::CreateNewEncryptionKey() is called and
	 *    after deleting all password entries sync can resume normal sync
	 *    operations for passwords.
	 *
	 * @param context is the ReencryptEncryptionKeyContext on which to specify
	 *  the result of the User's choice. The context is valid until any of its
	 *  callback methods are called (either
	 *  ReencryptEncryptionKeyContext::DecryptWithPassword() or
	 *  ReencryptEncryptionKeyContext::CreateNewEncryptionKey()) or until
	 *  OnSyncReencryptEncryptionKeyCancel() is called.
	 */
	virtual void OnSyncReencryptEncryptionKeyFailed(ReencryptEncryptionKeyContext* context) = 0;

	/**
	 * For some reason the specified ReencryptEncryptionKeyContext is no longer
	 * valid. This may happen on shutdown, when the context is deleted. This may
	 * also happen when the encryption-key was received.
	 *
	 * This method is also called (with context == 0) if
	 * ReencryptEncryptionKeyContext::DecryptWithPassword() was called with a
	 * password that successfully decrypted the received encryption-key.
	 *
	 * @param context is the ReencryptEncryptionKeyContext instance that shall
	 *  not be called any more.
	 */
	virtual void OnSyncReencryptEncryptionKeyCancel(ReencryptEncryptionKeyContext* context) = 0;
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
};

/**
 * This class is both a data listener and a data provider in one.
 *
 * General note about error handling for all methods:
 *
 * If the listener returns an error, the sync module will not update the sync
 * state and will assume that all data sent to the listener is lost and has to
 * be downloaded again from the server at some later time. When this happen,
 * OnSyncError() will be called on the UI listener to signal a serious error has
 * occurred. Therefore, the data client should be careful when it returns an
 * error as it will disable synchronisation and be signalled to the UI.  Only
 * return an error on serious, non-recoverable errors for synchronization (such
 * as OOM).
 */
class OpSyncDataClient
{
public:
	virtual ~OpSyncDataClient() {}

	/**
	 * This listener method is called for a data type the first time this
	 * particular data type is ready to be synchronised. Typically, the listener
	 * would then do house cleaning such as performing a pre-sync backup or
	 * similar.
	 *
	 * No data needs to be sent as a reaction to this call as the listener will
	 * receive a call to OpSyncDataFlush() immediately after this method has
	 * been called.
	 *
	 * @param type The type of data that can be initialised for first
	 *  synchronisation.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR or OpStatus::ERR_NO_MEMORY in case of an error.
	 */
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type) = 0;

	/**
	 * Called when a synchronisation was completed successfully. The
	 * OpSyncCollection is only valid for the during of this call, so the
	 * listener should not keep a pointer around but handle the data right away.
	 * The collection will only contain the data type(s) the listener has
	 * registered to receive notification about.
	 *
	 * If the implementation of this method cannot handle all of the specified
	 * new_items immediately - e.g. because it needs to display a dialog and
	 * wait for some user interaction - then it shall set the data_error to
	 * SYNC_DATAERROR_ASYNC. In that case the OpSyncCollection is kept as
	 * received items and either on the next sync-connection or when the client
	 * calls OpSyncCollection::ContinueSyncData() this method is called again
	 * with a collection that contains at least the same collection of
	 * OpSyncDataItem instances (though it may contain further items as well
	 * that were received in the new sync-connection).
	 *
	 * @param new_items New items received from the server
	 * @param data_error OpSyncDataError set by the data client if an data
	 *  related error occurs
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR or OpStatus::ERR_NO_MEMORY in case of an error.
	 */
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error) = 0;

	/**
	 * Called when the sync module needs the listener to provide data to it.
	 * This might happen if the server has notified the sync module that a data
	 * type is "dirty" and needs a complete data set sent. This might also
	 * happen if the sync module detects that a new data type has been enabled
	 * that has previously not been synchronised with the server.
	 *
	 * @param type The type of data the listener must send to the module.  If
	 *	the listener is called due to a dirty request from the server, the data
	 *	type provided will be DATAITEM_GENERIC.
	 * @param first_sync TRUE if this is the first synchronisation with this
	 *	data type (sync state is 0).
	 * @param is_dirty If TRUE, the server requested that a "dirty"
	 *	synchronisation should take place. This means that all data items for
	 *	this type should be added using OpSyncItem::CommitItem with the dirty
	 *	parameter set to TRUE so that this list can be merged with the servers
	 *	list of items when received.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR or OpStatus::ERR_NO_MEMORY in case of an error.
	 */
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty) = 0;

	/**
	 * This listener method is called for a data type each time the data type is
	 * turned on or off through the supports method. Used by data only clients
	 * who do not use the supports method.
	 *
	 * @param type The type of data that was enabled or disabled.
	 * @param has_support State of the data type now.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR or OpStatus::ERR_NO_MEMORY in case of an error.
	 */
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support) = 0;
};

class OpInternalSyncListener
{
public:
	virtual ~OpInternalSyncListener() {}

	/** Called when a synchronisation was started.
	 *
	 * @param items_sending TRUE if actual items are being sent to the server.
	 */
	virtual void OnSyncStarted(BOOL items_sending) = 0;

	/** Called when a synchronisation was completed unsuccessfully. The error
	 * code given describes the problem. The listener might also be called if
	 * synchronisation was disabled using SetSyncActive() and the client tries
	 * to do a synchronisation.
	 */
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_message) = 0;

	/**
	 * Called when a synchronisation was completed successfully. The
	 * OpSyncDataCollection is only valid for the during of this call, so the
	 * listener should not keep a pointer around but handle the data right away.
	 *
	 * @param new_items New items received from the server.
	 * @param sync_state Synchronisation state required by the server and kept
	 *  on the client.
	 * @param sync_server_info The OpSyncServerInformation that contains the
	 *  information about the Link server, that was sent in the server response.
	 */
	virtual void OnSyncCompleted(OpSyncCollection* new_items, OpSyncState& sync_state, OpSyncServerInformation& sync_server_info) = 0;

	/**
	 * Called when an item was successfully added for outgoing synchronisation.
	 * Will usually be used to start timers or other operations needed to
	 * synchronise new items.
	 *
	 * @param new_item TRUE when this is a new item.
	 */
	virtual void OnSyncItemAdded(BOOL new_item) = 0;

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	/**
	 * Is called by the SyncEncryptionKeyManager when a new encryption-key is
	 * created. In this case all data types that use the encryption-key need
	 * to perform a dirty sync.
	 */
	virtual void OnEncryptionKeyCreated() = 0;

	/**
	 * Called by the SyncEncryptionKeyManager when sync received an encrypted
	 * encryption-key it cannot decrypt and no local copy of the encryption-key
	 * is available. The implementation should notify all OpSyncUIListener
	 * instances about this event.
	 * @see OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed()
	 */
	virtual void OnSyncReencryptEncryptionKeyFailed(OpSyncUIListener::ReencryptEncryptionKeyContext* context) = 0;

	/**
	 * Called by the SyncEncryptionKeyManager when the
	 * ReencryptEncryptionKeyContext that was passed in a call to
	 * OnSyncReencryptEncryptionKeyFailed is no longer valid. The implementation
	 * should notify all OpSyncUIListener instances about this event.
	 * @see OpSyncUIListener::OnSyncReencryptEncryptionKeyCancel()
	 */
	virtual void OnSyncReencryptEncryptionKeyCancel(OpSyncUIListener::ReencryptEncryptionKeyContext* context) = 0;
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
};

/**
 * There is one global OpSyncCoordinator instance that is initialized in
 * SyncModule::Init(). That instance can be accessed via \c g_sync_coordinator.
 *
 * The platform needs to initialize the global OpSyncCoordinator instance by
 * - calling Init() to provide an OpSyncFactory instance;
 * - calling SetSyncUIListener() to add an OpSyncUIListener that is notified
 *   about some sync events;
 * - calling SetSyncDataClient() to set OpSyncDataClient instances for each
 *   different OpSyncDataItem::DataItemType that is provided by the platform.
 *
 * To perform a sync process, the user needs to provide the login information
 * (i.e. username and password) for the Link server (see SetLoginInformation()).
 *
 * On calling SetSyncActive(TRUE) the sync process is activated (the default
 * state is that syncing is not activated) and the first synchronization is
 * performed. Activating the sync process means that a single synchronization
 * process is repeatedly performed in an interval. The interval is controlled by
 * the Link server, the default value is 1:10 minutes (see
 * DEFAULT_SYNC_INTERVAL_LONG).
 *
 * If the sync process is active, an additional single synchronization can be
 * performed by executing the action OpInputAction::ACTION_SYNC_NOW (if that
 * action is enabled) or by calling SyncNow().
 *
 * A single synchronization (SyncNow()) consists of the following steps:
 *
 * - Initialize the OpSyncParser with username, password and the current sync
 *   state (OpSyncParser::Init()).
 * - Let the OpSyncParser collect the local data that should be sent to the Link
 *   server for each OpSyncDataItem::DataItemType from all registered
 *   OpSyncDataClient instances. The implementation of the OpSyncDataClient
 *   should get a OpSyncItem from this instance (GetSyncItem()), populate its
 *   data (OpSyncItem::SetData()), commit the item (OpSyncItem::CommitItem(),
 *   which creates a copy of the item in the OpSyncDataQueue::SYNCQUEUE_ACTIVE
 *   OpSyncCollection) and release the item (ReleaseSyncItem()).
 *   (See also GetOutOfSyncData()), BroadcastSyncDataInitialize(),
 *   OpSyncDataClient::SyncDataInitialize(), BroadcastSyncDataFlush() and
 *   OpSyncDataClient::SyncDataFlush()).
 * - Initialize the OpSyncTransportProtocol (OpSyncTransportProtocol::Init()).
 * - Move all items from the OpSyncDataQueue::SYNCQUEUE_ACTIVE to the
 *   OpSyncDataQueue::SYNCQUEUE_OUTGOING OpSyncCollection.
 * - Connect to the Link server to send the outgoing OpSyncCollection (see
 *   OpSyncTransportProtocol::Connect()). The MyOperaSyncTransportProtocol sends
 *   the data as an XML document (see MyOperaSyncParser::GenerateXML()).
 * - Wait for an answer from the Link server.
 * - The OpSyncTransportProtocol calls to OpSyncParser::Parse() to parse the
 *   received data.
 * - The MyOperaSyncParser::Parse() expects an XML document with a new sync
 *   state and sync items to update. The parser creates new OpSyncItem instances
 *   which are added to its incoming OpSyncDataCollection (see
 *   OpSyncParser::GetIncomingSyncItems()).
 * - The MyOperaSyncTransportProtocol creates a copy of the incoming
 *   OpSyncDataCollection and passes it to the
 *   OpInternalSyncListener::OnSyncCompleted() (see OnSyncCompleted()).
 * - OnSyncCompleted() handles the incoming OpSyncItem collection: if there is
 *   any dirty OpSyncItem (i.e. the OpSyncDataCollection
 *   OpSyncDataQueue::SYNCQUEUE_DIRTY_ITEMS is not empty), the incoming
 *   collection and the collection of dirty items is merged, producing a
 *   collection of items missing on the client and items missing on the server.
 *   If there are no dirty items, all received items need to be updated on the
 *   client.
 *
 * @section OpPrefsListener_Implementation Implementation of OpPrefsListener
 *
 * The OpSyncCoordinator implements the OpPrefsListener to be notified when the
 * user changes any sync prefs. The pref can either be changed directly by
 * calling
 * @code
 *  g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncBookmarks, 1);
 * @endcode
 * or it can be changed indirectly by calling SetSupports():
 * @code
 *  g_sync->SetSupports(SYNC_SUPPORTS_BOOKMARK);
 * @endcode
 * Both calls are equivalent and both calls will enable syncing of the specified
 * item type.
 * @see PrefChanged(), BroadcastSyncSupportChanged()
 */
class OpSyncCoordinator
	: public OpInternalSyncListener
	, public OpSyncTimeout
	, public OpPrefsListener
	, public OperaOauthManager::LoginListener
{
public:
	OpSyncCoordinator();
	virtual ~OpSyncCoordinator();

	/**
	 * Initialises the sync engine with client provided values or defaults.
	 *
	 * The OpSyncCoordinator instance registers itself as listener to
	 * PrefsCollectionSync pref changes.
	 *
	 * @param factory specifies the OpSyncFactory to use for getting the
	 *  instances that are used for syncing the data (i.e. OpSyncParser,
	 *  OpSyncTransportProtocol, OpSyncDataQueue and OpSyncAllocator). If NULL
	 *  is specified, the OpSyncCoordinator will create the default instance
	 *  which creates the MyOperaSyncParser, MyOperaSyncTransportProtocol, etc.
	 *  If the factory is not NULL, then the OpSyncCoordinator instance obtains
	 *  ownership of the specified class, i.e. the factory is OP_DELETE:d on
	 *  deleting this instance.
	 *  This method calls OpSyncFactory::Init("myopera") to initialize the
	 *  factory and then uses the factory to create all required instances.
	 * @param use_disk_queue is passed on to the same argument in a call to
	 *  OpSyncFactory::GetQueueHandler(). The value is TRUE if the
	 *  OpSyncDataQueue instance should store the sync queue in a persistent
	 *  file on disk and load that file. Such a file is used keep a queue of
	 *  sync items between two Opera sessions when the data was not yet synced.
	 *
	 *  If the default OpSyncFactory is used (or a factory that creates the
	 *  default OpSyncDataQueue instance) then this value may only be TRUE for
	 *  the global g_sync_coordinator, and it MUST be FALSE for any other
	 *  instances, as otherwise the file may be overwritten.
	 *
	 *  Using the disk queue can also be changed later by calling
	 *  SetUseDiskQueue().
	 * @return OpStatus::OK on success or an error code on failure.
	 *
	 * @note This class obtains the ownership of the specified factory even if
	 *  an error status is returned.
	 */
	OP_STATUS Init(OpSyncFactory* factory, BOOL use_disk_queue);

	/**
	 * Enable or disable storing the sync queue on disc.
	 *
	 * @param value if TRUE, the associated OpSyncDataQueue instance shall (from
	 *  now on) store the sync queue in a persistent file on disk and load that
	 *  file. Such a file is used keep a queue of sync items between two Opera
	 *  sessions when the data was not yet synced.
	 *
	 *  If the default OpSyncFactory is used (or a factory that creates the
	 *  default OpSyncDataQueue instance) then this value may only be TRUE for
	 *  the global g_sync_coordinator, and it MUST be FALSE for any other
	 *  instances, as otherwise the file may be overwritten.
	 */
	void SetUseDiskQueue(BOOL value);
	BOOL UseDiskQueue() const;

	/**
	 * Cleans up and frees resources.
	 *
	 * @return OK
	 */
	OP_STATUS Cleanup();

	/**
	 * Sets the listener(s) that will receive status notifications during sync.
	 * Multiple listeners can be set, but you must remove each listener with
	 * RemoveSyncDataClient() before the listener is freed.
	 *
	 * The listeners will be called in the order they are added, so make sure to
	 * set a listener for folder data types to make sure items referencing the
	 * folders are not orphaned when processing items.
	 *
	 * When the listener is set an initial call to OnSyncDataSupportsChanged()
	 * will be made to notify the listener of the initial support status of the
	 * data type.
	 *
	 * @param listener is the OpSyncDataClient instance that shall receive the
	 *  notifications. The caller remains the owner of the listener, i.e. the
	 *  caller needs to delete the listener instance when it is no longer used
	 *  (and call RemoveSyncDataClient() in that case).
	 * @param type specifies for which sync type the listener wants to receive
	 *  the information. One OpSyncDataClient instance may register itself for
	 *  multiple types.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS SetSyncDataClient(OpSyncDataClient* listener, OpSyncDataItem::DataItemType type);

	/**
	 * Removes a listener previously set with SetSyncListener().
	 *
	 * @param listener is the OpSyncDataClient to remove.
	 * @param type specifies for which sync type to remove the listener. One
	 *  OpSyncDataClient instance may register itself for multiple types, if you
	 *  delete the listener or if the listener shall not be notified anymore the
	 *  caller shall remove the listener for each type for which it was
	 *  registered.
	 */
	void RemoveSyncDataClient(OpSyncDataClient* listener, OpSyncDataItem::DataItemType type);

	/**
	 * Adds the specified OpSyncUIListener. That listener is notified about sync
	 * events.
	 *
	 * @note The caller remains the owner of the listener, i.e. the caller must
	 *  delete that instance when it is no longer needed.
	 *
	 * @note The listener shall be removed by calling RemoveSyncUIListener()
	 *  when the listener is no longer interested in the notifications or when
	 *  it is deleted.
	 *
	 * @param listener is the OpSyncUIListener to add.
	 */
	OP_STATUS SetSyncUIListener(OpSyncUIListener* listener);
	void RemoveSyncUIListener(OpSyncUIListener* listener);

	/**
	 * Gets a new OpSyncItem from the OpSyncAllocator that was provided by the
	 * OpSyncFactory.
	 *
	 * @note The default OpSyncAllocator calls OpSyncParser::GetSyncItem() to
	 *  get an OpSyncItem. The default MyOperaSyncParser::GetSyncItem() creates
	 *  a new MyOperaSyncItem instance, which is returned to the caller.
	 *
	 * Using this method is the only valid way to get a new OpSyncItem. It
	 * provides an easier way to get and add a new item to be synchronized.
	 *
	 * When you are done adding data using the OpSyncItem::SetData() method, you
	 * must call OpSyncItem::CommitItem() to signal the sync module that it can
	 * commit the changes into the queue for synchronising.
	 *
	 * When you no longer need the OpSyncItem (e.g. after calling
	 * OpSyncItem::CommitItem()), you must free the item using OP_DELETE or by
	 * calling ReleaseSyncItem().
	 *
	 * @param item receives on success the OpSyncItem that can by used to
	 *  synchronize some data.
	 * @param type specifies the OpSyncDataItem::DataItemType for the item.
	 * @param primary_key a OpSyncItem::Key definition of the unique primary key
	 *  for this type of data.
	 * @param key_data is the data associated with the specified
	 *  primary_key. This will be used by the sync module for identifying unique
	 *  items.
	 * @return OpStatus::OK on success or an error code if the OpSyncItem could
	 *  not be created.
	 *
	 * @see OpSyncAllocator::GetSyncItem()
	 */
	OP_STATUS GetSyncItem(OpSyncItem** item, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data);

	/**
	 * Frees a OpSyncItem previously allocated with GetSyncItem().
	 *
	 * @param item is the OpSyncItem to release.
	 */
	void ReleaseSyncItem(OpSyncItem* item) const { OP_DELETE(item); }

	/**
	 * Returns true if sync is enabled. Sync can be enabled by setting the
	 * preference SyncEnabled to 1.
	 * @note Testing if sync is enabled is different to test if sync had been
	 *  used once (see PrefsCollectionSync::SyncUsed). After sync has been used,
	 *  the user may disable sync again, so SyncEnabled() may return false while
	 *  PrefsCollectionSync::SyncUsed remains true.
	 * @return true if sync is enabled.
	 */
	bool SyncEnabled() const {
		return static_cast<bool>(g_opera && g_pcsync && g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncEnabled));
	}

	/**
	 * Method for the client to tell the sync module what data it supports. The
	 * client should call this method for each data type it supports
	 * synchronising.
	 *
	 * @param supports OpSyncSupports item with the data the client supports.
	 * @param has_support TRUE (default) to set support for this type, FALSE to
	 *  remove support for it.
	 * @retval OpStatus::OK on success
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if the parser is not initialised.
	 * @retval OpStatus::ERR_NO_MEMORY, OpStatus::ERR_OUT_OF_RANGE or other
	 *  errors.
	 */
	OP_STATUS SetSupports(OpSyncSupports supports, BOOL has_support = TRUE);
	BOOL GetSupports(OpSyncSupports supports);

	/**
	 * Resets the supports state for the specified supports type. This means
	 * that in the next sync-connection this client requests the back-log since
	 * state "0". I.e. all local data for the specified supports type is
	 * uploaded to the Link server and the missing data is downloaded from the
	 * Link server. This can be useful if the client suspects that the local
	 * data is compromised.
	 *
	 * @param supports specifies the OpSyncSupports type to reset. If this value
	 *  is SYNC_SUPPORTS_MAX, then the supports state is reset for all supports
	 *  types.
	 * @param disable_support can be set to TRUE if the caller wants to disable
	 *  syncing for the specified supports type.
	 */
	OP_STATUS ResetSupportsState(OpSyncSupports supports, BOOL disable_support=FALSE);

	/**
	 * Method to get the data item types associated with a support type.
	 *
	 * Note that the entries in the INT32 vector must be cast to
	 * OpSyncDataItem::DataItemType before use.
	 *
	 * @param supports The support to check
	 * @param types	   The data item types associated with the support type.
	 * 				   Cast to OpSyncDataItem::DataItemType before use
	 * @return OK er ERR_NO_MEMORY
	 */
	static OP_STATUS GetTypesFromSupports(OpSyncSupports supports, OpINT32Vector& types);

	/**
	 * Method to get the supports type associated with a data item type.
	 *
	 * @param type The data item type to check.
	 * @return The associated OpSyncSupports type.
	 */
	static OpSyncSupports GetSupportsFromType(OpSyncDataItem::DataItemType type);

	/**
	 * Method for the client to tell the sync module to clear all previous data
	 * set with SetSupports().
	 *
	 * @retval OpStatus::OK on success
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if the parser is not initialised.
	 */
	OP_STATUS ClearSupports();

	/**
	 * Method for the client to tell the sync module information about the
	 * platform.
	 *
	 * @param type OpSyncSystemInformationType item with the data the client
	 *  supports
	 * @param data The data for the given key.
	 * @retval OpStatus::OK on success
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if the parser is not initialised.
	 * @retval OpStatus::ERR_NO_MEMORY, OpStatus::ERR_OUT_OF_RANGE or other
	 *  errors.
	 */
	OP_STATUS SetSystemInformation(OpSyncSystemInformationType type, const OpStringC& data);

	/**
	 * Set if sync should be active or not. If the client enters offline mode,
	 * it can signal the sync module to not try to do any unattended
	 * synchronisations. The default mode is that sync is not active.
	 *
	 * @param active TRUE to activate sync, FALSE to enter offline mode.
	 * @return OpStatus::OK on success
	 */
	OP_STATUS SetSyncActive(BOOL active);

	/**
	 * Returns if sync is active or not. Note that this is not if a sync is
	 * currently in progress, but just if sync is disabled or not.
	 *
	 * @return TRUE if sync is active
	 */
	BOOL IsSyncActive() const { return m_sync_active; }

	/**
	 * Returns if sync is currently in progress or not.
	 *
	 * @return TRUE if sync is in progress
	 */
	BOOL IsSyncInProgress() const { return m_sync_in_progress; }

	/**
	 * Sets a predefined username and password for the given provider.
	 *
	 * @param provider Only "myopera" is currently available
	 * @param username If authentication is required, this is the username.
	 * @param password If authentication is required, this is the password.
	 * @retval OpStatus::OK on success.
	 * @retval Any other error depending on the error condition.
	 */
	OP_STATUS SetLoginInformation(const uni_char* provider, const OpStringC& username, const OpStringC& password);

 	/**
 	 * Get the login information
	 *
	 * @param username	The username used to log into the provider.
	 * @param password  The password used to log into the provider.
	 * @return OK or ERROR_NO_MEMORY
	 */
	OP_STATUS GetLoginInformation(OpString& username, OpString& password);

	/**
	 * Updates other items scheduled for sending with the given information.
	 *
	 * @param item_type The item type to update. This will be used to find the
	 *  correct primary key for the item to update.
	 * @param next_item_key_id This argument will contain the data of the
	 *  primary key of the item to update.
	 * @param next_item_key_to_update If next_item_key_id is provided, this is
	 *  the key to update on the other item.
	 * @param next_item_key_data_to_update If next_item_key_id is provided, this
	 *  is the data for the next_item_key_to_update to update on the other item.
	 * @retval OpStatus::OK on success.
	 * @retval Any other error depending on the error condition.
	 */
	OP_STATUS UpdateItem(OpSyncDataItem::DataItemType item_type, const uni_char* next_item_key_id, OpSyncItem::Key next_item_key_to_update, const uni_char* next_item_key_data_to_update);

	/** Handle the supplied OpInputAction.
	 * @param action Is the action to handle.
	 * @return TRUE if the action was handled, FALSE otherwise.
	 */
	BOOL OnInputAction(OpInputAction* action);

	/**
	 * Starts a synchronisation of data immediately.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval Any other error depending on the error condition.
	 */
	OP_STATUS SyncNow();

	/**
	 * Gets an existing OpSyncItem that has been added for synchronisation
	 * previously.
	 * This method is normally not used by any client as GetSyncItem() will
	 * allow you to update the data on existing items too. This is primarily
	 * used for selftests to verify data.
	 *
	 * @param item Item to be synchronised.
	 * @param type a OpSyncDataItem::DataItemType definition for the type of
	 *  item to get.
	 * @param primary_key a OpSyncItem::Key definition of the unique primary key
	 *  for this type of data.
	 * @param key_data The data associated with the primary_key given. This will
	 *  be used by the sync module for identifying unique items.
	 * @retval OpStatus::OK on success.
	 * @retval Any other error depending on the error condition.
	 */
	OP_STATUS GetExistingSyncItem(OpSyncItem** item, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data);

	BOOL FreeCachedData(BOOL toplevel_context);

	/**
	 * Called by an OpSyncDataClient when the client wants to continue to
	 * receive a pending OpSyncDataItem collection. If this method is called and
	 * if there is data available for the specified supports type, then all
	 * registered OpSyncDataClient instances for the specified supports type
	 * receive a call to OpSyncDataClient::SyncDataAvailable().

	 * If an implementation of OpSyncDataClient::SyncDataAvailable() returns the
	 * OpSyncDataError code SYNC_DATAERROR_ASYNC it indicates that the client
	 * needs to perform an asynchronous action and that it cannot handle the
	 * data right now. When the asynchronous action is finished, the
	 * OpSyncDataClient instance can call this method.
	 *
	 * @note If there is no data available for the specified supports type, the
	 *  method OpSyncDataClient::SyncDataAvailable() is not called.
	 *
	 * @param supports is the OpSyncSupports type for which this instance shall
	 *  call OpSyncDataClient::SyncDataAvailable().
	 */
	void ContinueSyncData(OpSyncSupports supports);

	/**
	 * @name Implementation of OpInternalSyncListener
	 * @{
	 */

	virtual void OnSyncStarted(BOOL items_sent);
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_msg);
	virtual void OnSyncCompleted(OpSyncCollection* new_items, OpSyncState& sync_state, OpSyncServerInformation& sync_server_info);
	virtual void OnSyncItemAdded(BOOL new_item);
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	virtual void OnEncryptionKeyCreated();
	virtual void OnSyncReencryptEncryptionKeyFailed(OpSyncUIListener::ReencryptEncryptionKeyContext* context);
	virtual void OnSyncReencryptEncryptionKeyCancel(OpSyncUIListener::ReencryptEncryptionKeyContext* context);
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

	/** @} */ // Implementation of OpInternalSyncListener

	/**
	 * Checks if there are items to be sent to the server
	 *
	 * @return TRUE if there are local items ready to be sent to the server,
	 * otherwise FALSE
	 */
	BOOL HasQueuedItems() { return m_data_queue->HasQueuedItems(); }

private:
	/**
	 * Starts a synchronisation.
	 *
	 * If the prefs value of PrefsCollectionSync::CompleteSync is true or the
	 * data-queue has a dirty item, a complete synchronization of all data on
	 * the server will be performed.
	 *
	 * If not yet logged in (on the Auth server), the client requests an Oauth
	 * token from the Auth server (see SyncAuthenticate()) and when the Oauth
	 * token is received (see OnLoginStatusChange()) the client connects itself
	 * to the Link server (see SyncConnect()).
	 * \msc
	 * OpSyncCoordinator,OperaOauthManager,Auth_Server;
	 * OpSyncCoordinator->OpSyncCoordinator [label="Post MSG_SYNC_AUTHENTICATE"];
	 * OpSyncCoordinator=>OpSyncCoordinator [label="SyncAuthenticate()", url="\ref SyncAuthenticate()"];
	 * OpSyncCoordinator=>OperaOauthManager [label="g_opera_oauth_manager->Login()", url="\ref OperaOauthManager::Login()"];
	 * OperaOauthManager->Auth_Server [label="log in"];
	 * OperaOauthManager<<Auth_Server [label="oauth_token"];
	 * OperaOauthManager=>OpSyncCoordinator [label="OnLoginStatusChange()", url="\ref OnLoginStatusChange()"];
	 * OpSyncCoordinator->OpSyncCoordinator [label="Post MSG_SYNC_CONNECT"];
	 * OpSyncCoordinator->OpSyncCoordinator [label="SyncConnect()", url="\ref SyncConnect()"];
	 * \endmsc
	 *
	 * @retval OpStatus::OK on success.
	 * @retval ERR_NO_ACCESS if sync is disabled. In this case the listeners
	 *  will also be called with the error code SYNC_ERROR_SYNC_DISABLED.
	 * @retval Any other error depending on the error condition.
	 */
	OP_STATUS StartSync();

	void SetSyncInProgress(BOOL in_progress) {
		OP_NEW_DBG("OpSyncCoordinator::SetSyncInProgress()", "sync");
		OP_DBG(("%s -> %s",
				(m_sync_in_progress?"in progress":m_sync_active?"active":"not active"),
				(in_progress?"in progress":m_sync_active?"active":"not active")));
		m_sync_in_progress = in_progress;
	}

	/**
	 * Requests an oauth token from the Auth server. The oauth token is used to
	 * authenticate this client to the Link server. When the oauth token is
	 * available, OnLoginStatusChange() is called.
	 *
	 * This method is called on handling the MSG_SYNC_AUTHENTICATE message.
	 */
	void SyncAuthenticate();

	/**
	 * @name Implementation of OperaOauthManager::LoginListener
	 * @{
	 */

	/**
	 * This callback method is called as a result to an authentication request
	 * to the Auth server.
	 *
	 * If the Auth server returned an Oauth token, the specified auth_status is
	 * OperaOauthManager::OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED and in that
	 * case the sync process is continued by posting the message
	 * MSG_SYNC_CONNECT which will be handled by calling SyncConnect().
	 *
	 * In case of an error auth_status has an error status and OnSyncError() is
	 * called with a translation of that status and the specified
	 * error-message.
	 *
	 * @param auth_status is the status of the authentication request.
	 * @param server_error_message contains the error message from the Auth
	 *  server in case of an error.
	 */
	virtual void OnLoginStatusChange(OperaOauthManager::OAuthStatus auth_status, const OpStringC& server_error_message);

	/** @} */ // Implementation of OperaOauthManager::LoginListener

	/**
	 * Start a connection to the Link server. This method is called in the sync
	 * process when the client is logged in on the Auth server and has an
	 * Oauth token.
	 */
	void SyncConnect();

	/**
	 * Gets any data that is currently supported and on a sync state less that
	 * the current main sync state and requests it from the client and adds it
	 * to the current server query.
	 *
	 * @return OK or Error
	 */
	OP_STATUS GetOutOfSyncData(SyncSupportsState supports_state);

	class OpSyncDataListenerVector : public OpVector<class OpSyncDataClientItem>
	{
	public:
		virtual ~OpSyncDataListenerVector() {
			OP_ASSERT(GetCount() == 0 && "An OpSyncDataClient was not removed");
		}

		OpSyncDataClientItem* FindListener(OpSyncDataClient* item);
	};

protected:
	/**
	 * Handles the items from a dirty synchronisation.  The sequence is as
	 * follows:
	 *
	 * - Client sends all the items it has to the module.
	 * - Module requests all items from the server.
	 * - Data from the server is merged with existing items in the client
	 *   collection.
	 * - Client is notified about new items it's missing and changed items due
	 *   to a merge.
	 * - Server is notified about items it's missing that the client has.
	 *
	 * @param items_from_client A complete set of items that the client has.
	 * @param items_from_server A complete set of items that the server has.
	 * @param items_missing_on_client The resulting set of items missing from
	 *  the client or items that was changed by server data due to a merge.
	 * @param items_missing_on_server The resulting set of items missing from
	 *  the server.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS MergeDirtySyncItems(/* in */ OpSyncDataCollection& items_from_client, /* in */ OpSyncDataCollection& items_from_server, /* out */ OpSyncDataCollection& items_missing_on_client, /* out */ OpSyncDataCollection& items_missing_on_server);

private:
	/** Called when the sync state for a data type is still at the default
	 * value. This method calls OpSyncDataClient::SyncDataInitialize() on all
	 * data clients that are registered for the specified type. */
	virtual void BroadcastSyncDataInitialize(OpSyncDataItem::DataItemType type);

	/** Called when the sync module needs data from the client. This method
	 * calls OpSyncDataClient::SyncDataFlush() on all data clients that are
	 * registered for the specified type. */
	void BroadcastSyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);

	void BroadcastSyncError(OpSyncError error, const OpStringC& error_msg);

	/**
	 * @name Implementation of OpSyncTimeout
	 * @{
	 */

	virtual void OnTimeout();

	/** @} */ // Implementation of OpSyncTimeout

	/**
	 * @name Implementation of MessageObject
	 * @{
	 */

	/**
	 * Handles the messages
	 * - MSG_SYNC_AUTHENTICATE by calling SyncAuthenticate()
	 * - MSG_SYNC_CONNECT by calling SyncConnect()
	 * - MSG_SYNC_FLUSH_DATATYPE by calling BroadcastSyncDataFlush() where par1 is
	 *   the OpSyncDataItem::DataItemType to flush.
	 * Other messages are passed on to OpSyncTimeout::HandleCallback(), so
	 * OnTimeout() may be called.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** @} */ // Implementation of OpSyncTimeout

	/**
	 * Calls OpSyncDataClient::SyncDataAvailable() on all registered
	 * OpSyncDataClient instances that need to receive that call.
	 *
	 * If supports is SYNC_SUPPORTS_MAX, then
	 * OpSyncDataClient::SyncDataAvailable() is called for all OpSyncSupports
	 * types. Otherwise it is only called for the specified supports type.
	 *
	 * The OpSyncSupports type is associated to a list of
	 * OpSyncDataItem::DataItemType types. The received OpSyncItem items are
	 * sorted by its OpSyncDataItem::DataItemType and
	 * OpSyncDataClient::SyncDataAvailable() is called for each
	 * OpSyncDataItem::DataItemType with the corresponding collection if the
	 * OpSyncDataClient supports that type.
	 *
	 * The collection of OpSyncItem items that was handled is released.
	 *
	 * @param supports_state specifies for which OpSyncSupports type the
	 *  registered OpSyncDataClient instances shall be called.
	 * @param sync_state is the current OpSyncState as received from the Link
	 *  server. If an OpSyncDataClient::SyncDataAvailable() returns
	 *  SYNC_DATAERROR_ASYNC for one supports type, that sync state is marked as
	 *  not persistent, i.e. it will not be updated in the preferences file.
	 *
	 * @retval OpStatus::OK on successfully notifying all registered
	 *  OpSyncDataClient instances.
	 * @retval The error-code returned by OpSyncDataClient::SyncDataAvailable()
	 *  if one OpSyncDataClient reports an error. When any OpSyncDataClient
	 *  returns an error status we stop notifying any other OpSyncDataClient.
	 *
	 * @todo Check how to handle the remaining sync-items if any
	 *  OpSyncDataClient returns an error status.
	 */
	OP_STATUS BroadcastDataAvailable(SyncSupportsState supports_state, OpSyncState& sync_state);

	void ErrorCleanup(OP_STATUS status);

	/**
	 * @name Implementation of OpPrefsListener
	 *
	 * @see @ref OpPrefsListener_Implementation
	 * @{
	 */

	virtual void PrefChanged(OpPrefsCollection::Collections is, int pref, int newvalue);
	virtual void PrefChanged(OpPrefsCollection::Collections is, int pref, const OpStringC& newvalue);

	/** @} */ // Implementation of OpPrefsListener

	/**
	 * While a sync is in progress, BroadcastSyncSupportChanged() does not
	 * notify the registered OpSyncDataClient instances. Instead a flag is set
	 * in m_pending_support_state and this method is called when a single
	 * sync-connection is finished (see OnSyncCompleted()) to deliver the
	 * notification.
	 */
	void BroadcastPendingSyncSupportChanged();

	/**
	 * If sync was used once and is disabled now, we want to continue to collect
	 * the client's sync updates in the update-queue for the next
	 * SYNC_CHECK_LAST_USED_DAYS days. Thus on re-enabling sync within that
	 * period we can simply upload the queue.
	 * @note See also DSK-309316 "Manually stopping Link should completely
	 *  'unlink' the client from the synchronization process".
	 *
	 * This method returns true if we should keep data clients enabled even if
	 * sync is currently disabled.
	 * @retval true if sync is enabled (i.e. SyncEnabled() is true).
	 * @retval true if sync is disabled, but sync was last used within
	 *  SYNC_CHECK_LAST_USED_DAYS days.
	 * @retval false if sync is disabled and sync was not used within
	 *  SYNC_CHECK_LAST_USED_DAYS days.
	 */
	bool KeepDataClientsEnabled() const;

	/**
	 * Informs all OpSyncDataClient instances that were registered for all
	 * OpSyncDataItem::DataItemType that are associated with the specified
	 * OpSyncSupports type, that the support has been changed (i.e. the support
	 * was enabled or disabled).
	 * @param supports is the supports type that has changed.
	 * @see OpSyncDataClient::SyncDataSupportsChanged()
	 * @see SetSyncDataClient()
	 *
	 * @note An OpSyncDataClient is registered for an
	 *  OpSyncDataItem::DataItemType. An OpSyncSupports type is related to one
	 *  or more OpSyncDataItem::DataItemType. See GetTypesFromSupports().
	 */
	void BroadcastSyncSupportChanged(OpSyncSupports supports, bool has_support);

	/**
	 * For each OpSyncDataItem::DataItemType all OpSyncDataClient instances that
	 * are registered for that type are informed about the current supports
	 * status. This method is called e.g. when the SyncEnabled preference
	 * setting is changed.
	 *
	 * @note If sync is disabled now, but was used once within
	 *  SYNC_CHECK_LAST_USED_DAYS days, the OpSyncDataClients for a supports
	 *  type that has an enabled pref will be kept enabled. Thus these clients
	 *  can continue to write updates to the sync queue. The sync queue can then
	 *  be picked up when sync is enabled again.
	 * @note See also DSK-309316 "Manually stopping Link should completely
	 *  'unlink' the client from the synchronization process".
	 * @note gogi and Desktop currently disagree on when the preference SyncUsed
	 *  is set. gogi sets SyncUsed to a true value the first time data is
	 *  received from the Link Server. Desktop sets SyncUsed to a true value on
	 *  enabling Sync, i.e. before Opera connects with the Link Server. This may
	 *  result in different behaviour.
	 *
	 * @see OpSyncDataClient::SyncDataSupportsChanged()
	 * @see SetSyncDataClient()
	 * @see KeepDataClientsEnabled()
	 */
	void BroadcastSyncSupportChanged();

protected:
	OpSyncFactory* m_sync_factory;
	OpSyncAllocator* m_allocator;
	OpSyncTransportProtocol* m_transport;
	OpSyncParser* m_parser;
	OpSyncDataQueue* m_data_queue;

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
public:
	/**
	 * Called by SyncEncryptionKeyManager::InitL() to set the associated
	 * SyncEncryptionKeyManager instance.
	 * @see m_encryption_key_manager
	 */
	void SetSyncEncryptionKeyManager(SyncEncryptionKeyManager* encryption_key_manager) { m_encryption_key_manager = encryption_key_manager; }

private:
	/**
	 * The associated SyncEncryptionKeyManager instance. For the global
	 * g_sync_coordinator this is the global g_sync_encryption_key_manager.
	 * For selftests this is the corresponding instance from the test.
	 * SyncEncryptionKeyManager::Init() sets this attribute.
	 */
	SyncEncryptionKeyManager*	m_encryption_key_manager;
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

private:
	// variables used if the sync module needs to start an unattended sync
	OpString m_save_server;
	UINT32 m_save_port;
	OpString m_save_username;
	OpString m_save_password;

	/** Stores whether or not each OpSyncSupports type is enabled. */
	SyncSupportsState m_supports_state;

	/**
	 * If the support for an OpSyncSupports type was changed while a sync
	 * connection is in progress we should not notify the associated
	 * OpSyncDataClient instance immediately. Instead the change for the
	 * OpSyncSupports type is recorded here by setting the supports state to
	 * true. When the sync-connection is completed (see OnSyncCompleted(),
	 * m_sync_in_progress), the change is broadcast to all listeners.
	 *
	 * @see SetSyncDataClient(), SetSupports(), IsSyncInProgress(),
	 *  m_sync_in_progress, BroadcastSyncSupportChanged(),
	 *  BroadcastPendingSyncSupportChanged()
	 */
	SyncSupportsState m_pending_support_state;

	/** Stores the current sync-state for each OpSyncSupports type. */
	OpSyncState m_sync_state;

	OpSyncDataListenerVector m_listeners;
	OpVector<OpSyncUIListener> m_uilisteners;

	/** Information sent from the server to the client. */
	OpSyncServerInformation m_sync_server_info;
	BOOL m_sync_active;					///< Is sync active?
	BOOL m_sync_in_progress;			///< Is a sync in progress?
#ifdef SELFTEST
	BOOL m_is_selftest;
#endif // SELFTEST
	/** Used when a complete sync is in progress so we don't ask the client for
	 * all items more than once (on errors). */
	BOOL m_is_complete_sync_in_progress;

	/**
	 * TRUE if Init() was called. Init() registers this instance as listener to
	 * sync prefs changes. Only if this flag is true, the destructor needs to
	 * unregister itself again.
	 */
	BOOL m_is_initialised;

	/**
	 * TRUE if this instance is registered as OperaOauthManager::LoginListener
	 * in the g_opera_oauth_manager instance. Usually this is done on the first
	 * sync connection (the first time SyncNow() is called).
	 */
	BOOL m_is_login_listener;

#ifdef SELFTEST
	friend class SelftestHelper;
public:
	class SelftestHelper
	{
		OpSyncCoordinator* m_coordinator;
	public:
		SelftestHelper(OpSyncCoordinator* coordinator)
			: m_coordinator(coordinator) {}

		const OpSyncDataQueue* GetSyncDataQueue() const {
			return m_coordinator->m_data_queue; }
		OpSyncDataQueue* GetSyncDataQueue() {
			return m_coordinator->m_data_queue; }

		const OpSyncParser* GetSyncParser() const {
			return m_coordinator->m_parser; }
		OpSyncParser* GetSyncParser() {
			return m_coordinator->m_parser; }

		const OpSyncState& GetSyncState() const {
			return m_coordinator->m_sync_state; }

		void SetSyncInProgress(BOOL in_progress) {
			m_coordinator->m_sync_in_progress = in_progress;
		}

		OP_STATUS BroadcastDataAvailable(SyncSupportsState supports_state, OpSyncState& sync_state) {
			return m_coordinator->BroadcastDataAvailable(supports_state, sync_state);
		}

		OP_STATUS StartFileSync(const OpStringC& filename) {
			RETURN_IF_ERROR(m_coordinator->m_save_server.Set(filename));
			m_coordinator->m_is_selftest = TRUE;
			m_coordinator->SyncConnect();
			m_coordinator->m_is_selftest = FALSE;
			return OpStatus::OK;
		}

		OP_STATUS MergeDirtySyncItems(OpSyncDataCollection& items_from_client, OpSyncDataCollection& items_from_server, OpSyncDataCollection& items_missing_on_client, OpSyncDataCollection& items_missing_on_server) {
			return m_coordinator->MergeDirtySyncItems(items_from_client, items_from_server, items_missing_on_client, items_missing_on_server);
		}

		void ClearDataQueue() {
			// clears all data queues:
			m_coordinator->m_data_queue->Shutdown();
		}
	};
#endif // SELFTEST
};

#endif //_SYNC_COORDINATOR_H_INCLUDED_
