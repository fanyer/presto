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

#ifndef _CIPHER_LIST_H_
#define _CIPHER_LIST_H_

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/cipher_struct.h"

PREFIX_CONST_ARRAY(static, struct Cipher_spec, Cipher_ciphers, libssl)
		
	SSL_CIPHER_SPEC(0x00,0x02, NoCipher, SHA, RSA, V)			// 2

#ifdef _SUPPORT_TLS_1_2
	SSL_CIPHER_SPEC(0x00,0x3B, NoCipher, SHA_256, RSA, V)		// 3
#endif

#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x1B, 3DES, SHA, ADH, V)				// 4

#ifdef _SUPPORT_TLS_1_2
	SSL_CIPHER_SPEC(0x00,0x6C, AES_128, SHA_256,  ADH, V)		// 6
	SSL_CIPHER_SPEC(0x00,0x6D, AES_256, SHA_256, ADH, V)		// 7
#endif
#endif

	SSL_CIPHER_SPEC(0x00,0x0A, 3DES, SHA, RSA, S)				// 8
#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x10, 3DES, SHA, DH, S)				// 9
	SSL_CIPHER_SPEC(0x00,0x16, 3DES, SHA, DHE, S)				// 10
	SSL_CIPHER_SPEC(0x00,0x0D, 3DES, SHA, DH_DSS, S)			// 11
	SSL_CIPHER_SPEC(0x00,0x13, 3DES, SHA, DHE_DSS, S)			// 12
#endif

	SSL_CIPHER_SPEC(0x00,0x04, RC4, MD5, RSA, S)				// 13
	SSL_CIPHER_SPEC(0x00,0x05, RC4, SHA, RSA, S)				// 14

#ifndef OPENSSL_NO_AES
	SSL_CIPHER_SPEC(0x00,0x2F, AES_128, SHA, RSA, S)			// 15
#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x30, AES_128, SHA, DH_DSS, S)			// 16
	SSL_CIPHER_SPEC(0x00,0x31, AES_128, SHA, DH, S)				// 17
	SSL_CIPHER_SPEC(0x00,0x32, AES_128, SHA, DHE_DSS, S)		// 18
	SSL_CIPHER_SPEC(0x00,0x33, AES_128, SHA, DHE, S)			// 19
#endif
#ifdef _SUPPORT_TLS_1_2
	SSL_CIPHER_SPEC(0x00,0x3C, AES_128, SHA_256, RSA, S)		// 20
#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x3E, AES_128, SHA_256, DH_DSS, S)		// 21
	SSL_CIPHER_SPEC(0x00,0x3F, AES_128, SHA_256, DH, S)			// 22
	SSL_CIPHER_SPEC(0x00,0x40, AES_128, SHA_256, DHE_DSS, S)	// 23
	SSL_CIPHER_SPEC(0x00,0x67, AES_128, SHA_256, DHE, S)		// 24
#endif
#endif

	SSL_CIPHER_SPEC(0x00,0x35, AES_256, SHA, RSA, S)			// 25
#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x36, AES_256, SHA, DH_DSS, S)			// 26
	SSL_CIPHER_SPEC(0x00,0x37, AES_256, SHA, DH, S)				// 27
	SSL_CIPHER_SPEC(0x00,0x38, AES_256, SHA, DHE_DSS, S)		// 28
	SSL_CIPHER_SPEC(0x00,0x39, AES_256, SHA, DHE, S)			// 29
#endif
#ifdef _SUPPORT_TLS_1_2
	SSL_CIPHER_SPEC(0x00,0x3D, AES_256, SHA_256, RSA, S)		// 30
#ifdef __SSL_WITH_DH_DSA__
	SSL_CIPHER_SPEC(0x00,0x68, AES_256, SHA_256, DH_DSS, S)		// 31
	SSL_CIPHER_SPEC(0x00,0x69, AES_256, SHA_256, DH, S)			// 32
	SSL_CIPHER_SPEC(0x00,0x6A, AES_256, SHA_256, DHE_DSS, S)	// 33
	SSL_CIPHER_SPEC(0x00,0x6B, AES_256, SHA_256, DHE, S)		// 34
#endif

#endif
#endif // OPENSSL_NO_AES
CONST_END(Cipher_ciphers)

#endif
#endif
