/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */


#ifndef CRYPTO_BLOB_H_
#define CRYPTO_BLOB_H_

#ifdef CRYPTO_BLOB_ENCRYPTION_SUPPORT // import API_CRYPTO_BLOB_ENCRYPTION
#include "modules/libcrypto/include/CryptoStreamEncryptionCBC.h"
#include "modules/libcrypto/include/CryptoHash.h"

/**
 * Class for creating encrypted blobs as defined in
 * https://wiki.oslo.osa/developerwiki/Opera_Link/Protocol/Data_Types/Password_Manager
 *
 * The blob provides encryption with AES CBC mode, message integrity with SHA256
 * and AES CBC, and random salt to prevent against rainbow attacks.
 */

class CryptoBlobEncryption
{
public:
	/**
	 * Create CryptoBlobEncryption object.
	 *
	 * @param aes_key_size The key size in bytes for AES.
	 *                     Legal values are 16,24 or 32.
	 *
	 * @return valid CryptoBlobEncryption object, or NULL if memory error
	 */
	static CryptoBlobEncryption *Create(int aes_key_size = 16);

	/**
	 * Get the key size of the underlying cipher.
	 *
	 * @return the key size
	 */
	int GetKeySize() const { return m_aes_cbc->GetKeySize(); }

	/**
	 * Set the GetKeySize() bytes long key used for encryption/decryption.
	 *
	 * @param key The key of length GetKeySize().
	 *
	 * @retval OpStatus::OK  On success.
	 * @retval OpStatus::ERR If key is NULL.
	 */
	OP_STATUS SetKey(const UINT8 *key) { return m_aes_cbc->SetKey(key); }


	/** Set fixed salt (not recommended, but might be preferred in some cases).
	 *
	 * If fixed salt is not set(default), a random salt will be generated
	 * by Encrypt (recommended) and prepended to the blob.
	 *
	 * If fixed salt is set, the given salt will be prepended to the blob generated
	 * by Encrypt().
	 *
	 * If fixed_salt_is_secret is TRUE, the salt will not be prepended to the blob
	 * after encryption. Thus the blob cannot be decrypted without setting salt
	 * with SetFixedSalt(salt, TRUE). This can be done if both the encryptor and
	 * decryptor share a secret salt.
	 *
	 * If fixed_salt_is_secret is FALSE, the salt set here is ignored when decrypting
	 * since it's prepended to the given blob. Thus, there is no need
	 * to explicitly storing the fixed salt elsewhere, when sending the blob
	 * to the receiver provided the salt is not secret.
	 *
	 * @param fixed_salt            The 8 bytes long fixed salt. To unset call with NULL.
	 *                              Calling with NULL will also set the salt back to public.
	 * @param fixed_salt_is_secret  If TRUE, the salt is not prepended to the blob. To
	 *                              decrypt this blob, the decryptor MUST call this
	 *                              function with SetFixedSalt(salt, TRUE).
	 */
	OP_STATUS SetFixedSalt(const UINT8 *fixed_salt = NULL, BOOL fixed_salt_is_secret = FALSE);

	/**
	 * Create an encrypted blob from a plain message.
	 *
	 * @param      message        The message to encrypt
	 * @param      message_length The length of the message
	 * @param[out] blob           The encrypted blob.  The caller takes ownership
	 *                            and is responsible for deleting the array.
	 * @param[out] blob_length    The length of the blob
	 *
	 * @retval OpStatus::OK                Success
	 * @retval OpStatus::ERR_NO_MEMORY     OOM
	 * @retval OpStatus::ERR_OUT_OF_RANGE  If length of the message is out of range
	 *                                     Legal range: 0 <= message_length < 65536.
	 */
	OP_STATUS Encrypt(const UINT8 *message, int message_length, UINT8 *&blob, int &blob_length);

