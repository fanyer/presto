/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/util/str.h"
#include "modules/util/opstring.h"

#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_encryption_key.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_transport.h"
#include "modules/sync/sync_parser.h"
#include "modules/sync/sync_util.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/opera_auth/opera_auth_module.h"
#include "modules/opera_auth/opera_auth_oauth.h"

/**
 * This class is used in the vector class OpSyncDataListenerVector. One
 * OpSyncDataClient instance can be associated to multiple
 * OpSyncDataItem::DataItemType values. An instance of this class maps this
 * association. An instance of OpSyncDataListenerVector keeps a list of all
 * associations.
 */
class OpSyncDataClientItem
{
public:
	OpSyncDataClientItem(OpSyncDataClient* listener)
		: m_listener(listener) {}
	virtual ~OpSyncDataClientItem() {}

	OpSyncDataClient* Get() { return m_listener; }
	const OpSyncDataClient* Get() const { return m_listener; }

	BOOL HasType(OpSyncDataItem::DataItemType type) const {
		return (type == OpSyncDataItem::DATAITEM_GENERIC ||
				m_types.Find(static_cast<INT32>(type)) >= 0);
	}

	OP_STATUS AddType(OpSyncDataItem::DataItemType type) {
		if (HasType(type))
			return OpStatus::OK;
		RETURN_IF_ERROR(m_types.Add(static_cast<INT32>(type)));
		return OpStatus::OK;
	}

	void RemoveType(OpSyncDataItem::DataItemType type) {
		m_types.RemoveByItem(static_cast<INT32>(type));
	}

	unsigned int TypeCount() const { return m_types.GetCount(); }
	OpSyncDataItem::DataItemType GetType(unsigned int index) const {
		return static_cast<OpSyncDataItem::DataItemType>(m_types.Get(index));
	}

private:
	OpSyncDataClient* m_listener;
	OpINT32Vector m_types;
};

// ========== OpSyncTimeout ===================================================

OpSyncTimeout::OpSyncTimeout(OpMessage message)
	: m_timeout(SYNC_LOADING_TIMEOUT)
	, m_message(message)
{
}

/* virtual */
OpSyncTimeout::~OpSyncTimeout()
{
	StopTimeout();
	g_main_message_handler->UnsetCallBack(this, m_message);
}

unsigned int OpSyncTimeout::SetTimeout(unsigned int timeout)
{
	OP_NEW_DBG("OpSyncTimeout::SetTimeout()", "sync");
	OP_DBG(("%d -> %d", m_timeout, timeout));
	unsigned int old_timeout = m_timeout;
	m_timeout = timeout;
	return old_timeout;
}

/* virtual */
void OpSyncTimeout::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == m_message)
		OnTimeout();
	else
		OP_ASSERT(!"Unknown message!");
}

OP_STATUS OpSyncTimeout::StartTimeout(unsigned int timeout)
{
	OP_NEW_DBG("OpSyncTimeout::StartTimeout()", "sync");
	OP_DBG(("timeout: %ds", timeout));
	if (!g_main_message_handler->HasCallBack(this, m_message, 0))
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, m_message, 0));
	g_main_message_handler->RemoveDelayedMessage(m_message, 0, 0);
	g_main_message_handler->PostDelayedMessage(m_message, 0, 0, timeout*1000);
	return OpStatus::OK;
}

OP_STATUS OpSyncTimeout::RestartTimeout()
{
	OP_NEW_DBG("OpSyncTimeout::RestartTimeout()", "sync");
	return StartTimeout(m_timeout);
}

void OpSyncTimeout::StopTimeout()
{
	if (g_main_message_handler->HasCallBack(this, m_message, 0))
	{
		OP_NEW_DBG("OpSyncTimeout::StopTimeout()", "sync");
		g_main_message_handler->RemoveDelayedMessage(m_message, 0, 0);
		g_main_message_handler->UnsetCallBack(this, m_message);
	}
}

// ========== OpSyncCoordinator::OpSyncDataListenerVector =====================

OpSyncDataClientItem* OpSyncCoordinator::OpSyncDataListenerVector::FindListener(OpSyncDataClient* item)
{
	for (unsigned i = 0; i < GetCount(); i++)
	{
		OpSyncDataClientItem* tmp = Get(i);
		OP_ASSERT(tmp);
		if (tmp->Get() == item)
			return tmp;
	}
	return NULL;
}

// ========== OpSyncCoordinator ===============================================

OpSyncCoordinator::OpSyncCoordinator()
	: OpSyncTimeout(MSG_SYNC_POLLING_TIMEOUT)
	, m_sync_factory(NULL)
	, m_allocator(NULL)
	, m_transport(NULL)
	, m_parser(NULL)
	, m_data_queue(NULL)
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	, m_encryption_key_manager(0)
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
	, m_sync_active(FALSE)
	, m_sync_in_progress(FALSE)
#ifdef SELFTEST
	, m_is_selftest(FALSE)
#endif // SELFTEST
	, m_is_complete_sync_in_progress(FALSE)
	, m_is_initialised(FALSE)
	, m_is_login_listener(FALSE)
{
	OP_NEW_DBG("OpSyncCoordinator::OpSyncCoordinator()", "sync");
#ifdef _DEBUG
//	int size = sizeof(OpSyncDataItem);
//	size = size;	// get rid of warning
#endif

	g_main_message_handler->SetCallBack(this, MSG_SYNC_FLUSH_DATATYPE, (MH_PARAM_1)0);
	g_main_message_handler->SetCallBack(this, MSG_SYNC_AUTHENTICATE, (MH_PARAM_1)0);
	g_main_message_handler->SetCallBack(this, MSG_SYNC_CONNECT, (MH_PARAM_1)0);
}

OpSyncCoordinator::~OpSyncCoordinator()
{
	OP_NEW_DBG("OpSyncCoordinator::~OpSyncCoordinator()", "sync");
	g_main_message_handler->RemoveDelayedMessage(MSG_SYNC_FLUSH_DATATYPE, 0, 0);
	g_main_message_handler->UnsetCallBack(this, MSG_SYNC_FLUSH_DATATYPE);
	g_main_message_handler->UnsetCallBack(this, MSG_SYNC_AUTHENTICATE);
	g_main_message_handler->UnsetCallBack(this, MSG_SYNC_CONNECT);
	Cleanup();
}

OP_STATUS OpSyncCoordinator::SetSyncDataClient(OpSyncDataClient* listener, OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("OpSyncCoordinator::SetSyncDataClient()", "sync");
	OP_DBG(("") << "listener: " << listener << "; type: " << type);
	type = OpSyncDataItem::BaseTypeOf(type);
	OP_DBG(("base type: ") << type);

	OpSyncDataClientItem* item = m_listeners.FindListener(listener);
	if (!item)
	{
		OpAutoPtr<OpSyncDataClientItem> auto_item(OP_NEW(OpSyncDataClientItem, (listener)));
		RETURN_OOM_IF_NULL(auto_item.get());
		OP_DBG(("Add new OpSyncDataClientItem: %p", auto_item.get()));
		RETURN_IF_ERROR(m_listeners.Add(auto_item.get()));
		item = auto_item.release();
	}
	RETURN_IF_ERROR(item->AddType(type));

	bool sync_enabled = KeepDataClientsEnabled();
#ifdef _DEBUG
	if (sync_enabled && !SyncEnabled())
		OP_DBG(("sync was last used less than %d days ago at: %d; so don't disable any type that is currently enabled.", SYNC_CHECK_LAST_USED_DAYS, g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLastUsed)));
#endif // _DEBUG
	OpSyncSupports supports = OpSyncCoordinator::GetSupportsFromType(type);
	return item->Get()->SyncDataSupportsChanged(type, sync_enabled && GetSupports(supports));
}

void OpSyncCoordinator::RemoveSyncDataClient(OpSyncDataClient* listener, OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("OpSyncCoordinator::RemoveSyncDataClient()", "sync");
	OP_DBG(("") << "listener: " << listener << "; type: " << type);

	OpSyncDataClientItem* item = m_listeners.FindListener(listener);
	if (item)
	{
		item->RemoveType(OpSyncDataItem::BaseTypeOf(type));
		if (!item->TypeCount())
		{
			m_listeners.RemoveByItem(item);
			OP_DBG(("Removed OpSyncDataClientItem: %p", item));
			OP_DELETE(item);
		}
	}
}

OP_STATUS OpSyncCoordinator::SetSyncUIListener(OpSyncUIListener* listener)
{
	if (m_uilisteners.Find(listener) < 0)
	{
		OP_NEW_DBG("OpSyncCoordinator::SetSyncUIListener()", "sync");
		OP_DBG(("Add OpSyncUIListener %p", listener));
		return m_uilisteners.Add(listener);
	}
	return OpStatus::OK;
}

void OpSyncCoordinator::RemoveSyncUIListener(OpSyncUIListener* listener)
{
	if (m_uilisteners.Find(listener) > -1)
	{
		OP_NEW_DBG("OpSyncCoordinator::RemoveSyncUIListener()", "sync");
		OP_DBG(("Remove OpSyncUIListener %p", listener));
		m_uilisteners.RemoveByItem(listener);
	}
}

OP_STATUS OpSyncCoordinator::Cleanup()
{
	OP_NEW_DBG("OpSyncCoordinator::Cleanup()", "sync");
	if (m_is_login_listener)
		OpStatus::Ignore(g_opera_oauth_manager->RemoveListener(this));
	m_is_login_listener = FALSE;
	if (m_is_initialised)
		g_pcsync->UnregisterListener(this);
	m_is_initialised = FALSE;
	if (m_data_queue)
		m_data_queue->Shutdown();
	OP_DELETE(m_transport);
	m_transport = NULL;
	OP_DELETE(m_sync_factory);
	m_sync_factory = NULL;
	OP_DELETE(m_data_queue);
	m_data_queue = NULL;
	OP_DELETE(m_parser);
	m_parser = NULL;
	OP_DELETE(m_allocator);
	m_allocator = NULL;

	StopTimeout();
	return OpStatus::OK;
}

