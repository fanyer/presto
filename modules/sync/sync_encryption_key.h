/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SYNC_ENCRYPTION_KEY_H
#define SYNC_ENCRYPTION_KEY_H
#ifdef SYNC_ENCRYPTION_KEY_SUPPORT

#include "modules/sync/sync_coordinator.h"

class SyncEncryptionKeyWandLoginCallback;

/**
 * This class is used to store the encrypted encryption-key. The encryption-key
 * can be used to encrypt blobs for the communication between the client and the
 * Opera Link server. The encryption-key is encrypted with the Opera Account
 * username and password.
 *
 * To set the encrypted encryption-key call SetEncryptedEncryptionKey(). To set
 * the unencrypted encryption-key call SetEncryptionKey() - this encrypts the
 * encryption-key with the provided Opera Account credentials.
 *
 * To get the encrypted encryption-key call GetEncryptedEncryptionKey(). To get
 * the decrypted encryption-key call Decrypt().
 *
 * If no encryption-key is available, IsEmpty() is true.
 *
 * @note This class is paranoid about the memory that contained any Opera
 *  Account password or the encryption-key. Before freeing any internal copy of
 *  the password the password is overwritten with 0.
 */
class SyncEncryptionKey
{
public:
	SyncEncryptionKey() {}
	~SyncEncryptionKey() { Clear(); }

	/**
	 * Sets the encryption-key to the specified value. The unencrypted
	 * encryption-key is expected to be a 16 byte long binary sequence. This
	 * sequence is specified as a hex-string (of length 32 characters). The
	 * unencrypted encryption-key (as hex-string) is encrypted as a blob with
	 * the Opera Account username+password and stored in this instance.
	 * @param key is the unencrypted encryption-key as hex-string.
	 * @param username, password are the Opera Account credentials. The username
	 *  and password are used as the key to encrypt the encryption-key.
	 * @todo ensure that the old key remains if the operation fails...
	 * @see GetHash()
	 */
	OP_STATUS SetEncryptionKey(const OpStringC& key, const OpStringC& username, const OpStringC& password);

	/**
	 * Sets the encryption-key to the specified value.
	 * @param key is the encrypted encryption-key. The encryption-key is
	 *  encrypted as a blob with the Opera Account username+password.
	 * @todo ensure that the old key remains if the operation fails...
	 */
	OP_STATUS SetEncryptedEncryptionKey(const OpStringC& key);

	/**
	 * Exports the encrypted encryption-key to the specified argument.
	 * @param key receives on success the encrypted encryption-key.
	 */
	OP_STATUS GetEncryptedEncryptionKey(OpString& key) const {
		return key.Set(m_encryption_key_encrypted);
	}

	/** Clears the encryption-key. */
	void Clear();

	/** Returns true if the encryption-key is empty. */
	bool IsEmpty() const { return m_encryption_key_encrypted.IsEmpty() ? true : false; }

	/**
	 * Decrypts the encryption-key and stores the decrypted encryption-key in
	 * the specified string.
	 *
	 * @note This version of Decrypt() does not fail if the decrypted
	 *  encryption-key is not a hex-string of 32 characters. If you want to
	 *  verify this, use any of the other two Decrypt() methods.
	 *
	 * @param encryption_key receives on success the decrypted encryption-key as
	 *  hex-string. The hex-string will contain 32 characters and can thus be
	 *  converted to a 16 byte long binary key.
	 * @param username, password are the Opera Account credentials. The username
	 *  and password are used as the key to decrypt the encryption-key.
	 *
	 * @retval OpStatus::OK if the encrypted encryption-key was decrypted
	 *  successfully with the specified credentials. If the encrypted
	 *  encryption-key is empty (IsEmpty()), encryption_key will be empty and
	 *  OpStatus::OK is returned as well.
	 * @retval OpStatus::ERR_NO_ACCESS if the specified credentials cannot be
	 *  used to decrypt the encrypted encryption-key.
	 * @retval OpStatus::ERR or other error status in case of other errors.
	 *
	 * @see GetHash()
	 */
	OP_STATUS Decrypt(OpString& encryption_key, const OpStringC& username, const OpStringC& password) const;

	/**
	 * Decrypts the encryption-key and stores the decrypted encryption-key in
	 * the specified byte-array. The returned key can be passed to
	 * CryptoBlobEncryption::SetKey().
	 * @param encryption_key receives on success the decrypted encryption-key as
	 *  16 byte long array.
	 * @param username, password are the Opera Account credentials. The username
	 *  and password are used as the key to decrypt the encryption-key.
	 *
	 * @retval OpStatus::OK if the encrypted encryption-key was decrypted
	 *  successfully with the specified credentials.
	 * @retval OpStatus::ERR_NO_ACCESS if the specified credentials cannot be
	 *  used to decrypt the encrypted encryption-key.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the decrypted encryption-key is not
	 *  a valid hex-string of 32 characters and therefore cannot be converted to
	 *  the specified encryption_key array.
	 * @retval OpStatus::ERR or other error status in case of other errors.
	 *
	 * @see GetHash()
	 */
	OP_STATUS Decrypt(UINT8 encryption_key[16], const OpStringC& username, const OpStringC& password) const;

