/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SYNC_ENCRYPTION_KEY_SUPPORT

#include "modules/sync/sync_encryption_key.h"
#include "modules/libcrypto/include/CryptoBlob.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/sync/sync_util.h"
#include "modules/wand/wandmanager.h"

// ==================== SyncEncryptionKeyReencryptContext

/**
 * This class implements the OpSyncUIListener::ReencryptEncryptionKeyContext
 * that is used to notify the OpSyncUIListener when the SyncEncryptionKeyManager
 * cannot decrypt a received encryption-key.
 *
 * @see OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed()
 * @see SyncEncryptionKeyManager::ProcessSyncItemKey()
 *
 * A new instance is created by SyncEncryptionKeyManager::ProcessSyncItemKey()
 * and assigned to SyncEncryptionKeyManager::m_reencrypt_context. The encrypted
 * encryption-key that cannot be decrypted, is stored in an attribute of this
 * instance: SetEncryptedKey(). That key is passed to
 * SyncEncryptionKeyManager::ReencryptEncryptionKey() when the user has entered
 * the old password (see DecryptWithPassword()).
 *
 * \msc
 * Link,SyncEncryptionKeyManager,Context,OpSyncUIListener,User;
 * Link->SyncEncryptionKeyManager [label="ProcessSyncItemKey(OpSyncItem)", url="\ref SyncEncryptionKeyManager::ProcessSyncItemKey()"];
 * SyncEncryptionKeyManager=>Context [label="create"];
 * SyncEncryptionKeyManager->OpSyncUIListener [label="OnSyncReencryptEncryptionKeyFailed()", url="\ref OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed()"];
 * OpSyncUIListener->User [label="prompt user"];
 * OpSyncUIListener<<User [label="enter old password"];
 * Context<<=OpSyncUIListener [label="DecryptWithPassword()", url="\ref DecryptWithPassword()"];
 * Context=>SyncEncryptionKeyManager [label="ReencryptEncryptionKey()", url="\ref SyncEncryptionKeyManager::ReencryptEncryptionKey()"];
 * SyncEncryptionKeyManager->Link [label="send re-encrypted key"];
 * \endmsc
 */
class SyncEncryptionKeyReencryptContext
	: public OpSyncUIListener::ReencryptEncryptionKeyContext
{
private:
	SyncEncryptionKeyManager* m_manager;
	/** The encrypted encryption-key that was received from the Link server.
	 * SetEncryptedKey() stores the value and DecryptWithPassword() passes this
	 * value to SyncEncryptionKeyManager::ReencryptEncryptionKey().
	 */
	OpString m_encrypted_key;

public:
	/**
	 * The constructor gets a pointer to the associated SyncEncryptionKeyManager
	 * to be able to call SyncEncryptionKeyManager::ReencryptEncryptionKey()
	 * when the user entered the old password.
	 */
	SyncEncryptionKeyReencryptContext(SyncEncryptionKeyManager* manager)
		: m_manager(manager) {}
	virtual ~SyncEncryptionKeyReencryptContext() {}

	/**
	 * Sets the encrypted encryption-key that was received from the Link
	 * server. This key is passed to
	 * SyncEncryptionKeyManager::ReencryptEncryptionKey() when the user entered
	 * the old password.
	 */
	OP_STATUS SetEncryptedKey(const OpStringC& encrypted_key) {
		return m_encrypted_key.Set(encrypted_key);
	}
	OP_STATUS TakeEncryptedKey(OpString& encrypted_key) {
		return m_encrypted_key.TakeOver(encrypted_key);
	}

	/**
	 * @name Implementation of OpSyncUIListener::ReencryptEncryptionKeyContext
	 * @{
	 */

	virtual void DecryptWithPassword(const OpStringC& password) {
		// tell the m_manager to remove its reference to this instance:
		m_manager->OnReencryptEncryptionKeyContextDone(this);
		m_manager->ReencryptEncryptionKey(m_encrypted_key, password);
		OP_DELETE(this);
	}

	virtual void CreateNewEncryptionKey() {
		// tell the m_manager to remove its reference to this instance:
		m_manager->OnReencryptEncryptionKeyContextDone(this);
		m_manager->GenerateNewEncryptionKey();
		OP_DELETE(this);
	}

	/** @} */ // OpSyncUIListener::ReencryptEncryptionKeyContext
};

// ==================== SyncEncryptionKey

void SyncEncryptionKey::Clear()
{
	/* being paranoid to overwrite the memory that holds the encrypted key: */
	m_encryption_key_encrypted.Wipe();
}

OP_STATUS SyncEncryptionKey::SetEncryptionKey(const OpStringC& key, const OpStringC& username, const OpStringC& password)
{
	OP_NEW_DBG("SyncEncryptionKey::SetEncryptionKey()", "sync.encryption");
	OP_DBG((UNI_L("username: '%s'"), username.CStr()));
	OP_ASSERT(key.Length() == 32 && "The encryption-key is expected to be a 16 byte long binary data encoded as a hex-string.");

	OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
	RETURN_OOM_IF_NULL(blob_encryptor.get());
	/* The encryption key is encrypted with H(username+":"+password). So
	 * - create a hash over that string
	 * - encrypt the encrypted key with the created hash */
	UINT8* hash = 0;
	size_t hash_len = 0;
	RETURN_IF_ERROR(GetHash(&hash, &hash_len, username, password));
	OP_STATUS status = OpStatus::OK;
	if (blob_encryptor->GetKeySize() < 0 ||
		hash_len < (size_t)blob_encryptor->GetKeySize())
		status = OpStatus::ERR_OUT_OF_RANGE;
	/* The encrypted blob contains the utf8 encoded hex-string representation of
	 * the encryption-key. Note: as the encryption-key is a hex-string its utf8
	 * encoding is equal to the low byte of the utf16 encoded string, so we can
	 * simply use OpString8::Set()... */
	OpString8 key8;
	// And use that to encrypt the key
	if (OpStatus::IsSuccess(status))
	{
		blob_encryptor->SetKey(hash);
		status = key8.Set(key);
	}
	OpString encrypted_key;
	if (OpStatus::IsSuccess(status))
		status = blob_encryptor->EncryptBase64(key8.CStr(), encrypted_key);
	if (hash)
	{	// be paranoid and wipe the hash:
		op_memset(hash, 0, hash_len);
		OP_DELETEA(hash);
	}
	if (OpStatus::IsSuccess(status))
		status = m_encryption_key_encrypted.TakeOver(encrypted_key);
	key8.Wipe();
	return status;
}

OP_STATUS SyncEncryptionKey::SetEncryptedEncryptionKey(const OpStringC& key)
{
	Clear();
	if (key.HasContent())
		return m_encryption_key_encrypted.Set(key);
	return OpStatus::OK;
}

