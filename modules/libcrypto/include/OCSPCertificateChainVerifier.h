/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPCertificateChainVerifier.h
 *
 * OCSP certificate chain verifier interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OP_OCSP_CERTIFICATE_CHAIN_VERIFIER_H
#define OP_OCSP_CERTIFICATE_CHAIN_VERIFIER_H

#ifdef CRYPTO_OCSP_SUPPORT

#include "modules/libcrypto/include/CryptoCertificateChain.h"
#include "modules/url/url2.h"
#include "modules/util/opautoptr.h"

class OpOCSPRequest;
class OpOCSPRequestProducer;
class OpOCSPResponseVerifier;


/**
 * @class OCSPCertificateChainVerifier
 *
 * This class takes a certificate chain and verifies it using OCSP.
 *
 * The class user is supposed to start the verification, then wait
 * for MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED message,
 * unless an exception has been thrown during the starting process.
 *
 * This class is a processor.
 * It is asynchronous.
 *
 * Usage:
 *
 * 1) Starting:
 *
 *		@code
 *		// Recommended to be a class variable.
 *		OCSPCertificateChainVerifier m_verifier;
 *		// ...
 *		m_verifier.SetCertificateChain(certificate_chain);
 *		m_verifier.SetCAStorage(ca_storage);
 *		g_main_message_handler->SetCallBack(this,
 *			MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED, m_verifier.Id());
 *		m_verifier.ProcessL();
 *		@endcode
 *
 * 2) Verification result handling:
 *
 *		@code
 *		// After the class user's class has received
 *		// MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED.
 *		CryptoCertificateChain::VerifyStatus verify_status = m_verifier.GetVerifyStatus();
 *
 *		switch (verify_status)
 *		{
 *			case CryptoCertificateChain::OK_CHECKED_WITH_OCSP:
 *				// Handle success.
 *
 *			case CryptoCertificateChain::CERTIFICATE_REVOKED:
 *				// Handle error.
 *
 *			default:
 *				OP_ASSERT(verify_status == CryptoCertificateChain::VERIFY_STATUS_UNKNOWN);
 *				// Decrease security level.
 *		}
 *		@endcode
 *
 */
class OCSPCertificateChainVerifier : public MessageObject
{
public:
	OCSPCertificateChainVerifier();
	virtual ~OCSPCertificateChainVerifier();

public:
	// Functions to be called to start the verification.

	/** Set the certificate chain to be verified. */
	void SetCertificateChain(const CryptoCertificateChain* certificate_chain);

	/** Set trusted CA storage. */
	void SetCAStorage(const CryptoCertificateStorage* ca_storage);

	/** Sets retry count for failed OCSP requests.
	 *  Default is 0 (no retry).
	 */
	void SetRetryCount(unsigned int retry_count);

	/** Sets timeout (in seconds) for OCSP requests.
	 *  Default is 60 seconds.
	 */
	void SetTimeout(unsigned int timeout);

	/** Helper function for getting this object's id. */
	MH_PARAM_1 Id() const;

	/** Process asynchronous verification of the certificate chain.
	 *
	 * Exception means that something went wrong during launching,
	 * for example OOM, bad input parameters or internal error.
	 * In this case no verification-finished message will be posted afterwards.
	 *
	 * If the exception is not thrown - the caller (or its appointed listener)
	 * should wait for the callback with the following parameters:
	 *
	 * - msg  == MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED;
	 * - par1 == Id() - this verifier object's id;
	 * - par2 == 0.
	 */
	void ProcessL();

public:
	// Functions to be called after the verification is finished.

	/** Get the chain's OCSP verification status.
	 *
	 * Can be one of the following:
	 *
	 * - OK_CHECKED_WITH_OCSP
	 * - CERTIFICATE_REVOKED
	 * - VERIFY_STATUS_UNKNOWN
	 */
	CryptoCertificateChain::VerifyStatus GetVerifyStatus() const;

public:
	// MessageObject methods
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	void InitL();
	void FetchOCSPResponderURLL(OpString& responder_url_name);
	void ProduceOCSPRequestL();
	void PrepareURLL(const OpStringC& responder_url_name);
	void SubmitOCSPRequest();
	void ProcessURLHeader();
	void StopLoading();
	void ProcessURLDataLoaded();
	void FetchDataDescriptor();
	void ProcessURLLoadingFailed();
	void ProcessURLLoadingFinished();
	void ProcessURLLoadingFinishedL();
	void NotifyAboutFinishedVerification();

private:
	// These objects are set from outside. Pointed objects are not owned.
	const CryptoCertificateChain*   m_certificate_chain;
	const CryptoCertificateStorage* m_ca_storage;

private:
	// These objects are created and owned by this object.
	URL       m_url;
	URL_InUse m_url_inuse;
	OpAutoPtr <OpOCSPRequest>          m_request;
	OpAutoPtr <OpOCSPRequestProducer>  m_request_producer;
	OpAutoPtr <OpOCSPResponseVerifier> m_response_verifier;
	CryptoCertificateChain::VerifyStatus m_verify_status;
	unsigned int m_retry_count;
	unsigned int m_timeout;
	BOOL m_explicit_timeout_set;

private:
	// Integer constants.
	enum
	{
		// This is the maximum size for OCSP response.
		// Usual responses are just few KB in size.
		// The whole response will be read into memory for parsing,
		// so it makes sense to limit it to some sane value.
		// 640K should be enough for everyone.
		OCSP_RESPONSE_SIZE_LIMIT = 640 * 1024
	};
};


// Inlines implementation.

inline void OCSPCertificateChainVerifier::SetRetryCount(unsigned int retry_count)
{
	m_retry_count = retry_count;
}

inline void OCSPCertificateChainVerifier::SetTimeout(unsigned int timeout)
{
	m_timeout = timeout;
	m_explicit_timeout_set = TRUE;
	// There is no way to set m_explicit_timeout_set back to FALSE.
	// Nobody needed it so far. It can be implemented if needed.
}

#endif // CRYPTO_OCSP_SUPPORT
#endif // OP_OCSP_CERTIFICATE_CHAIN_VERIFIER_H
