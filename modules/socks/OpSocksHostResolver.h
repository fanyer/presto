/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OP_SOCKS_HOST_RESOLVER_H
#define OP_SOCKS_HOST_RESOLVER_H

#ifdef SOCKS_SUPPORT

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/socks/OpSocksSocket.h"


/** @short Host name lookup operation by means of abusing a SOCKS proxy
 *
 * Note that it requires an additionl method Resolve(hostname, port)
 * defined in the base class OpHostResolver.
 * This class aids in solving bug CT-386.
 */
class OpSocksHostResolver : public OpHostResolver , private OpSocketListener {
public:
	OpSocksHostResolver(OpHostResolverListener* listener)
		: m_hostResolverListener(listener)
		, m_curResolvedAddress(NULL)
		, m_sslinkToDel(NULL)
	{
		OP_ASSERT(listener != NULL);
	}

	virtual ~OpSocksHostResolver()
	{
		m_sslinks.Clear();
		if (m_sslinkToDel)
			OP_DELETE(m_sslinkToDel);
	}

	// -------------------------------------------------------------------------
	// OpHostResolver methods:
	// -------------------------------------------------------------------------
	virtual OP_STATUS ResolveEx(const uni_char* hostname, unsigned short  port);

	virtual OP_STATUS Resolve(const uni_char* hostname);

#ifdef SYNCHRONOUS_HOST_RESOLVING
	virtual OP_STATUS ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error);
#endif // SYNCHRONOUS_HOST_RESOLVING

	virtual OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error);

	virtual UINT GetAddressCount();

	virtual OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index);

	// -------------------------------------------------------------------------
	// SOcketListener methods:
	// -------------------------------------------------------------------------
	virtual void OnSocketConnected(OpSocket* socket);

	virtual void OnSocketDataReady(OpSocket* socket);

	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);

	virtual void OnSocketClosed(OpSocket* socket);

#ifdef SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectionRequest(OpSocket* socket);

	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error);
#endif // SOCKET_LISTEN_SUPPORT

	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);

	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);

	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);

	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);

#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT


private:
	class SSLink : public OpSocksSocket, public Link {
	public:
		SSLink(OpSocketListener* sl)
			: OpSocksSocket(sl, /*secure*/FALSE)
		{;}
	};

	OpHostResolverListener* m_hostResolverListener;
	OpSocketAddress* m_curResolvedAddress;
	Head    m_sslinks;
	SSLink* m_sslinkToDel; // used for delayed deletion (see OnSocketConnected method)
};

#endif//SOCKS_SUPPORT
#endif//OP_SOCKS_HOST_RESOLVER_H
