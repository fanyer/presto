/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpOCSPRequestProducer.h
 *
 * OCSP request producer platform interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OP_OCSP_REQUEST_PRODUCER_H
#define OP_OCSP_REQUEST_PRODUCER_H

#ifdef CRYPTO_OCSP_SUPPORT

class CryptoCertificate;
class OpOCSPRequest;

/**
 * @class OpOCSPRequestProducer
 *
 * This class produces OCSP request with the help of platform's
 * crypto library.
 *
 * This class defines a platform interface.
 * This class is a processor.
 * It is synchronous.
 *
 * Usage:
 *
 *		@code
 *		// Recommended to be class variables.
 *		OpOCSPRequest*         m_request  = OpOCSPRequest::Create();
 *		OpOCSPRequestProducer* m_producer = OpOCSPRequestProducer::Create();
 *		...
 *		m_producer->SetCertificate(certificate);
 *		m_producer->SetIssuerCertificate(issuer_certificate);
 *		m_producer->SetOutput(m_request);
 *		m_producer->ProcessL();
 *		...
 *		// Do something with request.
 *		...
 *		OP_DELETE(m_producer);
 *		OP_DELETE(m_request);
 *		@endcode
 *
 */
class OpOCSPRequestProducer
{
public:
	static OpOCSPRequestProducer* Create();
	virtual ~OpOCSPRequestProducer() {}

public:
	/** Set certificate to be verified. */
	virtual void SetCertificate(const CryptoCertificate* cert) = 0;

	/** Set issuer of the certificate to be verified. */
	virtual void SetIssuerCertificate(const CryptoCertificate* issuer_cert) = 0;

	/** Set output container for the request. */
	virtual void SetOutput(OpOCSPRequest* request) = 0;

	/** Process OCSP request production. */
	virtual void ProcessL() = 0;
};

#endif // CRYPTO_OCSP_SUPPORT
#endif // OP_OCSP_REQUEST_PRODUCER_H