	/**
	 * Decrypts the encryption-key and stores the decrypted encryption-key in
	 * the specified string and byte-array.
	 * @param encryption_key_hex receives on success the decrypted
	 *  encryption-key as hex-string. The hex-string will contain 32 characters
	 *  which are equivalent to the 16 byte long encryption_key_bin array.
	 * @param encryption_key_bin receives on success the decrypted
	 *  encryption-key as 16 byte long array. The returned key can be passed to
	 * CryptoBlobEncryption::SetKey().
	 * @param username, password are the Opera Account credentials. The username
	 *  and password are used as the key to decrypt the encryption-key.
	 *
	 * @retval OpStatus::OK if the encrypted encryption-key was decrypted
	 *  successfully with the specified credentials.
	 * @retval OpStatus::ERR_NO_ACCESS if the specified credentials cannot be
	 *  used to decrypt the encrypted encryption-key.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the decrypted encryption-key is not
	 *  a valid hex-string of 32 characters and therefore cannot be converted to
	 *  the specified encryption_key array.
	 * @retval OpStatus::ERR or other error status in case of other errors.
	 *
	 * @see GetHash()
	 */
	OP_STATUS Decrypt(OpString& encryption_key_hex, UINT8 encryption_key_bin[16], const OpStringC& username, const OpStringC& password) const;

private:
	/**
	 * Decrypts the encryption-key and stores the decrypted encryption-key in
	 * the specified string.
	 *
	 * @param encryption_key receives on success the decrypted encryption-key.
	 * @param key is the key that was used to encrypt the encryption-key.
	 * @param key_size is the size of the specified key-array in bytes.
	 *
	 * @retval OpStatus::OK on success
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified key is too short to
	 *  be used as decryption key.
	 * @retval OpStatus::ERR or any other error if the decryption failed.
	 */
	OP_STATUS Decrypt(OpString& encryption_key, const UINT8* key, size_t key_size) const;

	/**
	 * Calculates the hash from the specified Opera Account username and
	 * password that is used to encrypt or decrypt the encryption-key.
	 *
	 * The hash is calculated as sha256(utf8(username)+':'+utf8(password)).
	 *
	 * @param hash receives on success the hash over the specified username and
	 *  password. The caller needs to free this memory using OP_DELETEA.
	 * @param hash_len receives on success the length of the hash in bytes.
	 * @param username, password are the Opera Account credentials.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR or any other error status - in this case the output
	 *  argument hash is either set to 0 or not modified.
	 */
	OP_STATUS GetHash(UINT8** hash, size_t* hash_len, const OpStringC& username, const OpStringC& password) const;

	/**
	 * This attribute stores the encrypted encryption-key.
	 */
	OpString m_encryption_key_encrypted;

#ifdef SELFTEST
	friend class SelftestHelper;
public:
	/**
	 * This class is declared as a friend of SyncEncryptionKey to be used to
	 * test the private methods of SyncEncryptionKey.
	 */
	class SelftestHelper
	{
		SyncEncryptionKey* m_sync_encryption_key;
	public:
		SelftestHelper(SyncEncryptionKey* sync_encryption_key)
			: m_sync_encryption_key(sync_encryption_key) {}
		OP_STATUS GetHash(UINT8** hash, size_t* hash_len, const OpStringC& username, const OpStringC& password) const {
			return m_sync_encryption_key->GetHash(hash, hash_len, username, password);
		}
	};
#endif // SELFTEST
};

/**
 * This class handles the communication of the encryption_key data between the
 * Link server and the client.
 *
 * @section Sync-state
 *
 * The sync-state for the "encryption" supports type is not persistent, as
 * opposed to other support types. I.e. the sync-state is not stored in the
 * preferences. This is done to let the client receive the encryption-key on the
 * first sync connection from the server. Thus the client can always verify if
 * the local copy of the encryption-key matches the encryption-key that is
 * stored on the server.
 *
 * @section receive_key Receiving the encryption-key from the Link server
 *
 * If the Link server has an encryption-key, it is sent in the first sync
 * response from the server. On parsing the response SyncDataAvailable() is
 * called with the OpSyncItem that holds the encryption-key. The received
 * encryption-key is then decrypted and stored in wand and in the member
 * m_encryption_key.
 *
 * If the Link server has no encryption-key, SyncDataAvailable() is not
 * called. But OnSyncFinished() is called (after all sync-items are handled). In
 * this case the SyncEncryptionKeyManager knows that there is no encryption-key
 * on the Link server. Now the SyncEncryptionKeyManager first tries to load the
 * encryption-key from wand (LoadEncryptionKey()). If there is no encryption-key
 * stored in wand, it generates a new encryption-key
 * (GenerateNewEncryptionKey()), stores it in wand and adds it to the queue of
 * items that is uploaded to the Link server in the next connection.
 *
 * The encryption-key that is stored on the Link server is encrypted with the
 * Opera Account username and password (see SyncEncryptionKey::Decrypt()).
 * So if the Link server sends an encryption-key, but the
 * SyncEncryptionKeyManager fails to decrypt the key, we have to assume that the
 * Opera Account credentials were changed. In that case this instance tries to
 * load the encryption-key from wand. If we can load the encryption-key from
 * wand, we can re-encrypt the encryption-key with the current Opera Account
 * credentials and add it to the queue of items that is uploaded to the Link
 * server in the next connection.
 */
