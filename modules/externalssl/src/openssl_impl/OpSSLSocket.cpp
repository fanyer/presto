/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpSSLSocket.cpp
 *
 * SSL-capable socket creation function.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */
#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/OpSSLSocket.h"
#include "modules/externalssl/src/openssl_impl/OpenSSLSocket.h"


OP_STATUS OpSSLSocket::Create(OpSocket** socket, OpSocketListener* listener, BOOL secure)
{
	if (!socket || !listener)
		return OpStatus::ERR;

	OpenSSLSocket* ssl_socket = OP_NEW(OpenSSLSocket, (listener, secure));
	if (!ssl_socket)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = ssl_socket->Init();
	if (status != OpStatus::OK)
	{
		OP_DELETE(ssl_socket);
		return status;
	}

	*socket = ssl_socket;
	return OpStatus::OK;
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
