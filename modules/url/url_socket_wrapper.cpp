/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/url/url_socket_wrapper.h"
#ifdef URL_NETWORK_DATA_COUNTERS
#include "modules/url/url_counter_socket_wrapper.h"
#endif

#ifdef PI_INTERNET_CONNECTION
#include "modules/pi/network/OpInternetConnection.h"
#include "modules/url/url_connection_socket_wrapper.h"
#include "modules/url/url_dns_wrapper.h"
#endif // PI_INTERNET_CONNECTION

#ifdef SOCKS_SUPPORT
#include "modules/socks/OpSocksSocket.h"
#endif // SOCKS_SUPPORT

class OpSocket;

/* static */
OP_STATUS SocketWrapper::CreateTCPSocket(OpSocket **socket, OpSocketListener *listener, unsigned int flags)
{
	BOOL secure = (flags & SocketWrapper::USE_SSL) != 0;
	OpSocket *wrapped_socket = NULL;

	if (!socket)
		return OpStatus::ERR_NULL_POINTER;

	*socket = NULL;

		// start with a real OpSocket
		// don't specify a listener yet, add it below
	RETURN_IF_ERROR(OpSocket::Create(&wrapped_socket, NULL, secure));
		// TODO: should SSL sockets be implemented as an OpSocket wrapper?

		// add any other allowed wrappers...
		// Note: socket wrapper classes should also implement the OpSocketListener interface, and pass these messages up to parent listener

#ifdef URL_NETWORK_DATA_COUNTERS
	CounterSocketWrapper *counter_socket = NULL;

	if (!(counter_socket = OP_NEW(CounterSocketWrapper, (wrapped_socket))))
	{
		OP_DELETE(wrapped_socket);
		return OpStatus::ERR_NO_MEMORY;
	}

		// subsequent wrappers should now wrap this one
	wrapped_socket = counter_socket;
#endif // URL_NETWORK_DATA_COUNTERS

#ifdef PI_INTERNET_CONNECTION
	if (flags & SocketWrapper::ALLOW_CONNECTION_WRAPPER)
	{
		ConnectionSocketWrapper *conn_socket = NULL;

		if (!(conn_socket = OP_NEW(ConnectionSocketWrapper, (wrapped_socket))))
		{
			OP_DELETE(wrapped_socket);
			return OpStatus::ERR_NO_MEMORY;
		}

			// subsequent wrappers should now wrap this one
		wrapped_socket = conn_socket;
	}
#endif // PI_INTERNET_CONNECTION

#ifdef SOCKS_SUPPORT
	if (flags & SocketWrapper::ALLOW_SOCKS_WRAPPER && g_socks_IsSocksEnabled)
	{
		OpSocksSocket *socks_socket = OP_NEW(OpSocksSocket, (NULL, secure, *wrapped_socket));

		if (!socks_socket)
		{
			OP_DELETE(wrapped_socket);
			return OpStatus::ERR_NO_MEMORY;
		}

			// subsequent wrappers should now wrap this one
		wrapped_socket = socks_socket;
	}
#endif // SOCKS_SUPPORT

	wrapped_socket->SetListener(listener);

		// give the caller the possibly wrapped socket
	*socket = wrapped_socket;

	return OpStatus::OK;
}

#ifdef OPUDPSOCKET
/* static */
OP_STATUS SocketWrapper::CreateUDPSocket(OpUdpSocket **socket, OpUdpSocketListener *listener, unsigned int flags, OpSocketAddress* local_address)
{
	//BOOL secure            = flags & SocketWrapper::USE_SSL;
	//BOOL connectionWrapper = flags & SocketWrapper::ALLOW_CONNECTION_WRAPPER;
	//BOOL allowSOCKS        = flags & SocketWrapper::ALLOW_SOCKS_WRAPPER;

	OpUdpSocket *wrapped_socket = NULL;

	if (!socket)
		return OpStatus::ERR_NULL_POINTER;

	*socket = NULL;

		// start with a real OpUdpSocket
		// don't specify a listener yet, add it below
	RETURN_IF_ERROR(OpUdpSocket::Create(&wrapped_socket, NULL, local_address));

		// add any other allowed wrappers...
		// Note: socket wrapper classes should also implement the OpUdpSocketListener interface, and pass these messages up to parent listener

		// Nothing here yet :)

	wrapped_socket->SetListener(listener);

		// give the caller the possibly wrapped socket
	*socket = wrapped_socket;

	return OpStatus::OK;
}
#endif // OPUDPSOCKET

/* static */
OP_STATUS SocketWrapper::CreateHostResolver(OpHostResolver** host_resolver, OpHostResolverListener* listener)
{
	if (!host_resolver)
		return OpStatus::ERR_NULL_POINTER;

	*host_resolver = NULL;

	OpHostResolver *wrapped_resolver = NULL;
	OpHostResolverListener *listener_wrapper = listener;


		// first, create wrappers (that implement both OpHostResolver and OpHostResolverListener)

#ifdef PI_INTERNET_CONNECTION
	HostResolverWrapper *conn_resolver = NULL;

	if (!(conn_resolver = OP_NEW(HostResolverWrapper, ())))
		return OpStatus::ERR_NO_MEMORY;

	listener_wrapper = (OpHostResolverListener *) conn_resolver;
#endif // PI_INTERNET_CONNECTION


		// second, create the real OpHostResolver
	OP_STATUS status = OpHostResolver::Create(&wrapped_resolver, listener_wrapper);

	if (OpStatus::IsError(status))
	{
#ifdef PI_INTERNET_CONNECTION
		OP_DELETE(conn_resolver);
#endif // PI_INTERNET_CONNECTION
		return status;
	}


		// third, make sure that the wrappers have the correct inner resolver and listener

#ifdef PI_INTERNET_CONNECTION
	conn_resolver->SetResolver(wrapped_resolver);
	conn_resolver->SetListener(listener);
	wrapped_resolver = conn_resolver;
#endif // PI_INTERNET_CONNECTION


		// finally, return the possibly wrapped resolver

	*host_resolver = wrapped_resolver;

	return OpStatus::OK;
}

