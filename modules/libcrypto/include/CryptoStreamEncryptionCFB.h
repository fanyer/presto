/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CRYPTO_STREAM_ENCRYPTION_CFB_H
#define CRYPTO_STREAM_ENCRYPTION_CFB_H

// Turn on this api with API_CRYPTO_STREAM_ENCRYPTION_SUPPORT
#ifdef CRYPTO_STREAM_ENCRYPTION_SUPPORT

#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"


// Wrapper class that turns a block cipher into a CFB mode stream cipher 
class CryptoStreamEncryptionCFB // import API_CRYPTO_STREAM_ENCRYPTION
{
public:

	static CryptoStreamEncryptionCFB *Create(CryptoSymmetricAlgorithm *cipher_algorithm); // Takes over cipher_algorithm
	
	virtual void Encrypt(const UINT8 *source,  UINT8 *target, int len);
	virtual void Decrypt(const UINT8 *source,  UINT8 *target, int len);
		
	virtual OP_STATUS SetKey(const UINT8 *key);
	virtual void SetIV(const UINT8 *iv);
	
	virtual int GetKeySize(){ return m_algorithm->GetKeySize(); }
	virtual int GetBlockSize() { return m_algorithm->GetBlockSize(); }
	

	const UINT8 *GetState(){ return m_state; }
	
	virtual ~CryptoStreamEncryptionCFB();

private:
	CryptoStreamEncryptionCFB();
	CryptoSymmetricAlgorithm *m_algorithm;
	UINT8 *m_state;
	UINT8 *m_temp;
	unsigned int m_state_counter;
};

#endif // CRYPTO_STREAM_ENCRYPTION_SUPPORT
#endif // CRYPTO_STREAM_ENCRYPTION_CFB_H
