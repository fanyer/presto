/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_SIGNATURE_H
#define CRYPTO_SIGNATURE_H

#ifdef CRYPTO_API_SUPPORT

#include "modules/libcrypto/include/CryptoHash.h"


enum CryptoCipherAlgorithm
{
	CRYPTO_CIPHER_TYPE_RSA,
	CRYPTO_CIPHER_TYPE_DSA // not supported
};

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

class CryptoSignature
{
public:
	virtual ~CryptoSignature(){};

	static OP_STATUS Create(CryptoSignature *&sign, CryptoCipherAlgorithm cipher_algoritm, CryptoHashAlgorithm hash_algorithm);

	virtual OP_STATUS SetPublicKey(const UINT8 *public_key, int key_length) = 0;
	virtual OP_STATUS VerifyASN1(const UINT8 *hash_reference, int reference_length, UINT8 *signature_asn_format, int signature_length) = 0;
	virtual OP_STATUS Verify(const UINT8 *hash_reference, int reference_length, const UINT8 *signature, int signature_length) = 0;

protected:
	CryptoSignature(){};
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
#endif // CRYPTO_API_SUPPORT

#endif /* CRYPTO_SIGNATURE_H */
