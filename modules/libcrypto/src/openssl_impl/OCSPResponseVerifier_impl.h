/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPResponseVerifier_impl.h
 *
 * OpenSSL-based OCSP response verifier interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OCSP_RESPONSE_VERIFIER_IMPL_H
#define OCSP_RESPONSE_VERIFIER_IMPL_H

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/include/OpOCSPResponseVerifier.h"


class OCSPResponseVerifier_impl : public OpOCSPResponseVerifier
{
public:
	OCSPResponseVerifier_impl();
	virtual ~OCSPResponseVerifier_impl();

public:
	// OpOCSPResponseVerifier methods.
	virtual void SetOCSPResponseDER(const unsigned char* response_der, unsigned int response_length);
	virtual void SetOCSPRequest(const OpOCSPRequest* request);
	virtual void SetCertificateChain(const CryptoCertificateChain* certificate_chain);
	virtual void SetCAStorage(const CryptoCertificateStorage* ca_storage);
	virtual void ProcessL();
	virtual CryptoCertificateChain::VerifyStatus GetVerifyStatus() const;

private:
	// These objects are set from outside. Pointed objects are not owned.
	const OpOCSPRequest* m_request;
	const unsigned char* m_response_der;
	unsigned int         m_response_length;
	const CryptoCertificateChain*   m_certificate_chain;
	const CryptoCertificateStorage* m_ca_storage;

private:
	// These objects are created and owned by this object.
	CryptoCertificateChain::VerifyStatus m_verify_status;
};

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
#endif // OCSP_RESPONSE_VERIFIER_IMPL_H
