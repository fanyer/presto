/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#ifdef CRYPTO_BLOB_ENCRYPTION_SUPPORT // api CRYPTO_BLOB_ENCRYPTION_SUPPORT
#include "modules/libcrypto/include/CryptoBlob.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/include/CryptoUtility.h"

CryptoBlobEncryption::CryptoBlobEncryption()
	: m_aes_cbc(NULL)
	, m_sha_256(NULL)
	, m_fixed_salt(NULL)
	, m_fixed_salt_is_secret(FALSE)
{
}

CryptoBlobEncryption::~CryptoBlobEncryption()
{
	OP_DELETEA(m_fixed_salt);
	OP_DELETE(m_sha_256);
	OP_DELETE(m_aes_cbc);

}

/* static */
CryptoBlobEncryption *CryptoBlobEncryption::Create(int aes_key_size)
{
	CryptoBlobEncryption *blob = OP_NEW(CryptoBlobEncryption, ());
	if (blob)
	{
		blob->m_aes_cbc = CryptoStreamEncryptionCBC::Create(CryptoSymmetricAlgorithm::CreateAES(aes_key_size));
		blob->m_sha_256 = CryptoHash::CreateSHA256();

		if (!blob->m_aes_cbc || !blob->m_sha_256)
		{
			OP_DELETE(blob);
			return NULL;
		}
	}
	return blob;
}

int CryptoBlobEncryption::CalculateBlobLength(int length)
{
	 return (m_fixed_salt_is_secret ? 0 : 8) /* salt */
		 + 32 /* hash */
		 + 2 /* store length as two bytes*/
		 + length /* message len*/
		 + (16 - (length + 2) % 16) % 16 /* padding */;
}


OP_STATUS CryptoBlobEncryption::SetFixedSalt(const UINT8 *salt, BOOL fixed_salt_is_secret)
{
	if (!salt && fixed_salt_is_secret)
		return OpStatus::ERR;

	if (salt)
	{
		if (!m_fixed_salt)
		{
			m_fixed_salt = OP_NEWA(UINT8, 8);
			if (!m_fixed_salt)
				return OpStatus::ERR_NO_MEMORY;
		}

		op_memcpy(m_fixed_salt, salt, 8);
	}
	else
	{
		OP_DELETEA(m_fixed_salt);
		m_fixed_salt = NULL;
	}

	m_fixed_salt_is_secret = fixed_salt_is_secret;
	return OpStatus::OK;
}

