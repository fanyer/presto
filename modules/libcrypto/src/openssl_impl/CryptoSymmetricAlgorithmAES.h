/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_SYMMETRIC_ALGORITHM_AES_H
#define CRYPTO_SYMMETRIC_ALGORITHM_AES_H

#ifdef CRYPTO_ENCRYPTION_AES_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/aes.h"


class CryptoSymmetricAlgorithmAES : public CryptoSymmetricAlgorithm
{
public:
	virtual inline ~CryptoSymmetricAlgorithmAES();
	
	virtual inline void Encrypt(const UINT8 *source,  UINT8 *target);

	virtual inline void Decrypt(const UINT8 *source,  UINT8 *target);
	
	virtual inline OP_STATUS SetEncryptKey(const UINT8 *key);
	virtual inline OP_STATUS SetDecryptKey(const UINT8 *key);
	
	virtual inline int GetKeySize() const { return m_key_size; }
	virtual inline int GetBlockSize() const { return AES_BLOCK_SIZE; }
	
	inline CryptoSymmetricAlgorithmAES(int key_size);
	
private:
	int m_key_size; 
	AES_KEY m_key;	
};

#endif // CRYPTO_ENCRYPTION_AES_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_SYMMETRIC_ALGORITHM_AES_H */