OP_STATUS OpSyncCoordinator::Init(OpSyncFactory* factory, BOOL use_disk_queue)
{
	OP_NEW_DBG("OpSyncCoordinator::Init()", "sync");
	if (factory)
	{	// ownership is now transferred to this class
		OP_DELETE(m_sync_factory);
		m_sync_factory = factory;
	}
	else if (!m_sync_factory)
	{
		// use the default factory
		m_sync_factory = OP_NEW(OpSyncFactory, ());
		RETURN_OOM_IF_NULL(m_sync_factory);
	}

	if (!m_parser)
		RETURN_IF_ERROR(m_sync_factory->GetParser(&m_parser, this));
	if (!m_transport)
		RETURN_IF_ERROR(m_sync_factory->CreateTransportProtocol(&m_transport, this));
	if (!m_allocator)
		RETURN_IF_ERROR(m_sync_factory->GetAllocator(&m_allocator));
	if (!m_data_queue)
		RETURN_IF_MEMORY_ERROR(m_sync_factory->GetQueueHandler(&m_data_queue, this, use_disk_queue));
	m_parser->SetDataQueue(m_data_queue);

	// Register itself as listener to sync prefs changes:
	if (!m_is_initialised)
	{
		RETURN_IF_LEAVE(g_pcsync->RegisterListenerL(this));
		m_is_initialised = TRUE;
	}
	// Set the initial supports values
	BroadcastSyncSupportChanged();
	return OpStatus::OK;
}

void OpSyncCoordinator::SetUseDiskQueue(BOOL value)
{
	m_data_queue->SetUseDiskQueue(value);
}

BOOL OpSyncCoordinator::UseDiskQueue() const
{
	return m_data_queue->UseDiskQueue();
}

void OpSyncCoordinator::OnTimeout()
{
	OP_NEW_DBG("OpSyncCoordinator::OnTimeout()", "sync");
	if (IsSyncActive())
	{
		OP_DBG(("MSG_SYNC_POLLING_TIMEOUT%s", IsSyncInProgress()?"; sync already in progress":""));
		if (!IsSyncInProgress())
			OpStatus::Ignore(SyncNow());

		OpStatus::Ignore(StartTimeout(m_sync_server_info.GetSyncIntervalLong()));
	}
}

void OpSyncCoordinator::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("OpSyncCoordinator::HandleCallback()", "sync");
	switch (msg)
	{
	case MSG_SYNC_AUTHENTICATE:
		OP_DBG(("MSG_SYNC_AUTHENTICATE"));
		SyncAuthenticate();
		break;

	case MSG_SYNC_CONNECT:
		OP_DBG(("MSG_SYNC_CONNECT"));
		SyncConnect();
		break;

	case MSG_SYNC_FLUSH_DATATYPE:
		/* this message is triggered when inconsistency is detected on the
		 * client, which prompts the module to ask the client to flush all data
		 * for the data type as dirty.
		 * par1 is the OpSyncDataItem::DataItemType that shall be flushed. */
		OP_DBG(("MSG_SYNC_FLUSH_DATATYPE; type: ") << static_cast<OpSyncDataItem::DataItemType>(par1));
		BroadcastSyncDataFlush(static_cast<OpSyncDataItem::DataItemType>(par1), FALSE, TRUE);
		break;

	default:
		OpSyncTimeout::HandleCallback(msg, par1, par2);
	}
}

OP_STATUS OpSyncCoordinator::SetSyncActive(BOOL active)
{
	OP_NEW_DBG("OpSyncCoordinator::SetSyncActive()", "sync");
	OP_DBG(("%s", active?"active":"not active"));
	m_sync_active = active;
	if (m_sync_active)
	{
		unsigned long seconds, milliseconds;
		g_op_time_info->GetWallClock(seconds, milliseconds);
		OP_DBG(("SyncLastUsed: %lu", seconds));
		TRAPD(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncLastUsed, seconds));
		RETURN_IF_ERROR(SyncNow());
		// start timers
//		RETURN_IF_ERROR(StartTimeout(m_sync_server_info.GetSyncIntervalShort()));
		RETURN_IF_ERROR(StartTimeout(m_sync_server_info.GetSyncIntervalLong()));
	}
	else
	{	// stop timers
		StopTimeout();

		/* ensure that we request a new OAuth token the next time we activate
		 * sync again: */
		if (g_opera_oauth_manager->IsLoggedIn())
			g_opera_oauth_manager->Logout();
	}

	return OpStatus::OK;
}

OP_STATUS OpSyncCoordinator::GetSyncItem(OpSyncItem** item, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data)
{
	OP_NEW_DBG("OpSyncCoordinator::GetSyncItem()", "sync");
	OP_DBG(("type: ") << type);
	OP_DBG((UNI_L("id: '%s'"), key_data));
	return m_allocator->GetSyncItem(item, type, primary_key, key_data);
}

