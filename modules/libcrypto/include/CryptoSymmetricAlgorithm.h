/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_SYMMETRIC_ALGORITHM_H
#define CRYPTO_SYMMETRIC_ALGORITHM_H

/**
 * @file CryptoSymmetricAlgorithm.h
 *
 * Symmetric crypto algorithm porting interface. There is a tweak that decides
 * if the algorithm is implemented by core(libopeay)  or the platform.
 *
 * More ciphers can be added by adding CreateCIPHERNAME() functions,
 * in the same way the hashes are implemented.
 */

#ifdef CRYPTO_API_SUPPORT

// Maximum known cipher block size in bytes.
// It is 32 bytes == 256 bits, size of AES256.
#define CRYPTO_MAX_CIPHER_BLOCK_SIZE 32

/** @class CryptoSymmetricAlgorithm.
 *
 * Block encryption algorithm.
 *
 * Use to encrypt blocks of data with a symmetric algorithm.
 *
 * Don't use these encryption algorithms directly if you don't
 * really know what you are doing, as it is unsafe without a proper
 * encryption mode.
 *
 * Wrap it in CryptoStreamEncryptionCBC or CryptoStreamEncryptionCFB instead.
 *
 * @see CryptoStreamEncryptionCBC or CryptoStreamEncryptionCFB.
 */
class CryptoSymmetricAlgorithm
{
public:
	virtual ~CryptoSymmetricAlgorithm(){};

#ifdef CRYPTO_ENCRYPTION_AES_SUPPORT // import API_CRYPTO_ENCRYPTION_AES

	/** Create an AES encryption algorithm.
	 *
	 * @param key_size The key size in bytes for AES.
	 *                  Legal values are 16, 24 and 32.
	 */
	static CryptoSymmetricAlgorithm *CreateAES(int key_size = 16);
#endif // CRYPTO_ENCRYPTION_AES_SUPPORT

#ifdef CRYPTO_ENCRYPTION_3DES_SUPPORT // import API_CRYPTO_ENCRYPTION_3DES
	/** Create an 3DES encryption algorithm.
	 *
	 * @param key_size The key size in bytes for 3DES.
	 *                  Legal values are 16 and 24.
	 * @return the algorithm, or NULL for OOM or wrong key size.
	 */
	static CryptoSymmetricAlgorithm *Create3DES(int key_size = 24);
#endif // CRYPTO_ENCRYPTION_3DES_SUPPORT // import API_CRYPTO_ENCRYPTION_3DES

	/** Encrypt one block of data.
	 *
	 * Block size is given by GetBlockSize();
	 *
	 *  @param source Preallocated source array of length GetBlockSize()
	 *  @param target Preallocated target array of length GetBlockSize()
	 */
	virtual void Encrypt(const UINT8 *source,  UINT8 *target) = 0;

	/** Decrypt one block of data.
	 *
	 * Block size is given by GetBlockSize().
	 *
	 *  @param source Preallocated source array of length GetBlockSize()
	 *  @param target Preallocated target array of length GetBlockSize()
	 */
	virtual void Decrypt(const UINT8 *source,  UINT8 *target) = 0;

	/** Sets the encryption key.
	 *
	 * @param key The key of length GetKeySize();
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetEncryptKey(const UINT8 *key) = 0;

	/** Sets the decryption key.
	 *
	 * @param key The key of length GetKeySize().
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetDecryptKey(const UINT8 *key) = 0;

	/** Get key size.
	 *
	 * @return The key size in bytes.
	 * */
	virtual int GetKeySize() const = 0;

	/** Get block size.
	 *
	 * @return The block size in bytes.
	 * */
	virtual int GetBlockSize() const  = 0;
};

#endif // CRYPTO_API_SUPPORT
#endif /* CRYPTO_SYMMETRIC_ALGORITHM_H */
