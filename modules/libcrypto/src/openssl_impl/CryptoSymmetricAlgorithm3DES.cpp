/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file CryptoSymmetricAlgorithm3DES.cpp
 *
 * This file implements the libcrypto 3DES algorithm towards the
 * openssl crypto library(modules/libopeay)
 */

#include "core/pch.h"

#ifdef CRYPTO_ENCRYPTION_3DES_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/src/openssl_impl/CryptoSymmetricAlgorithm3DES.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/util/cleanse.h"
#include "modules/libopeay/crypto/des/des_locl.h"
#include "modules/libopeay/openssl/des.h"


CryptoSymmetricAlgorithm3DES::CryptoSymmetricAlgorithm3DES(int key_size)
	: m_key_size(key_size)
{
}

CryptoSymmetricAlgorithm3DES::~CryptoSymmetricAlgorithm3DES()
{
	OPERA_cleanse(&m_key1, sizeof(DES_key_schedule));
	OPERA_cleanse(&m_key2, sizeof(DES_key_schedule));
	OPERA_cleanse(&m_key3, sizeof(DES_key_schedule));
}

CryptoSymmetricAlgorithm *CryptoSymmetricAlgorithm::Create3DES(int key_size)
{
	if (key_size != 16 && key_size != 24)
		return NULL;

	return OP_NEW(CryptoSymmetricAlgorithm3DES, (key_size));
}

void CryptoSymmetricAlgorithm3DES::Encrypt(const UINT8 *source, UINT8 *target)
{
	CryptFunction3DES(source, target, DES_ENCRYPT);
}

void CryptoSymmetricAlgorithm3DES::Decrypt(const UINT8 *source, UINT8 *target)
{
	CryptFunction3DES(source, target, DES_DECRYPT);
}

OP_STATUS CryptoSymmetricAlgorithm3DES::SetEncryptKey(const UINT8 *key)
{
	OP_ASSERT(m_key_size == 16 || m_key_size == 24);

	if (!key)
		return OpStatus::ERR;

	DES_cblock des_key1, des_key2, des_key3;

	op_memcpy(des_key1, key,8);
	op_memcpy(des_key2, key + 8, 8);

	DES_set_key_unchecked(&des_key1, &m_key1);
	DES_set_key_unchecked(&des_key2, &m_key2);

	if (m_key_size == 24)
	{
		op_memcpy(des_key3, key + 16, 8);
		DES_set_key_unchecked(&des_key3, &m_key3);
	}
	else
		DES_set_key_unchecked(&des_key1, &m_key3);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);

	return OpStatus::OK;
}

OP_STATUS CryptoSymmetricAlgorithm3DES::SetDecryptKey(const UINT8 *key)
{
	return SetEncryptKey(key);
}

void CryptoSymmetricAlgorithm3DES::CryptFunction3DES(const unsigned char *source, unsigned char *target, int direction)
{
	OP_ASSERT(source && target);

	DES_LONG l0,l1;
	DES_LONG ll[2];

	c2l(source,l0);
	c2l(source,l1);
	ll[0]=l0;
	ll[1]=l1;
	if (direction)
		DES_encrypt3(ll, &m_key1, &m_key2, &m_key3);
	else
		DES_decrypt3(ll, &m_key1, &m_key2, &m_key3);

	l0=ll[0];
	l1=ll[1];
	l2c(l0,target);
	l2c(l1,target);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

#endif // CRYPTO_ENCRYPTION_3DES_USE_CORE_IMPLEMENTATION
