/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#ifdef USE_SSL_CERTINSTALLER
#include "modules/libssl/sslbase.h"
#include "modules/libssl/certs/certinst_base.h"

#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"
#include "modules/libssl/handshake/asn1certlist.h"

#include "modules/libssl/external/openssl/eayhead.h"
#ifdef SSL_ENABLE_READ_PKCS12
#include "modules/libopeay/openssl/pkcs12.h"

void CleanP12(PKCS12 *pkcs12, EVP_PKEY *pkey, STACK_OF(X509) *ca, X509 *cert)
{
	if(pkcs12)
		PKCS12_free(pkcs12);
	if(pkey)
		EVP_PKEY_free(pkey);
	if(ca)
		sk_X509_free(ca);
	if(cert)
		X509_free(cert);
}
#endif

void CleanP7(PKCS7 *cert7, PKCS7_SIGNED *cert7s, NETSCAPE_CERT_SEQUENCE *certns)
{
	if(cert7)
		PKCS7_free(cert7);
	if(cert7s)
		PKCS7_SIGNED_free(cert7s);
	if(certns)
		NETSCAPE_CERT_SEQUENCE_free(certns);
}


OP_STATUS ExtractCertificates(SSL_varvector32 &input, const OpStringC8 &pkcs12_password,
							  SSL_ASN1Cert_list &certificate_list, SSL_secure_varvector32 &private_key,
							  SSL_varvector16 &pub_key_hash, uint16 &bits, SSL_BulkCipherType &type)
{
	PKCS7 *cert7=NULL;
	PKCS7_SIGNED *cert7s=NULL;
	NETSCAPE_CERT_SEQUENCE *certns=NULL;
	STACK_OF(X509) *certs=NULL;
	X509 *actcert=NULL;
	uint16 i,n;
	
	certificate_list.Resize(0);
	private_key.Resize(0);
	pub_key_hash.Resize(0);
	bits = 0;
	type = SSL_RSA;



	ERR_clear_error();
	
	// Test PKCS#12 certificate
#ifdef SSL_ENABLE_READ_PKCS12
	do{ // while (0) // one pass only // break when handling errors
		PKCS12 *pkcs12 = NULL;
		EVP_PKEY *pkey = NULL;
		STACK_OF(X509) *ca = NULL;
		const unsigned char *packdata = input.GetDirect();
		pkcs12 = d2i_PKCS12(NULL, &packdata, (long) input.GetLength());
		
		if(pkcs12 == NULL)
		{
			// Not PKCS #12
			break;
		}
		
		if(!PKCS12_parse(pkcs12, pkcs12_password.CStr(), &pkey, &actcert, &ca))
		{
			CleanP12(pkcs12, pkey, ca, actcert);

			unsigned long  err;
			while((err = ERR_get_error()) != 0)
			{
				int lib = ERR_GET_LIB(err);
				int reason = ERR_GET_REASON(err);
				if(lib == ERR_LIB_PKCS12 && reason == PKCS12_R_MAC_VERIFY_FAILURE)
					return InstallerStatus::ERR_PASSWORD_NEEDED;
				if(lib == ERR_LIB_EVP && reason == EVP_R_UNKNOWN_PBE_ALGORITHM)
					return InstallerStatus::ERR_UNSUPPORTED_KEY_ENCRYPTION;
			}


			return InstallerStatus::ERR_PARSING_FAILED;
		}
		
		if(!pkey)
		{
			CleanP12(pkcs12, pkey, ca, actcert);
			return InstallerStatus::ERR_NO_PRIVATE_KEY;
		}
		
		SSL_varvector16 pub_key;
		if(!i2d_PublicKey_Vector(pkey, pub_key))
		{
			CleanP12(pkcs12, pkey, ca, actcert);
			return InstallerStatus::ERR_PARSING_FAILED;
		}
		
		if(pub_key.ErrorRaisedFlag || pub_key.GetLength() == 0)
		{
			CleanP12(pkcs12, pkey, ca, actcert);
			return InstallerStatus::ERR_PARSING_FAILED;
		}
		
		if(actcert)
		{
			EVP_PKEY *cpub_key = X509_get_pubkey(actcert);
			
			SSL_varvector16 cpub_key1;
			
			if(!cpub_key || !i2d_PublicKey_Vector(cpub_key, cpub_key1) || cpub_key1.GetLength() == 0)
			{
				EVP_PKEY_free(cpub_key);
				CleanP12(pkcs12, pkey, ca, actcert);
				return InstallerStatus::ERR_PARSING_FAILED;
			}
			EVP_PKEY_free(cpub_key);
			
			if(cpub_key1.ErrorRaisedFlag)
			{
				CleanP12(pkcs12, pkey, ca, actcert);
				return cpub_key1.GetOPStatus();
			}
			
			if(pub_key != cpub_key1)
			{
				CleanP12(pkcs12, pkey, ca, actcert);
				return InstallerStatus::ERR_WRONG_PRIVATE_KEY;
			}
			
			int i, cacount = (ca != NULL ? sk_X509_num(ca) : 0);
			certificate_list.Resize(cacount +1);
			if(certificate_list.ErrorRaisedFlag)
			{
				certificate_list.Resize(0);
				CleanP12(pkcs12, pkey, ca, actcert);
				return certificate_list.GetOPStatus();
			}
			
			i2d_X509_Vector(actcert, certificate_list[0]);
			for(i = 1; i<= cacount;i++)
			{
				if(!i2d_X509_Vector(sk_X509_value(ca, i-1), certificate_list[i]))
				{
					certificate_list.Resize(0);
					CleanP12(pkcs12, pkey, ca, actcert);

					return InstallerStatus::ERR_PARSING_FAILED;
				}
			}
		}
		if(certificate_list.ErrorRaisedFlag)
		{
			OP_STATUS op_err = certificate_list.GetOPStatus();
			CleanP12(pkcs12, pkey, ca, actcert);
			certificate_list.Resize(0);
			return op_err;
		}
		
		if(!i2d_PrivateKey_Vector(pkey, private_key))
		{
			certificate_list.Resize(0);
			CleanP12(pkcs12, pkey, ca, actcert);
			return InstallerStatus::ERR_PARSING_FAILED;
		}
		if(private_key.ErrorRaisedFlag)
		{
			OP_STATUS op_err = private_key.GetOPStatus();
			CleanP12(pkcs12, pkey, ca, actcert);
			certificate_list.Resize(0);
			private_key.Resize(0);
			return op_err;
		}

		SSL_Hash_Pointer hash(SSL_SHA);
		
		hash->CompleteHash(pub_key, pub_key_hash);
		if(pub_key_hash.ErrorRaisedFlag)
		{
			OP_STATUS op_err = private_key.GetOPStatus();
			CleanP12(pkcs12, pkey, ca, actcert);
			certificate_list.Resize(0);
			private_key.Resize(0);
			return op_err;
		}
		
		switch(pkey->type)
		{
		case NID_rsa:
		case NID_rsaEncryption:
			type = SSL_RSA;
			break;
		case NID_dsa:
			type = SSL_DSA;
			break;
		case NID_dhKeyAgreement:
			type = SSL_DH;
			break;
		}
		bits = EVP_PKEY_bits(pkey);

		CleanP12(pkcs12, pkey, ca, actcert);
		return  InstallerStatus::OK;
		
	}while (0); // break to clean up;
#endif

	// Test Pure X509 certificate
	actcert = d2i_X509_Vector(NULL,input);
	
	if(actcert != NULL)
	{
		certificate_list.Resize(1);
		certificate_list[0] = input;
		X509_free(actcert);
		return certificate_list.GetOPStatus();
	}
		
#ifdef TST_DUMP
	DumpTofile(source,len,"loaded binary cert","sslmult.txt");
#endif      
	// Test Multicert certificate alternatives
	cert7 = d2i_PKCS7_Vector(NULL,input);
	if(cert7)
	{
		if(cert7 == NULL || OBJ_obj2nid(cert7->type) != NID_pkcs7_signed)
		{
			CleanP7(cert7, cert7s, certns);
			
			return InstallerStatus::ERR_PARSING_FAILED;
		}
		certs = cert7->d.sign->cert;
	}
	else
	{
		cert7s = d2i_PKCS7_SIGNED_Vector(NULL,input);
		if(cert7s)
		{
			certs = cert7s->cert;
		}
		else
		{
			certns = d2i_NETSCAPE_CERT_SEQUENCE_Vector(NULL,input);
			
			if(!certns)
			{
				CleanP7(cert7, cert7s, certns);
				return InstallerStatus::ERR_PARSING_FAILED;
			}
			
			certs = certns->certs;
		}
	}
	
	if(!certs || sk_X509_num(certs) == 0)
	{
		CleanP7(cert7, cert7s, certns);
		
		return InstallerStatus::ERR_PARSING_FAILED;
	}
	
	n = sk_X509_num(certs);
	certificate_list.Resize(n);
	for(i=0;i<n;i++)
	{
		actcert = (X509 *) sk_X509_value(certs,i);
		
		if(actcert != NULL)
			i2d_X509_Vector(actcert, certificate_list[i]);

		if(actcert == NULL || certificate_list.ErrorRaisedFlag)
		{
			CleanP7(cert7, cert7s, certns);

			certificate_list.Resize(0);
			return InstallerStatus::ERR_PARSING_FAILED;
		}
		
#ifdef TST_DUMP
		DumpTofile(certchain[i],len1,"loaded cert","sslmult.txt");
#endif
	}
	CleanP7(cert7, cert7s, certns);

	return InstallerStatus::OK;
}
							  
#endif // USE_SSL_CERTINSTALLER

#endif
							  
							  
