/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_CERTIFICATE_CHAIN_H
#define CRYPTO_CERTIFICATE_CHAIN_H

#ifdef CRYPTO_API_SUPPORT
#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

class CryptoCertificate;
class CryptoCertificateStorage;


class CryptoCertificateChain
{
public:
	enum VerifyStatus
	{
		/** Verification status is unknown. */
		VERIFY_STATUS_UNKNOWN,

		/** Verification succeeded locally.
		 *
		 *  Verification of the certificate succeeded. The certificate
		 *  was verified successfully using the local CA storage.
		 *
		 *  OCSP check, however, could not be performed.
		 *  Some possible reasons are:
		 *  - certificate didn't provide a URL to the OCSP server
		 *  - the OCSP server couldn't be contacted
		 *  - the OCSP server didn't give a valid response
		 *  - more than one problem of such kind.
		 */
		OK_CHECKED_LOCALLY,

		/** Verification succeeded with OCSP.
		 *
		 *  Verification of the certificate succeeded. The certificate
		 *  was verified successfully using both the local CA storage
		 *  and the issuer's OCSP server.
		 */
		OK_CHECKED_WITH_OCSP,

		CERTIFICATE_GENERIC,
		CERTIFICATE_BAD_CERTIFICATE,
		CERTIFICATE_EXPIRED,
		CERTIFICATE_REVOKED,
		CERTIFICATE_INVALID_CERTIFICATE_CHAIN,
		CERTIFICATE_INVALID_SIGNATURE,
	};

	virtual ~CryptoCertificateChain(){}

	static OP_STATUS Create(CryptoCertificateChain *&chain);
	virtual OP_STATUS AddToChainBase64(const char *certificate_der) = 0; /* FIXME: should be implemented by core */

	/** Get length of the certificate chain. */
	virtual int GetNumberOfCertificates() const = 0;

	/**
	 * Get a certificate from the certificate chain.
	 *
	 * @param index [in] Index of the needed certificate.
	 *                   Numbering starts from 0.
	 *
	 * @return Certificate or NULL on error.
	 *         The object at the returned pointer is owned by the chain,
	 *         the caller shouldn't destruct it himself.
	 *         The returned pointer becomes invalid after
	 *         VerifyChainSignatures() call.
	 */
	virtual const CryptoCertificate* GetCertificate(int index) const = 0;
	virtual CryptoCertificate* GetCertificate(int index) = 0;

	/**
	 * Remove a certificate from the certificate chain and return it.
	 *
	 * @param index [in] Index of the needed certificate.
	 *                   Numbering starts from 0.
	 *
	 * @return Certificate or NULL on error.
	 *         The object at the returned pointer is owned by the caller,
	 *         he should destruct it using OP_DELETE.
	 */
	virtual CryptoCertificate* RemoveCertificate(int index) = 0;

	/** Verify the certificate chain against the supplied storage. */
	virtual OP_STATUS VerifyChainSignatures(VerifyStatus& reason, const CryptoCertificateStorage* ca_storage) = 0;
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
#endif // CRYPTO_API_SUPPORT

#endif /* CRYPTO_CERTIFICATE_CHAIN_H */