class SyncEncryptionKeyManager
	: public OpSyncDataClient
	, public OpSyncUIListener
{
public:
	SyncEncryptionKeyManager();

	/**
	 * Unregisters itself from the OpSyncCoordinator instance (that was
	 * specified in InitL()) as OpSyncDataClient.
	 */
	virtual ~SyncEncryptionKeyManager();

	/**
	 * Initialises the SyncEncryptionKeyManager. The instance is registered as
	 * OpSyncDataClient for the type OpSyncDataItem::DATAITEM_ENCRYPTION_KEY in
	 * the specified OpSyncCoordinator instance.
	 * @param sync_coordinator is the OpSyncCoordinator in which to register
	 *  itself. The specified OpSyncCoordinator instance must not be deleted
	 *  before this SyncEncryptionKeyManager instance is deleted.
	 */
	void InitL(OpSyncCoordinator* sync_coordinator);

	/**
	 * @name Implementation of OpSyncDataClient
	 * @{
	 */

	/**
	 * This listener method is called the first time the encryption-key data is
	 * ready to be synchronised. This is evaluated by looking at the sync-state
	 * (that is read from the preferences at startup and updated on each sync).
	 * If the sync-state is "0", it is the first time, this data type is
	 * synchronised. However the sync-state for the "encryption" supports type
	 * is not stored in the prefs. So each session starts with a sync-state "0".
	 * This means that this method is called once for each session.
	 *
	 * As we want to receive the encryption-key from the Link Server in the
	 * first connection, there is nothing to do here.
	 *
	 * @note This method does not sent data, because OpSyncDataFlush() is called
	 *  immediately after this method.
	 *
	 * @param type the type of data that can be initialized for first
	 *  synchronisation. This is expected to be
	 *  OpSyncDataItem::DATAITEM_ENCRYPTION_KEY.
	 */
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);

	/**
	 * Called when a sync has completed successfully. The OpSyncCollection is
	 * only valid for the during of this call, so the listener should not keep a
	 * pointer around but handle the data right away. As the collection only
	 * contains the data type(s) the listener has registered to receive
	 * notification about, there should be at most one data-item of type
	 * OpSyncDataItem::DATAITEM_ENCRYPTION_KEY.
	 *
	 * If an OpSyncDataItem::DATAITEM_ENCRYPTION_KEY item is received, the
	 * encryption-key is updated.
	 *
	 * @param new_items collection of new items received from the server.
	 * @param data_error is  not modified by this method.
	 *
	 * @see ProcessSyncItemKey(),  ProcessSyncItemType()
	 */
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection* new_items, OpSyncDataError& data_error);

	/**
	 * Called when the sync module needs the listener to provide data to it.
	 * This might happen if the server has notified the sync module that a data
	 * type is "dirty" and needs a complete data set sent. This might also
	 * happen if the sync module detects that a new data type has been enabled
	 * that has previously not been synchronized with the server.
	 *
	 * This implementation adds an OpSyncItem with the encrypted encryption-key
	 * to the sync-queue if there is an encryption-key and either this is the
	 * first sync or a dirty sync. If this is not the first sync, then the Link
	 * server is expected to already have the encryption-key. In that case it is
	 * not necessary to send the key again to the server.
	 *
	 * @param type The type of data the listener must send to the module.  If
	 *	the listener is called due to a dirty request from the server, the data
	 *	type provided will be OpSyncDataItem::DATAITEM_GENERIC. Otherwise this
	 *	is expected to be OpSyncDataItem::DATAITEM_ENCRYPTION_KEY.
	 * @param first_sync TRUE if this is the first synchronisation with this
	 *	data type (sync state is 0).
	 * @param is_dirty If TRUE, the server requested that a "dirty" sync should
	 *	take place. This means that the OpSyncItem for the encryption-key is
	 *	added using OpSyncItem::CommitItem() with the dirty parameter set to
	 *	TRUE.
	 */
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);

	/**
	 * This listener method is called for a data type each time the data type is
	 * turned on or off though the supports method. Used by data only clients
	 * who do not use the supports method.
	 *
	 * @param supports The type of data that was enabled or disabled
	 * @param has_support State of the data type now
	 */
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);

	/** @} */ // Implementation of OpSyncDataClient

	/**
	 * @name Implementation of OpSyncUIListener
	 *
	 * This class needs to implement the OpSyncUIListener to detect if this
	 * instance needs to generate the encryption-key.
	 *
	 * @{
	 */

	virtual void OnSyncStarted(BOOL) {}
	virtual void OnSyncError(OpSyncError, const OpStringC&) {}

	virtual void OnSyncFinished(OpSyncState& sync_state);

	virtual void OnSyncReencryptEncryptionKeyFailed(ReencryptEncryptionKeyContext* context) {}
	virtual void OnSyncReencryptEncryptionKeyCancel(ReencryptEncryptionKeyContext* context) {}

	/** @} */ // Implementation of OpSyncUIListener

	/**
	 * This method can be called if the encryption-key is needed to encrypt some
	 * blob. As the encryption-key is retrieved from wand and wand may be
	 * protected with a master password, the encryption-key may not be available
	 * immediately. The caller of this method should register itself as an
	 * EncryptionKeyListener. If the encryption-key is loaded
	 * EncryptionKeyListener::OnEncryptionKey() is called and the caller can
	 * continue.
	 * @note If the encryption-key is available,
	 *  EncryptionKeyListener::OnEncryptionKey() may be called before this
	 *  method returns.
	 * @return OpStatus::OK if the process to load the encryption-key was
	 *  successfully started. An error status otherwise.
	 */
	OP_STATUS LoadEncryptionKey();

	/**
	 * Generates a new encryption-key. The new encryption-key is stored in wand
	 * and send to the Link server in the next connection.
	 */
	OP_STATUS GenerateNewEncryptionKey();

	/**
	 * Clear the encryption-key in memory and informs all EncryptionKeyListener
	 * instances about this. After the encryption-key is deleted, this instance
	 * starts waiting to receive a new copy of the encryption-key from the Link
	 * server (see StartWaitingForEncryptionKey()).
	 *
	 * @note The encryption-key is not deleted from wand. So if the Server does
	 *  not send the encryption-key in the next connection or if the received
	 *  encryption-key cannot be decrypted, it is loaded from wand (see
	 *  LoadEncryptionKey()).
	 *
	 * @note Calling this method is useful if the encrypted encryption-key can
	 *  no longer be accessed, e.g. because of an error on changing the Opera
	 *  Account credentials (see ReencryptEncryptionKey()).
	 */
	void ClearEncryptionKey();

	/**
	 * Re-encrypts the encrypted encryption-key. I.e. decrypt the encrypted
	 * encryption-key (that is stored in m_encryption_key) with the specified
	 * pair username/old_password and encrypt it again with the specified pair
	 * username/new_password. The newly encrypted encryption-key is then
	 * prepared to be sent to the Link server.
	 *
	 * This method may be called when the user has changed the Opera Account
	 * password and both, the old password, the new password and the
	 * encryption-key are available (e.g. when calling
	 * OpSyncCoordinator::SetLoginInformation() while we already have an
	 * encryption-key: HasEncryptionKey()).
	 *
	 * @note If one step of re-encrypting the encryption-key fails, the
	 *  encryption-key is cleared (see ClearEncryptionKey()) - though it remains
	 *  in wand. Thus the client requests the encryption-key from the Link
	 *  server on the next connection.
	 */
	OP_STATUS ReencryptEncryptionKey(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password);

	/**
	 * Re-encrypts the specified encrypted encryption-key. I.e. get the current
	 * Opera Account credentials (username/password pair) from the associated
	 * OpSyncCoordinator, decrypt the specified key with the pair
	 * username/old_password and encrypt it again with the pair
	 * username/password. The decrypted encryption-key is then stored in wand
	 * and the newly encrypted encryption-key is prepared to be sent to the Link
	 * server.
	 *
	 * This method is called when we received an encryption-key from Link that
	 * we cannot decrypt with the current Opera Account credentials. In that
	 * case the OpSyncUIListener is notified (see
	 * OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed()) and the user may
	 * be prompted to input the correct (old) password (for the same username).
	 *
	 * If decrypting the specified key fails, the OpSyncUIListener is notified
	 * again (OpSyncUIListener::OnSyncReencryptEncryptionKeyFailed()).
	 *
	 * @param key is the encrypted encryption-key which we now try to decrypt
	 *  with the specified old_password.
	 * @param old_password is the password entered by the user to decrypt the
	 *  specified key.
	 */
	void ReencryptEncryptionKey(const OpStringC& key, const OpStringC& old_password);

	/**
	 * This class can be used to be notified when an encryption-key is added or
	 * modified or deleted. This is e.g. useful for any OpSyncDataClient that
	 * wants to use the encryption-key to encrypt or decrypt some data.
	 *
	 * When a new or modified encryption-key is received from the Link server,
	 * OnEncryptionKey() is called. When the encryption-key is delete,
	 * OnEncryptionKeyDeleted() is called.
	 *
	 * To add an EncryptionKeyListener, call
	 * SyncEncryptionKeyManager::AddEncryptionKeyListener(). The listener should
	 * remove itself by calling
	 * SyncEncryptionKeyManager::RemoveEncryptionKeyListener() when it is no
	 * longer interested in updates about the encryption-key or when it is
	 * deleted.
	 */
	class EncryptionKeyListener {
	public:
		virtual ~EncryptionKeyListener() {}

		/**
		 * The encryption-key was added or modified.
		 * @param key contains the new encryption-key.
		 */
		virtual void OnEncryptionKey(const SyncEncryptionKey& key) = 0;

		/**
		 * The encryption-key was deleted.
		 */
		virtual void OnEncryptionKeyDeleted() = 0;
	};

	/**
	 * Adds the specified listener instance to the list of listeners that are
	 * notified about updates on the encryption-key. If a new encryption-key is
	 * available EncryptionKeyListener::OnEncryptionKey() is called on the
	 * specified listener instance. If the encryption-key is deleted,
	 * EncryptionKeyListener::OnEncryptionKeyDeleted() is called.
	 * @param listener is the listener instance to add.
	 * @retval OpStatus::OK if the listener was added successfully.
	 */
	OP_STATUS AddEncryptionKeyListener(EncryptionKeyListener* listener) {
		OP_ASSERT(listener);
		return m_listener.Add(listener);
	}

	/**
	 * Removes the specified listener instance from the list of listeners that
	 * are notified about updates on the encryption-key.
	 * @param listener is the listener instance to remove.
	 * @retval OpStatus::OK if the listener was removed successfully.
	 */
	OP_STATUS RemoveEncryptionKeyListener(EncryptionKeyListener* listener) {
		OP_ASSERT(listener);
		return m_listener.RemoveByItem(listener);
	}

	/**
	 * Returns the encryption-key instance.
	 */
	const SyncEncryptionKey& GetEncryptionKey() const {
		return m_encryption_key; }

	/**
	 * Returns true if support for synchronising the encryption-key is enabled.
	 * This is the case if SyncDataSupportsChanged() was called with a true
	 * has_support argument.
	 */
	bool IsSupportEnabled() const { return m_support_enabled; }

	/**
	 * Returns true, if this instance has an encryption-key.
	 */
	bool HasEncryptionKey() const { return !m_encryption_key.IsEmpty(); }

	/**
	 * Returns true, if this instance has an encryption-key, support for
	 * synchronising the encryption-key is enabled and the encryption-key is
	 * usable, i.e. a supported encryption-type is set.
	 */
	bool IsEncryptionKeyUsable() const {
		return HasEncryptionKey() &&
			IsSupportEnabled() &&
			m_encryption_type == ENCRYPTION_TYPE_OPERA_ACCOUNT; }

	/**
	 * Called by the specified ReencryptEncryptionKeyContext instance to
	 * indicate, that the context is done and that it can be deleted. This
	 * method sets the attribute m_reencrypt_context to 0. The caller can then
	 * delete the context instance.
	 *
	 * There shall only be one active ReencryptEncryptionKeyContext, which is
	 * the instance stored in the member attribute m_reencrypt_context.
	 */
	void OnReencryptEncryptionKeyContextDone(ReencryptEncryptionKeyContext* context) {
		OP_ASSERT(context == m_reencrypt_context);
		m_reencrypt_context = 0;
	}

