/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/libcrypto/include/CryptoStreamEncryptionCBC.h"

#ifdef CRYPTO_STREAM_ENCRYPTION_CBC_SUPPORT
CryptoStreamEncryptionCBC::CryptoStreamEncryptionCBC(CryptoSymmetricAlgorithm *cipher_algorithm, PaddingStrategy padding_strategy)
	: m_algorithm(cipher_algorithm)
	, m_padding_strategy(padding_strategy)
	, m_state(NULL)
	, m_key(NULL)
	, m_state_counter(0)
{
}

CryptoStreamEncryptionCBC::~CryptoStreamEncryptionCBC()
{
	OP_DELETEA(m_state);
	OP_DELETEA(m_key);
	OP_DELETE(m_algorithm);
}

#ifdef CRYPTO_ENCRYPTION_AES_SUPPORT
CryptoStreamEncryptionCBC *CryptoStreamEncryptionCBC::Create(CryptoSymmetricAlgorithm *cipher_algorithm, PaddingStrategy padding_strategy)
{
	if (!cipher_algorithm)
		return NULL;
	int block_size = cipher_algorithm->GetBlockSize();

	OpStackAutoPtr<CryptoStreamEncryptionCBC> temp_alg(OP_NEW(CryptoStreamEncryptionCBC, (cipher_algorithm, padding_strategy)));
	if (!temp_alg.get())
		return NULL;

	if (!(temp_alg->m_state = OP_NEWA(byte, block_size)))
		return NULL;

	if (!(temp_alg->m_key = OP_NEWA(byte, cipher_algorithm->GetKeySize())))
		return NULL;

	return temp_alg.release();
}
#endif // CRYPTO_ENCRYPTION_AES_SUPPORT

void CryptoStreamEncryptionCBC::Encrypt(const UINT8 *source, UINT8 *target, int source_length, int target_length)
{
	// IMPROVE ME : Implement cipher stealing for arbitrary lengths

	OP_ASSERT(source && target && source_length >= 0 && target_length >= 0);

	if (target_length == 0 && m_padding_strategy == PADDING_STRATEGY_NONE)
		target_length = source_length;

	OP_ASSERT(target_length == CalculateEncryptedTargetLength(source_length));
	UINT8 padding_byte = target_length - source_length;

	m_algorithm->SetEncryptKey(m_key);

	int block_size = GetBlockSize();

	for (int i = 0; i < target_length; i++)
	{
		UINT8 source_byte = i < source_length ? source[i] : padding_byte;

		m_state[m_state_counter] = m_state[m_state_counter] ^ source_byte;

		m_state_counter = (m_state_counter + 1) % block_size;

		if (!m_state_counter)
		{
			m_algorithm->Encrypt(m_state, target + i + 1 - block_size);
			op_memmove(m_state, target + i + 1 - block_size, block_size);
		}
	}
}

void CryptoStreamEncryptionCBC::Decrypt(const UINT8 *source, UINT8 *target, int length)
{
	OP_ASSERT(source && target && length >= 0);

	m_algorithm->SetDecryptKey(m_key);

	int block_size = GetBlockSize();

	for (int i = 0; i < length; i++)
	{
		if (!m_state_counter)
		{
			m_algorithm->Decrypt(source + i, target + i);
		}

		target[i] ^= m_state[m_state_counter];

		m_state[m_state_counter] = source[i];

		m_state_counter = (m_state_counter + 1) % block_size;
	}
}

OP_STATUS CryptoStreamEncryptionCBC::SetKey(const UINT8 *key)
{
	OP_ASSERT(key);

	if (!key)
		return OpStatus::ERR;

	op_memmove(m_key, key, GetKeySize());
	return OpStatus::OK;
}

void CryptoStreamEncryptionCBC::SetIV(const UINT8 *iv)
{
	OP_ASSERT(m_state);
	OP_ASSERT(iv);

	op_memcpy(m_state, iv, m_algorithm->GetBlockSize());
	m_state_counter = 0;
}

int CryptoStreamEncryptionCBC::CalculateEncryptedTargetLength(int source_length) const
{
	int block_size = GetBlockSize();

	switch (m_padding_strategy)
	{
	case PADDING_STRATEGY_NONE:
		OP_ASSERT(source_length % block_size == 0);
		return source_length;

	case PADDING_STRATEGY_PCKS15:
		/*
		 * Pad the encrypted target with byte 'n' where n is the number
		 * of padding bytes. Bytes are added until target_length == 0 mod
		 * block_size
		 *
		 * If source length  == 0 mod block size, one block of padding
		 * is added.
		 */
		return source_length + (block_size - (source_length % block_size));

	default:
		OP_ASSERT(!"UNKNOWN PADDING STRATEGY");
		return source_length;
	}
}

OP_STATUS CryptoStreamEncryptionCBC::CalculateDecryptedTargetLength(int &decrypted_target_length, const UINT8 *decrypted_source, int source_length) const
{
	decrypted_target_length = 0;
	OP_STATUS status = OpStatus::OK;
	switch (m_padding_strategy)
	{
	case PADDING_STRATEGY_NONE:
		decrypted_target_length = source_length;
		break;

	case PADDING_STRATEGY_PCKS15:
		if (source_length >= decrypted_source[source_length - 1])
			decrypted_target_length = source_length - decrypted_source[source_length - 1];
		else
			status = OpStatus::ERR_PARSING_FAILED;
		break;

	default:
		OP_ASSERT(!"UNKNOWN PADDING STRATEGY");
		decrypted_target_length = source_length;
		break;
	}

	return status;
}


#endif // CRYPTO_STREAM_ENCRYPTION_SUPPORT
