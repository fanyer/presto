/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file CryptoSymmetricAlgorithmAES.cpp
 *
 * This file implements the libcrypto AES algorithm towards the
 * openssl crypto library(modules/libopeay)
 */

#include "core/pch.h"

#ifdef CRYPTO_ENCRYPTION_AES_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/src/openssl_impl/CryptoSymmetricAlgorithmAES.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/util/cleanse.h"


CryptoSymmetricAlgorithmAES::CryptoSymmetricAlgorithmAES(int key_size)
	: m_key_size(key_size)
{
}

CryptoSymmetricAlgorithmAES::~CryptoSymmetricAlgorithmAES()
{
	OPERA_cleanse(&m_key, sizeof(AES_KEY));
}

CryptoSymmetricAlgorithm *CryptoSymmetricAlgorithm::CreateAES(int key_size)
{
	return OP_NEW(CryptoSymmetricAlgorithmAES, (key_size ? key_size : 16));
}
	
void CryptoSymmetricAlgorithmAES::Encrypt(const UINT8 *source, UINT8 *target)
{
	OP_ASSERT(source && target);

	AES_encrypt(static_cast<const unsigned char *>(source), static_cast<unsigned char *>(target), &m_key);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoSymmetricAlgorithmAES::Decrypt(const UINT8 *source, UINT8 *target)
{
	OP_ASSERT(source && target);

	AES_decrypt(static_cast<const unsigned char *>(source), static_cast<unsigned char *>(target), &m_key);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}
	
OP_STATUS CryptoSymmetricAlgorithmAES::SetEncryptKey(const UINT8 *key)
{
	if (!key)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (AES_set_encrypt_key(static_cast<const unsigned char*>(key), m_key_size * 8, &m_key) >= 0)
		return OpStatus::OK;	
	else
	{
		// The function just called doesn't add errors to OpenSSL error queue.
		OP_ASSERT(ERR_peek_error() == 0);
		return OpStatus::ERR;
	}
}

OP_STATUS CryptoSymmetricAlgorithmAES::SetDecryptKey(const UINT8 *key)
{
	if (!key)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (AES_set_decrypt_key(static_cast<const unsigned char*>(key), m_key_size * 8, &m_key) >= 0)
		return OpStatus::OK;	
	else
	{
		// The function just called doesn't add errors to OpenSSL error queue.
		OP_ASSERT(ERR_peek_error() == 0);
		return OpStatus::ERR;
	}
}

#endif // CRYPTO_ENCRYPTION_AES_USE_CORE_IMPLEMENTATION