OP_STATUS SyncEncryptionKey::Decrypt(OpString& encryption_key, const OpStringC& username, const OpStringC& password) const
{
	OP_NEW_DBG("SyncEncryptionKey::Decrypt()", "sync.encryption");
	OP_DBG((UNI_L("username: '%s'"), username.CStr()));
	/* The encryption key is encrypted with H(username+":"+password). So
	 * - create a hash over that string
	 * - decrypt the encrypted key with the created hash */
	UINT8* hash = 0;
	size_t hash_size = 0;
	RETURN_IF_ERROR(GetHash(&hash, &hash_size, username, password));
	// And use that to decrypt the key
	OP_STATUS status = Decrypt(encryption_key, hash, hash_size);
	if (hash)
	{	// be paranoid and wipe the hash:
		op_memset(hash, 0, hash_size);
		OP_DELETEA(hash);
	}
	return status;
}

OP_STATUS SyncEncryptionKey::Decrypt(OpString& encryption_key, const UINT8* key, size_t key_size) const
{
	OP_NEW_DBG("SyncEncryptionKey::Decrypt()", "sync.encryption");
	encryption_key.Empty();
	if (IsEmpty()) return OpStatus::OK;	// nothing to do...

	OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
	RETURN_OOM_IF_NULL(blob_encryptor.get());
	if (blob_encryptor->GetKeySize() < 0 ||
		key_size < (size_t)blob_encryptor->GetKeySize())
		return OpStatus::ERR_OUT_OF_RANGE;
	OP_ASSERT(key);
	RETURN_IF_ERROR(blob_encryptor->SetKey(key));
	OpString8 encryption_key8;
	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(m_encryption_key_encrypted.CStr(), encryption_key8));
	/* The encrypted blob contains the utf8 encoded hex-string representation of
	 * the encryption-key. Note: as the encryption-key is a hex-string its utf8
	 * encoding is equal to the low byte of the utf16 encoded string, so we can
	 * simply use OpString::Set() to convert it to utf16... */
	OP_STATUS status = encryption_key.Set(encryption_key8);
	encryption_key8.Wipe();
	return status;
}

OP_STATUS SyncEncryptionKey::Decrypt(UINT8 encryption_key[16], const OpStringC& username, const OpStringC& password) const
{
	OP_NEW_DBG("SyncEncryptionKey::Decrypt()", "sync.encryption");
	OpString encryption_key_hex;
	return Decrypt(encryption_key_hex, encryption_key, username, password);
}

OP_STATUS SyncEncryptionKey::Decrypt(OpString& encryption_key_hex, UINT8 encryption_key[16], const OpStringC& username, const OpStringC& password) const
{
	OP_NEW_DBG("SyncEncryptionKey::Decrypt()", "sync.encryption");
	RETURN_IF_ERROR(Decrypt(encryption_key_hex, username, password));
	size_t length = encryption_key_hex.Length();
	unsigned int i, j = 0;
	for (i=0; i<length && j<16; i++) {
		uni_char hex_char = encryption_key_hex[i];
		UINT8 nibble = 0;
		if ('0' <= hex_char && hex_char <= '9')
			nibble = (UINT8)(hex_char - '0');
		else if ('a' <= hex_char && hex_char <= 'f')
			nibble = ((UINT8)(hex_char - 'a')+10);
		else if ('A' <= hex_char && hex_char <= 'F')
			nibble = ((UINT8)(hex_char - 'A')+10);
		else
			return OpStatus::ERR_OUT_OF_RANGE;
		if (i % 2 == 0)
			encryption_key[j] = (nibble << 4);
		else
			encryption_key[j++] |= nibble;
	}
	if (j < 16 || encryption_key_hex[i] != 0)
		return OpStatus::ERR_OUT_OF_RANGE;
	return OpStatus::OK;
}

