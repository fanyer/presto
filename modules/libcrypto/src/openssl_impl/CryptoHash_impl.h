/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_HASH_IMPL_H
#define CRYPTO_HASH_IMPL_H

#include "modules/libcrypto/include/CryptoHash.h"


#ifdef CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION

#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/md5.h"


class CryptoHashMD5: public CryptoHash
{
public:
	CryptoHashMD5(){};
	virtual inline ~CryptoHashMD5();

	virtual inline OP_STATUS InitHash();
	virtual inline void CalculateHash(const UINT8 *source, int len);
	virtual inline void CalculateHash(const char *source);
	virtual inline int BlockSize() { return 64; }
	virtual inline int Size() { return MD5_DIGEST_LENGTH; }
	virtual inline void ExtractHash(UINT8 *result);

private:
	MD5_CTX	m_md5_state;
};

#endif // CRYPTO_HASH_MD5_USE_CORE_IMPLEMENTATION


#ifdef CRYPTO_HASH_SHA1_USE_CORE_IMPLEMENTATION

#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/sha.h"


class CryptoHashSHA1: public CryptoHash
{
public:
	CryptoHashSHA1(){};
	virtual inline ~CryptoHashSHA1();

	virtual inline OP_STATUS InitHash();
	virtual inline void CalculateHash(const UINT8 *source, int len);
	virtual inline void CalculateHash(const char *source);
	virtual inline int BlockSize() { return 64; }
	virtual inline int Size() { return SHA_DIGEST_LENGTH; }
	virtual inline void ExtractHash(UINT8 *result);

private:
	SHA_CTX	m_sha1_state;
};

#endif // CRYPTO_HASH_SHA1_USE_CORE_IMPLEMENTATION


#ifdef CRYPTO_HASH_SHA256_USE_CORE_IMPLEMENTATION

#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/sha.h"


class CryptoHashSHA256: public CryptoHash
{
public:
	CryptoHashSHA256(){};
	virtual inline ~CryptoHashSHA256();

	virtual inline OP_STATUS InitHash();
	virtual inline void CalculateHash(const UINT8 *source, int len);
	virtual inline void CalculateHash(const char *source);
	virtual inline int BlockSize() { return 64; }
	virtual inline int Size() { return SHA256_DIGEST_LENGTH; }
	virtual inline void ExtractHash(UINT8 *result);

private:
	SHA256_CTX	m_sha256_state;
};

#endif // CRYPTO_HASH_SHA256_USE_CORE_IMPLEMENTATION

#endif /* CRYPTO_HASH_IMPL_H */
