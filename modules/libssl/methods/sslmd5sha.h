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

#ifndef __SSLMD5SHA_H
#define __SSLMD5SHA_H

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"

class SSL_MD5_SHA_Hash : public SSL_Hash
{
private:
	SSL_Hash_Pointer md5;
	SSL_Hash_Pointer sha;

public:

	SSL_MD5_SHA_Hash();
	SSL_MD5_SHA_Hash(const SSL_MD5_SHA_Hash *);
	SSL_MD5_SHA_Hash(const SSL_MD5_SHA_Hash &);
	~SSL_MD5_SHA_Hash();

	virtual uint16 Size() const {return hash_size;}
	
    virtual void InitHash();
    virtual const byte *CalculateHash(const byte *source,uint32 len);
    virtual byte *ExtractHash(byte *target);
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
    virtual SSL_Hash *Fork() const;
};
#endif
#endif
