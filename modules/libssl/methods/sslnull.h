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

#ifndef SSLNULL_H
#define SSLNULL_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslcipher.h"

class SSL_Null_Hash: public SSL_Hash{
public:
    virtual void InitHash();
    virtual const byte *CalculateHash(const byte *source,uint32 len);
    virtual byte *ExtractHash(byte *target);
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
    virtual SSL_Hash *Fork() const;
};


class SSL_Null_Cipher : public SSL_GeneralCipher{
public:
    virtual void InitEncrypt();
    virtual byte *Encrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen);
    virtual byte *FinishEncrypt(byte *target, uint32 &,uint32 bufferlen);

	virtual void InitDecrypt();
    virtual byte *Decrypt(const byte *source,uint32 len,byte *target, uint32 &,uint32 bufferlen); 
    virtual byte *FinishDecrypt(byte *target, uint32 &,uint32 bufferlen);

	virtual void SetCipherDirection(SSL_CipherDirection dir);
    virtual const byte *LoadKey(const byte *);
    virtual const byte *LoadIV(const byte *);
    virtual void BytesToKey(SSL_HashAlgorithmType, const SSL_varvector32 &string,  const SSL_varvector32 &salt, int count);
	//virtual SSL_GeneralCipher *Fork() const;
}; 

#endif
#endif /* SSLNULL_H */
