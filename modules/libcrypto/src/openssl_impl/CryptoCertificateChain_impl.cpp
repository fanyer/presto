/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
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

#include "modules/libcrypto/src/openssl_impl/CryptoCertificateChain_impl.h"

#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/src/openssl_impl/CryptoCertificate_impl.h"
#include "modules/libcrypto/src/openssl_impl/CryptoCertificateStorage_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/addon/purposes.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


OP_STATUS CryptoCertificateChain::Create(CryptoCertificateChain *&chain)
{
	return CryptoCertificateChain_impl::Make(chain);
}

OP_STATUS CryptoCertificateChain_impl::Make(CryptoCertificateChain *&chain)
{
	chain = NULL;

	CryptoCertificateChain_impl* temp_chain = OP_NEW(CryptoCertificateChain_impl, ());
	RETURN_OOM_IF_NULL(temp_chain);

	if (
		!(temp_chain->m_certificate_chain = sk_X509_new_null()) ||
		!(temp_chain->m_certificate_storage = X509_STORE_CTX_new())
	)
	{
		OP_DELETE(temp_chain);
		OPENSSL_VERIFY_OR_RETURN(FALSE);
	}

	chain = temp_chain;
	return OpStatus::OK;
}

CryptoCertificateChain_impl::~CryptoCertificateChain_impl()
{
	sk_X509_free(m_certificate_chain);
	X509_STORE_CTX_free(m_certificate_storage);
	OP_ASSERT(ERR_peek_error() == 0);
}

CryptoCertificateChain_impl::CryptoCertificateChain_impl()
	: m_certificate_chain(NULL)
	, m_certificate_storage(NULL)
{
}

OP_STATUS CryptoCertificateChain_impl::AddToChainBase64(const char *certificate_der)
{
	if (!certificate_der)
		return OpStatus::ERR_OUT_OF_RANGE;

	int length = op_strlen(certificate_der);
	unsigned long read_pos = 0;
	BOOL warning;
	unsigned char *certificate_binary = OP_NEWA(unsigned char, length);
	if (!certificate_binary)
		return OpStatus::ERR_NO_MEMORY;

	ANCHOR_ARRAY(unsigned char, certificate_binary);

	unsigned long certificate_binary_length = GeneralDecodeBase64(reinterpret_cast<const unsigned char*>(certificate_der), length, read_pos, certificate_binary, warning, length);

	if (certificate_binary_length == 0)
	{
		return OpStatus::ERR;
	}

	return AddToChainBinary(certificate_binary, certificate_binary_length);
}