private:
	/**
	 * Called by SyncDataAvailable() for each OpSyncItem of type
	 * OpSyncDataItem::DATAITEM_ENCRYPTION_KEY that is available.
	 *
	 * If the item's status is OpSyncDataItem::DATAITEM_ACTION_ADDED or
	 * OpSyncDataItem::DATAITEM_ACTION_MODIFIED this method tries to decrypt the
	 * encrypted encryption-key with the currently available Opera Account
	 * credentials. If that succeeds, the encrypted encryption-key is store in
	 * m_encryption_key and the decrypted encryption-key is stored in wand.
	 *
	 * If we cannot decrypt the encrypted encryption-key we try to load the
	 * encryption-key from wand. If we cannot load it from wand we notify the UI
	 * listener with a new SyncEncryptionKeyReencryptContext because the user
	 * may have changed the Opera Account password and may be able to enter the
	 * old Opera Account password to access the key and re-encrypt it with the
	 * current Opera Account password.
	 *
	 * If the item's status is OpSyncDataItem::DATAITEM_ACTION_DELETED we call
	 * DeleteEncryptionKey() to delete the encryption-key.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_PARSING_FAILED if the encrypted encryption-key in
	 *  the specified OpSyncItem is not a valid encryption-key, i.e. it cannot
	 *  be decrypted with the Opera Account username+password.
	 * @retval OpStatus::ERR or any other error to indicate failure.
	 *
	 * @see StartWaitingForEncryptionKey(), StopWaitingForEncryptionKey()
	 * @see ProcessSyncItemType()
	 */
	OP_STATUS ProcessSyncItemKey(OpSyncItem* item);

	/**
	 * Called by ProcessSyncItemKey when we received an encryption-key
	 * sync-item. The encrypted encryption-key is decrypted with the current
	 * Opera Account username+password and the decrypted encryption-key is
	 * stored in wand. All EncryptionKeyListener instances are notified about
	 * the new key.
	 *
	 * If we cannot decrypt the encrypted encryption-key we try to load the
	 * encryption-key from wand. If we cannot load it from wand we notify the UI
	 * listener with a new SyncEncryptionKeyReencryptContext because the user
	 * may have changed the Opera Account password and may be able to enter the
	 * old Opera Account password to access the key and re-encrypt it with the
	 * current Opera Account password.
	 *
	 * @param key is the received encrypted encryption-key. This method may
	 *  consume the string.
	 */
	OP_STATUS ReceivedNewKey(OpString& key);

	/**
	 * Called by SyncDataAvailable() for each OpSyncItem of type
	 * OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE that is available.
	 *
	 * The member m_encryption_type is set to the item's content, i.e. to the
	 * data of OpSyncItem::SYNC_KEY_NONE, which describes the encryption-type
	 * that is used with any encryption-key. If the content is empty or the
	 * value is "opera_account", then the encryption-key is expected to be
	 * encrypted with the Opera Account username+password. Any other type is
	 * currently unsupported.
	 *
	 * @retval OpStatus::OK on success.
	 * @see ProcessSyncItemKey()
	 */
	OP_STATUS ProcessSyncItemType(OpSyncItem* item);

	/**
	 * Creates an OpSyncItem with type OpSyncDataItem::DATAITEM_ENCRYPTION_KEY
	 * and status OpSyncDataItem::DATAITEM_ACTION_ADDED, stores the current
	 * encrypted encryption-key and commits the item. Thus the item will be send
	 * to the Link server on the next connection.
	 *
	 * This method expects to have an encryption-key (HasEncryptionKey() is
	 * true).
	 *
	 * @param is_dirty If TRUE, the server requested that a "dirty" sync should
	 *	take place. This means that the OpSyncItem for the encryption-key is
	 *	added using OpSyncItem::CommitItem() with the dirty parameter set to
	 *	TRUE.
	 */
	OP_STATUS CommitEncryptionKeySyncItem(BOOL is_dirty);

	/**
	 * Notifies all registered EncryptionKeyListener instance that the
	 * encryption-key was modified or added.
	 * @see EncryptionKeyListener::OnEncryptionKey()
	 */
	void NotifyEncryptionKey();

	/**
	 * Notifies all registered EncryptionKeyListener instance that the
	 * encryption-key was deleted.
	 * @see EncryptionKeyListener::OnEncryptionKeyDeleted()
	 */
	void NotifyEncryptionKeyDeleted();

	/**
	 * Deletes the encryption-key from memory and from wand and informs all
	 * EncryptionKeyListener instances about this. After the encryption-key is
	 * deleted, this instance either starts waiting to receive a new copy of the
	 * encryption-key from the Link server (see StartWaitingForEncryptionKey())
	 * or activates the specified
	 * OpSyncUIListener::ReencryptEncryptionKeyContext (see
	 * SetReencryptEncryptionKeyContext()).
	 *
	 * This method is called by ProcessSyncItemKey() when the encryption_key has
	 * status "deleted".
	 *
	 * @param delete_in_wand If this argument is true, the encryption-key is
	 *  also removed from wand. This means that a new encryption-key is
	 *  generated (see GenerateNewEncryptionKey()) if the Link server does not
	 *  send an encryption-key in the next connection.
	 *
	 *  The argument shall be set to false, if the encryption-key in wand is
	 *  still valid. This means the encryption-key from wand is loaded (see
	 *  LoadEncryptionKey()) if the Link server does not send an encryption-key
	 *  in the next connection.
	 * @param reencrypt_context If this context is 0, this instance starts
	 *  waiting to receive a new copy of the encryption-key from the Link server
	 *  (see StartWaitingForEncryptionKey()). If this context is not 0, the
	 *  context is activated (see SetReencryptEncryptionKeyContext()).
	 */
	void DeleteEncryptionKey(bool delete_in_wand, OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context=0);

	/**
	 * Returns true if this instance is currently waiting to receive the
	 * encryption-key from the Link server.
	 * @see StartWaitingForEncryptionKey(), StopWaitingForEncryptionKey()
	 */
	bool IsWaitingForEncryptionKey() const { return m_waiting_for_encryption_key; }

	/**
	 * Starts waiting for the next sync connection to receive the encryption-key
	 * from the Link server.
	 *
	 * If on the next sync connection the Link server sends an encryption-key,
	 * SyncDataAvailable() is called, the encryption-key is stored (see
	 * SetEncryptionKey()) and we can stop waiting for the encryption-key
	 * (StopWaitingForEncryptionKey()).
	 *
	 * If the Link server has no encryption-key, SyncDataAvailable() is not
	 * called and OnSyncFinished() is called. Then this instance stops waiting
	 * for the encryption-key (StopWaitingForEncryptionKey()) and tries to load
	 * the encryption-key from wand (LoadEncryptionKey()) or generates it.
	 *
	 * This method shall only be called if this instance has no encryption-key
	 * (i.e. !HasEncryptionKey() is false) and if support for encryption is
	 * enabled (i.e. IsSupportEnabled()); otherwise we may end up generating a
	 * new encryption-key while an old one is already in use.
	 *
	 * This method is called when encryption support is enabled (see
	 * SyncDataSupportsChanged()).
	 * @see StopWaitingForEncryptionKey()
	 */
	void StartWaitingForEncryptionKey();

	/**
	 * Removes this instance from the associated OpSyncCoordinator's list of
	 * OpSyncUIListener instances. This method can be called
	 * - on destruction;
	 * - or when it receives the encryption-key from the Link server (in
	 *   SyncDataAvailable()), then it no longer needs to wait for
	 *   OnSyncFinished();
	 * - or when the first sync connection of the session was completed
	 *   successfully (in OnSyncFinished()) without receiving the
	 *   encryption-key.
	 * @see StartWaitingForEncryptionKey()
	 */
	void StopWaitingForEncryptionKey();

	OpSyncCoordinator* m_sync_coordinator;
	bool m_support_enabled;

	enum EncryptionType {
		/** Indicates encryption support where the encryption-key is encrypted
		 * with the Opera-Account password and the password sync-items are
		 * encrypted with the encryption-key. */
		ENCRYPTION_TYPE_OPERA_ACCOUNT,
		/** Unsupported encryption type. */
		ENCRYPTION_TYPE_UNKNOWN
	};

	/**
	 * The current encryption-type. The encryption-type is set on receiving a
	 * sync-item of type OpSyncDataItem::DATAITEM_ENCRYPTION_TYPE. If the value
	 * is empty or "opera_account", the type is set to
	 * ENCRYPTION_TYPE_OPERA_ACCOUNT. Otherwise the type is set to
	 * ENCRYPTION_TYPE_UNKNOWN.
	 *
	 * - If the type is ENCRYPTION_TYPE_UNKNOWN password sync is not possible.
	 * - If the type is ENCRYPTION_TYPE_OPERA_ACCOUNT, the encryption-key is
	 *   encrypted with the Opera-Account password and the password sync-items
	 *   are encrypted with the encryption-key.
	 *
	 * Future versions may define other types of encryption support where e.g. a
	 * secondary password is used to encrypt some data.
	 */
	enum EncryptionType m_encryption_type;

	/**
	 * This member is true while this instance waits to receive the
	 * encryption-key from the Link server. The initial value is true and when
	 * the result of the first sync connection (of this session) arrives, it
	 * either brings an encryption-key (in that case SyncDataAvailable() is
	 * called with the encryption-key from the Link server) or there is no
	 * encryption-key on the server (in that case OnSyncFinished() is called
	 * without any previous call to SyncDataAvailable()).
	 *
	 * In both cases this attribute is set to false (because we are no longer
	 * waiting) and this instance will no longer listen for further
	 * OpSyncUIListener events. In the first case the encryption-key is stored
	 * (see EncryptionKey()). In the second case we either load the
	 * encryption-key from wand or generate a new encryption-key (if there is no
	 * encryption-key in wand; see ...).
	 */
	bool m_waiting_for_encryption_key;
	SyncEncryptionKey m_encryption_key;

	/**
	 * If we received a encryption-type sync item with an unsupported type, we
	 * don't handle any incoming encryption-keys. Instead the encryption-key is
	 * stored here. Thus it can be handled when we receive a supported
	 * encryption-type.
	 */
	OpString m_received_encryption_key;

	OpVector<EncryptionKeyListener> m_listener;

	/**
	 * This context is used when this instance cannot decrypt the encrypted
	 * encryption-key that was received in a sync-item (see
	 * ProcessSyncItemKey()) and cannot load the encryption-key from wand (see
	 * LoadEncryptionKeyFromWand()) and thus needs feedback from the user.
	 *
	 * If the context is not used, this pointer is 0.
	 */
	OpSyncUIListener::ReencryptEncryptionKeyContext* m_reencrypt_context;

	/**
	 * If there is an open OpSyncUIListener::ReencryptEncryptionKeyContext the
	 * OpSyncUIListener is notified that the request is cancelled (see
	 * OpSyncCoordinator::OnSyncReencryptEncryptionKeyCancel()) and the context
	 * is deleted.
	 *
	 * If a new OpSyncUIListener::ReencryptionKeyReencryptContext is specified,
	 * then the UI is notified (see
	 * OpSyncCoordinator::OnSyncReencryptEncryptionKeyFailed()) and
	 * m_reencrypt_context is set to the new instance.
	 *
	 * An OpSyncCoordinator::ReencryptEncryptionKeyContext is created when we
	 * receive an encrypted encryption-key (in ProcessSyncItemKey()) which we
	 * cannot decrypt. The context is created to allow the user to respond to
	 * that use-case.
	 *
	 * When we receive a new encryption-key or when the encryption-key is
	 * deleted (from the Link server, see ProcessSyncItemKey()) or when the user
	 * credentials are changed (see OpSyncCoordinator::SetLoginInformation() and
	 * ClearEncryptionKey()) we need to cancel such a request.
	 *
	 * @param reencrypt_context is the new
	 *  OpSyncUIListener::ReencryptionKeyReencryptContext to start or 0 to
	 *  cancel any current context without starting a new one.
	 * @see m_reencrypt_context
	 */
	void SetReencryptEncryptionKeyContext(OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context);