OP_STATUS SyncEncryptionKey::GetHash(UINT8** hash, size_t* hash_len, const OpStringC& username, const OpStringC& password) const
{
	OP_NEW_DBG("SyncEncryptionKeyManager::GetHash()", "sync.encryption");
	/* The encryption key is encrypted with H(username+":"+password). So
	 * - create the string username+":"+password,
	 * - create a hash over that string */
	OpAutoPtr<CryptoHash> hasher(CryptoHash::CreateSHA256());
	RETURN_OOM_IF_NULL(hasher.get());
	RETURN_IF_ERROR(hasher->InitHash());

	if (username.HasContent())
	{
		OpString8 username8;
		RETURN_IF_ERROR(username8.SetUTF8FromUTF16(username));
		hasher->CalculateHash(reinterpret_cast<const UINT8*>(username8.CStr()), username8.Length());
	}
	hasher->CalculateHash(reinterpret_cast<const UINT8*>(":"), 1);
	if (password.HasContent())
	{
		OpString8 password8;
		RETURN_IF_ERROR(password8.SetUTF8FromUTF16(password));
		hasher->CalculateHash(reinterpret_cast<const UINT8*>(password8.CStr()), password8.Length());
		password8.Wipe();
	}

	// Extract the hash into a byte-array:
	size_t hash_size = hasher->Size();
	if (hash_size)
	{
		*hash = OP_NEWA(UINT8, hash_size);
		RETURN_OOM_IF_NULL(hash);
		hasher->ExtractHash(*hash);
		if (hash_len)
			*hash_len = hash_size;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}


// ==================== SyncEncryptionKeyWandLoginCallback

/**
 * This is the WandLoginCallback class that is used to retrieve the
 * encryption-key from wand.
 *
 * SyncEncryptionKeyManager::LoadEncryptionKeyFromWand() creates an instance of
 * this class and sets SyncEncryptionKeyManager::m_wand_callback_encryption_key
 * to the instance.
 * WandManager::GetLoginPasswordWithoutWindow() calls the callback instance when
 * the encryption-key is available. The callback sets then
 * SyncEncryptionKeyManager::m_encryption_key to the loaded encryption-key.
 */
class SyncEncryptionKeyWandLoginCallback
	: public WandLoginCallback
{
private:
	SyncEncryptionKeyManager* m_manager;
	OpSyncUIListener::ReencryptEncryptionKeyContext* m_reencrypt_context;

public:
	/**
	 * Creates an instance of the WandLoginCallback and associates this instance
	 * with the specified SyncEncryptionKeyManager instance. I.e. m_manager
	 * points to the specified SyncEncryptionKeyManager instance and
	 * SyncEncryptionKeyManager::m_wand_callback_encryption_key points to this
	 * instance. This expects that there can not be more than one
	 * EncryptionKeyWandLoginCallback instance (per SyncEncryptionKeyManager).
	 *
	 * When the OnPasswordRetrieved() is called, the encryption-key in the
	 * specified manager is updated and this callback instance is deleted.
	 *
	 * @param manager is the associated SyncEncryptionKeyManager. This class is
	 *  declared as friend of the SyncEncryptionKeyManager to be able to call
	 *  some SyncEncryptionKeyManager methods on retrieving the callback from
	 *  wand.
	 * @param reencrypt_context is an
	 *  OpSyncUIListener::ReencryptEncryptionKeyContext instance (or 0). If this
	 *  callback instance is created on retrieving an encryption-key sync-item
	 *  from the link-server that we cannot decrypt, the caller creates this
	 *  context and tries to load the encryption-key from wand. When the
	 *  wand-entry is loaded (see OnPasswordRetrieved() or
	 *  OnPasswordRetrievalFailed()) and if it does not contain a valid
	 *  encryption-key we use the reencrypt_context to ask the user for the old
	 *  Opera Account password, which may be used to decrypt the encryption-key.
	 */
	SyncEncryptionKeyWandLoginCallback(SyncEncryptionKeyManager* manager, OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context);

	/**
	 * The destructor sets
	 * SyncEncryptionKeyManager::m_wand_callback_encryption_key of the
	 * associated SyncEncryptionKeyManager instance to 0.
	 */
	virtual ~SyncEncryptionKeyWandLoginCallback();

	/**
	 * The destructor of the SyncEncryptionKeyManager calls this method if the
	 * associated SyncEncryptionKeyManager instance is deleted before the
	 * callback instance received an answer. Thus the implementations of
	 * OnPasswordRetrieved() or OnPasswordRetrievalFailed() don't access an
	 * already deleted SyncEncryptionKeyManager object.
	 */
	void ResetEncryptionKeyManager() { m_manager = 0; }

	/**
	 * SyncEncryptionKeyManager::LoadEncryptionKeyFromWand() calls this method
	 * if the SyncEncryptionKeyManager already has an active wand login callback
	 * instance and the SyncEncryptionKeyManager tries again to load the
	 * encryption-key from wand. If an
	 * OpSyncUIListener::ReencryptEncryptionKeyContext is specified, it may
	 * replace an existing context.
	 */
	void UpdateReencryptEncryptionKeyContext(OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context) {
		if (reencrypt_context)
		{
			OP_DELETE(m_reencrypt_context);
			m_reencrypt_context = reencrypt_context;
		}
	}

	/**
	 * @name Implementation  of WandLoginCallback
	 * @{
	 */

	/**
	 * When the callback retrieves the password, it is stored in the associated
	 * SyncEncryptionKeyManager instance.
	 */
	virtual void OnPasswordRetrieved(const uni_char* password);

	/** If we cannot retrieve the password there is nothing to do. */
	virtual void OnPasswordRetrievalFailed();
	/** @} */ // Implementation of WandLoginCallback
};

SyncEncryptionKeyWandLoginCallback::SyncEncryptionKeyWandLoginCallback(SyncEncryptionKeyManager* manager, OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context)
	: m_manager(manager)
	, m_reencrypt_context(reencrypt_context)
{
	OP_NEW_DBG("SyncEncryptionKeyWandLoginCallback::SyncEncryptionKeyWandLoginCallback()", "sync.encryption");
	OP_DBG(("this: %p; manager: %p", this, m_manager));
	OP_ASSERT(m_manager);
	OP_ASSERT(m_manager->m_wand_callback_encryption_key == 0);
	m_manager->m_wand_callback_encryption_key = this;
}

/* virtual */
SyncEncryptionKeyWandLoginCallback::~SyncEncryptionKeyWandLoginCallback()
{
	OP_NEW_DBG("SyncEncryptionKeyWandLoginCallback::~SyncEncryptionKeyWandLoginCallback()", "sync.encryption");
	OP_DBG(("this: %p; manager: %p", this, m_manager));
	if (m_manager)
		m_manager->m_wand_callback_encryption_key = 0;
	OP_DELETE(m_reencrypt_context);
}

/* virtual */
void SyncEncryptionKeyWandLoginCallback::OnPasswordRetrieved(const uni_char* encryption_key)
{
	OP_NEW_DBG("SyncEncryptionKeyWandLoginCallback::OnPasswordRetrieved()", "sync.encryption");
	if (m_manager)
	{
		OP_STATUS status = m_manager->SetEncryptionKey(encryption_key);
		if (OpStatus::IsSuccess(status))
		{
			/* We loaded the encryption-key from wand, because the server did
			 * not have an encryption-key (or not a valid one), so now update
			 * the server: */
			m_manager->CommitEncryptionKeySyncItem(FALSE);
			/* and notify anybody who may want to use the it: */
			m_manager->NotifyEncryptionKey();
		}
		else if (OpStatus::ERR_OUT_OF_RANGE == status)
		{
			/* The encryption-key in wand is not valid, so delete it. And if we
			 * have a ReencryptEncryptionKeyContext, we have received an
			 * encryption-key from the Link server which we cannot decrypt,
			 * but we tried to load this wand-entry first. So now ask the user
			 * to decide: */
			OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context = m_reencrypt_context;
			m_reencrypt_context = 0;	// now the manager owns the context
			m_manager->DeleteEncryptionKey(true, reencrypt_context);
		}
	}
	OP_DELETE(this);
}

/* virtual */
void SyncEncryptionKeyWandLoginCallback::OnPasswordRetrievalFailed()
{
	OP_NEW_DBG("SyncEncryptionKeyWandLoginCallback::OnPasswordRetrievalFailed()", "sync.encryption");
	/* Notify the OpSyncUIListener that the master-password dialog was
	 * cancelled (or that there was some other error that prevents us
	 * from loading the encryption-key from wand). */
	m_manager->m_sync_coordinator->OnSyncError(SYNC_PENDING_ENCRYPTION_KEY, UNI_L(""));
	OP_DELETE(this);
}


// ==================== SyncEncryptionKeyManager

SyncEncryptionKeyManager::SyncEncryptionKeyManager()
	: m_sync_coordinator(0)
	, m_support_enabled(false)
	, m_encryption_type(ENCRYPTION_TYPE_OPERA_ACCOUNT)
	, m_waiting_for_encryption_key(false)
	, m_reencrypt_context(0)
	, m_wand_callback_encryption_key(0)
#ifdef SELFTEST
	, m_testing(0)
#endif // SELFTEST
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SyncEncryptionKeyManager()", "sync.encryption");
}

/* virtual */
SyncEncryptionKeyManager::~SyncEncryptionKeyManager()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::~SyncEncryptionKeyManager()", "sync.encryption");
	// Cancel any possible callback to the ReencryptEncryptionKeyContext
	SetReencryptEncryptionKeyContext(0);

	if (m_wand_callback_encryption_key)
		/* If there is a pending callback, reset the pointer to this instance,
		 * so when the callback is finally called, it will not access this
		 * instance anymore: */
		m_wand_callback_encryption_key->ResetEncryptionKeyManager();

	if (m_sync_coordinator)
	{
		m_sync_coordinator->RemoveSyncDataClient(this, OpSyncDataItem::DATAITEM_ENCRYPTION_KEY);
		StopWaitingForEncryptionKey();
	}
}

