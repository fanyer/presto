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

#if defined(_SSL_SUPPORT_) && (defined(_EXTERNAL_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_))

#include "modules/externalssl/externalssl.h"
#include "modules/url/tools/arrays.h"
#include "modules/libssl/sslenum.h"

struct External_Cipher_spec{
	cipherentry_st id;
	const uni_char *fullname_format; // Always a sprintf format string with one (1) %d
	const uni_char *KEAname;
	const uni_char *EncryptName;
	const uni_char *HashName; 
	SSL_SecurityRating security_rating;    
	SSL_AuthentiRating authentication;
};

#define EXT_SSL_CIPHER_SPEC(v1, v2, desc, kea, enc, hash, sec, auth) \
	CONST_ENTRY_BLOCK_START \
	{ CONST_DOUBLE_ENTRY(id.id[0], v1, id.id[1], v2) } \
	CONST_ENTRY_SINGLE(fullname_format,desc)\
	CONST_ENTRY_SINGLE(KEAname,kea)\
	CONST_ENTRY_SINGLE(EncryptName,enc)\
	CONST_ENTRY_SINGLE(HashName,hash)\
	CONST_ENTRY_SINGLE(security_rating,sec)\
	CONST_ENTRY_SINGLE(authentication,auth)\
	CONST_ENTRY_END

PREFIX_CONST_ARRAY(static, External_Cipher_spec, ExtSSL_Cipher_list, externalssl)
	// Preferred sequence
	// 1
	EXT_SSL_CIPHER_SPEC(0x00,0x04,
		UNI_L("128 bit C4 (RSA/MD5)"), UNI_L("RSA"), UNI_L("C4"), UNI_L("MD5"), 
		Secure, Authenticated)
	
	// 2
	EXT_SSL_CIPHER_SPEC(0x00,0x05,
		UNI_L("128 bit C4 (RSA/SHA)"), UNI_L("RSA"), UNI_L("C4"), UNI_L("SHA"), 
		Secure, Authenticated)

#ifdef _SUPPORT_TLS_1_2
	// 2
	EXT_SSL_CIPHER_SPEC(  0x00,0x3B,
		UNI_L("0 bit Authentication Only (RSA/SHA-256)"), UNI_L("RSA"), UNI_L("No encryption"), UNI_L("SHA-256"), 
		Vulnerable, Authenticated)
#endif
 