#ifdef WAND_SUPPORT
	/**
	 * Stores the specified decrypted encryption-key in wand.
	 *
	 * The encryption-key is stored in wand as the password of a login with the
	 * id "opera:encryption-key" and the Opera Account username.
	 *
	 * @param decrypted_key is the decrypted encryption-key to store.
	 * @param username is the Opera Account username.
	 * @return OpStatus::OK if the key was successfully stored in wand.
	 */
	OP_STATUS StoreEncryptionKeyInWand(const OpStringC& decrypted_key, const OpStringC& username) const;

	/**
	 * Deletes the encryption-key in wand.
	 */
	void DeleteEncryptionKeyInWand() const;

	/**
	 * Loads the decrypted encryption-key from wand.
	 *
	 * The encryption-key is stored as the password of a login with the id
	 * "opera:encryption-key" and an empty username.
	 *
	 * @param reencrypt_context May contain an
	 *  OpSyncUIListener::ReencryptEncryptionKeyContext instance. If this method
	 *  returns OpStatus::ERR_NO_SUCH_RESOURCE then the caller remains owner of
	 *  the context. Otherwise (on success or any other error) this method (or
	 *  the generated SyncEncryptionKeyWandLoginCallback instance took ownership
	 *  of the context and are responsible for deleting it.
	 *
	 * @retval OpStatus::OK if the encryption-key is located in wand and a
	 *  SyncEncryptionKeyWandLoginCallback is created to handle loading the
	 *  encryption-key from wand. If a reencrypt_context was specified then the
	 *  SyncEncryptionKeyWandLoginCallback has taken ownership of the context.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if there is no such key stored in
	 *  wand. If a reencrypt_context was specified then the caller has the
	 *  responsibility to delete it.
	 * @retval OpStatus::ERR or any other error status in case of other errors.
	 *  If a reencrypt_context was specified, then it was deleted by this
	 *  method and the caller shall not use it any more.
	 */
	OP_STATUS LoadEncryptionKeyFromWand(OpSyncUIListener::ReencryptEncryptionKeyContext* reencrypt_context);

	SyncEncryptionKeyWandLoginCallback* m_wand_callback_encryption_key;

	/* Declare SyncEncryptionKeyWandLoginCallback as friend to allow
	 * the callback access to the member m_encryption_key and
	 * m_wand_callback_encryption_key.
	 */
	friend class SyncEncryptionKeyWandLoginCallback;
