/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef CRYTPO_KEY_MANAGER_H_
#define CRYTPO_KEY_MANAGER_H_

#ifdef CRYPTO_KEY_MANAGER_SUPPORT // import API_CRYPTO_KEY_MANAGER_SUPPORT

/** This api is typically used by mobile phones that wants the profiles
 * to be encrypted with a device/sim card specific key.
 * 
 * Not supposed to be used by normal desktop profiles.
 *  
 */

class OpCryptoKeyManager
{
public:
	/** Get an obfuscated per-device unique key
	 * 
	 * The key array is owned by caller. GetObfuscatedDeviceKey 
	 * will produce a key of length key_length;
	 * 
	 * @param key (out)		The byte array to store the key from the device. 
	 * 						The array must be of length key_length.
	 * @param key_length 	Length of the key to be produced.
	 */ 
	static OP_STATUS GetObfuscatedDeviceKey(UINT8 *key, int key_length);

private:
	/** Get a per-device unique key
	 * 
	 * !! This function must be implemented by the platform !!
	 * 
	 * The key is owned by caller. GetDeviceKey
	 * must produce a key of length key_length
	 * 
	 * GetDeviceKey must produce an unique key  
	 * for each device. 
	 * 
	 * If the device doesn't have a key with 
	 * length key_length, pad the rest with 0.
	 * 
	 * @param key			The key from the device
	 * @param key_length 	Length of the key. 
	 */ 	
	static void GetDeviceKey(UINT8 *key, int key_length);
};

#endif // CRYPTO_KEY_MANAGER_SUPPORT

#endif // CRYTPO_KEY_MANAGER_H_