OP_STATUS OpSyncCoordinator::SetLoginInformation(const uni_char* provider, const OpStringC& username, const OpStringC& password)
{
	OP_NEW_DBG("OpSyncCoordinator::SetLoginInformation()", "sync");
	OP_DBG((UNI_L("provider: %s; username: %s"), provider, username.CStr()));
	if (!provider)
	{
		OP_ASSERT(!"No provider specified, you shall use 'myopera'!");
		return OpStatus::ERR_NULL_POINTER;
	}
	else if (uni_strcmp(provider, UNI_L("myopera")) != 0)
	{
		OP_ASSERT(!"Unsupported provider, you shall use 'myopera'!");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	BOOL username_changed = username != m_save_username;
	if (!username_changed && password == m_save_password)
		/* Nothing to do if the credentials did not change */
		return OpStatus::OK;

	/* ensure that we request a new OAuth token if username or password are
	 * different: */
	if (g_opera_oauth_manager->IsLoggedIn())
		g_opera_oauth_manager->Logout();

	OpString new_username;
	RETURN_IF_ERROR(new_username.Set(username.CStr()));
	OpString new_password;
	RETURN_IF_ERROR(new_password.Set(password.CStr()));

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	/* Try not to fail this method after re-encrypting the encryption-key,
	 * i.e. all steps that may fail have been done above, everything below is
	 * expected to pass... */
	if (m_encryption_key_manager->HasEncryptionKey())
	{
		if (username_changed)
		{
			/* If the username changed, the SyncEncryptionKeyManager needs to
			 * clear the encryption-key to request the (new) user's
			 * encryption-key from the Link server: */
			m_encryption_key_manager->ClearEncryptionKey();
		}
		else
		{
			/* If the password changed (and the username remains the same), the
			 * SyncEncryptionKeyManager needs to re-encrypt the encryption-key
			 * with the new credentials and send the newly encrypted
			 * encryption-key to the Link server: */
			OpStatus::Ignore(m_encryption_key_manager->ReencryptEncryptionKey(m_save_username, m_save_password, new_password));
			/* Note: ignore the return code here, because failing to re-encrypt
			 * the encryption-key means that the encryption-key was cleared (in
			 * ReencryptEncryptionKey()) and we will try to receive a new copy
			 * of the encryption-key from the Link server in the next connection
			 * and if that is not possible, we will load it from wand... */
		}
	}
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

	/* Note: TakeOver() is expected to always return OpStatus::OK, so we ignore
	 * the return value: */
	OpStatus::Ignore(m_save_username.TakeOver(new_username));
	OpStatus::Ignore(m_save_password.TakeOver(new_password));
	new_password.Wipe();
	return OpStatus::OK;
}

OP_STATUS OpSyncCoordinator::GetLoginInformation(OpString& username, OpString& password)
{
	RETURN_IF_ERROR(username.Set(m_save_username));
	return password.Set(m_save_password);
}

BOOL OpSyncCoordinator::OnInputAction(OpInputAction* action)
{
#ifdef ACTION_SYNC_NOW_ENABLED
	OP_ASSERT(action);

	if (action->GetAction() == OpInputAction::ACTION_SYNC_NOW)
	{
		OP_NEW_DBG("OpSyncCoordinator::OnInputAction()", "sync");
		OP_DBG(("OpInputAction::ACTION_SYNC_NOW: sync is %s%s", IsSyncActive()?"active":"not active", IsSyncInProgress()?" and in progress":""));
		if (IsSyncActive() && !IsSyncInProgress())
			SyncNow();

		return TRUE;
	}
#endif // ACTION_SYNC_NOW_ENABLED
	return FALSE;
}

OP_STATUS OpSyncCoordinator::SyncNow()
{
	OP_NEW_DBG("OpSyncCoordinator::SyncNow()", "sync");
	if (m_save_password.IsEmpty())
		return OpStatus::ERR_NO_ACCESS;

	else if (!m_transport || !m_parser)
		return OpStatus::ERR_NULL_POINTER;

	else if (!IsSyncActive())
	{
		OP_DBG(("SYNC_ERROR_SYNC_DISABLED: sync is not active"));
		BroadcastSyncError(SYNC_ERROR_SYNC_DISABLED, UNI_L(""));
		return OpStatus::ERR_NO_ACCESS;
	}

	else if (IsSyncInProgress())
	{
		OP_DBG(("SYNC_ERROR_SYNC_IN_PROGRESS: sync is already in progress"));
		OP_ASSERT(!"the caller does a sync before the previous sync has finished");
		BroadcastSyncError(SYNC_ERROR_SYNC_IN_PROGRESS, UNI_L(""));
		return OpStatus::ERR_NO_ACCESS;
	}

	RETURN_IF_ERROR(m_sync_state.ReadPrefs());
	RETURN_IF_LEAVE(g_pcsync->GetStringPrefL(PrefsCollectionSync::ServerAddress, m_save_server));
	m_save_port = 80;

	if (!m_is_login_listener)
	{
		OP_DBG(("Register as OperaOauthManager::LoginListener"));
		RETURN_IF_ERROR(g_opera_oauth_manager->AddListener(this));
		m_is_login_listener = TRUE;
	}

	return StartSync();
}

OP_STATUS OpSyncCoordinator::StartSync()
{
	OP_NEW_DBG("OpSyncCoordinator::StartSync()", "sync");

	/* If the OAuth manager is logged in, we can continue to connect to the
	 * Link server. If the OAuth manager is not yet logged in, we need to log
	 * in first to get the OAuth token. */
	OpMessage message = g_opera_oauth_manager->IsLoggedIn() ? MSG_SYNC_CONNECT : MSG_SYNC_AUTHENTICATE;
	if (g_main_message_handler->PostMessage(message, 0, 0))
	{
		/* If the message was posted, then the sync process is started, i.e.
		 * m_sync_in_progress is now TRUE until it is reset to FALSE in
		 * OnSyncCompleted(), OnSyncError() or ErrorCleanup() when the sync
		 * process is completely handled: */
		SetSyncInProgress(TRUE);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

void OpSyncCoordinator::SyncAuthenticate()
{
	OP_NEW_DBG("OpSyncCoordinator::SyncAuthenticate()", "sync");
	OP_STATUS status = g_opera_oauth_manager->Login(m_save_username, m_save_password);
	if (OpStatus::IsError(status))
		ErrorCleanup(status);
}

// ==================== <OperaOauthManager::LoginListener>

/* virtual */
void OpSyncCoordinator::OnLoginStatusChange(OperaOauthManager::OAuthStatus auth_status, const OpStringC& server_error_message)
{
	OP_NEW_DBG("OpSyncCoordinator::OnLoginStatusChange", "sync");
	OP_DBG(("OAuth status: %d", auth_status));
	OP_STATUS status = OpStatus::OK;
	switch (auth_status) {
	case OperaOauthManager::OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED:
		OP_DBG(("OperaOauthManager::OAUTH_LOGIN_SUCCESS_NEW_TOKEN_DOWNLOADED"));
		/* The login was a success and an OAuth token was downloaded. Now we
		 * can continue to connect. */
		if (!g_main_message_handler->PostMessage(MSG_SYNC_CONNECT, 0, 0))
			status = OpStatus::ERR; // could not post the message
		break;

	case OperaOauthManager::OAUTH_LOGIN_ERROR_WRONG_USERNAME_PASSWORD:
		OP_DBG(("OperaOauthManager::OAUTH_LOGIN_ERROR_WRONG_USERNAME_PASSWORD"));
		// Wrong username and/or password
		OnSyncError(SYNC_ACCOUNT_AUTH_FAILURE, server_error_message);
		break;

	case OperaOauthManager::OAUTH_LOGIN_ERROR_TIMEOUT:
		OP_DBG(("OperaOauthManager::OAUTH_LOGIN_ERROR_TIMEOUT"));
		// Timeout while downloading the tokens.
		OnSyncError(SYNC_ERROR_COMM_TIMEOUT, server_error_message);
		break;

	case OperaOauthManager::OAUTH_LOGIN_ERROR_NETWORK:
		OP_DBG(("OperaOauthManager::OAUTH_LOGIN_ERROR_NETWORK"));
		// Generic network error, downloading token failed
		OnSyncError(SYNC_ERROR_COMM_FAIL, server_error_message);
		break;

	case OperaOauthManager::OAUTH_LOGIN_ERROR_GENERIC:
		OP_DBG(("OperaOauthManager::OAUTH_LOGIN_ERROR_GENERIC"));
		// Generic/internal error, such as memory problems
		OnSyncError(SYNC_ERROR, server_error_message);
		break;

	case OperaOauthManager::OAUTH_LOGGED_OUT:
		OP_DBG(("OperaOauthManager::OAUTH_LOGGED_OUT"));
		// Someone has called Logout()
		status = OpStatus::ERR;  // TODO: use better error-code for cleanup...
		break;
	};

	if (OpStatus::IsError(status) && IsSyncInProgress())
		ErrorCleanup(status);
}

// ==================== </OperaOauthManager::LoginListener>

void OpSyncCoordinator::SyncConnect()
{
	OP_NEW_DBG("OpSyncCoordinator::SyncConnect()", "sync");
	OP_DBG((UNI_L("server: %s; port: %d; username: %s"), m_save_server.CStr(), m_save_port, m_save_username.CStr()));

	OP_ASSERT(m_transport && m_parser && "Was Init() called successfully?");

	int sync_flags = 0;
	if (g_pcsync->GetIntegerPref(PrefsCollectionSync::CompleteSync) != 0 ||
		m_data_queue->HasDirtyItems())
		sync_flags = OpSyncParser::SYNC_PARSER_COMPLETE_SYNC;

	SyncSupportsState supports_state = m_supports_state;
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
	if (supports_state.HasSupports(SYNC_SUPPORTS_ENCRYPTION) &&
		!m_encryption_key_manager->IsEncryptionKeyUsable())
		/* If we don't have a usable encryption-key, we don't upload
		 * or download the supports types that require an encryption-key: */
		supports_state.SetSupports(SYNC_SUPPORTS_PASSWORD_MANAGER, false);
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

	OP_STATUS status = m_parser->Init(sync_flags, m_sync_state, supports_state);
	if (OpStatus::IsSuccess(status))
		status = GetOutOfSyncData(supports_state);

	if (OpStatus::IsSuccess(status))
		status = m_transport->Init(m_save_server, m_save_port, m_parser);
	if (OpStatus::IsSuccess(status))
	{
		m_data_queue->PopulateOutgoingItems(supports_state);
		status = m_transport->Connect(*m_data_queue->GetSyncDataCollection(OpSyncDataQueue::SYNCQUEUE_OUTGOING), m_sync_state);
	}

	if (OpStatus::IsError(status))
	{
		OP_DBG(("error"));
		SetSyncActive(FALSE);
		OP_DELETE(m_transport);
		m_transport = NULL;
	}
}

OP_STATUS OpSyncCoordinator::GetOutOfSyncData(SyncSupportsState supports_state)
{
	OP_NEW_DBG("OpSyncCoordinator::GetOutOfSyncData()", "sync");
	if (m_is_complete_sync_in_progress)
		return OpStatus::OK;

	for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		OpSyncSupports supports = static_cast<OpSyncSupports>(i);
		if (!supports_state.HasSupports(supports))
			continue;

		OP_DBG(("") << supports);
		/* As there is currently no grouping we need to convert from the
		 * supports type to a data item type like this.
		 * Note: it is also assumed that we can send the "main" data type to the
		 * client and we will get all the info. (i.e. send bookmarks and we get
		 * bookmarks, bookmark folders, bookmark separators.) This is bad but
		 * until we have groups how it is. */
		OpINT32Vector types;
		RETURN_IF_ERROR(OpSyncCoordinator::GetTypesFromSupports(supports, types));
		for (unsigned int j = 0; j < types.GetCount(); j++)
		{
			OpSyncDataItem::DataItemType data_type = static_cast<OpSyncDataItem::DataItemType>(types.Get(j));

			/* If sync state for this datatype is 0 or out of sync with global
			 * sync state send BroadcastSyncDataFlush notification.
			 * If Sync state for this datatype is 0, then send
			 * BroadcastSyncDataInitialize() notification. */
			if (m_sync_state.IsDefaultValue(supports))
			{
				OP_DBG(("- ") << data_type << ": sync state is default");
				BroadcastSyncDataInitialize(data_type);
				BroadcastSyncDataFlush(data_type, TRUE, FALSE);
				m_sync_state.SetSyncStatePersistent(supports, TRUE);
			}
			else if (m_sync_state.IsOutOfSync(supports))
			{
				OP_DBG(("- ") << data_type << ": sync state is out of sync");
				/* Call back to the internal coordinator listener which will
				 * send to the external listeners */
				BroadcastSyncDataFlush(data_type, FALSE, FALSE);
			}
		}
	}
	return OpStatus::OK;
}

void OpSyncCoordinator::ContinueSyncData(OpSyncSupports supports)
{
	OP_NEW_DBG("OpSyncCoordinator::ContinueSyncData", "sync");
	OP_DBG(("supports: ") << supports);
	SyncSupportsState supports_state;
	supports_state.SetSupports(supports, true);
	OpStatus::Ignore(BroadcastDataAvailable(supports_state, m_sync_state));
}

void OpSyncCoordinator::OnSyncStarted(BOOL items_sent)
{
	OP_NEW_DBG("OpSyncCoordinator::OnSyncStarted()", "sync");
	OP_DBG(("items %ssent", items_sent?"":"not yet "));
	for (unsigned int i = 0; i < m_uilisteners.GetCount(); i++)
		m_uilisteners.Get(i)->OnSyncStarted(items_sent);
}

void OpSyncCoordinator::ErrorCleanup(OP_STATUS status)
{
	OP_NEW_DBG("OpSyncCoordinator::ErrorCleanup()", "sync");
	OpString empty;
	SetSyncInProgress(FALSE);
	BroadcastSyncError(OpStatus::IsMemoryError(status) ? SYNC_ERROR_MEMORY : SYNC_ERROR, empty);
	m_parser->ClearIncomingSyncItems(TRUE);
	m_parser->ClearError();
	m_data_queue->ClearReceivedItems();
	BroadcastPendingSyncSupportChanged();
}

void OpSyncCoordinator::OnSyncCompleted(OpSyncCollection* new_items, OpSyncState& sync_state, OpSyncServerInformation& sync_server_info)
{
	OP_NEW_DBG("OpSyncCoordinator::OnSyncCompleted()", "sync");
	OP_DBG(("received %d items", new_items->GetCount()));
	OP_DBG(("server-info: ") << sync_server_info);
	SyncSupportsState supports_state = m_parser->GetSyncSupports();
	bool have_queued_items = m_data_queue->HasQueuedItems(supports_state);
	OP_DBG(("have ") << (have_queued_items ? "" : "no") << " queued items for state " << supports_state);
	if (m_data_queue->HasDirtyItems())
	{
		OP_DBG(("dirty sync"));
		new_items->Clear();

		OpSyncDataCollection items_missing_on_client;
		OpSyncDataCollection items_missing_on_server;

		// TODO: OOM handling here
		/* we have outgoing items scheduled for a dirty sync. Let's merge it
		 * with the items the server sent and send the difference to the client
		 * and the server */
		OP_STATUS status = MergeDirtySyncItems(
			*m_data_queue->GetSyncDataCollection(OpSyncDataQueue::SYNCQUEUE_DIRTY_ITEMS),	// items from the client
			*m_parser->GetIncomingSyncItems(),	// items from the server
			items_missing_on_client, items_missing_on_server);

		if (OpStatus::IsError(status))
		{
			ErrorCleanup(status);
			return;
		}
		if (items_missing_on_client.HasItems())
		{
			OP_DBG(("handle items missing on client"));
			// items that the client need to know about
			OpSyncCollection items_missing_on_client_syncitems;

			// we need to convert them to a top-level OpSyncItem collection
			status = m_allocator->CreateSyncItemCollection(items_missing_on_client, items_missing_on_client_syncitems);
			if (OpStatus::IsError(status))
			{
				ErrorCleanup(status);
				return;
			}
			/* add them to the received collection and the client will get them
			 * as normal */
			status = m_data_queue->AddAsReceivedItems(items_missing_on_client_syncitems);
			if (OpStatus::IsError(status))
			{
				ErrorCleanup(status);
				return;
			}
		}
		if (items_missing_on_server.HasItems())
		{
			OP_DBG(("handle items missing on server"));
			// items the server should know about
			status = m_data_queue->Add(items_missing_on_server);
			if (OpStatus::IsError(status))
			{
				ErrorCleanup(status);
				return;
			}
		}
	}
	else
	{
		// TODO: OOM handling here
		OP_STATUS status = m_data_queue->AddAsReceivedItems(*new_items);
		if (OpStatus::IsError(status))
		{
			ErrorCleanup(status);
			return;
		}
	}
	/* The incoming items have now been moved from the parser's
	 * OpSyncDataCollection of parsed items to the OpSyncDataQueue's
	 * SYNCQUEUE_RECEIVED_ITEMS OpSyncCollection, so clear the parser's
	 * collection. */
	m_parser->ClearIncomingSyncItems(TRUE);

#ifdef SELFTEST
	if (!m_is_selftest)
#endif // SELFTEST
	{
		/* if the interval change, we need to reset the timer for the new
		 * interval right away, otherwise we'll just let the old default one
		 * continue ticking */
		unsigned int new_interval = 0;
		if (m_data_queue->HasQueuedItems())
		{
			/* If we have queued items we need to restart the timer if the
			 * server's short interval has changed: */
			if (sync_server_info.GetSyncIntervalShort() != m_sync_server_info.GetSyncIntervalShort())
				new_interval = sync_server_info.GetSyncIntervalShort();
		}
		else
		{	/* If we don't have queued items we need to restart the timer if the
			 * server's long interval has changed. */
			if (sync_server_info.GetSyncIntervalLong() != m_sync_server_info.GetSyncIntervalLong())
				new_interval = sync_server_info.GetSyncIntervalLong();
		}

		if (new_interval)
		{
			OP_DBG(("Sync interval changed: %d", new_interval));
			StartTimeout(new_interval);
		}
	}
	m_sync_server_info = sync_server_info;
	m_data_queue->SetMaxItems(m_sync_server_info.GetSyncMaxItems());

	/* We have now sent the items of SYNCQUEUE_OUTGOING, so we can clear that
	 * queue (and update the persistent queue on disk): */
	OP_STATUS status = m_data_queue->ClearItemsToSend();
	if (OpStatus::IsError(status))
	{
		ErrorCleanup(status);
		return;
	}
	SetSyncInProgress(FALSE);

	/* If we just have sent all queued items for all supports types that are
	 * currently enabled, we call BroadcastDataAvailable() to the corresponding
	 * OpSyncDataClient instances.
	 * Note: if this was a dirty sync, and MergeDirtySyncItems() has found some
	 * items missing on the server, then the outgoing queue is no longer empty
	 * by now, but we still want to broadcast the available data now and not
	 * after the next sync... */
	if (!have_queued_items)
	{
		OP_DBG(("broadcast sync-result"));
		OP_STATUS status = BroadcastDataAvailable(supports_state, sync_state);
		if (OpStatus::IsError(status))
		{
			ErrorCleanup(status);
			return;
		}
		else
		{
			// only save sync state and call OnSyncFinished() on success
			for (unsigned int i = 0; i < m_uilisteners.GetCount(); i++)
				m_uilisteners.Get(i)->OnSyncFinished(sync_state);
			m_sync_state.Update(sync_state);
			SetUseDiskQueue(TRUE);
		}
		m_is_complete_sync_in_progress = FALSE;
	}
	/* Note: broadcast any pending sync-support-changed notification after
	 * calling OnSyncFinished(). */
	BroadcastPendingSyncSupportChanged();

	/* If we have any more queued items for the already used supports type (this
	 * may be the case if we had more than max-items), we need to send another
	 * batch of items. */
	if (have_queued_items
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
		/* Also if we just received the encryption-key, we need to start another
		 * connection immediately (to possibly receive any new password manager
		 * entries). */
		|| (m_encryption_key_manager->IsEncryptionKeyUsable() &&
			m_supports_state.HasSupports(SYNC_SUPPORTS_PASSWORD_MANAGER) &&
			!supports_state.HasSupports(SYNC_SUPPORTS_PASSWORD_MANAGER))
#endif // SYNC_ENCRYPTION_KEY_SUPPORT
		)
	{
		OP_DBG(("start new sync-connection"));
		m_sync_state.Update(sync_state);
		OP_STATUS status = StartSync();
		if (OpStatus::IsError(status))
		{
			ErrorCleanup(status);
			return;
		}
	}

#ifdef SELFTEST
	if (!m_is_selftest)
#endif // SELFTEST
	{
		// TODO: Check if this should be here
		OP_DBG(("%s", m_sync_state.GetDirty()?"Dirty sync: start a complete sync next time":"no dirty sync"));
		TRAPD(err, g_pcsync->WriteIntegerL(PrefsCollectionSync::CompleteSync, m_sync_state.GetDirty()));
		m_sync_state.WritePrefs();
	}
	if (m_sync_state.GetDirty())
	{
		m_is_complete_sync_in_progress = TRUE;
		for (unsigned int i = 0; i < m_listeners.GetCount(); i++)
		{
			const OpSyncDataClientItem* listener_item = m_listeners.Get(i);
			for (unsigned int j = 0; j < listener_item->TypeCount(); j++)
			{
				// request a flush for this data type
				g_main_message_handler->PostDelayedMessage(MSG_SYNC_FLUSH_DATATYPE, static_cast<MH_PARAM_1>(listener_item->GetType(j)), 0, 50);
			}
		}
	}
	unsigned long seconds, milliseconds;
	g_op_time_info->GetWallClock(seconds, milliseconds);
	OP_DBG(("SyncLastUsed: %lu", seconds));
	TRAP(status, g_pcsync->WriteIntegerL(PrefsCollectionSync::SyncLastUsed, seconds));
}

void OpSyncCoordinator::BroadcastSyncError(OpSyncError error, const OpStringC& error_msg)
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastSyncError()", "sync");
	for (unsigned int i = 0; i < m_uilisteners.GetCount(); i++)
		m_uilisteners.Get(i)->OnSyncError(error, error_msg);
}

OP_STATUS OpSyncCoordinator::BroadcastDataAvailable(SyncSupportsState supports_state, OpSyncState& sync_state)
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastDataAvailable()", "sync");
	OpSyncCollection* received_items = m_data_queue->GetSyncCollection(OpSyncDataQueue::SYNCQUEUE_RECEIVED_ITEMS);
	if (received_items->IsEmpty())
		return OpStatus::OK;

	OP_DBG(("%d new items", received_items->GetCount()));
	/* Notify all OpSyncDataClient instances for the supports types that are
	 * enabled in supports_state: */
	bool broadcast_for[OpSyncDataItem::DATAITEM_MAX_DATAITEM];
	/* An OpSyncDataClient can indicate to not release the OpSyncCollection of
	 * that type by letting SyncDataAvailable() return SYNC_DATAERROR_ASYNC,
	 * which will set this flag to true: */
	bool keep_items[OpSyncDataItem::DATAITEM_MAX_DATAITEM];
	/* OpSyncDataItem::DATAITEM_GENERIC == 0 is not used, so no need to do
	 * anything with that type: */
	broadcast_for[0] = false;
	keep_items[0] = false;
	for (unsigned int i = 1; i < OpSyncDataItem::DATAITEM_MAX_DATAITEM; i++)
	{
		OpSyncDataItem::DataItemType item_type = static_cast<OpSyncDataItem::DataItemType>(i);
		OpSyncSupports supports = OpSyncCoordinator::GetSupportsFromType(item_type);
		broadcast_for[i] = supports_state.HasSupports(supports);
		// Keep all items for which we don't notify the client:
		keep_items[i] = !broadcast_for[i];
	}

	unsigned int i = 0;
	/* received_items is split into collections containing only the items of one
	 * type */
	OpSyncCollection temp_items[OpSyncDataItem::DATAITEM_MAX_DATAITEM];
	for (OpSyncItemIterator current(received_items->First()); *current; ++current)
	{
		OpSyncDataItem::DataItemType item_type = OpSyncDataItem::BaseTypeOf(current->GetType());
		OP_DBG(("%d: ", i++) << item_type);

		OP_ASSERT(item_type < OpSyncDataItem::DATAITEM_MAX_DATAITEM);
		if (item_type < OpSyncDataItem::DATAITEM_MAX_DATAITEM)
		{
			current->Out();
			current->Into(&temp_items[item_type]);
		}
	}

	OP_STATUS status = OpStatus::OK;
	/* Call the registered listeners for each registered data type that has data
	 * available: */
	for (i = 0; i < m_listeners.GetCount(); i++)
	{
		OpSyncDataClientItem* item = m_listeners.Get(i);
		OP_DBG(("%d: notify listener %p", i, item));

		// Notify the listener for each data type it was registered for.
		// (Note: type 0 is not used)
		for (unsigned int item_type = 1; item_type < (int)OpSyncDataItem::DATAITEM_MAX_DATAITEM; item_type++)
		{
			OpSyncDataError data_error = SYNC_DATAERROR_NONE;
			/* Only call SyncDataAvailable() if ... */
			if (/* ... notification was requested for this type ... */
				broadcast_for[item_type] &&
				/* ... there are some items of this type ... */
				temp_items[item_type].HasItems() &&
				/* ... and the client supports this type ... */
				item->HasType(static_cast<OpSyncDataItem::DataItemType>(item_type)))
			{
				OP_DBG(("type: ") << static_cast<OpSyncDataItem::DataItemType>(item_type));
				status = item->Get()->SyncDataAvailable(&temp_items[item_type], data_error);
				if (OpStatus::IsError(status))
				{	/* In case of an error don't continue to notify the clients,
					 * but ensure to move back the items into received_items */
					i = m_listeners.GetCount(); // exit outer loop ...
					OP_DBG(("error: %d", status));
					break; // ... after exiting inner loop
				}

				switch (data_error) {
				case SYNC_DATAERROR_INCONSISTENCY:
					OP_DBG(("SYNC_DATAERROR_INCONSISTENCY"));
					// request a flush for this data type
					g_main_message_handler->PostDelayedMessage(MSG_SYNC_FLUSH_DATATYPE, static_cast<MH_PARAM_1>(item_type), 0, 50);
					break;

				case SYNC_DATAERROR_ASYNC:
					OP_DBG(("SYNC_DATAERROR_ASYNC"));
					/* Don't delete the items of this type: */
					keep_items[item_type] = true;
					sync_state.SetSyncStatePersistent(OpSyncCoordinator::GetSupportsFromType(static_cast<OpSyncDataItem::DataItemType>(item_type)), FALSE);
					break;

				case SYNC_DATAERROR_NONE:
					sync_state.SetSyncStatePersistent(OpSyncCoordinator::GetSupportsFromType(static_cast<OpSyncDataItem::DataItemType>(item_type)), TRUE);
					break;
				}
			}
		}
	}

	/* Move all the items that are completely handled back into the original
	 * list so they can be released: */
	for (i = 0; i < (unsigned int)OpSyncDataItem::DATAITEM_MAX_DATAITEM; i++)
	{
		if (temp_items[i].HasItems() && !keep_items[i])
			received_items->AppendItems(&temp_items[i]);
	}

	/* Now release all items unless SYNC_DATAERROR_ASYNC was returned for that
	 * item type: */
	m_data_queue->ClearReceivedItems();

	/* Now move all items back that still need to be handled later: */
	for (i = 0; i < (unsigned int)OpSyncDataItem::DATAITEM_MAX_DATAITEM; i++)
	{
		if (temp_items[i].HasItems() && keep_items[i])
			received_items->AppendItems(&temp_items[i]);
	}
	return status;
}

