/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPRequestProducer_impl.cpp
 *
 * OpenSSL-based OCSP request producer implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/src/openssl_impl/OCSPRequestProducer_impl.h"

#include "modules/libcrypto/src/openssl_impl/CryptoCertificate_impl.h"
#include "modules/libcrypto/src/openssl_impl/OCSPRequest_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ocsp.h"


OpOCSPRequestProducer* OpOCSPRequestProducer::Create()
{
	return OP_NEW(OCSPRequestProducer_impl, ());
}


OCSPRequestProducer_impl::OCSPRequestProducer_impl()
	: m_certificate(0)
	, m_issuer_certificate(0)
	, m_request(0)
{}


OCSPRequestProducer_impl::~OCSPRequestProducer_impl()
{}


void OCSPRequestProducer_impl::SetCertificate(const CryptoCertificate* certificate)
{
	m_certificate = certificate;
}


void OCSPRequestProducer_impl::SetIssuerCertificate(const CryptoCertificate* issuer_certificate)
{
	m_issuer_certificate = issuer_certificate;
}


void OCSPRequestProducer_impl::SetOutput(OpOCSPRequest* request)
{
	m_request = request;
}


void OCSPRequestProducer_impl::ProcessL()
{
	if (!m_certificate || !m_issuer_certificate)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	// Default value. Will be checked in the end of the function.
	OP_STATUS status = OpStatus::OK;

	// Will be checked and deallocated in the end of the function.
	OCSP_CERTID*  ocsp_certid  = 0;
	OCSP_REQUEST* ocsp_request = 0;

	do
	{
		const CryptoCertificate_impl* cert_impl =
			static_cast <const CryptoCertificate_impl*> (m_certificate);
		const CryptoCertificate_impl* issuer_cert_impl =
			static_cast <const CryptoCertificate_impl*> (m_issuer_certificate);
		OCSPRequest_impl* request_impl =
			static_cast <OCSPRequest_impl*> (m_request);

		if (!cert_impl || !issuer_cert_impl || !request_impl)
		{
			OP_ASSERT(!"Only openssl_impl objects are supported!");
			status = OpStatus::ERR_OUT_OF_RANGE;
			break;
		}

		X509* x509   = cert_impl->GetX509();
		X509* issuer = issuer_cert_impl->GetX509();
		OP_ASSERT(x509 && issuer);
		// Both x509 and issuer are owned by their CryptoCertificate_impl's.

		ocsp_certid  = OCSP_cert_to_id(0, x509, issuer);
		OPENSSL_VERIFY_OR_BREAK2(ocsp_certid, status);
		// We own ocsp_certid.

		ocsp_request = OCSP_REQUEST_new();
		OPENSSL_VERIFY_OR_BREAK2(ocsp_request, status);
		// We own ocsp_request.

		OCSP_ONEREQ* ocsp_onereq = OCSP_request_add0_id(ocsp_request, ocsp_certid);
		OPENSSL_VERIFY_OR_BREAK2(ocsp_onereq, status);

		// ocsp_request now owns ocsp_certid.
		ocsp_certid  = 0;
		// ocsp_request owns ocsp_onereq.

		request_impl->TakeOverOCSP_REQUEST(ocsp_request);
		// request_impl now owns ocsp_request.
		ocsp_request = 0;

	} while(0);

	if(ocsp_request)
		OCSP_REQUEST_free(ocsp_request);

	if(ocsp_certid)
		OCSP_CERTID_free(ocsp_certid);

	// There shouldn't be any errors.
	OP_ASSERT(ERR_peek_error() == 0);

	LEAVE_IF_ERROR(status);
}

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