void SyncEncryptionKeyManager::InitL(OpSyncCoordinator* sync_coordinator)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::InitL()", "sync.encryption");
	OP_ASSERT(m_sync_coordinator == 0 && "InitL() must only be called once!");
	/* Note: we need to assign m_sync_coordinator before calling
	 * SetSyncDataClient(). SetSyncDataClient() may call back to
	 * SyncDataSupportsChanged() - if encryption support is enabled. In that
	 * case we also need to register this instance as OpSyncUIListener... */
	m_sync_coordinator = sync_coordinator;
	m_sync_coordinator->SetSyncEncryptionKeyManager(this);
	OP_STATUS status = m_sync_coordinator->SetSyncDataClient(this, OpSyncDataItem::DATAITEM_ENCRYPTION_KEY);
	if (OpStatus::IsError(status))
	{	/* error means that this instance is not set as OpSyncDataClient of the
		 * specified OpSyncCoordinator. So unregister us as OpSyncUIListener as
		 * well and set m_sync_coordinator to 0 - so we don't call
		 * RemoveSyncDataClient() in the destructor: */
		if (IsWaitingForEncryptionKey())
			StopWaitingForEncryptionKey();
		m_sync_coordinator = 0;
		LEAVE(status);
	}
}

// <OpSyncDataClient>

/* virtual */
OP_STATUS SyncEncryptionKeyManager::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SyncDataInitialize()", "sync.encryption");
	switch (type) {
	case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
		/* Initial syncing is started. If we have an encryption-key, we need to
		 * delete it (but only the copy in memory, not the copy in wand) and we
		 * will start waiting for the Link server to send us the
		 * encryption-key: */
		DeleteEncryptionKey(false, 0);
		break;

	case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
		// Nothing to do here
		break;

	default:
		OP_ASSERT(!"unexpected type");
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS SyncEncryptionKeyManager::SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SyncDataAvailable()", "sync.encryption");
	if (!new_items || !new_items->First())
	{
		OP_ASSERT(!"SyncDataAvailable() should never get an empty list!");
		return OpStatus::OK;
	}

	OpSyncItem* received_key = 0;
	OpSyncItem* received_type = 0;
	for (OpSyncItemIterator current(new_items->First()); *current; ++current)
	{
		switch (current->GetType())
		{
		case OpSyncDataItem::DATAITEM_ENCRYPTION_KEY:
			received_key = *current;
			break;

		case OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE:
			received_type = *current;
			break;

		default:
			OP_ASSERT(!"received sync-item with unsupported type");
		}
	}

	/* If we received a new key now, clear any previously received key to avoid
	 * an unnecessary EncryptionKeyListener::OnEncryptionKey() notification for
	 * the old key if ProcessSyncItemType() detects a supported type. */
	if (received_key)
		m_received_encryption_key.Empty();

	/* 1. handle the received encryption-type (if we received any).
	 * Note: do this before handling a received encryption-key to avoid
	 * notifying any EncryptionKeyListener instance about a "received-key" which
	 * is immediately followed by a deleted key because we cannot use the key
	 * for an unsupported type. */
	if (received_type)
		RETURN_IF_ERROR(ProcessSyncItemType(received_type));

	/* 2. handle the received encryption-key (if we received any). */
	if (received_key)
		RETURN_IF_ERROR(ProcessSyncItemKey(received_key));

	return OpStatus::OK;
}

/* virtual */
OP_STATUS SyncEncryptionKeyManager::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SyncDataFlush()", "sync.encryption");
	OP_ASSERT(type == OpSyncDataItem::DATAITEM_ENCRYPTION_KEY ||
			  type == OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE ||
			  type == OpSyncDataItem::DATAITEM_GENERIC);
	OP_DBG(("type: ") << type << (first_sync?"; first sync":"") << (is_dirty?"; is dirty":"") << (HasEncryptionKey()?"":"; no encryption-key available"));

	/* Only send the encryption key to the server if this is a dirty sync and we
	 * have an encryption key: */
	if (is_dirty && HasEncryptionKey())
		return CommitEncryptionKeySyncItem(is_dirty);
	return OpStatus::OK;
}

/* virtual */
OP_STATUS SyncEncryptionKeyManager::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SyncDataSupportsChanged()", "sync.encryption");
	OP_ASSERT(type == OpSyncDataItem::DATAITEM_ENCRYPTION_KEY || type == OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE);
	OP_DBG(("support %s", has_support?"enabled":"disabled"));
	m_support_enabled = has_support?true:false;
	if (m_support_enabled)
	{
		/* If we don't have an encryption-key yet, we wait for the first
		 * response from the Link server. If the Link server sends the
		 * encryption-key, we can store and use it (SetEncryptionKey()). If the
		 * Link server has no encryption-key, we load it from wand or generate
		 * a new one (OnSyncFinished()): */
		if (!HasEncryptionKey())
			StartWaitingForEncryptionKey();
	}
	else
		/* If encryption support is disabled, the Link server will not send the
		 * encryption-key, so we should stop waiting for it. Thus we don't load
		 * nor generate a new encryption-key when the Link server response
		 * arrives without encryption-key.
		 * Note: if support is disabled after sending the request from the
		 * client to the Link server and before retrieving the result from the
		 * server we still may receive the encryption-key (in which case we
		 * store it). */
		StopWaitingForEncryptionKey();
	return OpStatus::OK;
}

// </OpSyncDataClient>

// <OpSyncUIListener>

/* virtual */
void SyncEncryptionKeyManager::OnSyncFinished(OpSyncState& sync_state)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::OnSyncFinished()", "sync.encryption");
	/* We did not receive an encryption-key from the Link server */
	OP_ASSERT(!HasEncryptionKey());
	OP_ASSERT(IsWaitingForEncryptionKey() && "This method should only be called after the first sync connection of a session is completed successfully without receiving the encryption-key.");
	StopWaitingForEncryptionKey();
	OpStatus::Ignore(LoadEncryptionKey());
	/* Note: if LoadEncryptionKey() returns an error status, then we can neither
	 * load the encryption-key from wand, nor generate a new encryption-key, so
	 * we ignore the return-status. This means we continue without an
	 * encryption-key. However if another Opera client uploads a key to the Link
	 * server while this session is active, we will receive that in
	 * SyncDataAvailable() and start using it... */
}

// </OpSyncUIListener>

OP_STATUS SyncEncryptionKeyManager::LoadEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::LoadEncryptionKey()", "sync.encryption");
	OP_ASSERT(!HasEncryptionKey());
	OP_ASSERT(!IsWaitingForEncryptionKey() && "This method should only be called after the first sync connection of a session is completed successfully without receiving the encryption-key.");
	OP_STATUS status =
#ifdef WAND_SUPPORT
		LoadEncryptionKeyFromWand(0);
	if (OpStatus::ERR_NO_SUCH_RESOURCE == status)
		/* If the status is ERR_NO_SUCH_RESOURCE, then the encryption-key is not
		 * yet stored in wand. */
		status =
#endif // WAND_SUPPORT
			GenerateNewEncryptionKey();
	return status;
}

OP_STATUS SyncEncryptionKeyManager::GenerateNewEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::GenerateNewEncryptionKey()", "sync.encryption");
	OP_ASSERT(!HasEncryptionKey() && "We should only generate a new encryption-key if we don't have one.");
	OP_ASSERT(!IsWaitingForEncryptionKey() && "We should not generate a new encryption-key if we still wait to receive the encryption-key from the Link server.");
#ifdef WAND_SUPPORT
	OP_ASSERT(m_wand_callback_encryption_key == 0 && "We should not generate a new encryption-key if we still wait for the wand callback.");
