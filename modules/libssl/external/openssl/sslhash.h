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

#ifndef SSLHASH_H
#define SSLHASH_H

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_OPENSSL_
struct SSL_Digest_and_NID;

#include "modules/libssl/methods/sslhash.h"

class SSLEAY_Hash_Base : public SSL_Hash
{
protected:
	const SSL_Digest_and_NID *alg_spec;
    const EVP_MD *md_spec;
    
public:
    SSLEAY_Hash_Base(const SSL_Digest_and_NID *spec);
    virtual ~SSLEAY_Hash_Base();
	
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
    void CheckError();
};


class SSLEAY_Hash : public SSLEAY_Hash_Base{
protected:
    EVP_MD_CTX md_status;
    
public:
    SSLEAY_Hash(const SSL_Digest_and_NID *spec);
    SSLEAY_Hash(const SSLEAY_Hash &);
    SSLEAY_Hash(const SSLEAY_Hash *);
    virtual ~SSLEAY_Hash();
    
    virtual void InitHash();
    virtual const byte *CalculateHash(const byte *source,uint32 len);
    virtual byte *ExtractHash(byte *target);
    virtual SSL_Hash *Fork() const;
};

class SSLEAY_HMAC_Hash : public SSLEAY_Hash_Base{
private:
    HMAC_CTX md_status;
    
public:
    SSLEAY_HMAC_Hash(const SSL_Digest_and_NID *spec);
    SSLEAY_HMAC_Hash(const SSLEAY_HMAC_Hash &);
    SSLEAY_HMAC_Hash(const SSLEAY_HMAC_Hash *);
    virtual ~SSLEAY_HMAC_Hash();
    
    virtual void InitHash();
	virtual const byte *LoadSecret(const byte *, uint32 len);
    virtual const byte *CalculateHash(const byte *source,uint32 len);
    virtual byte *ExtractHash(byte *target);
    virtual SSL_Hash *Fork() const;
};

#endif
#endif /* SSLHASH_H */


