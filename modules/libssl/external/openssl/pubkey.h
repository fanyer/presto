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

#ifndef PUBKEY_H
#define PUBKEY_H

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_OPENSSL_

#include "modules/libopeay/openssl/objects.h"
#include "modules/libopeay/openssl/evp.h"

#include "modules/libssl/methods/sslpubkey.h"

class SSLEAY_PublicKeyCipher : public SSL_PublicKeyCipher{ 
private:
    EVP_PKEY *keyspec;

#ifdef SSL_DH_SUPPORT
    BIGNUM *DH_recv_public;
    SSL_secure_varvector16 DH_calc_common;
#endif

private:
    SSLEAY_PublicKeyCipher &operator =(const SSLEAY_PublicKeyCipher &); //not implemented. defined to avoid default
    SSLEAY_PublicKeyCipher(const SSLEAY_PublicKeyCipher &);  //not implemented. defined to avoid default
	void Init();

	void InternalInit(EVP_PKEY *item);
    void InternalInit(SSL_BulkCipherType type);
    void InternalInit(const SSLEAY_PublicKeyCipher *);
    
	void SetupInformation();

public:
    SSLEAY_PublicKeyCipher(SSL_BulkCipherType type);
    SSLEAY_PublicKeyCipher(EVP_PKEY *);
    SSLEAY_PublicKeyCipher(const SSLEAY_PublicKeyCipher *);
    
    virtual ~SSLEAY_PublicKeyCipher();
	SSL_BulkCipherType GetCipherType() const;
    
    virtual void InitEncrypt();
    virtual byte *Encrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen);
    virtual byte *FinishEncrypt(byte *target, uint32 &,uint32 bufferlen);
    virtual void InitDecrypt();
    virtual byte *Decrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen); 
    virtual byte *FinishDecrypt(byte *target, uint32 &,uint32 bufferlen);
    virtual BOOL Verify(const byte *reference,uint32 len, const byte *signature, uint32); 
	
    virtual uint32 KeyBitsLength() const;   
	
    virtual void LoadPublicKey(uint16, const SSL_varvector16 &);
    virtual void LoadPrivateKey(uint16, const SSL_varvector16 &);
    virtual byte *Sign(const byte *source,uint32 len,byte *target, uint32 &, uint32 bufferlen);
#ifdef USE_SSL_ASN1_SIGNING
    virtual byte *SignASN1(SSL_Hash *reference, byte *target, uint32 &, uint32 bufferlen);
	virtual byte *SignASN1(SSL_HashAlgorithmType alg, SSL_varvector32 &reference, byte *signature, uint32 &len, uint32 bufferlen);
	virtual BOOL VerifyASN1(SSL_Hash *reference, const byte *signature, uint32); 
#endif
    virtual uint16 PublicSize(uint16) const;   
    virtual void UnLoadPublicKey(uint16, SSL_varvector16 &);
#ifdef SSL_PUBKEY_DETAILS
    virtual uint16 PublicCount() const;   
    virtual uint16 GeneratedPublicCount() const;   
    virtual uint16 PrivateCount() const;   
    virtual uint16 GeneratedPrivateCount() const;   
    virtual uint16 PrivateSize(uint16) const;
#endif
    virtual void UnLoadPrivateKey(uint16, SSL_varvector16 &); 
    
#ifdef SSL_DH_SUPPORT 
    virtual void ProduceGeneratedPublicKeys();
    virtual void ProduceGeneratedPrivateKeys();
#endif
    virtual SSL_PublicKeyCipher *Fork()const ;
    
    virtual void LoadAllKeys(SSL_varvector32 &);    // Vector with a DER-encoded Private key
    void CheckError();
};
#endif /* PUBKEY_H */
#endif
