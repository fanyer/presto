/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SOCKS_SUPPORT

#include "modules/socks/OpSocksHostResolver.h"


OP_STATUS OpSocksHostResolver::Resolve(const uni_char* hostname)
{
	OP_ASSERT(0);
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS OpSocksHostResolver::ResolveEx(const uni_char* hostname, unsigned short port)
{
	SSLink* sslink = OP_NEW(SSLink, (this));
	if (!sslink)
		return OpStatus::ERR_NO_MEMORY;

	sslink->Into(&m_sslinks);
	OP_STATUS stat = sslink->Connect(hostname, port);
	if (OpStatus::IsError(stat))
	{
		sslink->Out();
		OP_DELETE(sslink);
		return stat;
	}

	return OpStatus::OK;
}

void OpSocksHostResolver::OnSocketConnected(OpSocket* socket)
{
	SSLink* sslink = static_cast<SSLink*>(socket);
	if (!m_sslinks.HasLink(sslink))
	{
		OP_ASSERT(0);
		return;
	}

	// Normally, we should practice 'delayed deleting' as sslink is a live 'this' pointer on the previous stack frame;
	// But this is causing a (warning) assert failure in OpSocksSocket, while this works (at least in wingogi):
/*	if (m_sslinkToDel)
	{
		SSLink* del = m_sslinkToDel;
		m_sslinkToDel = NULL;
		OP_DELETE(del);
	}
	m_sslinkToDel = sslink;
*/
	sslink->Out();
	OpSocketAddress* actual_address = sslink->RelinquishActualTargetAddress();

	if (actual_address == NULL)
	{
		m_hostResolverListener->OnHostResolverError(this, OpHostResolver::HOST_ADDRESS_NOT_FOUND);
	}
	else
	{
		OP_ASSERT(m_curResolvedAddress == NULL);
		m_curResolvedAddress = actual_address;
		m_hostResolverListener->OnHostResolved(this);
		m_curResolvedAddress = NULL;
		OP_DELETE(actual_address);
	}

	OP_DELETE(sslink);
}


OP_STATUS OpSocksHostResolver::GetLocalHostName(OpString* local_hostname, Error* error)
{
	OP_ASSERT(0);
	*error = HOST_ADDRESS_NOT_FOUND;
	return OpStatus::ERR_NOT_SUPPORTED;
}

UINT OpSocksHostResolver::GetAddressCount()
{
	if (!m_curResolvedAddress)
		return 0;
	else
		return 1;
}

OP_STATUS OpSocksHostResolver::GetAddress(OpSocketAddress* socket_address, UINT index)
{
	if (index >= GetAddressCount())
		return OpStatus::ERR_OUT_OF_RANGE;
	OP_ASSERT(index == 0);

	OP_STATUS stat = socket_address->Copy(m_curResolvedAddress);
	return stat;
}

void OpSocksHostResolver::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(0);
}

void OpSocksHostResolver::OnSocketDataSent(OpSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(0);
}

void OpSocksHostResolver::OnSocketClosed(OpSocket* socket)
{;}

#ifdef SOCKET_LISTEN_SUPPORT
void OpSocksHostResolver::OnSocketConnectionRequest(OpSocket* socket) { OP_ASSERT(0); }

void OpSocksHostResolver::OnSocketListenError(OpSocket* socket, OpSocket::Error error) { OP_ASSERT(0); }
#endif // SOCKET_LISTEN_SUPPORT

void OpSocksHostResolver::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	OnSocketConnected(socket);
}

void OpSocksHostResolver::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(0);
	OnSocketConnected(socket);
}

void OpSocksHostResolver::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(0);
	OnSocketConnected(socket);
}

void OpSocksHostResolver::OnSocketCloseError(OpSocket* socket, OpSocket::Error error)
{;}

#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
void OpSocksHostResolver::OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent)
{;}
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT

#ifdef SYNCHRONOUS_HOST_RESOLVING
OP_STATUS OpSocksHostResolver::ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error)
{
	OP_ASSERT(0);
}
#endif // SYNCHRONOUS_HOST_RESOLVING

#endif //SOCKS_SUPPORT
