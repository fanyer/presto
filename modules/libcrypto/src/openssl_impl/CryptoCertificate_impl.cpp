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

#include "modules/libcrypto/src/openssl_impl/CryptoCertificate_impl.h"

#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/pem.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


OP_STATUS CryptoCertificate::CreateBase64(CryptoCertificate*& certificate, const char *certificate_der, int length)
{
	CryptoCertificate_impl* certificate_impl;
	OP_STATUS status = CryptoCertificate_impl::CreateBase64(certificate_impl, certificate_der, length);
	// Passing both certificate value and status code through.
	certificate = certificate_impl;
	return status;
}

OP_STATUS CryptoCertificate_impl::CreateBase64(CryptoCertificate_impl*& certificate, const char *certificate_der, int length)
{
	if (!certificate_der || length <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Buffer for decoded certificate in DER form.
	unsigned char *certificate_binary = OP_NEWA(unsigned char, length);
	if (!certificate_binary)
		return OpStatus::ERR_NO_MEMORY;
	ANCHOR_ARRAY(unsigned char, certificate_binary);

	// Other needed variables.
	unsigned long read_pos;
	BOOL warning;
	const unsigned char* certificate_b64 =
		reinterpret_cast<const unsigned char*>(certificate_der);
	
	// Decode DER object from Base64 encoding.
	unsigned long certificate_binary_length =
		GeneralDecodeBase64(certificate_b64, length, read_pos,
			certificate_binary, warning, length);

	// Checking result.
	if (certificate_binary_length == 0)
		return OpStatus::ERR;
	OP_ASSERT((int)read_pos == length);

	// Temporary variable as recommended by "man d2i_X509".
	const unsigned char* certificate_binary_tmp = certificate_binary;
	// Construct X509 object.
	X509* x509 = d2i_X509(NULL, &certificate_binary_tmp, certificate_binary_length);
	OPENSSL_VERIFY_OR_RETURN(x509);

	return CreateCertOrDeleteX509(certificate, x509);
}


OP_STATUS CryptoCertificate_impl::CreateFromPEM(CryptoCertificate_impl*& certificate, const char* pem, size_t pem_len)
{
	if (!pem || pem_len == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	BIO* bio = BIO_new_mem_buf((void*) pem, pem_len);
	OPENSSL_VERIFY_OR_RETURN(bio);

	// Load certificate into X509 structure.
	X509* x509 = PEM_read_bio_X509(bio, 0, 0, 0);
	BIO_free(bio);
	OPENSSL_VERIFY_OR_RETURN(x509);
	// We own x509.

	return CreateCertOrDeleteX509(certificate, x509);
}


CryptoCertificate_impl::~CryptoCertificate_impl()
{
	if (m_key)
	{
		OPENSSL_free(m_key);
		m_key = NULL;
	}

	if (m_openssl_certificate)
	{
		X509_free(m_openssl_certificate);
		m_openssl_certificate = NULL;
	}
}


CryptoCertificate_impl::CryptoCertificate_impl(X509 *openssl_certificate) 
		: m_openssl_certificate(openssl_certificate)
		, m_key(NULL)
		, m_len(0)
{
}

const byte* CryptoCertificate_impl::GetPublicKey(int &len)
{
	OP_ASSERT(m_openssl_certificate);

	if (!m_key)
	{
		// No key - let's fetch it.
		X509_PUBKEY* x509_pubkey = X509_get_X509_PUBKEY(m_openssl_certificate);
		OP_ASSERT(x509_pubkey);
		// x509_pubkey is still owned by m_openssl_certificate.

		// Convert to DER.
		m_len = i2d_X509_PUBKEY(x509_pubkey, &m_key);
		if (m_len <= 0)
		{
			m_key = 0;
			m_len = 0;
			ERR_clear_error();
		}
		else
		{
			OP_ASSERT(m_key);
		}
	}
	
	len = m_len;
	return m_key;
}


#ifdef CRYPTO_OCSP_SUPPORT
OP_STATUS CryptoCertificate_impl::CalculateDERLength(int& len) const
{
	if (!m_openssl_certificate)
		return OpStatus::ERR;

	len = i2d_X509(m_openssl_certificate, NULL);
	OPENSSL_RETURN_IF(len <= 0);

	return OpStatus::OK;
}


OP_STATUS CryptoCertificate_impl::ExportAsDER(byte* der) const
{
	if (!der)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Temporary variable as recommended by "man i2d_X509".
	unsigned char* der_tmp = der;

	int len = i2d_X509(m_openssl_certificate, &der_tmp);
	OPENSSL_RETURN_IF(len <= 0);

	return OpStatus::OK;
}


OP_STATUS CryptoCertificate_impl::GetOCSPResponderURL(OpString& url_name) const
{
	OP_ASSERT(m_openssl_certificate);

	// Get AuthorityInfoAccess extension (defined in RFC2459, section 4.2.2.1).
	void* info_access_voidp =
		X509_get_ext_d2i(m_openssl_certificate, NID_info_access, NULL, NULL);
	AUTHORITY_INFO_ACCESS* info_access =
		reinterpret_cast <AUTHORITY_INFO_ACCESS*> (info_access_voidp);
	OPENSSL_VERIFY_OR_RETURN(info_access);

	// Default error.
	OP_STATUS status = OpStatus::ERR;

	// Look for the URL.
	int item_count = sk_ACCESS_DESCRIPTION_num(info_access);
	for (int i = 0; i < item_count; i++)
	{
		ACCESS_DESCRIPTION* info_item = sk_ACCESS_DESCRIPTION_value(info_access, i);
		if (!info_item)
			continue;

		if (OBJ_obj2nid(info_item->method) == NID_ad_OCSP && 
		    info_item->location->type == GEN_URI && 
		    info_item->location->d.ia5)
		{
			status = url_name.Set(
				(const char *) info_item->location->d.ia5->data,
				info_item->location->d.ia5->length);
			break;
		}
	}
	sk_ACCESS_DESCRIPTION_pop_free(info_access, ACCESS_DESCRIPTION_free);
	OP_ASSERT(ERR_peek_error() == 0);

	return status;
}
#endif // CRYPTO_OCSP_SUPPORT


OP_STATUS CryptoCertificate_impl::Init()
{
	int issuer_pos = X509_NAME_get_index_by_NID(m_openssl_certificate->cert_info->issuer, NID_organizationName, 0);
	X509_NAME_ENTRY *issuer = X509_NAME_get_entry(m_openssl_certificate->cert_info->issuer,  issuer_pos);
	
	int common_name_pos = X509_NAME_get_index_by_NID(m_openssl_certificate->cert_info->subject, NID_commonName, 0);
	X509_NAME_ENTRY *common_name = X509_NAME_get_entry(m_openssl_certificate->cert_info->subject,  common_name_pos);

	ASN1_TIME *not_before = m_openssl_certificate->cert_info->validity->notBefore;
	ASN1_TIME *not_after = m_openssl_certificate->cert_info->validity->notAfter;

	// Just to make sure hash is computed
	X509_check_purpose(m_openssl_certificate, -1, 0);

	// The above used OpenSSL functions supposedly cannot set errors.
	OP_ASSERT(ERR_peek_error() == 0);

	if (common_name)
	{
		RETURN_IF_ERROR(m_short_name.SetFromUTF8(reinterpret_cast<char *>(common_name->value->data), common_name->value->length));
		RETURN_IF_ERROR(m_full_name.SetFromUTF8(reinterpret_cast<char *>(common_name->value->data), common_name->value->length));
	}

	OpString8 time_buffer;
	
	if (not_before)
	{
		RETURN_IF_ERROR(PrintASN1Time(not_before, time_buffer));
		RETURN_IF_ERROR(m_valid_from.SetFromUTF8(time_buffer.CStr()));
	}
		
	if (not_after)
	{
		RETURN_IF_ERROR(PrintASN1Time(not_after, time_buffer));
		RETURN_IF_ERROR(m_valid_to.SetFromUTF8(time_buffer.CStr()));
	}


	if (issuer)
		RETURN_IF_ERROR(m_issuer.SetFromUTF8(reinterpret_cast<char *>(issuer->value->data), issuer->value->length));
	
	return m_extra_data.SetFromUTF8("");
}

OP_STATUS CryptoCertificate_impl::CreateCertOrDeleteX509(CryptoCertificate_impl*& certificate, X509* x509)
{
	certificate = OP_NEW(CryptoCertificate_impl, (x509));
	if (!certificate)
	{
		X509_free(x509);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = certificate->Init();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(certificate);
		// Extra safety.
		certificate = NULL;
	}

	return status;
}

OP_STATUS CryptoCertificate_impl::PrintASN1Time(ASN1_TIME *time, OpString8 &time_string)
{
	OP_ASSERT(time);

	BIO *bio = BIO_new(BIO_s_mem());
	OPENSSL_VERIFY_OR_RETURN(bio);

	// Print time to BIO.
	int res = ASN1_TIME_print(bio, time);
	if (res != 1)
	{
		OP_ASSERT(res == 0);
		ERR_clear_error();
		BIO_free(bio);
		OPENSSL_VERIFY_OR_RETURN(FALSE);
	}

	// Get printed text from bio.
	const char* data;
	long len = BIO_get_mem_data(bio, &data);
	// The function supposedly cannot fail.
	OP_ASSERT(ERR_peek_error() == 0);
	OP_ASSERT(data);
	OP_ASSERT(len > 0);
	OP_ASSERT(len <= INT_MAX);

	OP_STATUS status = time_string.Set(data, len);

	// Deallocation after using data, as it points inside bio.
	BIO_free(bio);

	return status;
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