#endif // WAND_SUPPORT

	/**
	 * Sets the encryption-key in attribute m_encryption_key to the specified
	 * value if it is a valid encryption-key. A valid encryption-key is a
	 * hex-string of length 32. I.e. the hex-string can be converted to a 16
	 * byte long binary array.
	 *
	 * @param decrypted_key is the decrypted encryption-key. This is expected to
	 *  be a hex-string of length 32.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the specified value is not a valid
	 *  encryption-key.
	 * @retval OpStatus::ERR or other errors in case of other failures.
	 */
	OP_STATUS SetEncryptionKey(const OpStringC& decrypted_key);

#ifdef SELFTEST
	friend class SelftestHelper;
public:
	/**
	 * This class is declared as a friend of SyncEncryptionKey to be used to
	 * test the private methods of SyncEncryptionKey.
	 */
	class SelftestHelper
	{
		SyncEncryptionKeyManager* m_manager;
		OpString m_wand_id;
		OpString m_wand_username;
		OpString m_wand_password;
		bool m_call_callback_immediately;
		class WandLoginCallback* m_callback;
	public:
		SelftestHelper(SyncEncryptionKeyManager* manager);
		~SelftestHelper();

		/**
		 * Helper function to set some key in the SyncEncryptionKeyManager and
		 * the wand emulation. Calling this method also sets the associated
		 * SyncEncryptionKeyManager to not waiting for the encryption key (see
		 * SyncEncryptionKeyManager::StopWaitingForEncryptionKey()).
		 */
		OP_STATUS SetEncryptionKey(const OpStringC& encrypted_key, const OpStringC& decrypted_key, const OpStringC& username);

		/**
		 * When the SyncEncryptionKeyManager tries to load the encryption-key
		 * from wand, the wand-emulation may call the WandLoginCallback
		 * immediately (when it is asked) or delayed. The default is to call the
		 * callback immediately. The real wand implementation does this e.g. if
		 * the wand data is not protected by a master password, or the master
		 * password is already known.
		 * This method sets a flag to not call it immediately. The caller needs
		 * to call CallWandLoginCallback(). This is used to test the use-case
		 * where the user needs to enter the master password.
		 */
		void DelayWandLoginCallback() { m_call_callback_immediately = false; }
		OP_STATUS CallWandLoginCallback();
		/**
		 * Helper function to access the private method
		 * SyncEncryptionKeyManager::IsWaitingForEncryptionKey().
		 */
		bool IsWaitingForEncryptionKey() const { return m_manager->IsWaitingForEncryptionKey(); }

		/**
		 * @name Emulate wand
		 *
		 * These methods are used to emulate wand. The SyncEncryptionKeyManager
		 * will call these functions (instead of the similar g_wand_manager
		 * functions) if the SyncEncryptionKeyManager is associated with a
		 * SelftestHelper instance. Thus we avoid modifying wand data on running
		 * selftests.
		 *
		 * @{
		 */
		OP_STATUS StoreLoginWithoutWindow(const uni_char* id, const uni_char* username, const uni_char* password);
		void DeleteLogin(const uni_char* id, const uni_char* username);
#ifdef WAND_SUPPORT
		class WandLogin* FindLogin(const uni_char* id, const uni_char* username) const;
		OP_STATUS GetLoginPasswordWithoutWindow(WandLogin* encryption_key_login, class WandLoginCallback* callback);
#endif // WAND_SUPPORT

		/** Returns the current value of the encryption-key that is stored in
		 * the wand emulation. */
		const OpStringC& WandPassword() const { return m_wand_password; }
		/** @} */
	};
private:
	/**
	 * To be able to test storing the encryption-key in wand and retrieving it
	 * from wand, the SelftestHelper class sets this attribute to itself, when
	 * it is associated to this instance. Then StoreEncryptionKeyInWand() stores
	 * the encryption-key in the SelftestHelper instance (instead of wand). Thus
	 * we avoid overwriting existing wand data with the selftest data.
	 */
	SelftestHelper* m_testing;
#endif // SELFTEST
};

#endif // SYNC_ENCRYPTION_KEY_SUPPORT
#endif // SYNC_ENCRYPTION_KEY_H