#endif // WAND_SUPPORT

	OP_ASSERT(g_opera && g_libcrypto_random_generator);
	/* use the OpRandomGenerator to generate a random encryption-key. The
	 * encryption-key is expected to be 16 byte long, so generate a 32
	 * character long hex-string: */
	OpString encryption_key;
	RETURN_IF_ERROR(g_libcrypto_random_generator->GetRandomHexString(encryption_key, 32));
	OpString username, password;
	OP_STATUS status = m_sync_coordinator->GetLoginInformation(username, password);
	password.Wipe();
	if (OpStatus::IsSuccess(status))
		status = SetEncryptionKey(encryption_key);
	if (OpStatus::IsSuccess(status))
		status = StoreEncryptionKeyInWand(encryption_key, username);
	encryption_key.Wipe();
	if (OpStatus::IsSuccess(status))
		status = CommitEncryptionKeySyncItem(FALSE);
	if (OpStatus::IsSuccess(status))
	{
		m_sync_coordinator->OnEncryptionKeyCreated();
		NotifyEncryptionKey();
	}
	else
		/* If there is any error on storing the encryption-key in wand or on
		 * adding the encryption-key to the next sync queue we give up without
		 * having an encryption-key. Now the Opera client needs to be restarted
		 * to try again. */
		m_encryption_key.Clear();
	return status;
}

void SyncEncryptionKeyManager::ClearEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ClearEncryptionKey()", "sync.encryption");

	/* Cancel any possible callback to the ReencryptEncryptionKeyContext (if
	 * there is such a context) */
	SetReencryptEncryptionKeyContext(0);

	if (HasEncryptionKey())
		/* If we have an encryption-key, clear it (but keep the copy in wand),
		 * notify all EncryptionKeyListener instances and start waiting for the
		 * encryption-key from the Link server: */
		DeleteEncryptionKey(false);
}

OP_STATUS SyncEncryptionKeyManager::ReencryptEncryptionKey(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ReencryptEncryptionKey()", "sync.encryption");
	if (!HasEncryptionKey())
		return OpStatus::OK;

	OpString decrypted_key;
	OP_STATUS status = m_encryption_key.Decrypt(decrypted_key, username, old_password);
	if (OpStatus::IsSuccess(status))
		status = m_encryption_key.SetEncryptionKey(decrypted_key, username, new_password);
	decrypted_key.Wipe();
	if (OpStatus::IsSuccess(status))
	{	// On success send the newly encrypted encryption-key to the Link server
		OpStatus::Ignore(CommitEncryptionKeySyncItem(FALSE));
		/* Note: we can ignore an error in CommitEncryptionKeySyncItem()
		 * because this "only" means that the Link server does not have the
		 * correctly encrypted encryption-key. On the next start of Opera the
		 * wrongly encrypted encryption-key is received from the Link server,
		 * the Opera client cannot decrypt it, the client loads the key instead
		 * from wand and commits the item again to the Link server until this
		 * process is successful... */
	}
	else
		// On error, clear the encryption-key
		ClearEncryptionKey();
	return status;
}

void SyncEncryptionKeyManager::ReencryptEncryptionKey(const OpStringC& key, const OpStringC& old_password)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ReencryptEncryptionKey()", "sync.encryption");
	OP_ASSERT(!m_reencrypt_context && "This method is called by SyncEncryptionKeyReencryptContext::DecryptWithPassword() which shall reset m_reencrypt_context before calling this method!");

	OP_STATUS status;
	OpString decrypted_key, username, password;
	SyncEncryptionKey test;
	status = test.SetEncryptedEncryptionKey(key);
	if (OpStatus::IsSuccess(status))
		status = m_sync_coordinator->GetLoginInformation(username, password);
	UINT8 decrypted_bin[16];	// ARRAY OK 2011-02-14 markuso
	if (OpStatus::IsSuccess(status))
		status = test.Decrypt(decrypted_key, decrypted_bin, username, old_password);
	op_memset(decrypted_bin, 0, 16);

	if (OpStatus::IsSuccess(status))
	{
		status = m_encryption_key.SetEncryptionKey(decrypted_key, username, password);
		if (OpStatus::IsSuccess(status))
		{	/* On success store the decrypted key in wand and send the newly
			 * encrypted encryption-key to the Link server: */
			OpStatus::Ignore(StoreEncryptionKeyInWand(decrypted_key, username));
			/* Note: we ignore the status of storing the encryption-key in
			 * wand. If we fail to store the encryption-key in wand, we still
			 * have the encryption-key available in non-persistent memory as
			 * long as the session lasts. And when we start a new session we
			 * first retrieve the encryption-key from the Link server. */

			OpStatus::Ignore(CommitEncryptionKeySyncItem(FALSE));
			/* Note: we can ignore an error in CommitEncryptionKeySyncItem()
			 * because this "only" means that the Link server does not have the
			 * correctly encrypted encryption-key. On the next start of Opera
			 * the wrongly encrypted encryption-key is received from the Link
			 * server, the Opera client cannot decrypt it, the client loads the
			 * key instead from wand and commits the item again to the Link
			 * server until this process is successful... */

			/* and notify anybody who may want to use the it: */
			NotifyEncryptionKey();
		}
		decrypted_key.Wipe();

		/* Call OnSyncReencryptEncryptionKeyCancel(0) to notify the UI that we
		 * could decrypt the encryption-key successfully with the old password
		 * that was entered by the user: */
		m_sync_coordinator->OnSyncReencryptEncryptionKeyCancel(0);
	}
	else if (OpStatus::ERR_OUT_OF_RANGE == status)
	{
		/* We now decrypted the encrypted encryption-key successfully, but it
		 * only contained invalid data. So delete the encryption-key; this will
		 * trigger to generate a new encryption-key on the next connection. */
		DeleteEncryptionKey(true);
		status = OpStatus::OK;

		/* Call OnSyncReencryptEncryptionKeyCancel(0) to notify the UI that we
		 * could decrypt the encryption-key successfully with the old password
		 * that was entered by the user (though the decrypted data was not
		 * usable): */
		m_sync_coordinator->OnSyncReencryptEncryptionKeyCancel(0);
	}
	else if (OpStatus::ERR_NO_ACCESS == status)
	{
		/* We could not decrypt the key, the specified old_password must be
		 * wrong, so notify the OpSyncUIListener with a new context: */
		OpAutoPtr<SyncEncryptionKeyReencryptContext> context(OP_NEW(SyncEncryptionKeyReencryptContext, (this)));
		if (context.get())
			status = context->SetEncryptedKey(key);
		else
			status = OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsSuccess(status))
			SetReencryptEncryptionKeyContext(context.release());
	}

	// Be paranoid to wipe the memory that contained the Opera Account password:
	password.Wipe();

	if (OpStatus::IsError(status))
		// If there was some error, inform the OpSyncUIListener about this:
		m_sync_coordinator->OnSyncError(OpStatus::IsMemoryError(status) ? SYNC_ERROR_MEMORY : SYNC_ERROR, UNI_L(""));
}

