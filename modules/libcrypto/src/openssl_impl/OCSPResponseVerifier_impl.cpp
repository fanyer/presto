/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpOCSPResponseVerifier_impl.cpp
 *
 * OpenSSL-based OCSP response verifier implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/src/openssl_impl/OCSPResponseVerifier_impl.h"

#include "modules/libcrypto/src/openssl_impl/CryptoCertificate_impl.h"
#include "modules/libcrypto/src/openssl_impl/CryptoCertificateChain_impl.h"
#include "modules/libcrypto/src/openssl_impl/CryptoCertificateStorage_impl.h"
#include "modules/libcrypto/src/openssl_impl/OCSPRequest_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ocsp.h"


OpOCSPResponseVerifier* OpOCSPResponseVerifier::Create()
{
	return OP_NEW(OCSPResponseVerifier_impl, ());
}


OCSPResponseVerifier_impl::OCSPResponseVerifier_impl()
	: m_request(0)
	, m_response_der(0)
	, m_response_length(0)
	, m_certificate_chain(0)
	, m_ca_storage(0)
	, m_verify_status(CryptoCertificateChain::VERIFY_STATUS_UNKNOWN)
{}


OCSPResponseVerifier_impl::~OCSPResponseVerifier_impl()
{}


void OCSPResponseVerifier_impl::SetOCSPResponseDER(
	const unsigned char* response_der, unsigned int response_length)
{
	m_response_der    = response_der;
	m_response_length = response_length;
}


void OCSPResponseVerifier_impl::SetOCSPRequest(const OpOCSPRequest* request)
{
	m_request = request;
}


void OCSPResponseVerifier_impl::SetCertificateChain(const CryptoCertificateChain* certificate_chain)
{
	m_certificate_chain = certificate_chain;
}


void OCSPResponseVerifier_impl::SetCAStorage(const CryptoCertificateStorage* ca_storage)
{
	m_ca_storage = ca_storage;
}


void OCSPResponseVerifier_impl::ProcessL()
{
	if (!m_request || !m_response_der || m_response_length == 0 ||
	    !m_certificate_chain || !m_ca_storage)
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	// Default value. Will be checked in the end of the function.
	OP_STATUS status = OpStatus::ERR;

	// Default verify status.
	m_verify_status  = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;

	OCSP_RESPONSE*  ocsp_response  = 0;
	OCSP_BASICRESP* ocsp_basicresp = 0;

	do
	{
		const OCSPRequest_impl* request_impl =
			static_cast <const OCSPRequest_impl*> (m_request);
		const CryptoCertificateChain_impl* chain_impl =
			static_cast <const CryptoCertificateChain_impl*> (m_certificate_chain);
		const CryptoCertificateStorage_impl* storage_impl =
			static_cast <const CryptoCertificateStorage_impl*> (m_ca_storage);

		if (!request_impl || !chain_impl || !storage_impl)
		{
			status = OpStatus::ERR_OUT_OF_RANGE;
			break;
		}

		OCSP_REQUEST*   ocsp_request = request_impl->GetOCSP_REQUEST();
		STACK_OF(X509)* x509_chain   = chain_impl->GetStackOfX509();
		X509_STORE*     x509_store   = storage_impl->GetX509Store();
		OP_ASSERT(ocsp_request && x509_chain && x509_store);
		// Both ocsp_request, x509_chain and x509_store are owned
		// by their container objects.

		// Temporary variable, according to the documentation.
		const unsigned char* response_tmp = m_response_der;
		ocsp_response = d2i_OCSP_RESPONSE(0, &response_tmp, m_response_length);
		OPENSSL_VERIFY_OR_BREAK2(ocsp_response, status);

		// Check that the OCSP responder was able to respond correctly.
		int ocsp_response_status = OCSP_response_status(ocsp_response);
		OPENSSL_BREAK2_IF(ocsp_response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL, status);

		ocsp_basicresp = OCSP_response_get1_basic(ocsp_response);
		OPENSSL_VERIFY_OR_BREAK2(ocsp_basicresp, status);

		// Verify that the response is correctly signed.
		int response_valid_status =
			OCSP_basic_verify(ocsp_basicresp, x509_chain, x509_store, /* flags = */ 0);
		OPENSSL_BREAK2_IF(response_valid_status != 1, status);

		OP_ASSERT(ocsp_request->tbsRequest && ocsp_request->tbsRequest->requestList);
		OCSP_ONEREQ* ocsp_onereq =
			sk_OCSP_ONEREQ_value(ocsp_request->tbsRequest->requestList, 0);
		OPENSSL_VERIFY_OR_BREAK(ocsp_onereq);
		// ocsp_request owns ocsp_onereq.

		OCSP_CERTID* ocsp_certid = ocsp_onereq->reqCert;
		OPENSSL_VERIFY_OR_BREAK(ocsp_certid);
		// ocsp_request owns ocsp_certid.

		int revocation_code = -1, response_code = -1;
		ASN1_GENERALIZEDTIME* thisupd = 0;
		ASN1_GENERALIZEDTIME* nextupd = 0;
		int err = OCSP_resp_find_status(
			ocsp_basicresp, ocsp_certid, &response_code, &revocation_code,
			0, &thisupd, &nextupd);
		OPENSSL_BREAK2_IF(err != 1 || response_code < 0, status);

		// Allow some difference in client and server clocks.
		const long int CLOCK_SHARPNESS = 3600;
		// Default age limit for responses without nextUpdate field.
		const long int DEFAULT_MAXAGE  = 100 * 24 * 3600;

		err = OCSP_check_validity(thisupd, nextupd, CLOCK_SHARPNESS, DEFAULT_MAXAGE);
		OPENSSL_BREAK2_IF(err != 1, status);

		switch (response_code)
		{
			case V_OCSP_CERTSTATUS_GOOD:
				m_verify_status = CryptoCertificateChain::OK_CHECKED_WITH_OCSP;
				status = OpStatus::OK;
				break;

			case V_OCSP_CERTSTATUS_REVOKED:
				m_verify_status = CryptoCertificateChain::CERTIFICATE_REVOKED;
				status = OpStatus::OK;
				break;

			default:
				OP_ASSERT(!"Unexpected OCSP response code!");
				// fall-through

			case V_OCSP_CERTSTATUS_UNKNOWN:
				OP_ASSERT(m_verify_status == CryptoCertificateChain::VERIFY_STATUS_UNKNOWN);
				break;
		}

	} while(0);


	if(ocsp_basicresp)
		OCSP_BASICRESP_free(ocsp_basicresp);

	if(ocsp_response)
		OCSP_RESPONSE_free(ocsp_response);

	// There shouldn't be any errors.
	OP_ASSERT(ERR_peek_error() == 0);

	LEAVE_IF_ERROR(status);
}


CryptoCertificateChain::VerifyStatus
OCSPResponseVerifier_impl::GetVerifyStatus() const
{
	return m_verify_status;
}

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
