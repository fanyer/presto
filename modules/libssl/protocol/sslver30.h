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

#ifndef SSLVER30_H
#define SSLVER30_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_GeneralCipher;

#include "modules/libssl/protocol/sslmac.h"
#include "modules/libssl/protocol/sslver.h"

class SSL_Version_3_0_MAC : public SSL_MAC
{
public:
    SSL_Version_3_0_MAC();
    SSL_Version_3_0_MAC(const SSL_Version_3_0_MAC *);

    virtual void CalculateRecordMAC_L(const uint64 &seqence_number,
		SSL_ProtocolVersion &version,	SSL_ContentType type, 
		SSL_varvector32 &payload, const byte *padding, uint16 p_len,
		SSL_varvector32 &target); 
    
    //virtual SSL_MAC *ForkMAC() const;
	
};


class SSL_Version_3_0 : public SSL_Version_Dependent
{
private:
    void Init();
    SSL_Version_3_0(const SSL_Version_3_0 *);

    BOOL PRF(SSL_varvector32 &result, uint32 len, 
		const SSL_varvector32 &secret_seed,
		const SSL_varvector32 &data_seed) const;
public:
    SSL_Version_3_0(uint8 ver_minor=0);
    virtual ~SSL_Version_3_0();
    
    virtual SSL_MAC *GetMAC() const;

    virtual void GetHandshakeHash(SSL_SignatureAlgorithm alg, SSL_secure_varvector32 &);
	void GetHandshakeHash(SSL_secure_varvector32 &, const byte *extra_msg, uint32 len);
    virtual void GetFinishedMessage(BOOL client, SSL_varvector32 &target);
	
    virtual void CalculateMasterSecret(SSL_secure_varvector16 &mastersecret, const SSL_secure_varvector16 &premaster);
	
    const byte *LoadKeyData( SSL_Hash_Pointer &hashmethod,
		const byte *secret, const SSL_varvector32 &seed,
		SSL_GeneralCipher *cipher, BOOL calckey) const;  
    virtual void CalculateKeys(const SSL_varvector16 &mastersecret,
		SSL_CipherSpec *client, SSL_CipherSpec *server);
	
    virtual BOOL SendAlert(SSL_Alert &, BOOL cont) const; // If necessary translate passed alert
};
#endif
#endif /* SSLVER30_H */
