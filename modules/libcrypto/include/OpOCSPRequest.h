/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpOCSPRequest.h
 *
 * OCSP request container platform interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OP_OCSP_REQUEST_H
#define OP_OCSP_REQUEST_H

#ifdef CRYPTO_OCSP_SUPPORT

/**
 * @class OpOCSPRequest
 *
 * An object of platform-dependent implementation of this class
 * is filled with the platform-dependent data
 * by the implementation of @ref OpOCSPRequestProducer.
 *
 * This class can export platform-dependent data in DER format,
 * suitable for sending it to OCSP responder.
 *
 * This class defines a platform interface.
 * This class is a container.
 *
 * @ref OpOCSPResponseVerifier uses this class to check the OCSP response.
 *
 * Usage:
 *
 *		@code
 *		// Recommended to be class variables.
 *		// Create an empty request container.
 *		OpOCSPRequest*          m_request  = OpOCSPRequest::Create();
 *		OpOCSPRequestProducer*  m_producer = OpOCSPRequestProducer::Create();
 *		OpOCSPResponseVerifier* m_verifier = OpOCSPResponseVerifier::Create();
 *		// ...
 *		// Produce OCSP request.
 *		m_producer->SetCertificate(certificate);
 *		m_producer->SetIssuerCertificate(issuer_certificate);
 *		m_producer->SetOutput(m_request);
 *		m_producer->ProcessL();
 *		// ...
 *		// Export as DER.
 *		unsigned int request_length = m_request->CalculateDERLengthL();
 *		const unsigned char* request_der = OP_NEWA_L(unsigned_char*, request_length);
 *		m_request->ExportAsDERL(request_der);
 *		// ...
 *		// Send request_der to OCSP responder.
 *		// ...
 *		// Receive response_der from OCSP responder.
 *		// ...
 *		// Verify OCSP response.
 *		m_verifier->SetOCSPResponseDER(response_der, response_length);
 *		// Set the original OCSP request as one of the parameters.
 *		m_verifier->SetOCSPRequest(m_request);
 *		m_verifier->SetCertificateChain(chain);
 *		m_verifier->SetCAStorage(ca_storage);
 *		m_verifier->ProcessL();
 *		CryptoCertificateChain::VerifyStatus verify_status =
 *			m_verifier->GetVerifyStatus();
 *		// ...
 *		OP_DELETE(m_verifier);
 *		OP_DELETE(m_producer);
 *		OP_DELETE(m_request);
 *		@endcode
 *
 */
class OpOCSPRequest
{
public:
	static OpOCSPRequest* Create();
	virtual ~OpOCSPRequest() {}

public:
	/** Calculate OCSP request length if converted to DER.
	 *
	 * @return  The calculated DER length.
	 */
	virtual unsigned int CalculateDERLengthL() const = 0;

	/** Export OCSP request as DER.
	 *
	 * The exported DER is owned by the caller. The caller must have
	 * sufficient memory allocated at der pointer. It's possible
	 * to find out how much is needed using @ref CalculateDERLengthL method.
	 *
	 * @param[out]  der  Where to write DER to.
	 */
	virtual void ExportAsDERL(unsigned char* der) const = 0;
};

#endif // CRYPTO_OCSP_SUPPORT
#endif // OP_OCSP_REQUEST_H
