/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PI_INTERNET_CONNECTION

#include "modules/url/url_dns_wrapper.h"
#include "modules/pi/network/OpInternetConnection.h"

HostResolverWrapper::HostResolverWrapper() :
	m_resolver(NULL), m_listener(NULL)
{
}

HostResolverWrapper::~HostResolverWrapper()
{
	Out(); // Remove the wrapper from the Internet connection listener list.
	OP_DELETE(m_resolver);
}

OP_STATUS HostResolverWrapper::Resolve(const uni_char* hostname)
{
	if (g_internet_connection.IsConnected() || !uni_stricmp(hostname, "localhost"))
		return m_resolver->Resolve(hostname);

	m_hostname.Set(hostname);
	return g_internet_connection.RequestConnection(this);
}

void HostResolverWrapper::OnConnectionEvent(BOOL connected)
{
	if (connected)
	{
		m_resolver->Resolve(m_hostname.CStr());
		m_hostname.Empty();
	}
	else
	{
		m_hostname.Empty();
		// OnHostResolverError may delete the wrapper, don't place any code after the call.
		m_listener->OnHostResolverError(this, INTERNET_CONNECTION_CLOSED);
	}
}

#ifdef SYNCHRONOUS_HOST_RESOLVING
OP_STATUS HostResolverWrapper::ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error) { return m_resolver->ResolveSync(hostname, socket_address, error); }
#endif
OP_STATUS HostResolverWrapper::GetLocalHostName(OpString* local_hostname, Error* error) { return m_resolver->GetLocalHostName(local_hostname, error); }
UINT HostResolverWrapper::GetAddressCount() { return m_resolver->GetAddressCount(); }
OP_STATUS HostResolverWrapper::GetAddress(OpSocketAddress* socket_address, UINT index) { return m_resolver->GetAddress(socket_address, index); }

void HostResolverWrapper::SetResolver(OpHostResolver *resolver) { m_resolver = resolver; }
void HostResolverWrapper::SetListener(OpHostResolverListener *listener) { m_listener = listener; }

void HostResolverWrapper::OnHostResolved(OpHostResolver* host_resolver) { m_listener->OnHostResolved(this); }
void HostResolverWrapper::OnHostResolved(OpHostResolver* host_resolver, unsigned int ttl) { m_listener->OnHostResolved(this, ttl); }
void HostResolverWrapper::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error) { m_listener->OnHostResolverError(this, error); }

#endif // PI_INTERNET_CONNECTION
