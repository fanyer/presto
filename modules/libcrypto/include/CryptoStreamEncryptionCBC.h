/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CRYPTO_STREAM_ENCRYPTION_CBC_H_
#define CRYPTO_STREAM_ENCRYPTION_CBC_H_

#ifdef CRYPTO_STREAM_ENCRYPTION_CBC_SUPPORT
#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"

/**
 * @class CryptoStreamEncryptionCBC.
 *
 * Wrapper class that turns a block cipher into a CBC mode stream cipher.
 *
 * Turn on this api with API_CRYPTO_STREAM_ENCRYPTION_CBC.
 *
 * The non-padding is trivial but you must make sure the length of the source and encrypted data
 * is a multiple of the underlying ciphers block size when encrypting and decrypting.
 *
 * For simplicity, there is no memory handling.
 *
 * Padding can be used, when source length is not a multiple of the block size. However, in that case
 * the resulting cipher text length may vary. The length of the resulting cipher text is calculated using
 * CalculateEncryptedTargetLength when encrypting. The same holds when decrypting using CalculateDecryptedTargetLength.
 *
 * Usage when using padding.
 * @code
 *
 * void encrypt_stuff(const UINT8 *data_to_encrypt, int data_to_encrypt_len, UINT8 *&encrypted_data, int &encrypted_data_length)
 * {
 *  CryptoStreamEncryptionCBC *cbc = CryptoStreamEncryptionCBC::Create(CryptoSymmetricAlgorithm::CreateAES(16), CryptoStreamEncryptionCBC::PADDING_STRATEGY_PCKS15);
 *
 *  cbc->SetIV( <some iv of length 16 bytes> );
 *  cbc->SetKey( <some key of length 16 bytes> );
 *
 *  encrypted_data_length = cbc->CalculateEncryptedTargetLength(data_to_encrypt_len);
 *  encrypted_data = OP_NEWA(UINT8, encrypted_data_length);
 *  m_des_cbc->Encrypt(data_to_encrypt, encrypted_data, data_to_encrypt_len, encrypted_data_length);
 *
 *  // Done
 *
 * }
 * @endcode
 */
class CryptoStreamEncryptionCBC // import API_CRYPTO_STREAM_ENCRYPTION_CBC
{
public:
	enum PaddingStrategy
	{
		PADDING_STRATEGY_NONE,  // Lengths if source and target must be equal and a multiple of the block size.
		PADDING_STRATEGY_PCKS15 // Enables the API to encrypt arbitrary long plain texts.
	};

	/**
	 * Creates the cbc mode cipher algorithm.
	 *
	 * @param cipher_algorithm The symmetric cipher used as base for CBC
	 * @param padding_strategy See documentation of PaddingStrategy.
	 * @return The created encryption algorithm, NULL if OOM or if cipher_algorithm is NULL
	 */
	static CryptoStreamEncryptionCBC *Create(CryptoSymmetricAlgorithm *cipher_algorithm, PaddingStrategy padding_strategy = PADDING_STRATEGY_NONE); // Takes over cipher_algorithm

	/**
	 * Encrypt.
	 *
	 * Note that if padding is not not length must be a multiple of
	 * the block size of the underlying cipher, GetBlockSize().
	 *
	 * @param source        The source to encrypt.
	 * @param target        Preallocated buffer. The encrypted result will be copied here.
	 *                      The length of the target must be calculated using
	 *                      CalculateEncryptedTargetLength()
	 * @param source_length The length of source.
	 * @param target_length The length of target. If 0, source_length will be used.
	 *                      Only to be used if padding strategy != PADDING_STRATEGY_NONE.
	 *                    	If used, calculate target_length using
	 *                    	CalculateEncryptedTargetLength(source_length)
	 *
	 */
	virtual void Encrypt(const UINT8 *source,  UINT8 *target, int source_length, int target_length = 0);

	/**
	 * Decrypt source and store the plaintext in target.
	 *
	 * Note that source length must be a multiple of the underlying ciphers
	 * block size (GetBlockSize())
	 *
	 * Depending on padding strategy, the real size of the decrypted target
	 * might be shorter than source length.
	 *
	 * The real size of the decrypted plain text can be calculated using
	 * CalculateDecryptedTargetLength() after decrypting has been done.
	 *
	 * @param source The source to decrypt.
	 * @param target Preallocated buffer. The decrypted result will be copied here. Target
	 *               must be at least 'length' bytes long
	 * @param length The length of source and target.
	 */
	virtual void Decrypt(const UINT8 *source,  UINT8 *target, int length);

	/**
	 * Set the encryption/decryption key.
	 *
	 * The size of the key must match the key size of the underlying cipher,
	 * GetKeySize().
	 *
	 * @param key The key used to decrypt.
	 *
	 */
	virtual OP_STATUS SetKey(const UINT8 *key);

	/**
	 * Set the initialization vector.
	 *
	 * This also initialize the inner state of the CBC encryption.
	 * Set before encryption of a new stream.
	 *
	 * Note that the length of the iv must match the size of the block size
	 * of the underlying cipher.
	 *
	 * @param iv the iv of length GetBlockSize()
	 */
	virtual void SetIV(const UINT8 *iv);

	/**
	 * Get the key size of the underlying cipher.
	 *
	 * @return the key size
	 */
	virtual int GetKeySize() const { return m_algorithm->GetKeySize(); }

	/**
	 * Get the block size of the underlying cipher. This also
	 * determines the iv size.
	 *
	 * @return the block size
	 */
	virtual int GetBlockSize() const { return m_algorithm->GetBlockSize(); }

	/**
	 * Get the internal state of the CBC mode (low level). Will
	 * be equal to the iv before Encrypt() has been called.
	 *
	 * @return The internal state
	 */
	const UINT8 *GetState() const { return m_state; }

	/**
	 * Calculates length of encrypted cipher text for
	 * given plain text length.
	 *
	 * This is only needed if padding has been used.
	 */
	int CalculateEncryptedTargetLength(int source_length) const;

	/**
	 * Calculate the real length of the plain text.
	 *
	 * @param decrypted_target_length[out] The real length of plain text.
	 * @param decrypted_source             The decrypted plain text.
	 * @param source_length	               The length of source
	 * @return OpSatus::OK or  OpStatus::ERR_PARSING_FAILED if format of decrypted text is wrong.
	 */
	OP_STATUS CalculateDecryptedTargetLength(int &decrypted_target_length, const UINT8 *decrypted_source, int source_length) const;

	virtual ~CryptoStreamEncryptionCBC();

private:
	CryptoStreamEncryptionCBC(CryptoSymmetricAlgorithm *cipher_algorithm, PaddingStrategy padding_strategy);



	CryptoSymmetricAlgorithm *m_algorithm;
	PaddingStrategy m_padding_strategy;
	UINT8 *m_state;
	UINT8 *m_key;
	unsigned int m_state_counter;
};

#endif // CRYPTO_STREAM_ENCRYPTION_SUPPORT
#endif /* CRYPTO_STREAM_ENCRYPTION_CBC_H_ */
