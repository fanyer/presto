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

#ifndef _TLS_VER_H_
#define _TLS_VER_H_

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/protocol/sslmac.h"
#include "modules/libssl/protocol/sslver.h"

class TLS_Version_1_MAC : public SSL_MAC
{
public:
    TLS_Version_1_MAC();
    TLS_Version_1_MAC(const TLS_Version_1_MAC *);
    virtual ~TLS_Version_1_MAC();
	
	virtual void SetHash(SSL_HashAlgorithmType nhash);
    virtual const byte *LoadSecret(const byte *, uint32 len);
    virtual void CalculateRecordMAC_L(const uint64 &seqence_number,
		SSL_ProtocolVersion &version,	SSL_ContentType type, 
		SSL_varvector32 &payload, const byte *padding, uint16 p_len,
		SSL_varvector32 &target); 
     
    //virtual SSL_MAC *ForkMAC() const;
};

class TLS_Version_1_Dependent : public SSL_Version_Dependent
{
protected:
	TLS_Version_1_Dependent(uint8 ver_minor): SSL_Version_Dependent(3, ver_minor) {};

    BOOL P_hash(SSL_varvector32 &result, uint32 len, 
		const SSL_varvector32 &secret_seed,
		const char *label,  //Null teminated string
		const SSL_varvector32 &data_seed,
		SSL_HashAlgorithmType hash) const;

    virtual BOOL PRF(SSL_varvector32 &result, uint32 len, 
		const SSL_varvector32 &secret_seed,
		const char *label,  //Null teminated string
		const SSL_varvector32 &data_seed) const = 0;

public:

    virtual void CalculateMasterSecret(SSL_secure_varvector16 &mastersecret, const SSL_secure_varvector16 &premaster);

	virtual void CalculateKeys(const SSL_varvector16 &mastersecret,
		SSL_CipherSpec *client, SSL_CipherSpec *server);

    virtual void GetFinishedMessage(BOOL client, SSL_varvector32 &target);

	virtual BOOL SendAlert(SSL_Alert &, BOOL cont) const; // If necessary translate passed alert
    
    virtual SSL_MAC *GetMAC() const;
};
#endif
#endif
