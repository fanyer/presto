/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if defined USE_SSL_CERTINSTALLER


#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/sslrand.h"

#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libopeay/openssl/x509v3.h"
#include "modules/formats/encoder.h"

#ifdef LIBOPEAY_PKCS12_SUPPORT
#include "modules/libopeay/openssl/pkcs12.h"

BOOL SSL_Options::ExportPKCS12_Key_and_Certificate(const OpStringC &filename, SSL_secure_varvector32 &priv_key, int n, const OpStringC8 &password)
{
	SSL_CertificateItem *item;

	Init(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE | SSL_LOAD_CLIENT_STORE);

	item = Find_Certificate(SSL_ClientStore, n);

	if(item == NULL)
		return TRUE;
	
	OpString8 name;
	SSL_DistinguishedName issuer_name;
	SSL_CertificateItem *issuer;
	PKCS12 *pkcs12 = NULL;
	EVP_PKEY *pkey = NULL;
	X509 *cert = NULL, *cert1;
	STACK_OF(X509) *ca = NULL;
	BOOL ret = FALSE;
	
	
	do{ // while (0) // one pass only // break when handling errors
		pkey = d2i_PrivateKey_Vector(NULL, priv_key);
		if(pkey == NULL)
		{
			break;
		}
		
		cert = d2i_X509_Vector(NULL, item->certificate);
		if(cert == NULL)
			break;
		
		ca = sk_X509_new_null();
		if(ca == NULL)
			break;
		
		cert1 = cert;
		while(cert1 && X509_check_issued(cert1, cert1) != 0)
		{
			X509 *cert2 = cert1;
			if(!i2d_X509_NAME_Vector(X509_get_issuer_name(cert1), issuer_name))
				break;
			cert1 = NULL;
			issuer = NULL;
			while((issuer = FindTrustedCA(issuer_name, issuer)) != NULL)
			{
				cert1 = d2i_X509_Vector(NULL, issuer->certificate);
				if(cert1)
				{
					if(X509_check_issued(cert1, cert2) == 0)
					{
						sk_X509_push(ca, cert1);
						break;
					}
					X509_free(cert1);
					cert1 = NULL;
				}
			}
		}
		
		if (OpStatus::IsError(name.Set(item->cert_title.CStr())))
			break;
		
		pkcs12 = PKCS12_create((/*const*/ char *) password.DataPtr(), /*const*/ name.DataPtr(), pkey, cert, ca,
			NID_pbe_WithSHA1And3_Key_TripleDES_CBC, NID_pbe_WithSHA1And3_Key_TripleDES_CBC, 0, 0,0);
		
		if(!pkcs12)
			break;
		
		SSL_varvector32 binary_certificate;

		OpFile certificate_file;

		i2d_PKCS12_Vector(pkcs12, binary_certificate);
		if(binary_certificate.Error() || binary_certificate.GetLength() == 0)
			break;

		if(OpStatus::IsError(certificate_file.Construct(filename)))
			break;

		if(OpStatus::IsError(certificate_file.Open(OPFILE_WRITE)))
			break;

		if(OpStatus::IsError(certificate_file.Write(binary_certificate.GetDirect(), binary_certificate.GetLength())))
		{
			certificate_file.Close();
			certificate_file.Delete();
			break;
		}
		certificate_file.Close();
	}while (0); // break to clean up;

	if(pkcs12)
		PKCS12_free(pkcs12);
	if(pkey)
		EVP_PKEY_free(pkey);
	if(cert)
		X509_free(cert);
	if(ca)
		sk_X509_free(ca);

	ERR_clear_error();
	return ret;
}
#endif

OP_STATUS SSL_Options::Export_Certificate(const OpStringC &filename, SSL_CertificateStore store, int n, BOOL multiple, BOOL save_as_pem, BOOL full_pkcs7)
{
	SSL_CertificateItem *item;

	Init(SSL_LOAD_ALL_STORES);
	item = Find_Certificate(store, n);

	if(item == NULL)
		return OpStatus::OK;

	SSL_DistinguishedName issuer_name;
	SSL_CertificateItem *issuer;
	PKCS7 *pkcs7 = NULL;
	X509 *cert = NULL, *cert1;
	OP_STATUS ret = OpStatus::ERR;
	
	
	do{ // while (0) // one pass only // break when handling errors

		if(multiple) 
		{
			pkcs7 = PKCS7_new();
			if(pkcs7 == NULL)
				break;
			if(!PKCS7_set_type(pkcs7, NID_pkcs7_signed))
				break;
		}

		cert = d2i_X509_Vector(NULL, item->certificate);
		if(cert == NULL)
			break;
		
		if(pkcs7)
		{
			if(!PKCS7_add_certificate(pkcs7, cert))
				break;
			
			cert1 = cert;
			while(cert1 && X509_NAME_cmp(X509_get_issuer_name(cert1), X509_get_subject_name(cert1)) != 0)
			{
				if(!i2d_X509_NAME_Vector(X509_get_issuer_name(cert1), issuer_name))
					break;
				cert1 = NULL;
				issuer = FindTrustedCA(issuer_name);
				if(issuer)
				{
					cert1 = d2i_X509_Vector(NULL, issuer->certificate);
					if(cert1)
						if(!PKCS7_add_certificate(pkcs7, cert1))
							break;
						
				}
			}
		}

		SSL_varvector32 binary_certificate;
		if(multiple)
		{
			if(full_pkcs7)
				i2d_PKCS7_Vector(pkcs7, binary_certificate);
			else
				i2d_PKCS7_SIGNED_Vector(pkcs7->d.sign, binary_certificate);
			
		}
		else
			i2d_X509_Vector(cert, binary_certificate);

		if(save_as_pem)
		{
			char *b64_encoded_cert = NULL;
			int b64_encoded_cert_len = 0;

			MIME_Encode_SetStr(b64_encoded_cert, b64_encoded_cert_len, (const char *) binary_certificate.GetDirect(), binary_certificate.GetLength(), NULL, GEN_BASE64);

			if(b64_encoded_cert == NULL || b64_encoded_cert_len == 0)
			{
				OP_DELETEA(b64_encoded_cert);
				break;
			}

			binary_certificate.SetResizeBlockSize(b64_encoded_cert_len+100);

			binary_certificate.Set("-----BEGIN CERTIFICATE-----\r\n\r\n");
			binary_certificate.Append(b64_encoded_cert, b64_encoded_cert_len);
			binary_certificate.Append("\r\n-----END CERTIFICATE-----\r\n");
		}
		
		if(binary_certificate.Error() || binary_certificate.GetLength() == 0)
			break;

		OpFile certificate_file;

		if(OpStatus::IsError(certificate_file.Construct(filename)))
			break;

		if(OpStatus::IsError(certificate_file.Open(OPFILE_WRITE)))
			break;

		if(OpStatus::IsError(certificate_file.Write(binary_certificate.GetDirect(), binary_certificate.GetLength())))
		{
			certificate_file.Close();
			certificate_file.Delete();
			break;
		}
		certificate_file.Close();

		ret = OpStatus::OK;
		
	}while (0); // break to clean up;

	if(pkcs7)
		PKCS7_free(pkcs7);
	if(cert)
		X509_free(cert);

	ERR_clear_error();
	return ret;
}
#endif


#endif
