/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "identd.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/glue/factory.h"

//********************************************************************
//
//	IdentdServer
//
//********************************************************************

IdentdServer::IdentdServer(IdentdListener &listener)
:	m_listener(listener)
{
}


IdentdServer::~IdentdServer()
{
}


OP_STATUS IdentdServer::Init(const OpStringC& ident_as)
{
	RETURN_IF_ERROR(m_ident_as.Set(ident_as.CStr()));

	SocketServer *sock_serv = OP_NEW(SocketServer, (*this));
	m_socket_server = sock_serv;
	if (!sock_serv)
		return OpStatus::ERR_NO_MEMORY;

	UINT port_used = 0;
	RETURN_IF_ERROR(sock_serv->Init(113, 113, port_used, 30));

	return OpStatus::OK;
}


void IdentdServer::SendResponseIfPossible()
{
	const int newline_pos = m_request.Find("\r\n");
	if (newline_pos != KNotFound)
	{
		// Chop away everything but the first line.
		m_request.Delete(newline_pos);

		// Return a response.
		OpString8 response;
		response.AppendFormat("%s: USERID : UNIX :%s\r\n", m_request.CStr(), m_ident_as.CStr());
		m_socket_server->Send(response.CStr(), response.Length());

		// Notify the listener.
		m_listener.OnIdentConfirmed(m_request, response);
		m_request.Empty();
	}
}


void IdentdServer::OnSocketConnected(const OpStringC &connected_to)
{
	m_listener.OnConnectionEstablished(connected_to);
}


void IdentdServer::OnSocketDataAvailable()
{
	char buffer[1024];
	unsigned int bytes_read = 0;

	m_socket_server->Read(buffer, 1024, bytes_read);
	while (bytes_read > 0)
	{
		m_request.Append(buffer, bytes_read);
		m_socket_server->Read(buffer, 1024, bytes_read);
	}

	SendResponseIfPossible();
}

#endif // IRC_SUPPORT
