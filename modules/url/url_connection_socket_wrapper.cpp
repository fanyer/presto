/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PI_INTERNET_CONNECTION

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpInternetConnection.h"
#include "modules/url/url_connection_socket_wrapper.h"
#include "modules/url/internet_connection_wrapper.h"

ConnectionSocketWrapper::ConnectionSocketWrapper (OpSocket *socket) :
	SocketWrapper(socket), m_socket_address(NULL)
{
}

ConnectionSocketWrapper::~ConnectionSocketWrapper()
{
	Out(); // Remove the wrapper from the Internet connection listener list.
}

OP_STATUS ConnectionSocketWrapper::Connect(OpSocketAddress *socket_address)
{
	if (g_internet_connection.IsConnected() || socket_address->GetNetType() == NETTYPE_LOCALHOST)
		return m_socket->Connect(socket_address);

	m_socket_address = socket_address;
	return g_internet_connection.RequestConnection(this);
}

void ConnectionSocketWrapper::OnConnectionEvent(BOOL connected)
{
	if (connected)
		m_socket->Connect(m_socket_address);
	else
		m_listener->OnSocketConnectError(m_socket, INTERNET_CONNECTION_CLOSED);
}

#endif // PI_INTERNET_CONNECTION
