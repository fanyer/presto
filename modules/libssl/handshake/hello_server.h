/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_HELLOSERVER_H
#define LIBSSL_HELLOSERVER_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/handshake/randfield.h"

class SSL_Server_Hello_st: public SSL_Handshake_Message_Base
{
private:
	SSL_ProtocolVersion server_version;
	SSL_Random random;
	SSL_varvector8 session_id;
	
	SSL_CipherID cipherid;
	DataStream_IntegralType<SSL_CompressionMethod, 1> compression_method;
	DataStream_ByteArray	extensions; // Will read everything until the end of the record
	
public:
    SSL_Server_Hello_st();
    SSL_Server_Hello_st(const SSL_Server_Hello_st &);
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);
	
#ifdef SSL_SERVER_SUPPORT
	
	void SetVersion(const SSL_ProtocolVersion &item){server_version=item;};
	void SetRandom(const SSL_Random &item){random=item;};
	void SetSessionID(const SSL_varvector8 &item){session_id=item;};
	
	void SetCipherID(const SSL_CipherID &item){cipherid=item;};
	void SetCompressionMethod(const SSL_CompressionMethod item){compression_method=item;};
#endif

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Server_Hello_st";}
#endif
};
#endif
#endif // LIBSSL_HELLOSERVER_H
