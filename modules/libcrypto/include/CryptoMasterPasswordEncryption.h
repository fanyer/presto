/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef CRYPTOMASTERPASSWORDENCRYPTION_H_
#define CRYPTOMASTERPASSWORDENCRYPTION_H_

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"
#include "modules/libcrypto/include/CryptoHash.h"

class ObfuscatedPassword;

/** @class CryptoMasterPasswordEncryption.
 *
 * Port of libssl's password encryption algorithm. Used to encrypt passwords using a master password.
 *
 * @deprecated by haavardm, 2012-01-25. WARNING! This API is legacy code and should not be used by new code.
 * Currently used by libssl and wand.
 *
 * If password encryption is needed, please use CryptoBlobEncryption(modules/libcrypto/include/CryptoBlob.h) instead.
 *
 * See "modules/libcrypto/include/CryptoMasterPasswordHandler.h" for how to retrieve the master password from user.
 *
 * Usage:
 * @code
 *
 * // Assuming CryptoMasterPasswordEncryption *g_libcrypto_master_password_encryption exists.
 * // Assuming the master password is stored in ObfuscatedPassword *obfuscated_master_password;
 *
 * // The point of having the master password stored in obfuscated form is to avoid information leakage, as code outside
 * // libcrypto does not have access to the master password in clear.
 *
 * // Declaring some input data
 * const char *password_to_encrypt = "some password";
 *
 * // The actual encryption code
 * UINT8 *encrypted_password;
 * int encrypted_password_length;
 * g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword
 * (
 *      encrypted_password, encrypted_password_length,
 *      (const UINT8*)password_to_encrypt, op_strlen(password_to_encrypt),
 *      obfuscated_master_password
 *  );
 *
 * // The encrypted password is now stored in encrypted_password and the length is stored in encrypted_password_length.
 *
 * ANCHOR_ARRAY(UINT8, encrypted_password); // We anchor it since caller is responsible for deallocation.
 *
 * @endcode
 */
class CryptoMasterPasswordEncryption
{
public:
	CryptoMasterPasswordEncryption();

	/** Initiates the API.
	 *
	 * LEAVES with OpStatus::ERR_NO_MEMORY
	 */
	void InitL();

	virtual ~CryptoMasterPasswordEncryption();

