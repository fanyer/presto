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

#ifndef _SSL_MAC_H_
#define _SSL_MAC_H_

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"

class SSL_MAC : public SSL_Hash
{
protected:
    SSL_Hash_Pointer hash;
    SSL_secure_varvector16 secret;
private:
	void InternalInit(const SSL_MAC *old);
public:
    SSL_MAC();
    SSL_MAC(const SSL_MAC *);
    virtual ~SSL_MAC();
	
    virtual void SetHash(SSL_HashAlgorithmType);
    virtual const byte *LoadSecret(const byte *, uint32 len);
    virtual void CalculateRecordMAC_L(const uint64 &seqence_number,
		SSL_ProtocolVersion &version,	SSL_ContentType type, 
		SSL_varvector32 &payload, const byte *padding, uint16 p_len,
		SSL_varvector32 &target) = 0; 
	
    virtual void InitHash();
    virtual const byte *CalculateHash(const byte *source,uint32 len);
    virtual byte *ExtractHash(byte *target);
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
	
    virtual SSL_Hash *Fork() const;
    //virtual SSL_MAC *ForkMAC() const= 0;
};

#endif
#endif