void OpSyncCoordinator::OnSyncError(OpSyncError error, const OpStringC& error_msg)
{
	OP_NEW_DBG("OpSyncCoordinator::OnSyncError()", "sync");
	OP_DBG((UNI_L("error: %s"), error_msg.CStr()));
	m_parser->ClearIncomingSyncItems(TRUE);
	m_parser->ClearError();
	SetSyncInProgress(FALSE);
	BroadcastPendingSyncSupportChanged();
	bool broadcast_error = true;

	switch (error)
	{
	case SYNC_ACCOUNT_OAUTH_EXPIRED:
		/* The Oauth token has expired and we need to get a new Oauth token and
		 * connect again with that token. */
		g_opera_oauth_manager->Logout();
		StartSync();
		broadcast_error = false;
		break;

	case SYNC_ACCOUNT_USER_UNAVAILABLE:
		/* For some reason the Link server cannot verify the user at the
		 * moment. This is a temporary error state. Try again to connect in the
		 * specified interval. */
		OpStatus::Ignore(StartTimeout(m_sync_server_info.GetSyncIntervalLong()));
		break;

	case SYNC_ACCOUNT_AUTH_FAILURE:
	case SYNC_ACCOUNT_USER_BANNED:
		/* The Link server failed to authenticate the user. So log out,
		 * broadcast the error and don't try again. */
		g_opera_oauth_manager->Logout();
		break;

	case SYNC_ERROR_INVALID_REQUEST:
	case SYNC_ERROR_PARSER:
	case SYNC_ERROR_PROTOCOL_VERSION:
	case SYNC_ERROR_CLIENT_VERSION:
	case SYNC_ERROR_INVALID_STATUS:
	case SYNC_ERROR_INVALID_BOOKMARK:
	case SYNC_ERROR_INVALID_SPEEDDIAL:
	case SYNC_ERROR_INVALID_NOTE:
	case SYNC_ERROR_INVALID_SEARCH:
	case SYNC_ERROR_INVALID_TYPED_HISTORY:
	case SYNC_ERROR_INVALID_FEED:
	case SYNC_ERROR_SERVER:
	case SYNC_ERROR:
	case SYNC_PENDING_ENCRYPTION_KEY:
		/* Broadcast all these errors. */
	default:
		break;
	}

	if (broadcast_error)
		BroadcastSyncError(error, error_msg);
}

