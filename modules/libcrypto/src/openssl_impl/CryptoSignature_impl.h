/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_SIGNATURE_IMPL_H
#define CRYPTO_SIGNATURE_IMPL_H

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoSignature.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


class CryptoSignature_impl : public CryptoSignature
{
public:
	CryptoSignature_impl( CryptoCipherAlgorithm cipher_algorithm, CryptoHashAlgorithm hash_algorithm);
	virtual ~CryptoSignature_impl();

	virtual OP_STATUS SetPublicKey(const UINT8 *public_key, int key_length);

	virtual OP_STATUS VerifyASN1(const UINT8 *hash_reference, int reference_length, UINT8 *signature, int signature_length);
	virtual OP_STATUS Verify(const UINT8 *hash_reference, int reference_length, const UINT8 *signature, int signature_length);

private:
	CryptoCipherAlgorithm m_cipher_algorithm;
	CryptoHashAlgorithm m_hash_algorithm;

	EVP_PKEY *m_keyspec;
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_SIGNATURE_IMPL_H */
