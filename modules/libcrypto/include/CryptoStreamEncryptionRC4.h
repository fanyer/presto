/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_STREAM_ENCRYPTION_RC4_H
#define CRYPTO_STREAM_ENCRYPTION_RC4_H

/**
 * @file CryptoStreamEncryptionRC4.h
 *
 * RC4 symmetric crypto algorithm porting interface. There is a tweak that decides
 * if the algorithm is implemented by core(libopeay)  or the platform.
 */

#ifdef CRYPTO_API_SUPPORT
#ifdef CRYPTO_STREAM_ENCRYPTION_RC4_SUPPORT


class CryptoStreamEncryptionRC4 // import API_CRYPTO_STREAM_ENCRYPTION_RC4
{
public:
	virtual ~CryptoStreamEncryptionRC4(){};

	static CryptoStreamEncryptionRC4 *CreateRC4(int key_size);

	virtual OP_STATUS Init(const UINT8 *key_and_iv) = 0;

	virtual void Encrypt(const UINT8 *source,  UINT8 *target, int len) = 0;
	virtual void Decrypt(const UINT8 *source,  UINT8 *target, int len) = 0;


	virtual int GetKeySize() = 0;
};

#endif // CRYPTO_STREAM_ENCRYPTION_RC4_SUPPORT
#endif // CRYPTO_API_SUPPORT
#endif /* CRYPTO_STREAM_ENCRYPTION_RC4_H */
