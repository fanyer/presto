/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef CRYPTOSTREAMCIPHERSNOW_H_
#define CRYPTOSTREAMCIPHERSNOW_H_

class CryptoStreamCipherSnow
{
public:

	virtual ~CryptoStreamCipherSnow();

	static CryptoStreamCipherSnow *CreateSnow();

	virtual OP_STATUS SetKey(const UINT8 *key, int key_size, const UINT8 *iv);	

	virtual void Encrypt(const UINT8 *source, UINT8 *target, int len);

	virtual void Decrypt(const UINT8 *source, UINT8 *target, int len) { Encrypt(source, target, len); }

	/* Do not use when encrypting. For random generator, selftest and inner workings of the cipher only */
	UINT32 ClockCipher();
	void RetrieveCipherStream(UINT32 *values, UINT32 length);

	void AddEntropy(const UINT8 *entropy_data, int entropy_length);
private:
	void ClockCipherFsm();
	CryptoStreamCipherSnow();

	enum KeySize
	{
		KEY_SIZE_NOT_SET,
		KEY_SIZE_128,
		KEY_SIZE_256
	} m_key_size;

	void InitiateKey(UINT32 *key, UINT32 *IV);

	UINT32 m_sliding_index;

	/* LFSR */
	UINT32 m_inner_state[2*16];
	UINT32 m_key[8];
	UINT32 m_iv[4];	

	/* FSM */
	UINT32 m_R1;
	UINT32 m_R2;

	int m_counter;
};

#endif /* CRYPTOSTREAMCIPHERSNOW_H_ */
