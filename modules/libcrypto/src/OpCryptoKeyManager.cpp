/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(CRYPTO_KEY_MANAGER_SUPPORT)

#include "modules/libcrypto/include/OpCryptoKeyManager.h"
#include "modules/libcrypto/include/CryptoHash.h"


/* static */ OP_STATUS OpCryptoKeyManager::GetObfuscatedDeviceKey(UINT8 *key, int key_length)
{
	if (!key || key_length <= 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	OpCryptoKeyManager::GetDeviceKey(key, key_length);

	CryptoHash *hasher;
	hasher = CryptoHash::CreateSHA1();
	if (hasher == NULL)
		return OpStatus::ERR_NO_MEMORY;

	UINT8 *obfuscated_key = OP_NEWA(byte, hasher->Size());
	if (!obfuscated_key)
	{
		OP_DELETE(hasher);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	hasher->InitHash();
	hasher->CalculateHash(key, key_length);
	hasher->ExtractHash(obfuscated_key);
	
	for (int i = 0; i < key_length; i++)
	{
		key[i] ^= (0xaa^obfuscated_key[i%hasher->Size()]);
		hasher->CalculateHash(key, key_length);
		hasher->ExtractHash(obfuscated_key);
	}
	
	OP_DELETEA(obfuscated_key);
	OP_DELETE(hasher);
	return OpStatus::OK;
}

#endif // CRYPTO_KEY_MANAGER_SUPPORT
