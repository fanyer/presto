/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_CERTIFICATE_STORAGE_H
#define CRYPTO_CERTIFICATE_STORAGE_H

#ifdef CRYPTO_API_SUPPORT
#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT


class CryptoCertificateStorage
{
public:
	virtual ~CryptoCertificateStorage(){}

	static OP_STATUS Create(CryptoCertificateStorage *&storage);
	virtual OP_STATUS AddToCertificateBase64(const char *certificate_der) = 0;

	/* Add PEM-encoded certificate. */
	virtual OP_STATUS AddPEM(const char* pem, size_t pem_len) = 0;

protected:
	CryptoCertificateStorage(){};
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
#endif // CRYPTO_API_SUPPORT

#endif /* CRYPTO_CERTIFICATE_STORAGE_H */