#ifdef __SSL_WITH_DH_DSA__
	// 3
	EXT_SSL_CIPHER_SPEC(  0x00,0x1B,
		UNI_L("168 bit 3-DES (Anonym DH/SHA)"), UNI_L("DH_Anon"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Vulnerable, No_Authentication)
	// 4
	EXT_SSL_CIPHER_SPEC(  0x00,0x18,
		UNI_L("128 bit ARC4 (Anonym DH/MD5)"), UNI_L("DH_Anon"), UNI_L("ARC4_128"), UNI_L("SHA"), 
		Vulnerable, No_Authentication)

#ifdef _SUPPORT_TLS_1_2
	// 3
	EXT_SSL_CIPHER_SPEC(  0x00,0x6C,
		UNI_L("128 bit AES (Anonym DH/SHA-256)"), UNI_L("DH_Anon"), UNI_L("AES_128"), UNI_L("SHA-256"), 
		Vulnerable, No_Authentication)

	// 4
	EXT_SSL_CIPHER_SPEC(  0x00,0x6D,
		UNI_L("256 bit AES (Anonym DH/SHA-256)"), UNI_L("DH_Anon"), UNI_L("AES_256"), UNI_L("SHA-256"), 
		Vulnerable, No_Authentication)
#endif
#endif

	// 3
	EXT_SSL_CIPHER_SPEC(0x00,0x0A,
		UNI_L("168 bit 3-DES (RSA/SHA)"), UNI_L("RSA"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Secure, Authenticated)

#ifdef __SSL_WITH_DH_DSA__
	// 6
	EXT_SSL_CIPHER_SPEC(  0x00,0x10,
		UNI_L("168 bit 3DES (DH_RSA/SHA)"), UNI_L("DH_RSA"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Secure, Authenticated)

	// 4
	EXT_SSL_CIPHER_SPEC(0x00,0x16,
		UNI_L("168 bit 3DES (DHE_RSA/SHA)"), UNI_L("DHE_RSA"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Secure, Authenticated)

	// 8
	EXT_SSL_CIPHER_SPEC(  0x00,0x0D,
		UNI_L("168 bit 3DES (DH_DSS/SHA)"), UNI_L("DH_DSS"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Secure, Authenticated)

	// 5
	EXT_SSL_CIPHER_SPEC(0x00,0x13,
		UNI_L("168 bit 3DES (DHE_DSS/SHA)"), UNI_L("DHE_DSS"), UNI_L("3DES_EDE_CBC"), UNI_L("SHA"), 
		Secure, Authenticated)
#endif

//	// 6
//	EXT_SSL_CIPHER_SPEC(0x00,0x12,
//		UNI_L("56 bit DES (DHE_DSS/SHA)"), UNI_L("DHE_DSS"), UNI_L("DES_CBC"), UNI_L("SHA"), 
//		Vulnerable, Authenticated)
//
//	// 7
//	EXT_SSL_CIPHER_SPEC(0x00,0x09,
//		UNI_L("56 bit DES (RSA/SHA)"), UNI_L("RSA"), UNI_L("DES_CBC"), UNI_L("SHA"), 
//		Vulnerable, Authenticated)

	// 8
/*	EXT_SSL_CIPHER_SPEC(0x00,0x15,
		UNI_L("56 bit DES (DHE_RSA/SHA)"), UNI_L("DHE_RSA"), UNI_L("DES_CBC"), UNI_L("SHA"), 
		Vulnerable, Authenticated)

	// 9
	EXT_SSL_CIPHER_SPEC(0x00,0x11,
		UNI_L("40 bit DES (DHE_DSS/SHA) Exportable"), UNI_L("DHE_DSS_EXPORT"), UNI_L("DES_CBC_40"), UNI_L("SHA"), 
		Vulnerable, Authenticated)

	// 10
	EXT_SSL_CIPHER_SPEC(0x00,0x14,
		UNI_L("40 bit DES (DHE_RSA/SHA) Exportable "), UNI_L("DHE_RSA_EXPORT"), UNI_L("DES_CBC_40"), UNI_L("SHA"), 
		Vulnerable, Authenticated)
*/
/* Presently not supported */
/*
	// 11
	EXT_SSL_CIPHER_SPEC(0x00,0x62,
		"56 bit DES (RSA/SHA) Exportable", "RSA_EXPORT", "DES56_CBC", "SHA", 
		Vulnerable, Authenticated)

	// 12
	EXT_SSL_CIPHER_SPEC(0x00,0x64,
		"56 bit C4 (RSA/SHA) Exportable", "RSA_EXPORT", "C4_56", "SHA", 
		Vulnerable, Authenticated)
*/

	// 13
//	EXT_SSL_CIPHER_SPEC(0x00,0x08,
//		UNI_L("40 bit DES (RSA/SHA) Exportable"), UNI_L("RSA_EXPORT"), UNI_L("DES40_CBC"), UNI_L("SHA"), 
//		Vulnerable, Authenticated)
//
//	// 14
//	EXT_SSL_CIPHER_SPEC(0x00,0x03,
//		UNI_L("40 bit C4 (RSA/MD5) Exportable"), UNI_L("RSA_EXPORT"), UNI_L("C4_40"), UNI_L("MD5"), 
//		Vulnerable, Authenticated)

#ifndef OPENSSL_NO_AES
	// 12
	EXT_SSL_CIPHER_SPEC(  0x00,0x2F,
		UNI_L("128 bit AES (RSA/SHA)"), UNI_L("RSA"), UNI_L("AES-128"), UNI_L("SHA"), 
		Secure, Authenticated)

#ifdef __SSL_WITH_DH_DSA__
	// 13
	EXT_SSL_CIPHER_SPEC(  0x00,0x30,
		UNI_L("128 bit AES (DH_DSS/SHA)"), UNI_L("DH_DSS"), UNI_L("AES-128"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 14
	EXT_SSL_CIPHER_SPEC(  0x00,0x31,
		UNI_L("128 bit AES (DH_RSA/SHA)"), UNI_L("DH_RSA"), UNI_L("AES-128"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 15
	EXT_SSL_CIPHER_SPEC(  0x00,0x32,
		UNI_L("128 bit AES (DHE_DSS/SHA)"), UNI_L("DHE_DSS"), UNI_L("AES-128"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 16
	EXT_SSL_CIPHER_SPEC(  0x00,0x33,
		UNI_L("128 bit AES (DHE_RSA/SHA)"), UNI_L("DHE_RSA"), UNI_L("AES-128"), UNI_L("SHA"), 
		Secure, Authenticated)
#endif
#ifdef _SUPPORT_TLS_1_2
	// 12
	EXT_SSL_CIPHER_SPEC(  0x00,0x3C,
		UNI_L("128 bit AES (RSA/SHA-256)"), UNI_L("RSA"), UNI_L("AES"), UNI_L("SHA-256"), 
		Secure, Authenticated)

#ifdef __SSL_WITH_DH_DSA__
	// 13
	EXT_SSL_CIPHER_SPEC(  0x00,0x3E,
		UNI_L("128 bit AES (DH_DSS/SHA-256)"), UNI_L("DH_DSS"), UNI_L("AES"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 14
	EXT_SSL_CIPHER_SPEC(  0x00,0x3F,
		UNI_L("128 bit AES (DH_RSA/SHA-256)"), UNI_L("DH_RSA"), UNI_L("AES"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 15
	EXT_SSL_CIPHER_SPEC(  0x00,0x40,
		UNI_L("128 bit AES (DHE_DSS/SHA-256)"), UNI_L("DHE_DSS"), UNI_L("AES"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 16
	EXT_SSL_CIPHER_SPEC(  0x00,0x67,
		UNI_L("128 bit AES (DHE_RSA/SHA-256)"), UNI_L("DHE_RSA"), UNI_L("AES"), UNI_L("SHA-256"), 
		Secure, Authenticated)
#endif
#endif
	// 17
	EXT_SSL_CIPHER_SPEC(  0x00,0x35,
		UNI_L("256 bit AES (RSA/SHA)"), UNI_L("RSA"), UNI_L("AES-256"), UNI_L("SHA"), 
		Secure, Authenticated)

#ifdef __SSL_WITH_DH_DSA__
	// 18
	EXT_SSL_CIPHER_SPEC(  0x00,0x36,
		UNI_L("256 bit AES (DH_DSS/SHA)"), UNI_L("DH_DSS"), UNI_L("AES-256"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 19
	EXT_SSL_CIPHER_SPEC(  0x00,0x37,
		UNI_L("256 bit AES (DH_RSA/SHA)"), UNI_L("DH_RSA"), UNI_L("AES-256"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 19
	EXT_SSL_CIPHER_SPEC(  0x00,0x38,
		UNI_L("256 bit AES (DHE_DSS/SHA)"), UNI_L("DHE_DSS"), UNI_L("AES-256"), UNI_L("SHA"), 
		Secure, Authenticated)
	// 20
	EXT_SSL_CIPHER_SPEC(  0x00,0x39,
		UNI_L("256 bit AES (DHE_RSA/SHA)"), UNI_L("DHE_RSA"), UNI_L("AES-256"), UNI_L("SHA"), 
		Secure, Authenticated)
#endif

#ifdef _SUPPORT_TLS_1_2
	// 17
	EXT_SSL_CIPHER_SPEC(  0x00,0x3D,
		UNI_L("256 bit AES (RSA/SHA)"), UNI_L("RSA"), UNI_L("AES-256"), UNI_L("SHA-256"), 
		Secure, Authenticated)

#ifdef __SSL_WITH_DH_DSA__
	// 18
	EXT_SSL_CIPHER_SPEC(  0x00,0x68,
		UNI_L("256 bit AES (DH_DSS/SHA-256)"), UNI_L("DH_DSS"), UNI_L("AES-256"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 19
	EXT_SSL_CIPHER_SPEC(  0x00,0x69,
		UNI_L("256 bit AES (DH_RSA/SHA-256)"), UNI_L("DH_RSA"), UNI_L("AES-256"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 19
	EXT_SSL_CIPHER_SPEC(  0x00,0x6A,
		UNI_L("256 bit AES (DHE_DSS/SHA-256)"), UNI_L("DHE_DSS"), UNI_L("AES-256"), UNI_L("SHA-256"), 
		Secure, Authenticated)
	// 20
	EXT_SSL_CIPHER_SPEC(  0x00,0x6B,
		UNI_L("256 bit AES (DHE_RSA/SHA-256)"), UNI_L("DHE_RSA"), UNI_L("AES-256"), UNI_L("SHA-256"), 
		Secure, Authenticated)
#endif

#endif
#endif // OPENSSL_NO_AES
CONST_END(Cipher_ciphers)

External_SSL_Options::External_SSL_Options()
{
	TLS_enabled = TRUE;
	cipher_count = 0;

	Ciphers = new cipherentry_st[CONST_ARRAY_SIZE(externalssl, ExtSSL_Cipher_list)];
	if(Ciphers != NULL)
	{
		// Enable All ciphers
		size_t i;
		for(i=0; i< CONST_ARRAY_SIZE(externalssl, ExtSSL_Cipher_list); i++)
		{
			Ciphers[cipher_count] = g_ExtSSL_Cipher_list[i].id;
			cipher_count ++;
		}
	}
}


External_SSL_Options::~External_SSL_Options()
{
	delete [] Ciphers;
}

const uni_char *External_SSL_Options::GetCipher(int item, UINT8 *cipher_id, BOOL &presently_enabled)
{
	presently_enabled = FALSE;
	if(item <0 || ((size_t) item) >CONST_ARRAY_SIZE(externalssl, ExtSSL_Cipher_list))
		return NULL;

	UINT8 c1 = g_ExtSSL_Cipher_list[item].id.id[0];
	UINT8 c2 = g_ExtSSL_Cipher_list[item].id.id[1];

	if(cipher_id)
	{
		cipher_id[0] = c1;
		cipher_id[1] = c2;
	}
	
	size_t i;
	for(i=0; i< cipher_count; i++)
	{
		if(Ciphers[i].id[0] == c1 && Ciphers[i].id[1] == c2)
		{
			presently_enabled = TRUE;
			break;
		}
	}

	return g_ExtSSL_Cipher_list[item].fullname_format;
}

const External_Cipher_spec *External_SSL_Options::LocateCipherSpec(UINT8 *cipher_id)
{
	return g_external_ssl_options->InternalLocateCipherSpec(cipher_id);
}

const External_Cipher_spec *External_SSL_Options::InternalLocateCipherSpec(UINT8 *cipher_id)
{
	if(cipher_id == NULL)
		return NULL;

	UINT8 c1 = cipher_id[0];
	UINT8 c2 = cipher_id[1];
	
	size_t i;
	for(i=0; i< CONST_ARRAY_SIZE(externalssl, ExtSSL_Cipher_list); i++)
	{
		if(g_ExtSSL_Cipher_list[i].id.id[0] == c1 && g_ExtSSL_Cipher_list[i].id.id[1] == c2)
		{
			return &g_ExtSSL_Cipher_list[i];
		}
	}

	return NULL;
}
const uni_char *External_SSL_Options::GetCipher(UINT8 *cipher_id, int &security_level)
{
	const External_Cipher_spec *spec = LocateCipherSpec(cipher_id);

	if(spec)
	{
		security_level = spec->security_rating;
		return spec->fullname_format;
	}

	return NULL;
}

void External_SSL_Options::SetEnableTLS(BOOL enabled)
{
	TLS_enabled = enabled;
}

#endif // HTTP_DIGEST_AUTH
