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

#ifndef  CRYPTO_SIGNED_TEXTFILE_H
#define CRYPTO_SIGNED_TEXTFILE_H

#if defined(CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT) // import API_CRYPTO_VERIFY_SIGNED_TEXTFILE

#include "modules/libcrypto/include/CryptoHash.h"


/**	First line of input file begins with the init parameter, with the rest 
 *	of the line being a Base64 encoded signature, The signed content starts after the CRLF
 *	The function will return FALSE if signature fails or if any errors occur,
 *
 *	@param	signed_file		URL containing the file to be verified. MUST be loaded, 
 *							which can be accomplished with signed_file.QuickLoad(TRUE)
 *	@param	prefix			Prefix used on the first line, after which the Base64 encoded 
 *							signature follows
 *	@param	key				Pointer to buffer containing the DER encoded public key associated 
 *							with the private key used to generate the signature, MUST be an 
 *							X509_PUBKEY structure (openssl rsa -pubout ... command result)
 *	@param	key_len			Length of the public key buffer
 *
 *	@param  alg				Algorithm used to calculate signature. Default CRYPTO_HASH_TYPE_SHA1
 *
 *	@return TRUE if the verification succeded, FALSE if there was any error.
 */

BOOL CryptoVerifySignedTextFile(URL &signed_file, const OpStringC8 &prefix, const unsigned char *key, unsigned long key_len, CryptoHashAlgorithm alg = CRYPTO_HASH_TYPE_SHA1);

#endif // CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
#endif // CRYPTO_SIGNED_TEXTFILE_H
