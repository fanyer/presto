/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_CERTIFICATE_H
#define CRYPTO_CERTIFICATE_H

#ifdef CRYPTO_API_SUPPORT
#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT


class CryptoCertificate
{
public:
	virtual ~CryptoCertificate() {}
	
	static OP_STATUS CreateBase64(CryptoCertificate *&certificate, const char *certificate_der, int length);
	
	/** Get the short name of the certificate. */
	virtual const uni_char* GetShortName() const = 0;

	/** Get the full name of the certificate. */
	virtual const uni_char* GetFullName() const = 0;

	/** Get the issuer of the certificate. */
	virtual const uni_char* GetIssuer() const = 0;

	virtual const uni_char* GetValidFrom() const = 0;

	virtual const uni_char* GetValidTo() const = 0;
	
	virtual const uni_char* GetInfo() const = 0;

	/** Get any extra data related to the certificate.
	 *
	 * This can include public keys, fingerprints, etc.
	 */
	virtual const byte* GetPublicKey(int& len) = 0;
	virtual const byte* GetCertificateHash(int& len) const = 0;

#ifdef CRYPTO_OCSP_SUPPORT
	/** Calculate certificate length if converted to DER.
	 *
	 * @param[out]  len  The calculated DER length.
	*/
	virtual OP_STATUS CalculateDERLength(int& len) const = 0;

	/** Export certificate as DER.
	 *
	 * The exported DER is owned by the caller. The caller must have
	 * sufficient memory allocated at der pointer. It's possible
	 * to find out how much is needed using @ref CalculateDERLength method.
	 *
	 * @param[out]  der  Where to write DER to.
	*/
	virtual OP_STATUS ExportAsDER(byte* der) const = 0;

	/** Get URL for checking the certificate validity using OCSP.
	 *
	 * @param[out]  url_name  The URL to OCSP responder service.
	 *
	 * @return  OpStatus::OK if OCSP URL is successfully retreived.
	 *          OpStatus::ERR or another error code on error,
	 *          for example if the certificate's extensions
	 *          don't contain the OCSP responder URL.
	*/
	virtual OP_STATUS GetOCSPResponderURL(OpString& url_name) const = 0;
#endif // CRYPTO_OCSP_SUPPORT
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
#endif // CRYPTO_API_SUPPORT

#endif /* CRYPTO_CERTIFICATE_H */
