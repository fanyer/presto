/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_SSLHAND3_H
#define LIBSSL_SSLHAND3_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_Server_Key_Exchange_st;
class SSL_CertificateRequest_st;

#include "modules/libssl/handshake/hand_types.h"

class SSL_HandShakeMessage : public LoadAndWritableList
{ 
private:
    LoadAndWritablePointer<LoadAndWritableList /*SSL_Handshake_Message_Base*/> msg;
    union{
		SSL_Server_Key_Exchange_st	*ServerKeys;
		SSL_CertificateRequest_st		*CertRequest;
	};

	DataStream_ByteArray dummy;
	
    SSL_ConnectionState  *connstate;
    
public: 
    SSL_HandShakeMessage();
    virtual ~SSL_HandShakeMessage();

	virtual void	PerformActionL(DataStream::DatastreamAction action, uint32 arg1=0, uint32 arg2=0);

    SSL_HandShakeMessage(const SSL_HandShakeMessage &old);
    OP_STATUS SetMessage( SSL_HandShakeType);
    SSL_HandShakeType GetMessage() const; 
	
    void SetCommState(SSL_ConnectionState *item);

	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);
	virtual void SetUpHandShakeMessageL(SSL_HandShakeType, SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_HandShakeMessage";}
#endif
};    
#endif
#endif // LIBSSL_SSLHAND3_H