OP_STATUS SyncEncryptionKeyManager::ProcessSyncItemKey(OpSyncItem* item)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ProcessSyncItemKey()", "sync.encryption");
	OP_ASSERT(item->GetType() == OpSyncDataItem::DATAITEM_ENCRYPTION_KEY);
	switch (item->GetStatus())
	{
	case OpSyncDataItem::DATAITEM_ACTION_ADDED:
	case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
	{
		/* Stop waiting for the encryption-key - even if we cannot load or
		 * decrypt the received key. If we don't stop waiting here,
		 * OnSyncFinished() may generate a new key, overwriting the
		 * encryption-key on the server... */
		StopWaitingForEncryptionKey();

		/* If there was an old ReencryptEncryptionKeyContext instance open, we
		 * cancel that, because now that we received a new encryption-key we no
		 * longer need to re-encrypt the old encryption-key: */
		SetReencryptEncryptionKeyContext(0);

		// Clear the current encryption-key, it is no longer valid:
		m_encryption_key.Clear();

		OpString key;
		OP_STATUS status = item->GetData(OpSyncItem::SYNC_KEY_NONE, key);
		OP_DBG((UNI_L("key: '%s'"), key.CStr()));
		if (OpStatus::IsSuccess(status))
		{
			if (m_encryption_type == ENCRYPTION_TYPE_OPERA_ACCOUNT)
				status = ReceivedNewKey(key);
			else
				status = m_received_encryption_key.TakeOver(key);
		}
		return status;
	}

	case OpSyncDataItem::DATAITEM_ACTION_DELETED:
		OP_DBG(("OpSyncDataItem::DATAITEM_ACTION_DELETED"));
		/* If there was an old ReencryptEncryptionKeyContext instance open, we
		 * cancel that, because now that the encryption-key was deleted, we no
		 * longer need to re-encrypt the deleted encryption-key: */
		SetReencryptEncryptionKeyContext(0);

		DeleteEncryptionKey(true);
		return OpStatus::OK;

	default:
	case OpSyncDataItem::DATAITEM_ACTION_NONE:
		OP_DBG(("OpSyncDataItem::DATAITEM_ACTION_NONE"));
		/* In any other case simply ignore this item... */
		return OpStatus::OK;
	}
}

OP_STATUS SyncEncryptionKeyManager::ReceivedNewKey(OpString& key)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ReceivedNewKey()", "sync.encryption");

	OpString decrypted_key, username;
	OP_STATUS status;
	{
		/* Try to decrypt the specified encryption key. If we cannot decrypt it,
		 * we return an error status: */
		SyncEncryptionKey test;
		RETURN_IF_ERROR(test.SetEncryptedEncryptionKey(key));
		OpString password;
		RETURN_IF_ERROR(m_sync_coordinator->GetLoginInformation(username, password));
		UINT8 decrypted_key_bin[16];	// ARRAY OK 2011-02-11
		status = test.Decrypt(decrypted_key, decrypted_key_bin, username, password);
		op_memset(decrypted_key_bin, 0, 16);

		/* Be paranoid to wipe the memory that contained the Opera Account
		 * password: */
		password.Wipe();
	}

	if (OpStatus::IsSuccess(status))
	{	/* Only store the key if we could successfully decrypt the key: */
		status = m_encryption_key.SetEncryptedEncryptionKey(key);

#ifdef WAND_SUPPORT
		/* If status is OK, then decrypted_key should contain the decrypted
		 * encryption-key: store that key in wand: */
		if (OpStatus::IsSuccess(status))
			OpStatus::Ignore(StoreEncryptionKeyInWand(decrypted_key, username));
		/* Note: we ignore the status of storing the encryption-key in wand. If
		 * we fail to store the encryption-key in wand, we still have the
		 * encryption-key available in non-persistent memory as long as the
		 * session lasts. And when we start a new session we first retrieve the
		 * encryption-key from the Link server. */
#endif // WAND_SUPPORT

	}
	else if (OpStatus::ERR_NO_ACCESS == status ||
			 OpStatus::ERR_OUT_OF_RANGE == status)
	{	/* We could not decrypt the received encryption-key (ERR_NO_ACCESS) or
		 * the received encryption-key was not a valid hex-string
		 * (ERR_OUT_OF_RANGE). As we received a successful answer from the Link
		 * server with the available username,password pair we assume that
		 * either the credentials have been changed (ERR_NO_ACCESS) or the data
		 * on the server is somehow compromised. */
		OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context = 0;
		if (OpStatus::ERR_NO_ACCESS == status)
		{	/* If we could not decrypt the encrypted encryption-key, we want to
			 * notify the OpSyncUIListener when we don't have a local copy of
			 * the encryption-key in wand. Maybe the user can enter an old Opera
			 * Account password and thus decrypt the received encryption-key, or
			 * the user decides to delete all wand data and create a new
			 * encryption-key. Or we can wait that some other client has a local
			 * copy of the encryption-key which can be re-encrypted with the new
			 * credentials and sent to the Link server. Thus this client
			 * receives the encryption-key as well. */
			OpAutoPtr<SyncEncryptionKeyReencryptContext> context(OP_NEW(SyncEncryptionKeyReencryptContext, (this)));
			if (!context.get())
				status = OpStatus::ERR_NO_MEMORY;
			else
				status = context->TakeEncryptedKey(key);
			if (OpStatus::IsSuccess(status))
				reencrypt_context = context.release();
		}
		else
			status = OpStatus::OK;

#ifdef WAND_SUPPORT
		// So try to load the encryption-key from wand:
		if (OpStatus::IsSuccess(status))
			status = LoadEncryptionKeyFromWand(reencrypt_context);
		/* If the status is ERR_NO_SUCH_RESOURCE, then the encryption-key is not
		 * yet stored in wand. */
		if (OpStatus::ERR_NO_SUCH_RESOURCE == status)
#endif // WAND_SUPPORT
		{
			if (reencrypt_context)
			{	/* We could not decrypt the received encryption-key, so ask the
				 * user to provide the correct password: */
				SetReencryptEncryptionKeyContext(reencrypt_context);
				reencrypt_context = 0;
			}
			else
				/* We received an invalid encryption-key from the server, so
				 * generate a new encryption-key when OnSyncFinished() is
				 * called: */
				StartWaitingForEncryptionKey();
			status = OpStatus::OK;
			OP_DELETE(reencrypt_context);
		}
	}

	/* Be paranoid to wipe the memory that contained the decrypted key: */
	decrypted_key.Wipe();

	if (OpStatus::IsSuccess(status) && HasEncryptionKey())
		/* We received an encryption-key (and a supported type), so notify
		 * anybody who might want to use it: */
		NotifyEncryptionKey();
	return status;
}

