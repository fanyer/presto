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

#ifndef GENCIPH_H
#define GENCIPH_H


#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_OPENSSL_

#include "modules/libopeay/openssl/objects.h"
#include "modules/libopeay/openssl/evp.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define TST_DUMP
#endif
#endif

#include "modules/libssl/methods/sslcipher.h"
class SSLEAY_GeneralCipher : public SSL_GeneralCipher
{
  private:
    const EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *encrypt_status;

#ifdef USE_SSL_PGP_CFB_MODE
    SSL_Cipher::PGP_CFB_Mode_Type PGP_CFB_mode;
	BOOL PGP_CFB_started;
	byte PGP_CFB_IVbuffer[EVP_MAX_IV_LENGTH+2]; /* ARRAY OK 2009-04-22 yngve */
	byte PGP_CFB_Resync[EVP_MAX_IV_LENGTH];		/* ARRAY OK 2009-04-22 yngve */
	int PGP_CFB_IVread;
#endif

#ifdef TST_DUMP
	SSL_secure_varvector32 debug_key;
#endif

    void InitCipher(const EVP_CIPHER_CTX *old = NULL);
    
  public:
    SSLEAY_GeneralCipher(SSL_BulkCipherType cipher_id, const EVP_CIPHER *);	
    SSLEAY_GeneralCipher(const SSLEAY_GeneralCipher *);
    virtual ~SSLEAY_GeneralCipher();
    
	virtual void SetCipherDirection(SSL_CipherDirection dir);
    virtual void InitEncrypt();
    virtual byte *Encrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen);
    virtual byte *FinishEncrypt(byte *target, uint32 &,uint32 bufferlen);
    virtual void InitDecrypt();
    virtual byte *Decrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen); 
    virtual byte *FinishDecrypt(byte *target, uint32 &,uint32 bufferlen);

#ifdef USE_SSL_PGP_CFB_MODE
	void CFB_Resync();
	void SetPGP_CFB_Mode(SSL_Cipher::PGP_CFB_Mode_Type mode);
	void UnloadPGP_Prefix(SSL_varvector32 &buf);
#endif

    virtual const byte *LoadKey(const byte *);
    virtual const byte *LoadIV(const byte *);
    virtual void BytesToKey(SSL_HashAlgorithmType, const SSL_varvector32 &string,  const SSL_varvector32 &salt, int count);
    void CheckError();
  private:
    void StartCipher();
    byte *CipherUpdate(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen);
    byte *FinishCipher(byte *target, uint32 &,uint32 bufferlen);
};

#endif

#endif /* GENCIPH_H */
