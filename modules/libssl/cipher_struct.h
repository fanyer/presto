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

#ifndef _CIPHER_STRUCT_H_
#define _CIPHER_STRUCT_H_

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/url/tools/arrays.h"

#ifdef HAS_COMPLEX_GLOBALS
#define SSL_CIPHER_ID_V3_SPEC(id1, id2) , {id1, id2} 
#else
#define SSL_CIPHER_ID_V3_SPEC(id1, id2) \
		CONST_ENTRY_SINGLE(id[0], id1)\
		CONST_ENTRY_SINGLE(id[1],id2)
#endif

#define TLS_1_2_ENTRY(a_hash, a_prf)\
		CONST_ENTRY_SINGLE(hash_tls12, a_hash)\
		CONST_ENTRY_SINGLE(prf_fun, a_prf)

#ifdef _SUPPORT_TLS_1_2
#define TLS_1_2_ALGS(a_hash, a_prf) TLS_1_2_ENTRY(hash, prf)
#define TLS_1_2_DEFALGS TLS_1_2_ENTRY(SSL_SHA_256, TLS_PRF_default)
#define TLS_1_2_NO_ALGS TLS_1_2_ENTRY(SSL_NoHash, TLS_PRF_default)
#else
#define TLS_1_2_ALGS(hash, prf) 
#define TLS_1_2_DEFALGS 
#define TLS_1_2_NO_ALGS 
#endif

#define SSL_CIPHER_SPEC0( id1, id2, _method, _hash, _kea_alg, _sigalg, _KeySize, \
		_fullname, _KEAname, _EncryptName, _HashName, \
		_security_rating, _authentication, _low_reason) \
		CONST_ENTRY_START(method,_method)\
		CONST_ENTRY_SINGLE(hash,_hash)\
		CONST_ENTRY_SINGLE(kea_alg,_kea_alg)\
		CONST_ENTRY_SINGLE(sigalg,_sigalg)\
		TLS_1_2_DEFALGS\
		SSL_CIPHER_ID_V3_SPEC(id1, id2)\
		CONST_ENTRY_SINGLE(KeySize,_KeySize) \
		CONST_ENTRY_SINGLE(fullname,_fullname)\
		CONST_ENTRY_SINGLE(KEAname,_KEAname)\
		CONST_ENTRY_SINGLE(EncryptName,_EncryptName)\
		CONST_ENTRY_SINGLE(HashName,_HashName)\
		CONST_ENTRY_SINGLE(security_rating,_security_rating)\
		CONST_ENTRY_SINGLE(authentication,_authentication)\
		CONST_ENTRY_SINGLE(low_reason,_low_reason)\
		CONST_ENTRY_END

struct Cipher_spec{
	SSL_BulkCipherType method;
	SSL_HashAlgorithmType hash;
	SSL_KeyExchangeAlgorithm  kea_alg;
	SSL_SignatureAlgorithm sigalg;
#ifdef _SUPPORT_TLS_1_2
	SSL_HashAlgorithmType hash_tls12;
	TLS_PRF_func	prf_fun;
#endif
	uint8 id[2];
	uint16 KeySize;
	const char *fullname;
	const char *KEAname;
	const char *EncryptName;
	const char *HashName;  
	SSL_SecurityRating security_rating;    
	SSL_AuthentiRating authentication;
	BYTE		low_reason;
};

#define CIPHER_NoCipher_Method SSL_NoCipher
#define CIPHER_NoCipher_Keysize 0
#define CIPHER_NoCipher_Text "0 bit Authentication Only"

#define CIPHER_3DES_Method SSL_3DES
#define CIPHER_3DES_Keysize 24
#define CIPHER_3DES_Text "168 bit 3-DES"

#define CIPHER_RC4_Method SSL_RC4
#define CIPHER_RC4_Keysize 16
#define CIPHER_RC4_Text "128 bit ARC4"

#define CIPHER_AES_128_Method SSL_AES_128_CBC
#define CIPHER_AES_128_Keysize 16
#define CIPHER_AES_128_Text "128 bit AES"

#define CIPHER_AES_256_Method SSL_AES_256_CBC
#define CIPHER_AES_256_Keysize 32
#define CIPHER_AES_256_Text "256 bit AES"


#define HASH_MD5_Method SSL_MD5
#define HASH_MD5_Text "MD5"

#define HASH_SHA_Method SSL_SHA
#define HASH_SHA_Text "SHA"

#define HASH_SHA_256_Method SSL_SHA_256
#define HASH_SHA_256_Text "SHA-256"

#define PUB_RSA_Method SSL_RSA_KEA
#define PUB_RSA_SigMethod SSL_Anonymous_sign
#define PUB_RSA_Text "RSA"
#define PUB_RSA_AuthStrength Authenticated

#define PUB_ADH_Method SSL_Anonymous_Diffie_Hellman_KEA
#define PUB_ADH_SigMethod SSL_Anonymous_sign
#define PUB_ADH_Text "Anonymous DH"
#define PUB_ADH_AuthStrength No_Authentication

#define PUB_DH_Method SSL_Diffie_Hellman_KEA
#define PUB_DH_SigMethod SSL_RSA_MD5_SHA_sign
#define PUB_DH_Text "DH_RSA"
#define PUB_DH_AuthStrength Authenticated

#define PUB_DHE_Method SSL_Ephemeral_Diffie_Hellman_KEA
#define PUB_DHE_SigMethod SSL_RSA_MD5_SHA_sign
#define PUB_DHE_Text "DHE_RSA"
#define PUB_DHE_AuthStrength Authenticated

#define PUB_DH_DSS_Method SSL_Diffie_Hellman_KEA
#define PUB_DH_DSS_SigMethod SSL_DSA_sign
#define PUB_DH_DSS_Text "DH_DSS"
#define PUB_DH_DSS_AuthStrength Authenticated

#define PUB_DHE_DSS_Method SSL_Ephemeral_Diffie_Hellman_KEA
#define PUB_DHE_DSS_SigMethod SSL_DSA_sign
#define PUB_DHE_DSS_Text "DHE_DSS"
#define PUB_DHE_DSS_AuthStrength Authenticated

#define SUITE_SecLevelV Vulnerable
#define SUITE_SecInfoV SECURITY_LOW_REASON_WEAK_METHOD

#define SUITE_SecLevelS Secure
#define SUITE_SecInfoS SECURITY_REASON_NOT_NEEDED

#define FULLSTRING(C,P,H) C " (" P "/" H ")"

#define SSL_CIPHER_SPEC(id1, id2, meth, hash, pub, level)\
	SSL_CIPHER_SPEC0( id1, id2, \
	CIPHER_##meth##_Method, HASH_##hash##_Method, PUB_##pub##_Method, PUB_##pub##_SigMethod, CIPHER_##meth##_Keysize, \
		FULLSTRING( CIPHER_##meth##_Text , PUB_##pub##_Text , HASH_##hash##_Text ), PUB_##pub##_Text, CIPHER_##meth##_Text, HASH_##hash##_Text, \
		SUITE_SecLevel##level , PUB_##pub##_AuthStrength, SUITE_SecInfo##level)


#endif
#endif
