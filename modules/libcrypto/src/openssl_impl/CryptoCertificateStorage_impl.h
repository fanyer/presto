/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_CERTIFICATE_STORAGE_IMPL_H
#define CRYPTO_CERTIFICATE_STORAGE_IMPL_H

#ifdef  CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoCertificateStorage.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"

class CryptoCertificate;
class CryptoCertificate_impl;


class CryptoCertificateStorage_impl : public CryptoCertificateStorage
{
public:
	/** @name CryptoCertificateStorage methods. */
	/** @{ */
	virtual ~CryptoCertificateStorage_impl();
	static OP_STATUS Create(CryptoCertificateStorage *&storage);
	virtual OP_STATUS AddToCertificateBase64(const char *certificate_der);
	virtual OP_STATUS AddPEM(const char* pem, size_t pem_len);
	/** @} */

public:
	/** @name Implementation-specific methods.
	 *  Only other *_impl classes may call these functions.
	 */
	/** @{ */
	X509_STORE* GetX509Store() const { return m_certificate_store; }
	int TypeCheck() const { return 0x12345679;} // Ugly cast check since we don't have dynamic cast
	/** @} */

private:
	CryptoCertificateStorage_impl();
	OP_STATUS AddOrDeleteCertificate(CryptoCertificate_impl* certificate);

private:
	X509_STORE  *m_certificate_store;
	OpAutoVector<CryptoCertificate> m_certificate_list;
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_CERTIFICATE_STORAGE_IMPL_H */