OP_STATUS CryptoCertificateChain_impl::AddToChainBinary(const UINT8 *certificate_der, int length)
{
	if (!certificate_der || length <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	const unsigned char* certificate_der_tmp = certificate_der;
	X509* x509certificate = d2i_X509(NULL, &certificate_der_tmp, length);
	OPENSSL_VERIFY_OR_RETURN(x509certificate);

	CryptoCertificate_impl* crypto_certificate = OP_NEW(CryptoCertificate_impl, (x509certificate));
	if (crypto_certificate == NULL)
	{
		X509_free(x509certificate);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(m_certificate_list.Add(crypto_certificate)))
	{
		OP_DELETE(crypto_certificate);
		return OpStatus::ERR_NO_MEMORY;
	}

	int idx = sk_X509_push(m_certificate_chain, x509certificate);
	if (idx == 0)
	{
		// sk_X509_push() doesn't add errors to OpenSSL error queue.
		OP_ASSERT(ERR_peek_error() == 0);
		OP_DELETE(crypto_certificate);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


OP_STATUS CryptoCertificateChain_impl::VerifyChainSignatures(VerifyStatus &reason, const CryptoCertificateStorage *ca_storage)
{
	if (!ca_storage)
		return OpStatus::ERR_OUT_OF_RANGE;

	reason = CERTIFICATE_GENERIC;
	const CryptoCertificateStorage_impl* openssl_ca_storage =
		static_cast <const CryptoCertificateStorage_impl*> (ca_storage);

	if (!openssl_ca_storage || openssl_ca_storage->TypeCheck() != 0x12345679)
	{
		OP_ASSERT(!"ca_storage cannot be NULL and MUST be of type CryptoCertificateChain_impl");
		return OpStatus::ERR;
	}

	X509_STORE* x509_store = openssl_ca_storage->GetX509Store();
	X509* x509 = sk_X509_value(m_certificate_chain, 0);

	int err = X509_STORE_CTX_init(m_certificate_storage, x509_store, x509, m_certificate_chain);
	OPENSSL_VERIFY_OR_RETURN(err == 1);

	OP_ASSERT(X509_PURPOSE_get_by_id(OPERA_X509_PURPOSE_CODE_SIGN) != -1);
	OP_ASSERT(!ERR_peek_error());

	err = X509_STORE_CTX_set_purpose(m_certificate_storage, OPERA_X509_PURPOSE_CODE_SIGN);
	OPENSSL_VERIFY_OR_RETURN(err == 1);

	// Main statement in this function. Verify the certificate
	// and build the verification chain.
	int result = X509_verify_cert(m_certificate_storage);

	// Check verification result.
	if (result > 0)
	{
		// Verified successfully.

		// OpenSSL may push X509_V_OK into the error queue.
		ERR_clear_error();

		// Update m_certificate_chain and m_certificate_list
		// from m_certificate_storage->chain.
		RETURN_IF_ERROR(UpdateCertificateList());
		OP_ASSERT(ERR_peek_error() == 0);

		reason = OK_CHECKED_LOCALLY;
	}
	else if (result < 0)
	{
		// Fatal Error
		reason = CERTIFICATE_BAD_CERTIFICATE;
		OPENSSL_VERIFY_OR_RETURN(FALSE);
	}
	else if (result == 0)
	{
		// Verifying error
		int error = X509_STORE_CTX_get_error(m_certificate_storage);
		switch(error)
		{
			case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
			case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
			case X509_V_ERR_INVALID_CA:
				reason = CERTIFICATE_INVALID_CERTIFICATE_CHAIN;
				break;

			case X509_V_ERR_CERT_SIGNATURE_FAILURE:
				reason = CERTIFICATE_INVALID_SIGNATURE;
				break;

			case X509_V_ERR_CERT_NOT_YET_VALID:
			case X509_V_ERR_CERT_HAS_EXPIRED:
				reason = CERTIFICATE_EXPIRED;
				break;

			case X509_V_OK :
				OP_ASSERT(!"Impossible? Certificate verification failed, but the certificate is suddenly OK!");
			case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
			case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
			default:
				reason = CERTIFICATE_BAD_CERTIFICATE;
		}

		ERR_clear_error();
	}

	OP_ASSERT(ERR_peek_error() == 0);
	return OpStatus::OK;
}


OP_STATUS CryptoCertificateChain_impl::UpdateCertificateList()
{
	// This function is supposed to be called after the successful
	// chain verification. It updates m_certificate_chain
	// and m_certificate_list with the resulting verified chain.

	// The  input chain now is m_certificate_storage->untrusted.
	// The output chain now is m_certificate_storage->chain.
	OP_ASSERT(m_certificate_storage->untrusted);
	OP_ASSERT(m_certificate_storage->chain);
	OP_ASSERT(m_certificate_storage->untrusted != m_certificate_storage->chain);
	OP_ASSERT(m_certificate_chain == m_certificate_storage->untrusted);

	// Deallocate the input chain.
	m_certificate_list.DeleteAll();
	// OpenSSL stacks do not own its members, so we don't need
	// to deallocate them. We only need to deallocate the stack itself.
	// The contained X509 objects were owned by the corresponding
	// CryptoCertificate_impl objects from m_certificate_list.
	sk_X509_free(m_certificate_chain);

	// This pointer now points to the deallocated memory,
	// thus should be nullified.
	m_certificate_storage->untrusted = 0;

	// Set m_certificate_chain to point to the output chain.
	m_certificate_chain = m_certificate_storage->chain;
	// We own all X509 objects in m_certificate_chain.

	// This pointer should be nullified, otherwise the output chain
	// with all contained X509 objects will be destructed
	// during m_certificate_storage destruction.
	m_certificate_storage->chain = 0;

	// Update m_certificate_list.

	const int x509_count = sk_X509_num(m_certificate_chain);
	OP_ASSERT(x509_count >= 0);

	// This variable stores amount of X509 objects from m_certificate_chain
	// that are owned by CryptoCertificate_impl objects from m_certificate_list.
	// It will help us if we run out of memory in the loop below.
	int owned_x509_count = 0;

	m_certificate_list.SetAllocationStepSize(x509_count);

	// This default value can be changed by the loop below.
	OP_STATUS status = OpStatus::OK;

	// Create CryptoCertificate_impl objects from the output chain.
	for (int i = 0; i < x509_count; i++)
	{
		// Get X509.
		X509* x509 = sk_X509_value(m_certificate_chain, i);
		OP_ASSERT(x509);
		// We own x509.

		// Make CryptoCertificate.
		CryptoCertificate_impl* cert = OP_NEW(CryptoCertificate_impl, (x509));
		if (!cert)
		{
			status = OpStatus::ERR_NO_MEMORY;
			break;
		}
		// We own cert.
		// cert owns x509.

		// Update number of owned X509 objects.
		owned_x509_count++;
		OP_ASSERT(owned_x509_count == i + 1);

		// Initialize CryptoCertificate.
		status = cert->Init();
		if (OpStatus::IsError(status))
		{
			OP_DELETE(cert);
			break;
		}

		// Add CryptoCertificate to list.
		status = m_certificate_list.Add(cert);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(cert);
			break;
		}
	} // for (int i = 0; i < x509_count; i++)

	if (OpStatus::IsError(status))
	{
		// Something went wrong in the previous loop.
		// Let's clean both m_certificate_list and m_certificate_chain.

		// Destruct m_certificate_list first.
		m_certificate_list.DeleteAll();
		// owned_x509_count X509 objects are destructed now.

		// Let's destruct the rest.
		for (int i = owned_x509_count; i < x509_count; i++)
		{
			X509* x509 = sk_X509_value(m_certificate_chain, i);
			OP_ASSERT(x509);
			X509_free(x509);
		}

		// Empty the stack.
		// Destructed X509 objects inside will not be destructed the 2nd time.
		sk_X509_zero(m_certificate_chain);

		OP_ASSERT(sk_X509_num(m_certificate_chain) == 0);
		OP_ASSERT(m_certificate_list.GetCount() == 0);
		return status;
	}

	OP_ASSERT(sk_X509_num(m_certificate_chain) == x509_count);
	OP_ASSERT((int)m_certificate_list.GetCount() == x509_count);
	OP_ASSERT(owned_x509_count == x509_count);
	return OpStatus::OK;
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
