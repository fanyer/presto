/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPRequestProducer_impl.h
 *
 * OpenSSL-based OCSP request producer interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OCSP_REQUEST_PRODUCER_IMPL_H
#define OCSP_REQUEST_PRODUCER_IMPL_H

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/include/OpOCSPRequestProducer.h"


class OCSPRequestProducer_impl : public OpOCSPRequestProducer
{
public:
	OCSPRequestProducer_impl();
	virtual ~OCSPRequestProducer_impl();

public:
	// OpOCSPRequestProducer methods.
	// Set input parameters.
	virtual void SetCertificate(const CryptoCertificate* certificate);
	virtual void SetIssuerCertificate(const CryptoCertificate* issuer_certificate);
	// Set output container.
	virtual void SetOutput(OpOCSPRequest* request);
	// Launch processing.
	virtual void ProcessL();

private:
	// These objects are set from outside. Pointed objects are not owned.
	const CryptoCertificate* m_certificate;
	const CryptoCertificate* m_issuer_certificate;
	OpOCSPRequest*           m_request;
};

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
#endif // OCSP_REQUEST_PRODUCER_IMPL_H
