/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
#include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"
#include "modules/libcrypto/include/CryptoStreamEncryptionCBC.h"
#include "modules/util/adt/bytebuffer.h"

#define SSL_EMAIL_PASSWORD_VERIFICATION "Opera Email Password Verification"

void CryptoMasterPasswordEncryption::InitL()
{
	m_des_cbc = CryptoStreamEncryptionCBC::Create(CryptoSymmetricAlgorithm::Create3DES(24), CryptoStreamEncryptionCBC::PADDING_STRATEGY_PCKS15);
	LEAVE_IF_NULL(m_des_cbc);
}

CryptoMasterPasswordEncryption::CryptoMasterPasswordEncryption()
	: m_des_cbc(NULL)
{
}

CryptoMasterPasswordEncryption::~CryptoMasterPasswordEncryption()
{
	OP_DELETE(m_des_cbc);
}

OP_STATUS CryptoMasterPasswordEncryption::EncryptPasswordWithSecurityMasterPassword(UINT8 *&encrypted_password_blob, int &encrypted_password_length,
		const UINT8 *password_to_encrypt, int password_to_encrypt_len, const UINT8 *master_password, int master_password_length)
{
	// Format: salt length data (4 bytes, network encoded) | salt | encrypted data length data (4 bytes, network encoded) | encrypted data
	encrypted_password_blob = NULL;

	int encrypted_data_length = m_des_cbc->CalculateEncryptedTargetLength(password_to_encrypt_len);
	encrypted_password_length = /*salt length */ 4 + /*salt*/ 8 + /*Encrypted password length */ 4 + /*Encrypted password*/ + encrypted_data_length;

	UINT8 *temp_encrypted_password_blob = OP_NEWA(UINT8, encrypted_password_length);
	RETURN_OOM_IF_NULL(temp_encrypted_password_blob);
	ANCHOR_ARRAY(UINT8, temp_encrypted_password_blob);


	UINT8 *salt_length_bytes = temp_encrypted_password_blob;
	UINT8 *salt = temp_encrypted_password_blob + 4;
	UINT8 *encrypted_data_length_bytes = temp_encrypted_password_blob + 12;
	UINT8 *encrypted_data = temp_encrypted_password_blob + 16;
	UINT8* encrypted_password;
	RETURN_IF_ERROR(EncryptWithPassword(encrypted_password, encrypted_data_length, password_to_encrypt, password_to_encrypt_len, salt, master_password, master_password_length, (const UINT8*)SSL_EMAIL_PASSWORD_VERIFICATION, op_strlen(SSL_EMAIL_PASSWORD_VERIFICATION)));

	op_memcpy(encrypted_data, encrypted_password, encrypted_data_length);
	OP_DELETEA(encrypted_password);

	/* puts the salt length and encrypted password length into the blob */
	ByteBuffer big_endian_salt_length;
	RETURN_IF_ERROR(big_endian_salt_length.Append4(8));
	big_endian_salt_length.Extract(0, big_endian_salt_length.Length(), (char*)salt_length_bytes);

	ByteBuffer big_endian_encrypted_password_length;
	RETURN_IF_ERROR(big_endian_encrypted_password_length.Append4((UINT32)encrypted_data_length));
	big_endian_encrypted_password_length.Extract(0, big_endian_salt_length.Length(), (char*)encrypted_data_length_bytes);

	ANCHOR_ARRAY_RELEASE(temp_encrypted_password_blob);
	encrypted_password_blob = temp_encrypted_password_blob;
	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordEncryption::EncryptPasswordWithSecurityMasterPassword(UINT8 *&encrypted_password, int &encrypted_password_length, const UINT8 *password_to_encrypt, int password_to_encrypt_len, const ObfuscatedPassword *master_password)
{
	OP_STATUS status;
	encrypted_password = 0;
	int master_password_length;
	if (!master_password)
		return OpStatus::ERR;

	UINT8* master_password_array = master_password->GetMasterPassword(master_password_length);
	if (!master_password_array)
		return OpStatus::ERR;

	ANCHOR_ARRAY(UINT8, master_password_array);

	status = EncryptPasswordWithSecurityMasterPassword(encrypted_password, encrypted_password_length, password_to_encrypt, password_to_encrypt_len, master_password_array, master_password_length);

	op_memset(master_password_array, 0, master_password_length);

	return status;
}

OP_STATUS CryptoMasterPasswordEncryption::DecryptPasswordWithSecurityMasterPassword(UINT8 *&decrypted_password, int &decrypted_password_length, const UINT8 *password_blob, int password_blob_len, const UINT8* password, int password_length)
{
	// Format: salt length data (4 bytes, network encoded) | salt | encrypted data length data (4 bytes, network encoded) | encrypted data
	decrypted_password = NULL;
	decrypted_password_length = 0;

	if (password_blob_len < 4 /* salt length data */ + 8 /* minimum salt length */ + 4 /* encrypted password length data */ + m_des_cbc->GetBlockSize())
		return OpStatus::ERR_PARSING_FAILED;

	int in_salt_length = password_blob[0] << 24 | password_blob[1] << 16 | password_blob[2] << 8 | password_blob[3];

	/* We cannot trust salt_length, must check it makes sense to avoid buffer overflow attacks. */
	if (in_salt_length > password_blob_len - 4 /* salt length data */ - 4 /* encrypted password length data */ - m_des_cbc->GetBlockSize() /* minimum encrypted data length */
	|| in_salt_length < 8)
		return OpStatus::ERR_PARSING_FAILED;

	const UINT8 *in_salt = password_blob + 4 /* salt length data */;
	const UINT8 *encrypted_data_length_data = in_salt + in_salt_length;
	int encrypted_data_length = encrypted_data_length_data[0] << 24 | encrypted_data_length_data[1] << 16 | encrypted_data_length_data[2] << 8 | encrypted_data_length_data[3];

	/* We cannot trust encrypted password length, must check it makes sense to avoid buffer overflow attacks. */
	if (encrypted_data_length > password_blob_len - 4 - 4 - in_salt_length || encrypted_data_length < m_des_cbc->GetBlockSize())
		return OpStatus::ERR_PARSING_FAILED;

	const UINT8 *encrypted_data = encrypted_data_length_data + 4 /*encrypted password length data */;

	return DecryptWithPassword(decrypted_password, decrypted_password_length, encrypted_data, encrypted_data_length, in_salt, in_salt_length, password, password_length, (const UINT8*)SSL_EMAIL_PASSWORD_VERIFICATION, op_strlen(SSL_EMAIL_PASSWORD_VERIFICATION));
}

OP_STATUS CryptoMasterPasswordEncryption::DecryptPasswordWithSecurityMasterPassword(UINT8 *&decrypted_password, int &decrypted_password_length, const UINT8 *password_blob, int password_blob_len, const ObfuscatedPassword *master_password)
{
	if (!master_password)
		return OpStatus::ERR;

	decrypted_password = NULL;
	decrypted_password_length = 0;

	int master_password_length;
	UINT8* master_password_array = master_password->GetMasterPassword(master_password_length);
	if (!master_password_array)
		return OpStatus::ERR;

	ANCHOR_ARRAY(UINT8, master_password_array);

	OP_STATUS status = OpStatus::OK;
	if (master_password_array)
		status = DecryptPasswordWithSecurityMasterPassword(decrypted_password, decrypted_password_length, password_blob, password_blob_len, master_password_array, master_password_length);

	op_memset(master_password_array, 0, master_password_length);

	return status;
}


OP_STATUS CryptoMasterPasswordEncryption::EncryptWithPassword(UINT8 *&encrypted_data, int &encrypted_data_length, const UINT8 *password_to_encrypt, int password_to_encrypt_len, UINT8 *salt, const UINT8 *master_password, int master_password_length, const UINT8 *plainseed, int plainseed_length)
{
	encrypted_data = NULL;
	encrypted_data_length = 0;
	int temp_encrypted_data_length = m_des_cbc->CalculateEncryptedTargetLength(password_to_encrypt_len);
	UINT8 *temp_encrypted_data = OP_NEWA(UINT8, temp_encrypted_data_length);
	RETURN_OOM_IF_NULL(temp_encrypted_data);

	ANCHOR_ARRAY(UINT8, temp_encrypted_data);

	RETURN_IF_ERROR(CalculateSalt(password_to_encrypt, password_to_encrypt_len, master_password, master_password_length, salt, 8, plainseed, plainseed_length));

	RETURN_IF_ERROR(CalculatedKeyAndIV(master_password, master_password_length, salt, 8));

	/* Puts the encrypted password into the blob. */
	m_des_cbc->Encrypt(password_to_encrypt, temp_encrypted_data, password_to_encrypt_len, temp_encrypted_data_length);

	ANCHOR_ARRAY_RELEASE(temp_encrypted_data);
	encrypted_data = temp_encrypted_data;
	encrypted_data_length = temp_encrypted_data_length;

	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordEncryption::DecryptWithPassword(UINT8 *&decrypted_data, int &decrypted_data_length, const UINT8 *encrypted_data, int encrypted_data_length, const UINT8 *in_salt, int in_salt_length, const UINT8 *password, int password_length, const UINT8 *plainseed, int plainseed_length)
{
	decrypted_data = NULL;
	decrypted_data_length = 0;
	UINT8 *temp_decrypted_data = OP_NEWA(UINT8, encrypted_data_length);
	ANCHOR_ARRAY(UINT8, temp_decrypted_data);

	UINT8 *calculated_salt = OP_NEWA(UINT8, in_salt_length);
	ANCHOR_ARRAY(UINT8, calculated_salt);

	if (!temp_decrypted_data || !calculated_salt)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(CalculatedKeyAndIV(password, password_length, in_salt, in_salt_length));

	m_des_cbc->Decrypt(encrypted_data, temp_decrypted_data, encrypted_data_length);

	int temp_decrypted_data_length = 0;
	if (OpStatus::IsError(m_des_cbc->CalculateDecryptedTargetLength(temp_decrypted_data_length, temp_decrypted_data, encrypted_data_length)))
		return OpStatus::ERR_NO_ACCESS;

	if (temp_decrypted_data_length)
	{
		/* Based on the decrypted data, we can re-calculate the salt, and check that it matches the salt given in in_data */
		RETURN_IF_ERROR(CalculateSalt(temp_decrypted_data, temp_decrypted_data_length, password, password_length, calculated_salt, in_salt_length, plainseed, plainseed_length));

		if (op_memcmp(calculated_salt, in_salt, in_salt_length) != 0)
			return OpStatus::ERR_NO_ACCESS;
	}

	ANCHOR_ARRAY_RELEASE(temp_decrypted_data);
	decrypted_data_length = temp_decrypted_data_length;
	decrypted_data = temp_decrypted_data;
	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordEncryption::BytesToKey(CryptoHashAlgorithm hash_alg, const UINT8 *in_data, int in_data_length, const UINT8 *salt, int salt_length, int hash_iteration_count, UINT8 *out_key, int out_key_length, UINT8 *out_iv, int out_iv_length)
{
	OpAutoPtr<CryptoHash> hasher(CryptoHash::Create(hash_alg));
	if (!hasher.get())
		return OpStatus::ERR_NO_MEMORY;

	int hasher_size = hasher->Size();
	UINT8 *previous_iteration_result = OP_NEWA(UINT8, hasher_size);

	ANCHOR_ARRAY(UINT8, previous_iteration_result);
	 if (!previous_iteration_result)
		return OpStatus::ERR_NO_MEMORY;

	int hash_count = 1;

	int current_key_data_generated_length = 0;
	int current_iv_data_generated_length = 0;

	while (current_key_data_generated_length + current_iv_data_generated_length  < out_key_length + out_iv_length)
	{

		for (int i = 0; i < hash_count; i++)
		{
			RETURN_IF_ERROR(hasher->InitHash());

			// Do not hash the result of previous iteration before actual having one.
			if (current_key_data_generated_length > 0 || current_iv_data_generated_length || i > 0)
				hasher->CalculateHash(previous_iteration_result, hasher_size);

			hasher->CalculateHash(in_data, in_data_length);
			hasher->CalculateHash(salt, salt_length);

			hasher->ExtractHash(previous_iteration_result);
		}

		int hash_index = 0;
		if (current_key_data_generated_length < out_key_length)
		{
			while (current_key_data_generated_length < out_key_length && hash_index < hasher_size)
				out_key[current_key_data_generated_length++] = previous_iteration_result[hash_index++];
		}

		if (current_key_data_generated_length >= out_key_length)
		{
			while (current_iv_data_generated_length < out_iv_length && hash_index < hasher_size)
				out_iv[current_iv_data_generated_length++] = previous_iteration_result[hash_index++];
		}
	}

	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordEncryption::CalculateSalt(const UINT8 *in_data, int in_data_len, const UINT8 *password, int password_length, UINT8 *salt, int salt_len, const UINT8 *plainseed, int plainseed_length)
{
	if (!salt)
		return OpStatus::ERR;

	OpAutoPtr<CryptoHash> sha(CryptoHash::CreateSHA1());
	RETURN_OOM_IF_NULL(sha.get());

	UINT8 digest_result[CRYPTO_MAX_HASH_SIZE]; /* ARRAY OK 2012-01-20 haavardm  */

	OP_ASSERT(salt_len <= sha->Size());

	RETURN_IF_ERROR(sha->InitHash());
	sha->CalculateHash(password, password_length);
	sha->CalculateHash(plainseed, plainseed_length);
	sha->CalculateHash(in_data, in_data_len);
	sha->ExtractHash(digest_result);

	/* puts salt into the blob */
	op_memcpy(salt, digest_result, salt_len); // only the 8 first bytes is used.

	return OpStatus::OK;
}


OP_STATUS CryptoMasterPasswordEncryption::CalculatedKeyAndIV(const UINT8 *password, int password_length, const UINT8 *salt, int salt_len)
{
	if (!password || !salt)
		return OpStatus::ERR;

	int out_key_len = m_des_cbc->GetKeySize();
	int out_iv_len = m_des_cbc->GetBlockSize();

	UINT8 *out_key = OP_NEWA(UINT8, out_key_len);
	RETURN_OOM_IF_NULL(out_key);
	ANCHOR_ARRAY(UINT8, out_key);

	UINT8 *out_iv = OP_NEWA(UINT8, out_iv_len);
	RETURN_OOM_IF_NULL(out_iv);
	ANCHOR_ARRAY(UINT8, out_iv);

	RETURN_IF_ERROR(BytesToKey(CRYPTO_HASH_TYPE_MD5, password, password_length, salt, salt_len, 1, out_key, out_key_len, out_iv, out_iv_len));

	m_des_cbc->SetIV(out_iv);
	return m_des_cbc->SetKey(out_key);
}
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT
