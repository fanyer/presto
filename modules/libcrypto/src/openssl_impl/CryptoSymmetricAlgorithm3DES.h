/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_SYMMETRIC_ALGORITHM_3DES_H
#define CRYPTO_SYMMETRIC_ALGORITHM_3DES_H

#ifdef CRYPTO_ENCRYPTION_3DES_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/des.h"

class CryptoSymmetricAlgorithm3DES : public CryptoSymmetricAlgorithm
{
public:
	virtual inline ~CryptoSymmetricAlgorithm3DES();

	virtual inline void Encrypt(const UINT8 *source,  UINT8 *target);
	virtual inline void Decrypt(const UINT8 *source,  UINT8 *target);

	virtual inline OP_STATUS SetEncryptKey(const UINT8 *key);
	virtual inline OP_STATUS SetDecryptKey(const UINT8 *key);

	virtual inline int GetKeySize() const { return m_key_size; }
	virtual inline int GetBlockSize() const { return 8; }

	inline CryptoSymmetricAlgorithm3DES(int key_length);

private:
	void CryptFunction3DES(const UINT8 *source,  UINT8 *target, int direction);

	DES_key_schedule m_key1, m_key2, m_key3;
	int m_key_size;
};

#endif // CRYPTO_ENCRYPTION_AES_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_SYMMETRIC_ALGORITHM_3DES_H */
