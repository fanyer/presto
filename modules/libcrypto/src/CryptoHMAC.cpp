/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CRYPTO_HMAC_SUPPORT

#include "modules/libcrypto/include/CryptoHMAC.h"


CryptoHMAC::~CryptoHMAC()
{
	OP_DELETEA(m_key_xor_opad);
	OP_DELETEA(m_key_xor_ipad);
	OP_DELETE(m_hash_algorithm);
}

CryptoHMAC::CryptoHMAC()
	: m_key_xor_opad(NULL)
	, m_key_xor_ipad(NULL)
	, m_key_len(0)
	, m_hash_algorithm(NULL) 
{
}

CryptoHMAC *CryptoHMAC::CreateHMAC(CryptoHash *hash, const UINT8 *key, int key_len)
{	
	if (!hash || !key || key_len <= 0)
	{ 
		return NULL;
	}
	
	OP_ASSERT(hash->Size() <= hash->BlockSize());

	int block_size = hash->BlockSize();
	
	OpAutoPtr<CryptoHMAC> hmac(OP_NEW(CryptoHMAC, ()));
	if (!hmac.get() || !(hmac->m_key_xor_opad = OP_NEWA(byte, block_size)) || !(hmac->m_key_xor_ipad = OP_NEWA(byte, block_size)))
		return NULL;
		
	hmac->m_hash_algorithm = hash;
	
	if (key_len > block_size)
	{
		hash->InitHash();
		hash->CalculateHash(key, key_len);
		hash->ExtractHash(hmac->m_key_xor_opad);
		op_memcpy(hmac->m_key_xor_ipad, hmac->m_key_xor_opad, hash->Size());
		key_len = hash->Size();	
	}
	else
	{	
		op_memcpy(hmac->m_key_xor_opad, key, key_len);
		op_memcpy(hmac->m_key_xor_ipad, key, key_len);
	}	
	
	op_memset(hmac->m_key_xor_opad + key_len, 0x5c, block_size - key_len);
	op_memset(hmac->m_key_xor_ipad + key_len, 0x36, block_size - key_len);

	
	int i;
	for (i = 0; i < key_len; i++)
	{
		hmac->m_key_xor_opad[i] ^= 0x5c;
		hmac->m_key_xor_ipad[i] ^= 0x36;
	}
	
	hmac->m_key_len = key_len;

	return hmac.release();
}
	
OP_STATUS CryptoHMAC::InitHash()
{
	RETURN_IF_ERROR(m_hash_algorithm->InitHash());
	m_hash_algorithm->CalculateHash(m_key_xor_ipad, m_hash_algorithm->BlockSize());
	return OpStatus::OK;
}

void CryptoHMAC::ExtractHash(UINT8 *result)
{
	OP_ASSERT(result);
	int hash_size = m_hash_algorithm->Size();

	OP_ASSERT(hash_size <= CRYPTO_MAX_HASH_SIZE);
	UINT8 temp_digest[CRYPTO_MAX_HASH_SIZE]; /* ARRAY OK 2011-11-15 roarl */
	
	m_hash_algorithm->ExtractHash(temp_digest);
	RETURN_VOID_IF_ERROR(m_hash_algorithm->InitHash());
	
	m_hash_algorithm->CalculateHash(m_key_xor_opad, m_hash_algorithm->BlockSize());
	m_hash_algorithm->CalculateHash(temp_digest, hash_size);
	m_hash_algorithm->ExtractHash(result);	
}

#endif // CRYPTO_HMAC_SUPPORT
