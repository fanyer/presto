/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#ifndef CRYPTO_RANDOM_H_
#define CRYPTO_RANDOM_H_

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslrand.h"
#else
void SSL_RND(UINT8 *target,UINT32 len);
void SSL_SEED_RND(byte *source,UINT32 len);
#endif

/**  Random generator. Will also be used for ssl when external ssl is turned off  */

class CryptoStreamCipherSnow;

class OpRandomGenerator
{
public:
	virtual ~OpRandomGenerator();
	
	/** Create a new generator.
	 * 
	 * @param secure If TRUE a certain amount of entropy must
	 * 				have been added before you can retrieve random
	 * 				numbers.
	 * @param feed_entropy_automatically if TRUE this generator will be automatically be feed 
	 * 									 with random data from ui, messagehandler, etc.
	 * @return The generator
	 */
	static OpRandomGenerator *Create(BOOL secure = TRUE, BOOL feed_entropy_automatically = TRUE);
	
	/** Feed the random generator with random bits.
	 * 
	 * The random generator needs at least n random 
	 * bits in total to be able to produce n-bit output 
	 * that takes O(2^n) computational time to guess. 
	 * Around 128 random bits should be enough. However, 
	 * most n-bit data chunck does not have n random bits.
	 * Give a roughly estimate of  number of random bits in 
	 * estimated_random_bits.
	 * 
	 * Maximum entropy of this function is currently 576 bits. 
	 * 2^576, however, is roughly number_of_atoms_in_universe^2.
	 *  
	 * 
	 * This function does not re-seed the generator. 
	 * 
	 * @param data		The random data
	 * @param length	The length of data in bytes
	 * @param estimated_random_bits	The estimated entropy or number
	 * 								of random bits in data.
	 */
	void AddEntropy(const void *data, int length, int estimated_random_bits = -1);
	
	/** Feed all random generators.
	 * 
	 * Same as AddEntropy for all created random generators.
	 *
	 * This function does not re-seed the generators.
	 * 
	 * @param data	The random data
	 * @param length The length of data in bytes
	 * @param estimated_random_bits The estimated entropy or number
	 * 								of random bits in data.
	 */
	static void AddEntropyAllGenerators(const void *data, int length, int estimated_random_bits = -1);
	
	/** Add time entropy (uncertainty)
	 * 
	 * Feed all random generators with time as random input.
	 */
	static void AddEntropyFromTimeAllGenerators();
	
	/** Get random bytes.
	 * 
	 * Note that bytes array must to be allocated by caller.
	 * 
	 * @param [out] bytes  The returned random data
	 * @param [in]  length The size of the pre-allocated bytes array
	 */	
	void GetRandom(UINT8 *bytes, int length);

	/** Get random hex string.
	 *
	 * Produces a string of random hex characters.
	 *
	 * @param [out] random_hex_string  The returned random data.
	 * @param [in]  length             The length of the random hex string.
	 */
	OP_STATUS GetRandomHexString(OpString8 &random_hex_string, int length);
	OP_STATUS GetRandomHexString(OpString &random_hex_string, int length);

	/** Get 8 random bits
	 * 
	 * @return 8 random bits. 
	 */
	UINT8 GetUint8();

	/** Get 16 random bits
	 * 
	 * @return 16 random bits.
	 */
	UINT16 GetUint16();
	
	/** Get 32 random bits
	 * 
	 * @return 32 random bits.
	 */
	UINT32 GetUint32();
	
	/** Get a double between 0 and 1.
	 * 	The double has 53 bits entropy(uncertainty) 
	 * 
	 * @return x, where  0 < x < 1.
	 */
	double GetDouble();
	
	/** Get the amount of entropy 
	 * 
	 * @return The amount of entropy (randomness) of the generator
	 */
	UINT GetEntropyState() { return m_entropy; }
	
private:
	OpRandomGenerator(BOOL secure, BOOL feed_entropy_automatically);
	OP_STATUS Init();
	CryptoStreamCipherSnow *m_random_source;
	UINT m_counter;
	UINT m_entropy;
	UINT m_number_of_feedings;
	BOOL m_secure;
	BOOL m_feed_entropy_automatically;
	UINT32 m_stream;
	
};

#endif /* CRYPTO_RANDOM_H_ */