	/**
	 * Port of SSL_API::EncryptWithSecurityPassword.
	 *
	 * Encryption of passwords using a master password.
	 *
	 * Outputs a formated blob containing an encrypted password
	 * and salt.
	 *
	 * This function will allocate the encrypted_password and caller
	 * is responsible for deallocating the encrypted_password with OP_DELETEA.
	 *
	 * @param encrypted_password[out]         The encrypted password.
	 * @param encrypted_password_length[out]  The length of the encrypted password.
	 * @param password_to_encrypt             The password to encrypt.
	 * @param password_to_encrypt_length      The length of the encrypted output.
	 * @param master_password                 The master password used to encrypt the password.
	 * @param master_password_length          The length of the master password.
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS EncryptPasswordWithSecurityMasterPassword(UINT8 *&encrypted_password, int &encrypted_password_length,
			const UINT8 *password_to_encrypt, int password_to_encrypt_len,
			const UINT8 *master_password, int master_password_length);

	/**
	 * Port of SSL_API::EncryptWithSecurityPassword.
	 *
	 * Same as above, but using obfuscated master password.
	 *
	 * Encryption of passwords using a master password.
	 *
	 * Outputs a formated blob containing an encrypted password
	 * and salt.
	 *
	 * This function will allocate the encrypted_password and
	 * caller is responsible for deallocating the encrypted_password with OP_DELETEA.
	 *
	 * @param encrypted_password[out]         The encrypted password.
	 * @param encrypted_password_length[out]  The length of the encrypted password.
	 * @param password_to_encrypt             The password to encrypt.
	 * @param password_to_encrypt_length      The length of the encrypted output.
	 * @param master_password                 The obfuscated master password.
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS EncryptPasswordWithSecurityMasterPassword(UINT8 *&encrypted_password, int &decrypted_password_length,
			const UINT8 *password_to_encrypt, int password_to_encrypt_len,
			const ObfuscatedPassword *master_password);

	/**
	 * Port of SSL_API::DecryptWithSecurityPassword.
	 *
	 * Decrypts a formated blob containing encrypted password
	 * and salt.
	 *
	 * This function will allocate the decrypted_password and caller
	 * is responsible for deallocating the decrypted_password password with OP_DELETEA.
	 *
	 * @param decrypted_password[out]        The decrypted password of length decrypted_password_length.
	 * @param decrypted_password_length[out] The length of the decrypted password
	 * @param password_blob                  The formatted blob to decrypt.
	 * @param password_blob_len              The length of the blob.
	 * @param out_len                        The length of the decrypted password.
	 * @param master_password                The master password.
	 * @param master_password_length         The length of the master password.
	 * @return OpStatus::ERR_NO_ACCESS if wrong password, OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS DecryptPasswordWithSecurityMasterPassword(UINT8 *&decrypted_password, int &decrypted_password_length,
			const UINT8 *password_blob, int password_blob_len,
			const UINT8* master_password, int master_password_length);

	/**
	 * Port of SSL_API::DecryptWithSecurityPassword.
	 *
	 * Same as above, but using obfuscated master password.
	 *
	 * Decrypts a formated blob containing encrypted password
	 * and salt.
	 *
	 * This function will allocate the decrypted_password and caller
	 * is responsible for deallocating the decrypted_password password with OP_DELETEA.
	 *
	 * @param decrypted_password[out]        The decrypted password of length decrypted_password_length.
	 * @param decrypted_password_length[out] The length of the decrypted password
	 * @param password_blob                  The formatted blob to decrypt.
	 * @param password_blob_len              The length of the blob.
	 * @param out_len                        The length of the decrypted password.
	 * @param master_password                The obfuscated master password.
	 * @return OpStatus::ERR_NO_ACCESS if wrong password, OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS DecryptPasswordWithSecurityMasterPassword(UINT8 *&decrypted_password, int &decrypted_password_length,
			const UINT8 *password_blob, int password_blob_len,
			const ObfuscatedPassword *master_password);

	/**
	 * Port of SSL_Options::EncryptWithPassword.
	 *
	 * Lower level function.
	 *
	 * Encrypts a password using a master password.
	 * Outputs a non-formatted encrypted password.
	 *
	 * This function will allocate the decrypted_password and caller
	 * is responsible for deallocating the decrypted_password password with OP_DELETEA.
	 *
	 * @param encrypted_data[out]        The encrypted password.
	 * @param encrypted_data_length[out] The length of the encrypted password.
	 * @param password                   The password to encrypt.
	 * @param password_length            The length of the password.
	 * @param [out] salt                 The generated salt of length 8. Must be pre-allocated.
	 * @param master_password            The master password.
	 * @param master_password_len        The length of the master password
	 * @param plainseed                  Seed used to generate the salt.
	 * @param plainseed_length           The length of the seed.
	 * @param out_length                 The length of the generated password.
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS EncryptWithPassword(UINT8 *&encrypted_data, int &encrypted_data_length,
			const UINT8 *password, int password_length,
			UINT8 *salt,
			const UINT8 *master_password, int master_password_len,
			const UINT8 *plainseed, int plainseed_length);

	/**
	 * Port of SSL_Options::DecryptWithPassword.
	 *
	 * Lower level function.
	 *
	 * Decrypts a non-formatted encrypted password.
	 *
	 * This function will allocate the decrypted_data and caller
	 * is responsible for deallocating the decrypted_data with OP_DELETEA.
	 *
	 * @param decrypted_data[out]        The decrypted password.
	 * @param decrypted_data_length[out] The length of the decrypted password.
	 * @param encrypted_data             The encrypted password.
	 * @param encrypted_data_len         The length of the encrypted password.
	 * @param salt                       The salt used to check correct decryption.
	 * @param salt_len                   The length of the salt.
	 * @param master_password            The master password used to encrypting the password.
	 * @param master_password_len        The length of the master password.
	 * @param plainseed                  The seed used to to check correct decryption.
	 * @param plainseed_length           The length of the plainseed.
	 * @param out_length                 The length of the decrypted password.
	 * @return OpStatus::ERR_NO_ACCESS if decrypted error, OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS DecryptWithPassword(UINT8 *&decrypted_data, int &decrypted_data_length, const UINT8 *encrypted_data, int encrypted_data_len, const UINT8 *salt, int salt_len, const UINT8 *master_password, int master_password_len, const UINT8 *plainseed, int plainseed_length);

	/**
	 * Creates a key and iv of given length from a stream of bytes.
	 * This stream of data might be a password given by user.
	 *
	 * Port of SSL_GeneralCipher::BytesToKey.
	 *
	 * Implements openssl's EVP_BytesToKey.
	 *
	 * From open ssl's documentation:
	 * "If the total key and IV length is less than the digest length
	 * and MD5 is used then the derivation algorithm is compatible with
	 * PKCS#5 v1.5 otherwise a non standard extension is used to derive
	 * the extra data."
	 *
	 * @param hash_alg             hash algorithm used.
	 * @param in_data              data used for key generation.
	 * @param in_data_length       length of id_data in bytes.
	 * @param salt                 salt used for key generation.
	 * @param salt_length          length of salt in bytes.
	 * @param hash_iteration_count Number of hash iterations used during generation.
	 * @param [out] out_key        The generated key.
	 * @param out_key_length       The length of the generated key.
	 * @param [out] out_iv         The generated IV.
	 * @param out_iv_length        The length of generated IV, set by caller.
	 * @return                     OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	static OP_STATUS BytesToKey(CryptoHashAlgorithm hash_alg, const UINT8 *in_data, int in_data_length, const UINT8 *salt, int salt_length, int hash_iteration_count, UINT8 *out_key, int out_key_length, UINT8 *out_iv, int out_iv_length);

private:

	OP_STATUS CalculateSalt(const UINT8 *in_data, int in_data_len, const UINT8 *password, int password_length, UINT8 *salt, int salt_len, const UINT8 *plainseed, int plainseed_length);
	OP_STATUS CalculatedKeyAndIV(const UINT8 *password, int password_length, const UINT8 *salt, int salt_len);

	class CryptoStreamEncryptionCBC *m_des_cbc;
};
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT
#endif /* CRYPTOMASTERPASSWORDENCRYPTION_H_ */