void OpSyncCoordinator::OnSyncItemAdded(BOOL new_item)
{
	// new item, start short interval sync
	if (new_item && IsSyncActive())
	{
		OP_NEW_DBG("OpSyncCoordinator::OnSyncItemAdded()", "sync");
		RETURN_VOID_IF_ERROR(StartTimeout(m_sync_server_info.GetSyncIntervalShort()));
	}
}

void OpSyncCoordinator::BroadcastSyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastSyncDataInitialize()", "sync");
	OP_DBG(("") << "type: " << type << "; " << m_listeners.GetCount() << " listeners");
	/* Notify the registered listeners for the specified data type that the
	 * corresponding supports type is synchronised for the first time: */
	for (unsigned int i = 0; i < m_listeners.GetCount(); i++)
	{
		OpSyncDataClientItem* item = m_listeners.Get(i);
		if (item->HasType(type) && GetSupports(OpSyncCoordinator::GetSupportsFromType(type)))
		{
			OP_DBG(("%d: has this type", i));
			OP_STATUS status = item->Get()->SyncDataInitialize(type);
			if (OpStatus::IsError(status))
			{
				// break out if anything fails
				OP_DBG(("error on initializing data"));
				ErrorCleanup(status);
				return;
			}
		}
	}
}

void OpSyncCoordinator::BroadcastSyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastSyncDataFlush()", "sync");
	OP_DBG(("") << "type: " << type << "; " << (first_sync?"first sync; ":"") << (is_dirty?"dirty; ":"") << m_listeners.GetCount() << " listeners");
	m_is_complete_sync_in_progress = TRUE;

	if (is_dirty)
	{
		OP_DBG(("remove queued items"));
		m_data_queue->RemoveQueuedItems(type);
	}

	/* Notify the registered listeners for each registered data type that it
	 * shall provide the data for an initial or dirty sync: */
	for (unsigned int i = 0; i < m_listeners.GetCount(); i++)
	{
		OpSyncDataClientItem* item = m_listeners.Get(i);

		if (item->HasType(type) && GetSupports(OpSyncCoordinator::GetSupportsFromType(type)))
		{
			OP_DBG(("%d: has this type", i));
			OP_STATUS status = item->Get()->SyncDataFlush(type, first_sync, is_dirty);
			if (OpStatus::IsError(status))
			{
				// break out if anything fails
				ErrorCleanup(status);
				return;
			}
		}
	}
}

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT
/* virtual */
void OpSyncCoordinator::OnEncryptionKeyCreated()
{
	OP_NEW_DBG("OpSyncCoordinator::OnEncryptionKeyCreated()", "sync");
	/* Reset the sync-state for all item types that use the encryption-key.
	 * Thus in the next connection we download all password manager items which
	 * shall result in re-encrypting all items with the newly generated key: */
	OpStatus::Ignore(ResetSupportsState(SYNC_SUPPORTS_PASSWORD_MANAGER));
}

/* virtual */
void OpSyncCoordinator::OnSyncReencryptEncryptionKeyFailed(OpSyncUIListener::ReencryptEncryptionKeyContext* context)
{
	OP_NEW_DBG("OpSyncCoordinator::OnSyncReencryptEncryptionKeyFailed()", "sync");
	for (unsigned int i = 0; i < m_uilisteners.GetCount(); i++)
		m_uilisteners.Get(i)->OnSyncReencryptEncryptionKeyFailed(context);
}

