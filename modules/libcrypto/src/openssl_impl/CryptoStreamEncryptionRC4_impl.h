/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_STREAM_ENCRYPTION_RC4_IMPL_H
#define CRYPTO_STREAM_ENCRYPTION_RC4_IMPL_H

#ifdef CRYPTO_STREAM_ENCRYPTION_RC4_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoStreamEncryptionRC4.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/rc4.h"


class CryptoStreamEncryptionRC4_impl : public CryptoStreamEncryptionRC4 // import API_CRYPTO_STREAM_ENCRYPTION
{
public:
	virtual inline OP_STATUS Init(const UINT8 *key);

	virtual inline void Encrypt(const UINT8 *source,  UINT8 *target, int len);
	virtual inline void Decrypt(const UINT8 *source,  UINT8 *target, int len);

	virtual inline int GetKeySize() { return m_key_len; }

	virtual ~CryptoStreamEncryptionRC4_impl(){};

	CryptoStreamEncryptionRC4_impl(int key_len);

private:
	RC4_KEY m_key;
	int m_key_len;
	BOOL m_encrypt;
};

#endif // CRYPTO_STREAM_ENCRYPTION_RC4_SUPPORT
#endif /* CRYPTO_STREAM_ENCRYPTION_RC4_IMPL_H */
