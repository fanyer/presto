/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file CryptoHash_impl.cpp
 *
 * This file implements the libcrypto hash algorithms towards the
 * openssl crypto library(modules/libopeay)
 */

#include "core/pch.h"
#include "modules/libcrypto/src/openssl_impl/CryptoHash_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/util/cleanse.h"

#ifdef CRYPTO_API_SUPPORT

/* static */ CryptoHash* CryptoHash::Create(CryptoHashAlgorithm algorithm)
{
	switch (algorithm)
	{
#ifdef CRYPTO_HASH_MD5_SUPPORT
		case CRYPTO_HASH_TYPE_MD5:    return CreateMD5();
#endif

#ifdef CRYPTO_HASH_SHA1_SUPPORT
		case CRYPTO_HASH_TYPE_SHA1:   return CreateSHA1();
#endif

#ifdef CRYPTO_HASH_SHA256_SUPPORT
		case CRYPTO_HASH_TYPE_SHA256: return CreateSHA256();
#endif
	}
	return NULL;
}

#endif // CRYPTO_API_SUPPORT


#ifdef CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

/* static */ CryptoHash * CryptoHash::CreateMD5()
{
	return OP_NEW(CryptoHashMD5, ());
}

CryptoHashMD5::~CryptoHashMD5()
{
	OPERA_cleanse(&m_md5_state, sizeof(MD5_CTX));
}

OP_STATUS CryptoHashMD5::InitHash()
{
	MD5_Init(&m_md5_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);

	return OpStatus::OK;
}

void CryptoHashMD5::CalculateHash(const UINT8 *source, int len)
{
	OP_ASSERT(source && len >= 0);

	MD5_Update(&m_md5_state, source, len);
	// The function just called doesn't add errors to OpenSSL error queue.

	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashMD5::CalculateHash(const char *source)
{
	OP_ASSERT(source);

	MD5_Update(&m_md5_state, source, op_strlen(source));

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashMD5::ExtractHash(UINT8 *result)
{
	OP_ASSERT(result);

	MD5_Final(result, &m_md5_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}
#endif // CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION


#ifdef CRYPTO_HASH_SHA1_USE_CORE_IMPLEMENTATION

CryptoHash *CryptoHash::CreateSHA1()
{
	return OP_NEW(CryptoHashSHA1, ());
}

CryptoHashSHA1::~CryptoHashSHA1()
{
	OPERA_cleanse(&m_sha1_state, sizeof(SHA_CTX));
}

OP_STATUS CryptoHashSHA1::InitHash()
{
	SHA1_Init(&m_sha1_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);

	return OpStatus::OK;
}

void CryptoHashSHA1::CalculateHash(const UINT8 *source, int len)
{
	OP_ASSERT(source && len >= 0);

	SHA1_Update(&m_sha1_state, source, len);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashSHA1::CalculateHash(const char *source)
{
	OP_ASSERT(source);

	SHA1_Update(&m_sha1_state, source, op_strlen(source));

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashSHA1::ExtractHash(UINT8 *result)
{
	OP_ASSERT(result);

	SHA1_Final(result, &m_sha1_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

#endif // CRYPTO_HASH_SHA1_USE_CORE_IMPLEMENTATION


#ifdef CRYPTO_HASH_SHA256_USE_CORE_IMPLEMENTATION

CryptoHash *CryptoHash::CreateSHA256()
{
	return OP_NEW(CryptoHashSHA256, ());
}

CryptoHashSHA256::~CryptoHashSHA256()
{
	OPERA_cleanse(&m_sha256_state, sizeof(SHA256_CTX));
}

OP_STATUS CryptoHashSHA256::InitHash()
{
	SHA256_Init(&m_sha256_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);

	return OpStatus::OK;
}

void CryptoHashSHA256::CalculateHash(const UINT8 *source, int len)
{
	OP_ASSERT(source && len >= 0);

	SHA256_Update(&m_sha256_state, source, len);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashSHA256::CalculateHash(const char *source)
{
	OP_ASSERT(source);

	SHA256_Update(&m_sha256_state, source, op_strlen(source));

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

void CryptoHashSHA256::ExtractHash(UINT8 *result)
{
	OP_ASSERT(result);

	SHA256_Final(result, &m_sha256_state);

	// The function just called doesn't add errors to OpenSSL error queue.
	OP_ASSERT(ERR_peek_error() == 0);
}

#endif // CRYPTO_HASH_SHA256_USE_CORE_IMPLEMENTATION