OP_STATUS CryptoBlobEncryption::Encrypt(const UINT8 *message, int length, UINT8 *&target, int &target_length)
{
	if (!message)
		return OpStatus::ERR;

	target = NULL;
	target_length = 0;

	if (length >= 65536 || length < 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	int temp_target_length = CalculateBlobLength(length);
	UINT8 *temp_target = OP_NEWA(UINT8, temp_target_length);
	if (!temp_target)
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY(UINT8, temp_target);

	int pad_length = (16 - (length + 2) % 16) % 16;

	int temp_length = 2 + length + pad_length;
	UINT8 *temp = OP_NEWA(UINT8, temp_length);
	if (!temp)
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY(UINT8, temp);

	// Create temp = length | message | padding
	temp[0] = (length >> 8) & 0xff;
	temp[1] = length & 0xff;
	op_memcpy(temp + 2, message, length);
	op_memset(temp + 2 + length, 0, pad_length);

	UINT8 salt_and_zeros[16];			// ARRAY OK 2011-02-14 markuso
	if (m_fixed_salt)
		op_memcpy(salt_and_zeros, m_fixed_salt, 8);
	else
		g_libcrypto_random_generator->GetRandom(salt_and_zeros, 8); // create salt in the first 8 bytes

	op_memset(salt_and_zeros + 8, 0, 8); // we must hash salt + 8 zero bytes

	if (!m_fixed_salt_is_secret)
		op_memcpy(temp_target, salt_and_zeros, 8); // the salt is prepended on the first 8 bytes, if not secret

	OP_ASSERT(m_sha_256->Size() == 32);
	UINT8 hash[32];						// ARRAY OK 2011-02-14 markuso
	RETURN_IF_ERROR(m_sha_256->InitHash());
	m_sha_256->CalculateHash(temp, temp_length); // hash length | message | padding
	m_sha_256->CalculateHash(salt_and_zeros, 8); // hash the salt
	m_sha_256->ExtractHash(hash);

	int salt_length = m_fixed_salt_is_secret ? 0 : 8;

	m_aes_cbc->SetIV(salt_and_zeros);
	m_aes_cbc->Encrypt(hash, temp_target + salt_length, 32); // encrypts the hash, 32 bytes
	m_aes_cbc->Encrypt(temp, temp_target + salt_length + 32, temp_length); // encrypts the length | message | pad;

	target = temp_target;
	ANCHOR_ARRAY_RELEASE(temp_target);

	target_length = temp_target_length;

	OP_ASSERT(target_length - salt_length >= 48 && target_length - salt_length < 65536+48 && (target_length - salt_length) % 16 == 0);
	return OpStatus::OK;
}

OP_STATUS CryptoBlobEncryption::EncryptBase64(const uni_char *plain_text, OpString8 &result_blob_base_64)
{
	if (!plain_text)
		plain_text = UNI_L("\0"); // Force creating an empty blob for NULL

	UINT8 *blob;
	int blob_length;

	OpString8 plain_text_utf8;
	RETURN_IF_ERROR(plain_text_utf8.SetUTF8FromUTF16(plain_text));

	// We do not encrypt the terminating '\0'. We will set that when decrypting for safety.
	RETURN_IF_ERROR(Encrypt(reinterpret_cast<const UINT8*>(plain_text_utf8.CStr()), uni_strlen(plain_text), blob, blob_length));
	ANCHOR_ARRAY(UINT8, blob);

	return CryptoUtility::ConvertToBase64(blob, blob_length, result_blob_base_64);
}

OP_STATUS CryptoBlobEncryption::EncryptBase64(const uni_char *plain_text, OpString &result_blob_base_64)
{
	OpString8 result;
	RETURN_IF_ERROR(EncryptBase64(plain_text, result));
	return result_blob_base_64.Set(result);
}

OP_STATUS CryptoBlobEncryption::EncryptBase64(const char *plain_text, OpString8 &result_blob_base_64)
{
	if (!plain_text)
		plain_text = "\0";

	UINT8 *blob;
	int blob_length;

	// We do not encrypt the terminating '\0'. We will set that when decrypting for safety.
	RETURN_IF_ERROR(Encrypt(reinterpret_cast<const UINT8*>(plain_text), op_strlen(plain_text), blob, blob_length));
	ANCHOR_ARRAY(UINT8, blob);

	return CryptoUtility::ConvertToBase64(blob, blob_length, result_blob_base_64);
}

OP_STATUS CryptoBlobEncryption::EncryptBase64(const char *plain_text, OpString &result_blob_base_64)
{
	OpString8 result;
	RETURN_IF_ERROR(EncryptBase64(plain_text, result));
	return result_blob_base_64.Set(result);
}

OP_STATUS CryptoBlobEncryption::Decrypt(const UINT8 *blob, int length, UINT8 *&target, int &target_length)
{
	if (!blob)
		return OpStatus::ERR;

	target = NULL;
	target_length = 0;

	int salt_length = m_fixed_salt_is_secret ? 0 : 8;
	int message_length = length - salt_length;

	/* 48 (the encrypted blob + salt_length (the length of prepended salt) is
	 * the minimal blob length (zero message length).
	 *
	 * The message part must be a multiple of 16 since we use CBC encryption
	 * mode and block length 16.
	 */
	if (message_length < 48   || message_length >= 65536 + 48  || (message_length) % 16 != 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	UINT8 *decrypted = OP_NEWA(UINT8, message_length);
	if (!decrypted)
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY(UINT8, decrypted);

	UINT8 salt_and_zeros[16];			// ARRAY OK 2011-02-14 markuso
	if (m_fixed_salt && m_fixed_salt_is_secret)
		op_memcpy(salt_and_zeros, m_fixed_salt, 8);
	else if (!m_fixed_salt_is_secret)
		op_memcpy(salt_and_zeros, blob, 8);
	else
		return OpStatus::ERR;


	op_memset(salt_and_zeros + 8, 0, 8);

	m_aes_cbc->SetIV(salt_and_zeros);
	m_aes_cbc->Decrypt(blob + salt_length, decrypted, message_length);

	RETURN_IF_ERROR(m_sha_256->InitHash());
	m_sha_256->CalculateHash(decrypted + 32, message_length - 32);
	m_sha_256->CalculateHash(salt_and_zeros, 8);

	OP_ASSERT(m_sha_256->Size() == 32);

	UINT8 hash[32];						// ARRAY OK 2011-02-14 markuso
	m_sha_256->ExtractHash(hash);

	target_length = decrypted[32] << 8 | decrypted[33];

	if (op_memcmp(hash, decrypted, 32))
		return OpStatus::ERR_NO_ACCESS; // wrong key has been used

	if (CalculateBlobLength(target_length) != length)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Copy message_length, the rest is padding.
	op_memmove(decrypted, decrypted + 34, target_length);

	target = decrypted;
	ANCHOR_ARRAY_RELEASE(decrypted);

	return OpStatus::OK;
}

OP_STATUS CryptoBlobEncryption::DecryptBase64(const char *blob_base_64, OpString &result_plain_text)
{
	if (!blob_base_64)
		return OpStatus::ERR;

	UINT8 *plain_binary;
	int plain_binary_length;

	RETURN_IF_ERROR(DecryptBase64(blob_base_64, plain_binary, plain_binary_length));

	ANCHOR_ARRAY(UINT8, plain_binary);

	// The terminating '\0' will be set by Opstring.
	return result_plain_text.SetFromUTF8(reinterpret_cast<char*>(plain_binary), plain_binary_length);
}

OP_STATUS CryptoBlobEncryption::DecryptBase64(const uni_char *blob_base_64, OpString &result_plain_text)
{
	if (!blob_base_64)
		return OpStatus::ERR;

	OpString8 blob;
	// The blob should only contain ascii characters, as it is encoded in base64.
	RETURN_IF_ERROR(blob.Set(blob_base_64));
	return DecryptBase64(blob, result_plain_text);
}

OP_STATUS CryptoBlobEncryption::DecryptBase64(const char *blob_base_64, OpString8 &result_plain_text)
{
	if (!blob_base_64)
		return OpStatus::ERR;

	UINT8 *plain_binary;
	int plain_binary_length;

	RETURN_IF_ERROR(DecryptBase64(blob_base_64, plain_binary, plain_binary_length));
	ANCHOR_ARRAY(UINT8, plain_binary);
	// The terminating '\0' will be set Opstring.
	return result_plain_text.Set(reinterpret_cast<char*>(plain_binary), plain_binary_length);
}

OP_STATUS CryptoBlobEncryption::DecryptBase64(const uni_char *blob_base_64, OpString8 &result_plain_text)
{
	if (!blob_base_64)
		return OpStatus::ERR;

	OpString8 blob;
	// The blob should only contain ascii characters, as it is encoded in base64.
	RETURN_IF_ERROR(blob.Set(blob_base_64));
	return DecryptBase64(blob, result_plain_text);

}

OP_STATUS CryptoBlobEncryption::DecryptBase64(const char *blob_base_64, UINT8 *&message, int &message_length)
{
	if (!blob_base_64)
		return OpStatus::ERR;

	UINT8 *blob_binary;
	int blob_binary_length;
	RETURN_IF_ERROR(CryptoUtility::ConvertFromBase64(blob_base_64, blob_binary, blob_binary_length));
	ANCHOR_ARRAY(UINT8, blob_binary);

	return Decrypt(blob_binary, blob_binary_length, message, message_length);
}
#endif // CRYPTO_BLOB_ENCRYPTION_SUPPORT