OP_STATUS SyncEncryptionKeyManager::ProcessSyncItemType(OpSyncItem* item)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::ProcessSyncItemType()", "sync.encryption");

	OpString type;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_NONE, type));
	OP_DBG(("OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE: ") << type);
	enum EncryptionType new_type =
		(type.IsEmpty() || type == "opera_account")
		? ENCRYPTION_TYPE_OPERA_ACCOUNT : ENCRYPTION_TYPE_UNKNOWN;
	if (m_encryption_type == new_type)
		return OpStatus::OK;

	m_encryption_type = new_type;
	switch (m_encryption_type)
	{
	case ENCRYPTION_TYPE_OPERA_ACCOUNT:
		if (m_received_encryption_key.HasContent())
		{
			/* We received a key while the encryption-type was unknown, so try
			 * to handle the received key now. */
			RETURN_IF_ERROR(ReceivedNewKey(m_received_encryption_key));
			m_received_encryption_key.Empty();
		}
		else if (HasEncryptionKey())
			NotifyEncryptionKey();
		break;

	case ENCRYPTION_TYPE_UNKNOWN:
	default:
		/* If we received an unsupported encryption-type we disable password
		 * sync silently until we receive a supported encryption-type: */
		StopWaitingForEncryptionKey();
		m_encryption_type = ENCRYPTION_TYPE_UNKNOWN;

		/* If we have an encryption-key, pretend it was deleted, so nobody
		 * will use it any more. */
		if (HasEncryptionKey())
			NotifyEncryptionKeyDeleted();
		break;
	}
	return OpStatus::OK;
}

OP_STATUS SyncEncryptionKeyManager::CommitEncryptionKeySyncItem(BOOL is_dirty)
{
	OP_ASSERT(m_sync_coordinator);
	OP_ASSERT(HasEncryptionKey());

	OpString encrypted_key;
	RETURN_IF_ERROR(m_encryption_key.GetEncryptedEncryptionKey(encrypted_key));

	OP_STATUS status;
	OpSyncItem* sync_item = 0;
	status = m_sync_coordinator->GetSyncItem
		(&sync_item, OpSyncDataItem::DATAITEM_ENCRYPTION_KEY,
		 OpSyncItem::SYNC_KEY_ID, UNI_L("0"));
	if (OpStatus::IsSuccess(status))
		status = sync_item->SetData(OpSyncItem::SYNC_KEY_NONE, encrypted_key);
	if (OpStatus::IsSuccess(status))
		status = sync_item->SetStatus(OpSyncDataItem::DATAITEM_ACTION_ADDED);
	if (OpStatus::IsSuccess(status))
		status = sync_item->CommitItem(is_dirty, FALSE);
	m_sync_coordinator->ReleaseSyncItem(sync_item);
	encrypted_key.Wipe();
	return status;
}

void SyncEncryptionKeyManager::NotifyEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::NotifyEncryptionKey()", "sync.encryption");
	if (m_encryption_type == ENCRYPTION_TYPE_OPERA_ACCOUNT)
		for (unsigned int i=0; i<m_listener.GetCount(); ++i)
		{
			OP_ASSERT(m_listener.Get(i));
			m_listener.Get(i)->OnEncryptionKey(m_encryption_key);
		}
}

void SyncEncryptionKeyManager::NotifyEncryptionKeyDeleted()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::NotifyEncryptionKeyDeleted()", "sync.encryption");
	for (unsigned int i=0; i<m_listener.GetCount(); ++i)
	{
		OP_ASSERT(m_listener.Get(i));
		m_listener.Get(i)->OnEncryptionKeyDeleted();
	}
}

void SyncEncryptionKeyManager::DeleteEncryptionKey(bool delete_in_wand, OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::DeleteEncryptionKey()", "sync.encryption");
	m_encryption_key.Clear();
	m_received_encryption_key.Empty();
#ifdef WAND_SUPPORT
	if (delete_in_wand)
		DeleteEncryptionKeyInWand();
#endif // WAND_SUPPORT
	NotifyEncryptionKeyDeleted();

	/* If a SyncEncryptionKeyReencryptContext was specified, we need to
	 * activate it, i.e. notify the OpSyncUIListener. Then either the user
	 * enters the old password and we use the encryption-key from this context,
	 * or the user ignores this, or the user selects to delete all wand data and
	 * generate a new key. The last option results in calling
	 * SyncEncryptionKeyReencryptContext::CreateNewEncryptionKey(), which calls
	 * GenerateNewEncryptionKey(). */
	if (reencrypt_context)
		SetReencryptEncryptionKeyContext(reencrypt_context);
	else if (IsSupportEnabled() && !IsWaitingForEncryptionKey())
		/* Without a context we want to create a new encryption-key as soon as
		 * possible (since the encryption-key is deleted on the server and in
		 * wand). This is done by calling StartWaitingForEncryptionKey() (unless
		 * we already wait...): when the OpSyncCoordinator has finished handling
		 * all received items, OnSyncFinished() is called which then calls
		 * LoadEncryptionKey() and GenerateNewEncryptionKey() to generate a new
		 * key (after LoadEncryptionKeyFromWand() failed to find the
		 * encryption-key - because we deleted it). */
		StartWaitingForEncryptionKey();
}

void SyncEncryptionKeyManager::SetReencryptEncryptionKeyContext(OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SetReencryptEncryptionKeyContext()", "sync.encryption");
	if (m_reencrypt_context)
	{	/* Cancel any possible callback to the ReencryptEncryptionKeyContext
		 * before deleting it: */
		OpSyncUIListener::ReencryptEncryptionKeyContext* old_context = m_reencrypt_context;
		m_reencrypt_context = 0;
		m_sync_coordinator->OnSyncReencryptEncryptionKeyCancel(old_context);
		OP_DELETE(old_context);
	}
	m_reencrypt_context = reencrypt_context;
	if (m_reencrypt_context)
		m_sync_coordinator->OnSyncReencryptEncryptionKeyFailed(m_reencrypt_context);
}

OP_STATUS SyncEncryptionKeyManager::StoreEncryptionKeyInWand(const OpStringC& decrypted_key, const OpStringC& username) const
{
	OP_NEW_DBG("SyncEncryptionKeyManager::StoreEncryptionKeyInWand()", "sync.encryption");
	const uni_char* id_encryption_key = UNI_L("opera:encryption-key");
#ifdef SELFTEST
	if (m_testing)
		return m_testing->StoreLoginWithoutWindow(id_encryption_key, username.CStr(), decrypted_key.CStr());
	else
#endif // SELFTEST
	return g_wand_manager->StoreLoginWithoutWindow(id_encryption_key, username.CStr(), decrypted_key.CStr());
}

void SyncEncryptionKeyManager::DeleteEncryptionKeyInWand() const
{
	OP_NEW_DBG("SyncEncryptionKeyManager::DeleteEncryptionKeyInWand()", "sync.encryption");
	const uni_char* id_encryption_key = UNI_L("opera:encryption-key");
	OpString username, password;
	OpStatus::Ignore(m_sync_coordinator->GetLoginInformation(username, password));
	password.Wipe();
#ifdef SELFTEST
	if (m_testing)
		m_testing->DeleteLogin(id_encryption_key, username.CStr());
	else
#endif // SELFTEST
	g_wand_manager->DeleteLogin(id_encryption_key, username.CStr());
}

OP_STATUS SyncEncryptionKeyManager::LoadEncryptionKeyFromWand(OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::LoadEncryptionKeyFromWand()", "sync.encryption");
	const uni_char* id_encryption_key = UNI_L("opera:encryption-key");
	OpString username, password;
	OpStatus::Ignore(m_sync_coordinator->GetLoginInformation(username, password));
	password.Wipe();
	WandLogin* encryption_key_login = 0;
#ifdef SELFTEST
	if (m_testing)
		encryption_key_login = m_testing->FindLogin(id_encryption_key, username.CStr());
	else
#endif // SELFTEST
	encryption_key_login = g_wand_manager->FindLogin(id_encryption_key, username.CStr());
	if (0 == encryption_key_login)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	if (m_wand_callback_encryption_key)
	{	/* A different callback is already in progress of retrieving the
		 * password, so no need to start another one ... */
		m_wand_callback_encryption_key->UpdateReencryptEncryptionKeyContext(reencrypt_context);
		return OpStatus::OK;
	}

	WandLoginCallback* callback = OP_NEW(SyncEncryptionKeyWandLoginCallback, (this, reencrypt_context));
	if (!callback)
	{
		OP_DELETE(reencrypt_context);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status;
#ifdef SELFTEST
	if (m_testing)
		status = m_testing->GetLoginPasswordWithoutWindow(encryption_key_login, callback);
	else
#endif // SELFTEST
	status = g_wand_manager->GetLoginPasswordWithoutWindow(encryption_key_login, callback);
	if (OpStatus::IsError(status))
		OP_DELETE(callback);
	/* Note: in case of success, the callback instance is deleted when the
	 * g_wand_manager calls the callback methods. */
	return status;
}

OP_STATUS SyncEncryptionKeyManager::SetEncryptionKey(const OpStringC& decrypted_key)
{
	OP_NEW_DBG("SyncEncryptionKeyManager::SetEncryptionKey()", "sync.encryption");
	OpString username, password;
	RETURN_IF_ERROR(m_sync_coordinator->GetLoginInformation(username, password));
	/* First verify that the specified decrypted encryption-key is valid by
	 * encrypting and decrypting the data. Return OpStatus::ERR_OUT_OF_RANGE if
	 * decrypted_key is not 32 characters long... */
	if (decrypted_key.Length() != 32)
		return OpStatus::ERR_OUT_OF_RANGE;

	/* ... or if decrypted_key is not a valid hex-string (in that case
	 * SyncEncryptionKey::Decrypt() returns that error): */
	SyncEncryptionKey test;
	OP_STATUS status = test.SetEncryptionKey(decrypted_key, username, password);
	UINT8 decrypted_bin[16];	// ARRAY OK 2011-02-14 markuso
	if (OpStatus::IsSuccess(status))
		status = test.Decrypt(decrypted_bin, username, password);
	password.Wipe();
	op_memset(decrypted_bin, 0, 16);
	RETURN_IF_ERROR(status);

	/* On success we can use the encrypted encryption-key: */
	OpString encrypted_key;
	RETURN_IF_ERROR(test.GetEncryptedEncryptionKey(encrypted_key));
	return m_encryption_key.SetEncryptedEncryptionKey(encrypted_key);
}

void SyncEncryptionKeyManager::StartWaitingForEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::StartWaitingForEncryptionKey()", "sync.encryption");
	OP_ASSERT(IsSupportEnabled());
	OP_ASSERT(!HasEncryptionKey());
	if (m_sync_coordinator && !IsWaitingForEncryptionKey())
	{
		m_sync_coordinator->SetSyncUIListener(this);
		m_waiting_for_encryption_key = true;
	}
}

void SyncEncryptionKeyManager::StopWaitingForEncryptionKey()
{
	OP_NEW_DBG("SyncEncryptionKeyManager::StopWaitingForEncryptionKey()", "sync.encryption");
	if (m_sync_coordinator && IsWaitingForEncryptionKey())
		m_sync_coordinator->RemoveSyncUIListener(this);
	m_waiting_for_encryption_key = false;
}


#ifdef SELFTEST
// ==================== SyncEncryptionKeyManager::SelftestHelper

SyncEncryptionKeyManager::SelftestHelper::SelftestHelper(SyncEncryptionKeyManager* manager)
	: m_manager(manager)
	, m_call_callback_immediately(true)
	, m_callback(0)
{
	m_manager->m_testing = this;
}

SyncEncryptionKeyManager::SelftestHelper::~SelftestHelper()
{
	m_manager->m_testing = 0;
	if (m_callback)
		m_callback->OnPasswordRetrievalFailed();
	m_callback = 0;
}

OP_STATUS SyncEncryptionKeyManager::SelftestHelper::SetEncryptionKey(const OpStringC& encrypted_key, const OpStringC& decrypted_key, const OpStringC& username)
{
	RETURN_IF_ERROR(m_manager->m_encryption_key.SetEncryptedEncryptionKey(encrypted_key));
	m_manager->StopWaitingForEncryptionKey();
	return StoreLoginWithoutWindow(UNI_L("opera:encryption-key"), username.CStr(), decrypted_key);
}

OP_STATUS SyncEncryptionKeyManager::SelftestHelper::CallWandLoginCallback()
{
	if (m_callback)
	{
		m_callback->OnPasswordRetrieved(m_wand_password.CStr());
		m_callback = 0;
		return OpStatus::OK;
	}
	return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS SyncEncryptionKeyManager::SelftestHelper::StoreLoginWithoutWindow(const uni_char* id, const uni_char* username, const uni_char* password)
{
	RETURN_IF_ERROR(m_wand_id.Set(id));
	RETURN_IF_ERROR(m_wand_username.Set(username));
	return m_wand_password.Set(password);
}

void SyncEncryptionKeyManager::SelftestHelper::DeleteLogin(const uni_char* id, const uni_char* username)
{
	if (m_wand_id == id && m_wand_username == username) {
		m_wand_id.Empty();
		m_wand_username.Empty();
		m_wand_password.Empty();
	}
}

#ifdef WAND_SUPPORT
WandLogin* SyncEncryptionKeyManager::SelftestHelper::FindLogin(const uni_char* id, const uni_char* username) const
{
	if (m_wand_id == id && m_wand_username == username && m_wand_password.CStr())
		/* this pointer is used on the next call to
		 * GetLoginPasswordWithoutWindow() */
		return (WandLogin*)(0x0000deaf);
	else
		return 0; // no encryption-key stored
}

OP_STATUS SyncEncryptionKeyManager::SelftestHelper::GetLoginPasswordWithoutWindow(WandLogin* encryption_key_login, WandLoginCallback* callback)
{
	m_callback = callback;
	if (encryption_key_login == (WandLogin*)(0x0000deaf))
		if (m_call_callback_immediately)
			return CallWandLoginCallback();
		else
			return callback ? OpStatus::OK : OpStatus::ERR;
	else return OpStatus::ERR;
}
#endif // SELFTEST

#endif // WAND_SUPPORT

#endif // SYNC_ENCRYPTION_KEY_SUPPORT
