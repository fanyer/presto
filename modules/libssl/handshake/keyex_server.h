/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_KEYEXSERV_H
#define LIBSSL_KEYEXSERV_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/handshake/vectormess.h"
#include "modules/libssl/handshake/sslsig.h"

class SSL_ServerRSAParams;
class SSL_ServerDHParams;

class SSL_Server_Key_Exchange_st : public SSL_Handshake_Message_Base
{ 
private:
    SSL_KeyExchangeAlgorithm alg;
	
    LoadAndWritablePointer<LoadAndWritableList> params;
    SSL_Signature sign;  
    union {
        SSL_ServerRSAParams *rsa;
#ifdef SSL_DH_SUPPORT
        SSL_ServerDHParams *dh;
#endif // SSL_DH_SUPPORT
    };
	SSL_VectorMessage dummy;
	 
    SSL_PublicKeyCipher *sigcipher;
	SSL_Hash_Pointer sighasher;
    SSL_ConnectionState  *connstate;
    
public:
    SSL_Server_Key_Exchange_st(); 
    SSL_Server_Key_Exchange_st(const SSL_Server_Key_Exchange_st &);
    SSL_Server_Key_Exchange_st &operator =(const SSL_Server_Key_Exchange_st &);
    virtual ~SSL_Server_Key_Exchange_st();

    virtual BOOL Valid(SSL_Alert *msg=NULL) const;  
 
    void SetKEA(SSL_KeyExchangeAlgorithm);
    SSL_KeyExchangeAlgorithm GetKEA() const {return alg;};
    void SetSignatureAlgorithm(SSL_SignatureAlgorithm item);
    SSL_SignatureAlgorithm GetSignatureAlgorithm() const;
	
    void SetParam(const SSL_ServerRSAParams &);  
    void GetParam(SSL_ServerRSAParams &);
	
#ifdef SSL_DH_SUPPORT
    void SetParam(const SSL_ServerDHParams &);  
    void GetParam(SSL_ServerDHParams &);
#endif // SSL_DH_SUPPORT
	
    const SSL_Signature &GetSignature() const;
    void SetCipher(SSL_PublicKeyCipher *cipher);
    BOOL Verify(SSL_varvector16 &client, SSL_varvector16 &server, SSL_Alert *msg = NULL);

#ifdef SSL_SERVER_SUPPORT
    void Sign(SSL_varvector16 &client, SSL_varvector16 &server);
#endif
    
    void SetCommState(SSL_ConnectionState *item);
	void SetMessageLength(uint32 len);
private:
	void InternalInit();

	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Server_Key_Exchange_st";}
#endif
};
#endif
#endif // LIBSSL_KEYEXSERV_H
