/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland   <haavardm@opera.com>
** @author Alexei Khlebnikov <alexeik@opera.com>
**
*/

#include "core/pch.h"

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/src/openssl_impl/CryptoCertificateStorage_impl.h"

#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/src/openssl_impl/CryptoCertificate_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


CryptoCertificateStorage_impl::CryptoCertificateStorage_impl()
	: m_certificate_store(NULL)
{
}

CryptoCertificateStorage_impl::~CryptoCertificateStorage_impl()
{
	X509_STORE_free(m_certificate_store);
	OP_ASSERT(ERR_peek_error() == 0);
}


OP_STATUS CryptoCertificateStorage::Create(CryptoCertificateStorage *&storage)
{
	return CryptoCertificateStorage_impl::Create(storage);
}

OP_STATUS CryptoCertificateStorage_impl::Create(CryptoCertificateStorage *&storage)
{
	storage = NULL;
	OpAutoPtr<CryptoCertificateStorage_impl> temp_storage(OP_NEW(CryptoCertificateStorage_impl, ()));
	RETURN_OOM_IF_NULL(temp_storage.get());

	temp_storage->m_certificate_store = X509_STORE_new();
	OPENSSL_VERIFY_OR_RETURN(temp_storage->m_certificate_store);

	storage = temp_storage.release();
	return OpStatus::OK;
}


OP_STATUS CryptoCertificateStorage_impl::AddToCertificateBase64(const char *certificate_der)
{
	if (!certificate_der)
		return OpStatus::ERR_OUT_OF_RANGE;

	int length = op_strlen(certificate_der);
	if (length == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	CryptoCertificate_impl* certificate = NULL;
	OP_STATUS status = CryptoCertificate_impl::CreateBase64(certificate, certificate_der, length);
	RETURN_IF_ERROR(status);
	OP_ASSERT(certificate);

	return AddOrDeleteCertificate(certificate);
}


OP_STATUS CryptoCertificateStorage_impl::AddPEM(const char* pem, size_t pem_len)
{
	CryptoCertificate_impl* certificate = NULL;
	OP_STATUS status = CryptoCertificate_impl::CreateFromPEM(certificate, pem, pem_len);
	RETURN_IF_ERROR(status);
	OP_ASSERT(certificate);

	return AddOrDeleteCertificate(certificate);
}


OP_STATUS CryptoCertificateStorage_impl::AddOrDeleteCertificate(CryptoCertificate_impl* certificate)
{
	OP_STATUS status = m_certificate_list.Add(certificate);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(certificate);
		return status;
	}

	X509* x509 = certificate->GetX509();
	OP_ASSERT(x509);

	int ret = X509_STORE_add_cert(m_certificate_store, x509);
	if (ret != 1)
	{
		OP_ASSERT(ret == 0);
		// x509 is owned by certificate. So no need to deallocate it explicitly.
		// Remove certificate from the list and deallocate it.
		m_certificate_list.Delete(certificate);
		OPENSSL_VERIFY_OR_RETURN(FALSE);
	}

	return OpStatus::OK;
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