	/**
	 * Same as Encrypt, with 0-terminated string as plaintext and encoded
	 * base64 string as output.
	 *
	 * Note that the encrypted blob MUST be decrypted using DecryptBase64.
	 *
	 * @param      plain_text          0-terminated string.
	 * @param[out] result_blob_base_64 Encrypted blob in base64.
	 *
	 * @retval OpStatus::OK                Success
	 * @retval OpStatus::ERR_NO_MEMORY     OOM
	 * @retval OpStatus::ERR_OUT_OF_RANGE  If length of the message is out of range
	 *                                     Legal range: 0 <= message_length < 65536.
	 */
	OP_STATUS EncryptBase64(const uni_char *plain_text, OpString8 &result_blob_base_64);
	OP_STATUS EncryptBase64(const uni_char *plain_text, OpString &result_blob_base_64);
	OP_STATUS EncryptBase64(const char *plain_text, OpString8 &result_blob_base_64);
	OP_STATUS EncryptBase64(const char *plain_text, OpString &result_blob_base_64);

	/**
	 * Decrypt the blob and restore the message.
	 *
	 * @param      blob             The blob
	 * @param      length           The length of the blob. The minimal length
	 *                              of a blob is 56 bytes. Maximum is 65536+56.
	 * @param[out] message          The restored message. The caller takes ownership
	 *                              and is responsible for deleting the array.
	 * @param[out] message_length   The length of the message
	 *
	 * @retval OpStatus::OK                Success.
	 * @retval OpStatus::ERR_NO_MEMORY     OOM
	 * @retval OpStatus::ERR_OUT_OF_RANGE  If length of the blob is too small/big
	 *                                     OR if the decrypted message reports a
	 *                                     length that wouldn't fit in the blob.
	 * @retval OpStatus::ERR_NO_ACCESS     If decryption failed, due to tampering
	 *                                     of the blob, or using wrong key.
	 */
	OP_STATUS Decrypt(const UINT8 *blob, int length, UINT8 *&message, int &message_length);

	/**
	 * Same as Decrypt, with base 64 encoded 0-terminated string as encrypted blob,
	 * and OpString as output.
	 *
	 * Note that the blob MUST have been encrypted using EncryptBase64. Also
	 * you MUST use the same UTF encoding on the plaintext as used during encrypt.
	 *
	 * @param      blob_base_64       Base64 encoded and 0-terminated string.
	 * @param[out] result_plain_text  Decrypted result.
	 *
	 * @retval OpStatus::OK                Success.
	 * @retval OpStatus::ERR_NO_MEMORY     OOM
	 * @retval OpStatus::ERR_OUT_OF_RANGE  If length of the blob is too small/big
	 *                                     OR if the decrypted message reports a
	 *                                     length that wouldn't fit in the blob.
	 * @retval OpStatus::ERR_NO_ACCESS     If decryption failed, due to tampering
	 *                                     of the blob, or using wrong key.
	 */
	OP_STATUS DecryptBase64(const char *blob_base_64, OpString &result_plain_text);
	OP_STATUS DecryptBase64(const uni_char *blob_base_64, OpString &result_plain_text);
	OP_STATUS DecryptBase64(const char *blob_base_64, OpString8 &result_plain_text);
	OP_STATUS DecryptBase64(const uni_char *blob_base_64, OpString8 &result_plain_text);

	virtual ~CryptoBlobEncryption();

private:
	CryptoBlobEncryption();

	// Utility functions
	OP_STATUS ConvertToBase64(UINT8 *source, int source_length, OpString8 &base_64);
	OP_STATUS ConvertFromBase64(const char *source, UINT8 *&bytes, int& length);
	OP_STATUS DecryptBase64(const char *blob_base_64, UINT8 *&message, int &message_length);
	/**
	 * Calculates the length of the resulting blob when encrypting a message.
	 * of 'length' bytes.
	 */
	int CalculateBlobLength(int length);

	CryptoStreamEncryptionCBC *m_aes_cbc;

	CryptoHash *m_sha_256;

	UINT8 *m_fixed_salt;
	BOOL m_fixed_salt_is_secret;
};

#endif // CRYPTO_BLOB_ENCRYPTION_SUPPORT // api CRYPTO_BLOB_ENCRYPTION_SUPPORT
#endif /* CRYPTO_BLOB_H_ */