/* virtual */
void OpSyncCoordinator::OnSyncReencryptEncryptionKeyCancel(OpSyncUIListener::ReencryptEncryptionKeyContext* context)
{
	OP_NEW_DBG("OpSyncCoordinator::OnSyncReencryptEncryptionKeyCancel()", "sync");
	for (unsigned int i = 0; i < m_uilisteners.GetCount(); i++)
		m_uilisteners.Get(i)->OnSyncReencryptEncryptionKeyCancel(context);
}
#endif // SYNC_ENCRYPTION_KEY_SUPPORT

BOOL OpSyncCoordinator::FreeCachedData(BOOL toplevel_context)
{
	OP_NEW_DBG("OpSyncCoordinator::FreeCachedData()", "sync");
	if (m_parser)
		return m_parser->FreeCachedData(toplevel_context);
	return FALSE;
}

/* static */
OP_STATUS OpSyncCoordinator::GetTypesFromSupports(OpSyncSupports supports, OpINT32Vector& types)
{
	OpSyncDataItem::DataItemType type;
	switch (supports)
	{
	case SYNC_SUPPORTS_BOOKMARK:
		type =  OpSyncDataItem::DATAITEM_BOOKMARK;
		break;
	case SYNC_SUPPORTS_CONTACT:
		type =  OpSyncDataItem::DATAITEM_CONTACT;
		break;
	case SYNC_SUPPORTS_ENCRYPTION:
		RETURN_IF_ERROR(types.Add(OpSyncDataItem::DATAITEM_ENCRYPTION_KEY));
		return types.Add(OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE);
	case SYNC_SUPPORTS_EXTENSION:
		type = OpSyncDataItem::DATAITEM_EXTENSION;
		break;
	case SYNC_SUPPORTS_FEED:
		type = OpSyncDataItem::DATAITEM_FEED;
		break;
	case SYNC_SUPPORTS_NOTE:
		type =  OpSyncDataItem::DATAITEM_NOTE;
		break;
	case SYNC_SUPPORTS_PASSWORD_MANAGER:
		RETURN_IF_ERROR(types.Add(OpSyncDataItem::DATAITEM_PM_HTTP_AUTH));
		return types.Add(OpSyncDataItem::DATAITEM_PM_FORM_AUTH);
	case SYNC_SUPPORTS_SEARCHES:
		type = OpSyncDataItem::DATAITEM_SEARCH;
		break;
	case SYNC_SUPPORTS_SPEEDDIAL:
		type =  OpSyncDataItem::DATAITEM_SPEEDDIAL;
		break;
	case SYNC_SUPPORTS_SPEEDDIAL_2:
		RETURN_IF_ERROR(types.Add(OpSyncDataItem::DATAITEM_SPEEDDIAL_2));
		return types.Add(OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS);
		break;
	case SYNC_SUPPORTS_TYPED_HISTORY:
		type = OpSyncDataItem::DATAITEM_TYPED_HISTORY;
		break;
	case SYNC_SUPPORTS_URLFILTER:
		type = OpSyncDataItem::DATAITEM_URLFILTER;
		break;

	default:
		OP_ASSERT(!"This is an unsupported OpSyncSupports type");
		type = OpSyncDataItem::DATAITEM_GENERIC;
	}

	return types.Add(type);
}

/* static */
OpSyncSupports OpSyncCoordinator::GetSupportsFromType(OpSyncDataItem::DataItemType type)
{
	switch (type)
	{
	case OpSyncDataItem::DATAITEM_BOOKMARK_FOLDER:
	case OpSyncDataItem::DATAITEM_BOOKMARK_SEPARATOR:
	case OpSyncDataItem::DATAITEM_BOOKMARK:		return SYNC_SUPPORTS_BOOKMARK;
	case OpSyncDataItem::DATAITEM_CONTACT:		return SYNC_SUPPORTS_CONTACT;
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:return SYNC_SUPPORTS_ENCRYPTION;
	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:return SYNC_SUPPORTS_ENCRYPTION;
	case OpSyncDataItem::DATAITEM_EXTENSION:	return SYNC_SUPPORTS_EXTENSION;
	case OpSyncDataItem::DATAITEM_FEED:			return SYNC_SUPPORTS_FEED;
	case OpSyncDataItem::DATAITEM_NOTE_FOLDER:
	case OpSyncDataItem::DATAITEM_NOTE_SEPARATOR:
	case OpSyncDataItem::DATAITEM_NOTE:			return SYNC_SUPPORTS_NOTE;
	case OpSyncDataItem::DATAITEM_PM_FORM_AUTH:	return SYNC_SUPPORTS_PASSWORD_MANAGER;
	case OpSyncDataItem::DATAITEM_PM_HTTP_AUTH:	return SYNC_SUPPORTS_PASSWORD_MANAGER;
	case OpSyncDataItem::DATAITEM_SEARCH:		return SYNC_SUPPORTS_SEARCHES;
	case OpSyncDataItem::DATAITEM_SPEEDDIAL:	return SYNC_SUPPORTS_SPEEDDIAL;
	case OpSyncDataItem::DATAITEM_BLACKLIST:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2:
	case OpSyncDataItem::DATAITEM_SPEEDDIAL_2_SETTINGS: return SYNC_SUPPORTS_SPEEDDIAL_2;
	case OpSyncDataItem::DATAITEM_TYPED_HISTORY:return SYNC_SUPPORTS_TYPED_HISTORY;
	case OpSyncDataItem::DATAITEM_URLFILTER:	return SYNC_SUPPORTS_URLFILTER;
	default:
		OP_ASSERT(!"This is an unsupported OpSyncDataItem::DataItemType");
	}
	return SYNC_SUPPORTS_MAX;
}

