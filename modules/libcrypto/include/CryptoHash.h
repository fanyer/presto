/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_HASH_H
#define CRYPTO_HASH_H

/*
 * @file CryptoHash.h
 *
 * Hash algorithms porting interface. For each algorithm there is a tweak that decides
 * if the algorithm is implemented by core(libopeay)  or the platform.
 */

#ifdef CRYPTO_API_SUPPORT

// Maximum known hash size in bytes.
// It is 64 bytes == 512 bits, size of SHA512.
#define CRYPTO_MAX_HASH_SIZE 64


enum CryptoHashAlgorithm
{
	CRYPTO_HASH_TYPE_UNKNOWN,

#ifdef 	CRYPTO_HASH_MD5_SUPPORT
	CRYPTO_HASH_TYPE_MD5,
#endif

#ifdef 	CRYPTO_HASH_SHA1_SUPPORT
	CRYPTO_HASH_TYPE_SHA1,
#endif

#ifdef 	CRYPTO_HASH_SHA256_SUPPORT
	CRYPTO_HASH_TYPE_SHA256,
#endif

	/* Add new hash functions here */

	CRYPTO_HASH_TYPES_END
};


class CryptoHash
{
public:
	virtual ~CryptoHash(){};

	/** Create the hasher with the needed CryptoHashAlgorithm.
	 *
	 * The hasher is owned by the caller and must be destructed
	 * using OP_DELETE(). The created hasher is not initialized.
	 * The caller must initialize it himself using InitHash().
	 *
	 * Calling this function is equivalent to calling one of
	 * the more specific functions below.
	 *
	 * @param algorithm The algorithm to use for hash calculation.
	 *
	 * @return newly created hasher on success, NULL if the supplied
	 *         algorithm is unknown or OOM happened.
	 */
	static CryptoHash* Create(CryptoHashAlgorithm algorithm);

#ifdef CRYPTO_HASH_MD5_SUPPORT	// import API_CRYPTO_HASH_MD5
	static CryptoHash *CreateMD5();
#endif // CRYPTO_HASH_MD5_SUPPORT

#ifdef CRYPTO_HASH_SHA1_SUPPORT // import API_CRYPTO_HASH_SHA1
	static CryptoHash *CreateSHA1();
#endif	// CRYPTO_HASH_SHA1_SUPPORT

#ifdef CRYPTO_HASH_SHA256_SUPPORT // import API_CRYPTO_HASH_SHA256
	static CryptoHash *CreateSHA256();
#endif	// CRYPTO_HASH_SHA256_SUPPORT

	/** Reset the inner state
	 *
	 * @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS InitHash() = 0;

	/** Calculate the hash from bytes
	 *
	 * @param source The data to be hashed
	 * @param len The length of the data
	 */
	virtual void CalculateHash(const UINT8 *source, int len) = 0;

	/** Calculate hash from string
	 *
	 * Equivalent to @ref CalculateHash((const byte*)char_str, op_strlen(char_str));
	 *
	 * @param source_string The string to be hashed
	 */
	virtual void CalculateHash(const char *source_string) = 0;

	/** Return the size of the compression block in bytes
	 *
	 * These values should be returned (from http://en.wikipedia.org/wiki/Cryptographic_hash_function);
	 *
	 * SHA1:64
	 * MD5:64
	 * SHA256/224:64
	 * SHA512/384:1024
	 *
	 * @return the block size of the hash function
	 */
	virtual int  BlockSize() = 0;

	/** The digest output size in bytes
	 *
	 * @return the size of the output of ExtractHash()
	 */
	virtual int  Size() = 0;

	/** Get the digest
	 *
	 * @param [out] result The result of the hashing. The byte
	 * array must be at least have length Size().
	 */
	virtual void ExtractHash(UINT8 *result) = 0;
};

#endif // CRYPTO_API_SUPPORT
#endif /* CRYPTO_HASH_H */
