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

#ifndef __SSLHASH_H
#define __SSLHASH_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_Hash: public SSL_Error_Status
{
protected:
	SSL_HashAlgorithmType hash_alg;
	uint16	hash_size;

public:
	SSL_Hash() : hash_alg(SSL_NoHash), hash_size(0){}
    virtual ~SSL_Hash();
	virtual uint16 Size() const {return hash_size;}
	SSL_HashAlgorithmType HashID() const{return hash_alg;};
	
    virtual const byte *LoadSecret(const byte *, uint32 len);
    virtual void InitHash()= 0;
    virtual const byte *CalculateHash(const byte *source,uint32 len)= 0;
    virtual byte *ExtractHash(byte *target)=0;
#ifdef LIBSSL_VECTOR_EXTRACT_DIGESTS
    void ExtractHash(SSL_varvector32 &target);
#endif
    virtual BOOL Valid(SSL_Alert *msg=NULL) const=0;
    virtual SSL_Hash *Fork() const= 0;
#ifdef LIBSSL_VECTOR_EXTRACT_DIGESTS
    void CalculateHash(const SSL_varvector32 &source);
#endif
    void CalculateHash(const SSL_Hash *source);
    void CalculateHash(byte pad, uint32 i);
#ifdef LIBSSL_VECTOR_EXTRACT_DIGESTS
     void CompleteHash(const byte *source,uint32 len, SSL_varvector32 &target);
   void CompleteHash(const SSL_varvector32 &source, SSL_varvector32 &target);
#endif
    void CalculateHash(const char *source);

#ifdef EXTERNAL_DIGEST_API
	virtual OP_STATUS PerformInitOperation(int operation, void *params);
#endif
};
#endif
#endif
