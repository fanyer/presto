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

#ifndef PKCS11_PUBKEY_H
#define PKCS11_PUBKEY_H

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_PKCS11_

#include "modules/libssl/external/p11/p11_man.h"

class PKCS11_PublicKeyCipher : public SSL_PublicKeyCipher
{ 
private:
	
	OpSmartPointerNoDelete<PKCS_11_Manager> master;
	OpSmartPointerNoDelete<PKCS_11_Token> token;

	CK_FUNCTION_LIST_PTR functions;

	CK_SLOT_ID slot_id;
	CK_SESSION_HANDLE session;

    BOOL use_privatekey;
    uint32 templen,temppos;
    SSL_varvector16 tempbuffer;
    SSL_BulkCipherType ciphertype;
    
private:
    PKCS11_PublicKeyCipher &operator =(const PKCS11_PublicKeyCipher &); //not implemented. defined to avoid default
    PKCS11_PublicKeyCipher(const PKCS11_PublicKeyCipher &);  //not implemented. defined to avoid default
    
public:
    PKCS11_PublicKeyCipher(PKCS_11_Manager *mstr, PKCS_11_Token *tokn, CK_FUNCTION_LIST_PTR func, CK_SLOT_ID slot, SSL_BulkCipherType ctyp);
    PKCS11_PublicKeyCipher(const PKCS11_PublicKeyCipher *);
    
    virtual ~PKCS11_PublicKeyCipher();
    virtual SSL_CipherType Type() const; 
    virtual SSL_BulkCipherType CipherID() const;
	SSL_BulkCipherType GetCipherType() const;
    virtual void SetUsePrivate(BOOL);
    virtual BOOL UsingPrivate() const;

    virtual void Login(SSL_secure_varvector32 &password);
	virtual BOOL Need_Login();
	virtual BOOL Need_PIN();

    virtual uint16 InputBlockSize() const;
    virtual uint16 OutputBlockSize() const;
    virtual void InitEncrypt();
	void EncryptStep(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen);
    virtual byte *Encrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen);
    virtual byte *FinishEncrypt(byte *target, uint32 &,uint32 bufferlen);
    virtual void InitDecrypt();
	void DecryptStep(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen);
    virtual byte *Decrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen); 
    virtual byte *FinishDecrypt(byte *target, uint32 &,uint32 bufferlen);
    virtual BOOL Verify(const byte *reference,uint32 len, const byte *signature, uint32); 
	
    virtual uint32 KeyBitsLength() const;   
	
    virtual void LoadPublicKey(uint16, const SSL_varvector16 &);
    virtual uint16 PublicSize(uint16) const;   
#ifdef SSL_PUBKEY_DETAILS
	virtual uint16 PublicCount() const;   
    virtual uint16 GeneratedPublicCount() const;   
    virtual uint16 PrivateCount() const;   
    virtual uint16 GeneratedPrivateCount() const;   
    virtual uint16 PrivateSize(uint16) const;
#endif

    virtual void LoadPrivateKey(uint16, const SSL_varvector16 &);
    virtual void UnLoadPublicKey(uint16, SSL_varvector16 &);
    virtual void UnLoadPrivateKey(uint16, SSL_varvector16 &); 
    virtual BOOL VerifyASN1(SSL_Hash *reference, const byte *signature, uint32); 
    virtual byte *Sign(const byte *source,uint32 len,byte *target, uint32 &, uint32 bufferlen);
    virtual byte *SignASN1(SSL_Hash *reference, byte *target, uint32 &, uint32 bufferlen);
#ifdef SSL_DH_SUPPORT 
    virtual void ProduceGeneratedPublicKeys();
    virtual void ProduceGeneratedPrivateKeys();
#endif
    virtual SSL_PublicKeyCipher *Produce() const;
    virtual SSL_PublicKeyCipher *Fork()const ;
    
    virtual void LoadAllKeys(SSL_varvector32 &);    // Vector with a DER-encoded Private key
	/** Return TRUE if ERROR */
    BOOL CheckError(CK_RV status); 
    BOOL Error(SSL_Alert *msg=NULL) const;

private:
	/** Return TRUE if session was created */
	BOOL CheckSession();
	void CloseSession();
	BOOL SetupKey(CK_OBJECT_HANDLE &key);

};
#endif /* PUBKEY_H */
#endif