OP_STATUS OpSyncCoordinator::SetSupports(OpSyncSupports supports, BOOL has_support)
{
	OP_NEW_DBG("OpSyncCoordinator::SetSupports()", "sync");
	OP_DBG(("") << supports << ": " << (has_support?"on":"off"));
	switch (supports) {
	case SYNC_SUPPORTS_BOOKMARK:
	case SYNC_SUPPORTS_EXTENSION:
	case SYNC_SUPPORTS_NOTE:
	case SYNC_SUPPORTS_PASSWORD_MANAGER:
	case SYNC_SUPPORTS_SEARCHES:
	case SYNC_SUPPORTS_SPEEDDIAL:
	case SYNC_SUPPORTS_SPEEDDIAL_2:
	case SYNC_SUPPORTS_TYPED_HISTORY:
	case SYNC_SUPPORTS_URLFILTER:
	{
		PrefsCollectionSync::integerpref pref = OpSyncState::Supports2EnablePref(supports);
		if (pref != PrefsCollectionSync::DummyLastIntegerPref)
			RETURN_IF_LEAVE(g_pcsync->WriteIntegerL(pref, has_support));
		else
			OP_DBG(("support for ") << supports << " not enabled");
		break;
	}

	case SYNC_SUPPORTS_CONTACT:
	case SYNC_SUPPORTS_FEED:
		// Types not supported yet
		return OpStatus::ERR_NOT_SUPPORTED;

	case SYNC_SUPPORTS_ENCRYPTION:
		/* encryption support depends on any support that uses it, enabling
		 * encryption support alone does not do anything, especially it does
		 * not write a preference... */
		return OpStatus::OK;

	default:
		OP_ASSERT(!"This is an unsupported OpSyncSupports type");
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	return OpStatus::OK;
}

BOOL OpSyncCoordinator::GetSupports(OpSyncSupports supports)
{
	switch (supports)
	{
	case SYNC_SUPPORTS_BOOKMARK:
	case SYNC_SUPPORTS_EXTENSION:
	case SYNC_SUPPORTS_NOTE:
	case SYNC_SUPPORTS_PASSWORD_MANAGER:
	case SYNC_SUPPORTS_SEARCHES:
	case SYNC_SUPPORTS_SPEEDDIAL:
	case SYNC_SUPPORTS_SPEEDDIAL_2:
	case SYNC_SUPPORTS_TYPED_HISTORY:
	case SYNC_SUPPORTS_URLFILTER:
	{
		PrefsCollectionSync::integerpref pref = OpSyncState::Supports2EnablePref(supports);
		if (pref != PrefsCollectionSync::DummyLastIntegerPref)
			return g_pcsync->GetIntegerPref(pref);
		break;
	}

	case SYNC_SUPPORTS_ENCRYPTION:
		/* password manager requires encryption, so encryption is enabled if
		 * password manager is: */
		return GetSupports(SYNC_SUPPORTS_PASSWORD_MANAGER);

	case SYNC_SUPPORTS_CONTACT:
	case SYNC_SUPPORTS_FEED:
		// Implementation missing
		return FALSE;

	default:
		OP_ASSERT(!"This is an unsupported OpSyncSupports type");
	}
	return FALSE;
}

OP_STATUS OpSyncCoordinator::ResetSupportsState(OpSyncSupports supports, BOOL disable_support)
{
	OP_NEW_DBG("OpSyncCoordinator::ResetSupportsState", "sync");
	OP_DBG(("") << supports << (disable_support?"; disable this type":""));
	OP_ASSERT(supports <= SYNC_SUPPORTS_MAX);
	if (SYNC_SUPPORTS_MAX == supports)
	{
		RETURN_IF_ERROR(m_sync_state.SetSyncState(UNI_L("0")));
		for (unsigned int i = 0; i < SYNC_SUPPORTS_MAX; i++)
		{
			OpSyncSupports supports = static_cast<OpSyncSupports>(i);
			RETURN_IF_ERROR(m_sync_state.ResetSyncState(supports));
			if (disable_support)
				OpStatus::Ignore(SetSupports(supports, FALSE));
		}
	}
	else
	{
		RETURN_IF_ERROR(m_sync_state.ResetSyncState(supports));
		if (disable_support)
			OpStatus::Ignore(SetSupports(supports, FALSE));
	}
	return m_sync_state.WritePrefs();
}

OP_STATUS OpSyncCoordinator::ClearSupports()
{
	m_supports_state.Clear();
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS OpSyncCoordinator::SetSystemInformation(OpSyncSystemInformationType type, const OpStringC& data)
{
	if (m_parser)
		return m_parser->SetSystemInformation(type, data);
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

// TODO: make this in the parser
extern const uni_char* GetNamedKeyFromKey(OpSyncItem::Key key);

OP_STATUS OpSyncCoordinator::UpdateItem(OpSyncDataItem::DataItemType item_type, const uni_char* next_item_key_id, OpSyncItem::Key next_item_key_to_update, const uni_char* next_item_key_data_to_update)
{
	OP_NEW_DBG("OpSyncCoordinator::UpdateItem", "sync");
	OP_DBG(("type: ") << item_type);
	OP_DBG((UNI_L("next item key id: '%s'; next item key to update: '%s'; next item key data to update: '%s'"), next_item_key_id, GetNamedKeyFromKey(next_item_key_to_update), next_item_key_data_to_update));

	OpSyncItem* sync_item_ptr = 0;
	OpString data;
	OP_STATUS s = GetExistingSyncItem(&sync_item_ptr, item_type, m_parser->GetPrimaryKey(item_type), next_item_key_id);
	if (OpStatus::IsError(s) || !sync_item_ptr)
	{
		// not finding the item is not an error
		return OpStatus::OK;
	}
	// Delete the OpSyncItem instance on returning from this method:
	OpAutoPtr<OpSyncItem> sync_item(sync_item_ptr);

	if (sync_item->GetStatus() != OpSyncDataItem::DATAITEM_ACTION_DELETED &&
		sync_item->GetStatus() != OpSyncDataItem::DATAITEM_ACTION_ADDED)
		sync_item->SetStatus(OpSyncDataItem::DATAITEM_ACTION_MODIFIED);
	OP_DBG((UNI_L("UpdateItem found item, updating data: next item key to update: '%s'; next item key data to update: '%s'"), GetNamedKeyFromKey(next_item_key_to_update), next_item_key_data_to_update));

	RETURN_IF_ERROR(sync_item->SetData(next_item_key_to_update, next_item_key_data_to_update));
	RETURN_IF_ERROR(sync_item->CommitItem(FALSE, TRUE));

	return OpStatus::OK;
}

OP_STATUS OpSyncCoordinator::GetExistingSyncItem(OpSyncItem** item_out, OpSyncDataItem::DataItemType type, OpSyncItem::Key primary_key, const uni_char* key_data)
{
	const uni_char* primary_key_name = GetNamedKeyFromKey(primary_key);
	OpSyncDataCollection* collection = m_data_queue->GetSyncDataCollection(OpSyncDataQueue::SYNCQUEUE_ACTIVE);
	OpSyncDataItem* item = collection->FindPrimaryKey(OpSyncDataItem::BaseTypeOf(type), primary_key_name, key_data);
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	OpSyncItem* sync_item;
	RETURN_IF_ERROR(GetSyncItem(&sync_item, type, primary_key, key_data));
	sync_item->SetDataSyncItem(item);
	*item_out = sync_item;
	return OpStatus::OK;
}

/*
 * Dirty sync merge code
 *
 * Dirty sync involves the following steps:
 * - Receive all client items from the client code and store these for later
 * - Request all items from the server
 * - Find the difference in the 2 collections
 * - Send items missing on the client to the client
 * - Send items missing on the server to the server
 */
OP_STATUS OpSyncCoordinator::MergeDirtySyncItems(/* in */ OpSyncDataCollection& items_from_client, /* in */ OpSyncDataCollection& items_from_server, /* out */ OpSyncDataCollection& items_missing_on_client, /* out */ OpSyncDataCollection& items_missing_on_server)
{
	OP_NEW_DBG("OpSyncCoordinator::MergeDirtySyncItems", "sync");

	/* Iterate over all items received from the server (items_from_server).
	 * If the items_from_client don't have an item with the same id, the item
	 * needs to be added to the client, i.e. added to items_missing_on_client.
	 * If items_from_client has an item with the same id, the item is merged and
	 * - added to items_missing_on_client if the item on the client had
	 *   different values or less children/attributes than the item from the
	 *   server.
	 * - added to items_missing_on_server if the item on the client had some
	 *   more children/attributes than the item from the server.
	 * - deleted if both items are equal.
	 * All items in items_from_client that are left, need to be added to
	 * items_missing_on_server.
	 */
	OP_DBG(("items from server"));
	for (OpSyncDataItemIterator server_item(items_from_server.First()); *server_item; ++server_item)
	{
		if (server_item->m_key.IsEmpty())
		{
			/* Ignore items without a primary key. */
			items_from_server.RemoveItem(*server_item);
		}
		else
		{
			OP_DBG(("") << "item: " << (*server_item) << ": " << server_item->m_key.CStr() << "=" << server_item->m_data.CStr());
			OpSyncDataItem* client_item = items_from_client.FindPrimaryKey(server_item->GetBaseType(), server_item->m_key, server_item->m_data);
			if (client_item)
			{
				OP_DBG(("found duplicate in list from client %p", client_item));

				switch (server_item->GetStatus())
				{
				case OpSyncDataItem::DATAITEM_ACTION_DELETED:
					/* the item was deleted on the server, so forward this to
					 * the client (unless the client reports it as deleted as
					 * well): */
					if (client_item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
						items_from_server.RemoveItem(*server_item);
					else
						items_missing_on_client.AddItem(*server_item);
					items_from_client.RemoveItem(client_item);
					break;

				case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
				case OpSyncDataItem::DATAITEM_ACTION_ADDED:
					if (client_item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
					{
						/* client_item was deleted, while the server added or
						 * modified it, so keep the item from the server: */
						items_from_client.RemoveItem(client_item);
						items_missing_on_client.AddItem(*server_item);
					}
					else
					{
						OpSyncDataCollection auto_delete;
						/* both server_item and client_item have some data (with
						 * status added), now merge the items such that the
						 * server_item overwrites data from the client_item and
						 * if the client_item was changed, add it to
						 * items_missing_on_client and if the client_item has
						 * data that is not on the server_item, add a copy to
						 * items_missing_on_server: */
						OpSyncDataItem* merged_item = OP_NEW(OpSyncDataItem, ());
						RETURN_OOM_IF_NULL(merged_item);
						auto_delete.AddItem(merged_item);
						merged_item->SetType(server_item->GetType());
						merged_item->m_key.TakeOver(server_item->m_key);
						merged_item->m_data.TakeOver(server_item->m_data);
						merged_item->SetStatus(OpSyncDataItem::DATAITEM_ACTION_MODIFIED);
						bool update_server = false;
						bool update_client = false;

						/* Merge attributes from server_item: */
						OpSyncDataItem* from_server;
						while (0 != (from_server = server_item->GetFirstAttribute()))
						{
							// add the attribute to the merged item:
							merged_item->AddAttribute(from_server);
							// check if the client_item has the same value:
							OpSyncDataItem* from_client = client_item->FindAttributeById(from_server->m_key.CStr());
							if (from_client)
							{
								if (from_server->m_data != from_client->m_data)
									/* value is different, so we need to update
									 * the client */
									update_client = true;
								from_client->GetList()->RemoveItem(from_client);
							}
							else
								/* attribute is missing on the client, so we
								 * need to update the client: */
								update_client = true;
						}
						/* If there is any attribute left on the client_item,
						 * add it to merged_item and then we need to update the
						 * server: */
						if (client_item->HasAttributes())
						{
							update_server = true;
							while (client_item->GetFirstAttribute())
								merged_item->AddAttribute(client_item->GetFirstAttribute());
						}

						/* Merge children from server_item: */
						while (0 != (from_server = server_item->GetFirstChild()))
						{
							// add the child to the merged item:
							merged_item->AddChild(from_server);
							// check if the client_item has the same value:
							OpSyncDataItem* from_client = client_item->FindChildById(from_server->m_key.CStr());
							if (from_client)
							{
								if (from_server->m_data != from_client->m_data)
									/* value is different, so we need to update
									 * the client */
									update_client = true;
								from_client->GetList()->RemoveItem(from_client);
							}
							else
								/* child is missing on the client, so we need to
								 * update the client: */
								update_client = true;
						}
						/* If there is any child left on the client_item, add it
						 * to merged_item and then we need to update the
						 * server: */
						if (client_item->HasChildren())
						{
							update_server = true;
							while (client_item->GetFirstChild())
								merged_item->AddChild(client_item->GetFirstChild());
						}

						if (update_client && update_server)
							/* If we need to update both, then we need one more
							 * copy of the merged_item: */
							items_missing_on_client.AddItem(merged_item->Copy());
						else if (update_client)
							items_missing_on_client.AddItem(merged_item);
						if (update_server)
							items_missing_on_server.AddItem(merged_item);
					}
					items_from_server.RemoveItem(*server_item);
					items_from_client.RemoveItem(client_item);
					break;

				default:
					/* item had an invalid status, so ignore it and send the
					 * dupe_item to the server instead: */
					items_from_server.RemoveItem(*server_item);
					items_missing_on_server.AddItem(client_item);
				}

			}
			else
			{
				OP_DBG(("item was on the server but not on the client, add it to items_missing_on_client"));
				items_missing_on_client.AddItem(*server_item);
			}
		}
	}

	// all remaining items in items_from_client are missing on the server:
	OP_DBG(("items from client"));
	while (items_from_client.First())
		items_missing_on_server.AddItem(items_from_client.First());
	return OpStatus::OK;
}


// <Implementation of OpPrefsListener>

/* virtual */
void OpSyncCoordinator::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	OP_NEW_DBG("OpSyncCoordinator::PrefChanged()", "sync");
	OP_ASSERT(OpPrefsCollection::Sync == id);
	OP_DBG(("pref: %d; value: %d", pref, newvalue));
	switch (pref) {
	case PrefsCollectionSync::LastCachedAccess:
	case PrefsCollectionSync::LastCachedAccessNum:
	case PrefsCollectionSync::SyncLastUsed:
	case PrefsCollectionSync::SyncUsed:
	case PrefsCollectionSync::CompleteSync:
	case PrefsCollectionSync::SyncLogTraffic:
	default:
		// Nothing to do ...
		break;

	case PrefsCollectionSync::SyncEnabled:
		/* Syncing was enabled/disabled. Currently it is the task of the UI to
		 * start the sync process.
		 * TODO: let sync automatically start the sync process when this
		 * preference is enabled. And use a UI listener, which is notified about
		 * this. */
		OP_DBG(("PrefsCollectionSync::SyncEnabled"));
		/* If sync is enabled, this means that some data-clients will be
		 * enabled as well: */
		BroadcastSyncSupportChanged();
		break;

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case PrefsCollectionSync::SyncPasswordManager:
		/* This type depends on support for encryption-key. So whenever this
		 * support type is enabled, also enable SYNC_SUPPORTS_ENCRYPTION (and
		 * fall through). And whenever this is disabled (and no other type that
		 * depends on encryption-key is enabled - this is currently easy,
		 * because only password-manager depends on it), disable
		 * SYNC_SUPPORTS_ENCRYPTION (and fall through): */
		BroadcastSyncSupportChanged(SYNC_SUPPORTS_ENCRYPTION, newvalue ? true : false);
		/* fall through ... to also broadcast the change of
		 * SYNC_SUPPORTS_PASSWORD_MANAGER */
#endif // SYNC_HAVE_PASSWORD_MANAGER

#ifdef SYNC_HAVE_BOOKMARKS
	case PrefsCollectionSync::SyncBookmarks:
#endif // SYNC_HAVE_BOOKMARKS
#ifdef SYNC_HAVE_EXTENSIONS
	case PrefsCollectionSync::SyncExtensions:
#endif // SYNC_HAVE_EXTENSIONS
#ifdef SYNC_HAVE_FEEDS
	case PrefsCollectionSync::SyncFeeds:
#endif // SYNC_HAVE_FEEDS
#ifdef SYNC_HAVE_NOTES
	case PrefsCollectionSync::SyncNotes:
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PERSONAL_BAR
	case PrefsCollectionSync::SyncPersonalbar:
#endif // SYNC_HAVE_PERSONAL_BAR
#ifdef SYNC_HAVE_SEARCHES
	case PrefsCollectionSync::SyncSearches:
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
	case PrefsCollectionSync::SyncSpeeddial:
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_HAVE_TYPED_HISTORY
	case PrefsCollectionSync::SyncTypedHistory:
#endif // SYNC_HAVE_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case PrefsCollectionSync::SyncURLFilter:
#endif // SYNC_CONTENT_FILTERS
	{
		// Syncing this type was enabled/disabled
		OpSyncSupports supports = OpSyncState::EnablePref2Supports(pref);
		OP_DBG(("supports: ") << supports);
		if (supports != SYNC_SUPPORTS_MAX)
		{
			bool sync_enabled = KeepDataClientsEnabled();
#ifdef _DEBUG
			if (sync_enabled && !SyncEnabled())
				OP_DBG(("sync was last used less than %d days ago at: %d; so don't disable any type that is currently enabled.", SYNC_CHECK_LAST_USED_DAYS, g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLastUsed)));
#endif // _DEBUG
			BroadcastSyncSupportChanged(supports, newvalue && sync_enabled);
		}
		break;
	}

	}
}

/* virtual */
void OpSyncCoordinator::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC& newvalue)
{
	OP_NEW_DBG("OpSyncCoordinator::PrefChanged()", "sync");
	OP_ASSERT(OpPrefsCollection::Sync == id);
	OP_DBG((UNI_L("pref: %d; value: '%s'"), pref, newvalue.CStr()));
	switch (pref) {
	case PrefsCollectionSync::SyncClientState:
	case PrefsCollectionSync::SyncClientStateBookmarks:
#ifdef SYNC_HAVE_NOTES
	case PrefsCollectionSync::SyncClientStateNotes:
#endif // SYNC_HAVE_NOTES
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	case PrefsCollectionSync::SyncClientStatePasswordManager:
#endif // SYNC_HAVE_PASSWORD_MANAGER
#ifdef SYNC_HAVE_SEARCHES
	case PrefsCollectionSync::SyncClientStateSearches:
#endif // SYNC_HAVE_SEARCHES
#ifdef SYNC_HAVE_SPEED_DIAL
# ifdef SYNC_HAVE_SPEED_DIAL_2
	case PrefsCollectionSync::SyncClientStateSpeeddial2:
# else
	case PrefsCollectionSync::SyncClientStateSpeeddial:
# endif // SYNC_HAVE_SPEED_DIAL_2
#endif // SYNC_HAVE_SPEED_DIAL
#ifdef SYNC_TYPED_HISTORY
	case PrefsCollectionSync::SyncClientStateTypedHistory:
#endif // SYNC_TYPED_HISTORY
#ifdef SYNC_CONTENT_FILTERS
	case PrefsCollectionSync::SyncClientStateURLFilter:
#endif // SYNC_CONTENT_FILTERS
	case PrefsCollectionSync::SyncDataProvider:
	case PrefsCollectionSync::ServerAddress:
	default:
		// nothing to do...
		break;
	}
}

// </Implementation of OpPrefsListener>

void OpSyncCoordinator::BroadcastPendingSyncSupportChanged()
{
	if (m_pending_support_state.HasAnySupports())
	{
		OP_NEW_DBG("OpSyncCoordinator::BroadcastPendingSyncSupportChanged()", "sync");
		for (int i = 0; i < SYNC_SUPPORTS_MAX; i++)
		{
			OpSyncSupports supports = static_cast<OpSyncSupports>(i);
			if (m_pending_support_state.HasSupports(supports))
				BroadcastSyncSupportChanged(supports, GetSupports(supports) ? true : false);
		}
		m_pending_support_state.Clear();
	}
}

void OpSyncCoordinator::BroadcastSyncSupportChanged(OpSyncSupports supports, bool has_support)
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastSyncSupportChanged()", "sync");
	OP_DBG(("support: ") << supports << (has_support?" enabled":" disabled"));
	if (m_is_initialised)
	{
		if (IsSyncInProgress())
		{	/* If a sync-connection is in progress we don't want to notify the
			 * registered OpSyncSupportClient instances immediately, because
			 * that may interfere with the on-going connection. Instead we set
			 * the change as pending and on the end of the connection we notify
			 * the listener.
			 * See m_sync_in_progress */
			m_pending_support_state.SetSupports(supports, true);
			return;
		}

		m_supports_state.SetSupports(supports, has_support);
		OpINT32Vector types;
		RETURN_VOID_IF_ERROR(OpSyncCoordinator::GetTypesFromSupports(supports, types));
		for (unsigned int j = 0; j < types.GetCount(); j++)
		{
			OpSyncDataItem::DataItemType type = static_cast<OpSyncDataItem::DataItemType>(types.Get(j));
			for (unsigned int i = 0; i < m_listeners.GetCount(); i++)
			{
				OpSyncDataClientItem* listener_item = m_listeners.Get(i);
				if (listener_item->HasType(type))
				{
					OP_DBG(("") << i << ": has type " << type);
					OpStatus::Ignore(listener_item->Get()->SyncDataSupportsChanged(type, has_support));
				}
			}
		}
	}
}

bool OpSyncCoordinator::KeepDataClientsEnabled() const
{
	bool sync_enabled = SyncEnabled();
	bool sync_used = static_cast<bool>(g_opera && g_pcsync && g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncUsed));
	if (sync_used && !sync_enabled)
	{	/* If sync was used once and it is disabled now, we want to continue to
		 * collect the client's sync updates in the update-queue for the next
		 * SYNC_CHECK_LAST_USED_DAYS days. Thus on re-enabling sync within that
		 * period we can simply upload the queue. */
		long sync_last_used = g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLastUsed);
		if (sync_last_used > 0)
		{
			unsigned long seconds_now, milliseconds_now;
			g_op_time_info->GetWallClock(seconds_now, milliseconds_now);
			if (seconds_now <= (unsigned long)sync_last_used + (SYNC_CHECK_LAST_USED_DAYS*60*60*24))
				return true;
		}
	}
	return sync_enabled;
}

void OpSyncCoordinator::BroadcastSyncSupportChanged()
{
	OP_NEW_DBG("OpSyncCoordinator::BroadcastSyncSupportChanged()", "sync");
	OP_DBG(("sync support %s", SyncEnabled()?"enabled":"disabled"));

	bool sync_enabled = KeepDataClientsEnabled();
#ifdef _DEBUG
	if (sync_enabled && !SyncEnabled())
		OP_DBG(("sync was last used less than %d days ago at: %d; so don't disable any type that is currently enabled.", SYNC_CHECK_LAST_USED_DAYS, g_pcsync->GetIntegerPref(PrefsCollectionSync::SyncLastUsed)));
#endif // _DEBUG

	for (int i = 0; i < SYNC_SUPPORTS_MAX; i++)
	{
		OpSyncSupports supports_type = static_cast<OpSyncSupports>(i);
		bool supports = sync_enabled && GetSupports(supports_type);
		OP_DBG(("Supports ") << supports_type << ": " << (supports?"yes":"no"));
		BroadcastSyncSupportChanged(supports_type, supports);
	}
}

#endif // SUPPORT_DATA_SYNC
