/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland   <haavardm@opera.com>
** @author Alexei Khlebnikov <alexeik@opera.com>
**
*/

#include "core/pch.h"

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/src/openssl_impl/CryptoSignature_impl.h"

#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


CryptoSignature_impl::CryptoSignature_impl( CryptoCipherAlgorithm cipher_algorithm, CryptoHashAlgorithm hash_algorithm)
	: m_cipher_algorithm(cipher_algorithm)
	, m_hash_algorithm(hash_algorithm)
	, m_keyspec(NULL)
{
}

CryptoSignature_impl::~CryptoSignature_impl()
{
	EVP_PKEY_free(m_keyspec);
}

OP_STATUS CryptoSignature_impl::SetPublicKey(const UINT8* public_key, int key_length)
{
	if (!public_key || key_length <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	// This function supports public keys in different formats, such as
	// X509_PUBKEY and EVP_PKEY-with-RSA-key-only objects.
	// It should be made more generic, simple and straightforward:
	// only EVP_PKEY objects should be supported.

	// This function tries to decode data at "public_key" pointer as X509_PUBKEY,
	// if fails - as EVP_PKEY_RSA.

	// Clear the existing key, if any.
	if (m_keyspec)
	{
		EVP_PKEY_free(m_keyspec);
		m_keyspec = NULL;
	}


	// Try hypothesis: public key is X509_PUBKEY.

	// Temporary variable as recommended by "man d2i_X509".
	const unsigned char* public_key_tmp = public_key;
	// Construct X509_PUBKEY object. We own it.
	X509_PUBKEY* pubkey = d2i_X509_PUBKEY(NULL, &public_key_tmp, key_length);
	if (pubkey)
	{
		m_keyspec = X509_PUBKEY_get(pubkey);
		// X509_PUBKEY_get allocates new EVP_PKEY, so it's safe
		// to deallocate pubkey now.
		X509_PUBKEY_free(pubkey);
	}
	ERR_clear_error();


	// Try hypothesis: public key is EVP_PKEY_RSA.

	if (m_keyspec == NULL)
	{
		switch (m_cipher_algorithm )
		{
		case CRYPTO_CIPHER_TYPE_RSA:
			// Temporary variable my have changed its value, resetting.
			public_key_tmp = public_key;
			m_keyspec = d2i_PublicKey(EVP_PKEY_RSA, NULL, &public_key_tmp, key_length);
			ERR_clear_error();
			break;

		default:
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}


	if (m_keyspec)
		return OpStatus::OK;

	return OpStatus::ERR;
}

OP_STATUS CryptoSignature_impl::VerifyASN1(const UINT8* hash_reference, int reference_length, UINT8* signature, int signature_length)
{
	if (!m_keyspec || !hash_reference || reference_length <= 0 ||
	    !signature || signature_length <= 0)
			return OpStatus::ERR_OUT_OF_RANGE;

	int nid;
	switch(m_hash_algorithm)
	{
#ifdef CRYPTO_HASH_MD5_SUPPORT
	case CRYPTO_HASH_TYPE_MD5:
		nid = NID_md5;
		break;
#endif
	case CRYPTO_HASH_TYPE_SHA1:
		nid = NID_sha1;
		break;
#ifdef CRYPTO_HASH_SHA256_SUPPORT
	case CRYPTO_HASH_TYPE_SHA256:
		nid = NID_sha256;
		break;
#endif // CRYPTO_HASH_SHA256_SUPPORT
	default:
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	if (m_cipher_algorithm == CRYPTO_CIPHER_TYPE_RSA)
	{
		int res = RSA_verify(nid, hash_reference, reference_length,
			signature, signature_length, m_keyspec->pkey.rsa);
		OP_ASSERT(res == 0 || res == 1);
		OPENSSL_VERIFY_OR_RETURN(res == 1);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS CryptoSignature_impl::Verify(const UINT8 *hash_reference, int reference_length, const UINT8 *signature, int signature_length)
{
	if (!m_keyspec || !hash_reference || reference_length <= 0 ||
	    !signature || signature_length <= 0)
			return OpStatus::ERR_OUT_OF_RANGE;

	unsigned char* target = OP_NEWA(unsigned char, signature_length);
	RETURN_OOM_IF_NULL(target);
	ANCHOR_ARRAY(unsigned char, target);

	if (m_cipher_algorithm == CRYPTO_CIPHER_TYPE_RSA)
	{
		int decrypted_length = RSA_public_decrypt(signature_length, signature,
			target, m_keyspec->pkey.rsa, RSA_PKCS1_PADDING);
		OPENSSL_VERIFY_OR_RETURN(decrypted_length != -1);

		if (decrypted_length == reference_length && !op_memcmp(target, hash_reference, reference_length))
			return OpStatus::OK;
		else
			return OpStatus::ERR;
	}
	else
		return OpStatus::ERR_NOT_SUPPORTED;
}


/* static */ OP_STATUS CryptoSignature::Create(CryptoSignature *&signature, CryptoCipherAlgorithm cipher_algorithm, CryptoHashAlgorithm hash_algorithm)
{
	if ((signature = new CryptoSignature_impl(cipher_algorithm, hash_algorithm)) == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
