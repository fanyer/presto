/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CRYPTO_CERTIFICATE_IMPL_H
#define CRYPTO_CERTIFICATE_IMPL_H

#ifdef  CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoCertificate.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


class CryptoCertificate_impl : public CryptoCertificate
{
public:

	/** Construction/destruction. */
	CryptoCertificate_impl(X509 *openssl_certificate);
	OP_STATUS Init();
	static OP_STATUS CreateBase64(CryptoCertificate_impl*& certificate,
		const char *certificate_der, int length);
	static OP_STATUS CreateFromPEM(CryptoCertificate_impl*& certificate,
		const char* pem, size_t pem_len);
	virtual ~CryptoCertificate_impl();

	/** Get the short name of the certificate. */
	virtual const uni_char* GetShortName() const { return m_short_name.CStr(); }

	/** Get the full name of the certificate. */
	virtual const uni_char* GetFullName() const { return m_full_name.CStr(); }

	/** Get the issuer of the certificate. */
	virtual const uni_char* GetIssuer() const { return m_issuer.CStr(); }

	virtual const uni_char* GetValidFrom() const { return m_valid_from.CStr(); }

	virtual const uni_char* GetValidTo() const { return m_valid_to.CStr(); }
	
	virtual const uni_char* GetInfo() const { return m_info.CStr(); }

	/** Get any extra data related to the certificate.
	 *
	 * This can include public keys, fingerprints, etc.
	 */
	virtual const byte* GetPublicKey(int& len);
	virtual const byte* GetCertificateHash(int& len) const { len = SHA_DIGEST_LENGTH; return m_openssl_certificate->sha1_hash; }

#ifdef CRYPTO_OCSP_SUPPORT
	virtual OP_STATUS CalculateDERLength(int& len) const;
	virtual OP_STATUS ExportAsDER(byte* der) const;
	virtual OP_STATUS GetOCSPResponderURL(OpString& url_name) const;
#endif // CRYPTO_OCSP_SUPPORT

	/** Implementation-specific methods. */
	X509* GetX509() const { return m_openssl_certificate; }

private:
	static OP_STATUS CreateCertOrDeleteX509(CryptoCertificate_impl*& certificate, X509* x509);
	OP_STATUS PrintASN1Time(ASN1_TIME *time, OpString8 &time_string);
	OP_STATUS CreateNames();	

private:
	X509 *m_openssl_certificate;
	unsigned char *m_key;
	unsigned int m_len;
		
	OpString m_short_name;
	OpString m_full_name;
	OpString m_issuer;
	OpString m_extra_data;
	OpString m_valid_from;
	OpString m_valid_to;
	OpString m_info;
	
	friend class CryptoCertificate;
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_CERTIFICATE_IMPL_H */
