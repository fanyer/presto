/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_HMAC_H_
#define CRYPTO_HMAC_H_

#ifdef CRYPTO_HMAC_SUPPORT

#include "modules/libcrypto/include/CryptoHash.h"


/**
 * hmac (keyed hash function). Implemented according to http://www.ietf.org/rfc/rfc2104.txt
 * 
 */
class CryptoHMAC : public CryptoHash
{
public:
	virtual ~CryptoHMAC();

	static CryptoHMAC *CreateHMAC(CryptoHash *hash, const UINT8 *key, int key_len); /* Takes over hash */
	
	virtual OP_STATUS InitHash();
	virtual void CalculateHash(const UINT8 *source, int len) { m_hash_algorithm->CalculateHash(source, len); }
	virtual void CalculateHash(const char *source_string) { m_hash_algorithm->CalculateHash(source_string); }
	virtual int BlockSize(){ return m_hash_algorithm->BlockSize(); }
	virtual int  Size(){ return m_hash_algorithm->Size(); };
	virtual void ExtractHash(UINT8 *result);

private:
	CryptoHMAC();
	
	UINT8 *m_key_xor_opad;
	UINT8 *m_key_xor_ipad;
	int m_key_len;
	
	CryptoHash *m_hash_algorithm;
};
#endif // CRYPTO_HMAC_SUPPORT

#endif /* CRYPTO_HMAC_H_ */
