/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"

/**
 * @file CryptoStreamEncryptionRC4_impl.cpp
 *
 * This file implements RC4 algorithm towards the
 * openssl crypto library(modules/libopeay)
 */

#ifdef CRYPTO_STREAM_ENCRYPTION_RC4_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/src/openssl_impl/CryptoStreamEncryptionRC4_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"


/* static */ CryptoStreamEncryptionRC4 *CryptoStreamEncryptionRC4::CreateRC4(int key_len)
{
	return OP_NEW(CryptoStreamEncryptionRC4_impl, (key_len));
}

void CryptoStreamEncryptionRC4_impl::Encrypt(const UINT8 *source, UINT8 *target, int len)
{
	OP_ASSERT(source && target && len >= 0);

	RC4(&m_key, len, source, target);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoStreamEncryptionRC4_impl::Decrypt(const UINT8 *source, UINT8 *target, int len)
{
	OP_ASSERT(source && target && len >= 0);

	RC4(&m_key, len, source, target);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

OP_STATUS CryptoStreamEncryptionRC4_impl::Init(const UINT8 *key)
{
	if (!key)
		return OpStatus::ERR_OUT_OF_RANGE;

	RC4_set_key(&m_key, m_key_len, key);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);

	return OpStatus::OK;
}

CryptoStreamEncryptionRC4_impl::CryptoStreamEncryptionRC4_impl(int key_len)
	: m_key_len(key_len)
{
}

#endif // CRYPTO_STREAM_ENCRYPTION_RC4_USE_CORE_IMPLEMENTATION
