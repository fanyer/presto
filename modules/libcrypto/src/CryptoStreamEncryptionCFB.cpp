/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CRYPTO_STREAM_ENCRYPTION_SUPPORT

#include "modules/libcrypto/include/CryptoStreamEncryptionCFB.h"
#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"


CryptoStreamEncryptionCFB::CryptoStreamEncryptionCFB()
	: m_algorithm(NULL)
	, m_state(NULL)
	, m_temp(NULL)
	, m_state_counter(0)
{
	
}

CryptoStreamEncryptionCFB::~CryptoStreamEncryptionCFB()
{
	OP_DELETEA(m_state);
	OP_DELETEA(m_temp);
	OP_DELETE(m_algorithm);
}

#ifdef CRYPTO_ENCRYPTION_AES_SUPPORT
CryptoStreamEncryptionCFB *CryptoStreamEncryptionCFB::Create(CryptoSymmetricAlgorithm *cipher_algorithm)
{
	if (!cipher_algorithm)
		return NULL;
	int block_size = cipher_algorithm->GetBlockSize();
	
	OpStackAutoPtr<CryptoStreamEncryptionCFB> temp_alg(OP_NEW(CryptoStreamEncryptionCFB, ()));
	if (!temp_alg.get())
		return NULL;

	if (!(temp_alg->m_state = OP_NEWA(byte, block_size)))
		return NULL;

	if (!(temp_alg->m_temp = OP_NEWA(byte, block_size)))
		return NULL;
	
	temp_alg->m_algorithm = cipher_algorithm;
	
	return temp_alg.release();
}
#endif // CRYPTO_ENCRYPTION_AES_SUPPORT
	
void CryptoStreamEncryptionCFB::Encrypt(const UINT8 *source, UINT8 *target, int len)
{	
	OP_ASSERT(source && target && len >= 0);

	int i;
	int block_size = GetBlockSize();
	
	for (i = 0; i < len; i++)
	{
		if (!m_state_counter)
		{
			m_algorithm->Encrypt(m_state, m_temp); // copy target to temporary in case the algorithm can't handle overlapping pointers 
		}
		
		target[i] = m_state[m_state_counter] = m_temp[m_state_counter] ^ source[i];

		m_state_counter = (m_state_counter + 1) % block_size;
	}
}


void CryptoStreamEncryptionCFB::Decrypt(const UINT8 *source, UINT8 *target, int len)
{
	OP_ASSERT(source && target && len >= 0);

	int i;
	int block_size = GetBlockSize();
	
	for (i = 0; i < len; i++)
	{
		if (!m_state_counter)
		{
			m_algorithm->Encrypt(m_state, m_temp ); // copy target to temporary in case the algorithm can't handle overlapping pointers 
		}
		
		m_state[m_state_counter] = source[i];
		
		target[i] = m_temp[m_state_counter] ^ source[i]; 
		
		m_state_counter = (m_state_counter + 1) % block_size;
	}	
}

OP_STATUS CryptoStreamEncryptionCFB::SetKey(const UINT8 *key)
{
	OP_ASSERT(key);
	return m_algorithm->SetEncryptKey(key);
}
	
void CryptoStreamEncryptionCFB::SetIV(const UINT8 *iv)
{
	OP_ASSERT(m_state);
	OP_ASSERT(iv);
	
	op_memcpy(m_state, iv, m_algorithm->GetBlockSize());
	m_state_counter = 0;
}

#endif // CRYPTO_STREAM_ENCRYPTION_SUPPORT
