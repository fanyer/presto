/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpOCSPResponseVerifier.h
 *
 * OCSP response verifier platform interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OP_OCSP_RESPONSE_VERIFIER_H
#define OP_OCSP_RESPONSE_VERIFIER_H

#ifdef CRYPTO_OCSP_SUPPORT

#include "modules/libcrypto/include/CryptoCertificateChain.h"

class OpOCSPRequest;


/**
 * @class OpOCSPResponseVerifier
 *
 * This class takes OCSP response and verifies it with the help of platform's
 * crypto library.
 *
 * This class defines a platform interface.
 * This class is a processor.
 * It is synchronous.
 *
 * Usage:
 *
 *		@code
 *		// Recommended to be a class variable.
 *		OpOCSPResponseVerifier* m_verifier = OpOCSPResponseVerifier::Create();
 *		...
 *		m_verifier->SetOCSPResponseDER(response_der, response_length);
 *		m_verifier->SetOCSPRequest(request);
 *		m_verifier->SetCertificateChain(chain);
 *		m_verifier->SetCAStorage(ca_storage);
 *		m_verifier->ProcessL();
 *		CryptoCertificateChain::VerifyStatus verify_status =
 *			m_verifier->GetVerifyStatus();
 *		...
 *		// Do something with verify_status.
 *		...
 *		OP_DELETE(m_verifier);
 *		@endcode
 *
 */
class OpOCSPResponseVerifier
{
public:
	static OpOCSPResponseVerifier* Create();
	virtual ~OpOCSPResponseVerifier() {}

public:
	/** Set OCSP response to be verified, DER-encoded. */
	virtual void SetOCSPResponseDER(const unsigned char* response_der, unsigned int response_length) = 0;

	/** Set OCSP request, corresponding to the response. */
	virtual void SetOCSPRequest(const OpOCSPRequest* request) = 0;

	/** Set the chain, built during the local verification of the certificate in question. */
	virtual void SetCertificateChain(const CryptoCertificateChain* certificate_chain) = 0;

	/** Set trusted CA storage. */
	virtual void SetCAStorage(const CryptoCertificateStorage* ca_storage) = 0;

	/** Process OCSP response verification. */
	virtual void ProcessL() = 0;

	/** Get verification result. */
	virtual CryptoCertificateChain::VerifyStatus GetVerifyStatus() const = 0;
};

#endif // CRYPTO_OCSP_SUPPORT
#endif // OP_OCSP_RESPONSE_VERIFIER_H
