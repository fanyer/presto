/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_HELLOCLIENT_H
#define LIBSSL_HELLOCLIENT_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/handshake/randfield.h"

typedef DataStream_IntegralType<SSL_CompressionMethod,1> Loadable_SSL_CompressionMethod;
typedef SSL_LoadAndWriteableListType<Loadable_SSL_CompressionMethod, 1, 0xff> SSL_varvectorCompress;

class SSL_Client_Hello_st: public SSL_Handshake_Message_Base
{
public:
	SSL_ProtocolVersion client_version;
	SSL_Random random;
	SSL_varvector16 session_id;

	SSL_CipherSuites ciphersuites;
	SSL_varvectorCompress compression_methods;
	DataStream_ByteArray	extensions;

private:
    SSL_Client_Hello_st(const SSL_Client_Hello_st &);
	void InternalInit();

protected:
	SSL_Client_Hello_st(BOOL dontadd_fields);

public:
    SSL_Client_Hello_st();

	virtual void SetUpMessageL(SSL_ConnectionState *pending);

#ifdef SSL_SERVER_SUPPORT
	//	  BOOL Valid(SSL_Alert *msg=0) const;
#endif

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Client_Hello_st";}
#endif
};
#endif
#endif // LIBSSL_HELLOCLIENT_H
