/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_DNS_WRAPPER_H_
#define _URL_DNS_WRAPPER_H_

#ifdef PI_INTERNET_CONNECTION

#include "modules/pi/network/OpHostResolver.h"
#include "modules/pi/network/OpInternetConnection.h"

class HostResolverWrapper : public OpHostResolver, public OpInternetConnectionListener, public OpHostResolverListener
{
public:
	HostResolverWrapper();
	void SetResolver(OpHostResolver *resolver);
	void SetListener(OpHostResolverListener *listener);

		/* OpHostResolver implementation */
	~HostResolverWrapper();

	OP_STATUS Resolve(const uni_char* hostname);
#ifdef SYNCHRONOUS_HOST_RESOLVING
	OP_STATUS ResolveSync(const uni_char* hostname, OpSocketAddress* socket_address, Error* error);
#endif
	OP_STATUS GetLocalHostName(OpString* local_hostname, Error* error);
	UINT GetAddressCount();
	OP_STATUS GetAddress(OpSocketAddress* socket_address, UINT index);

		/* OpInternetConnectionListener implementation */
	void OnConnectionEvent(BOOL connected);

		/* OpHostResolverListener implementation */
	virtual void OnHostResolved(OpHostResolver* host_resolver);
	virtual void OnHostResolved(OpHostResolver* host_resolver, unsigned int ttl);
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);

private:
	OpString m_hostname;
	OpHostResolver *m_resolver;
	OpHostResolverListener *m_listener;
};

#endif // PI_INTERNET_CONNECTION

#endif // _URL_DNS_WRAPPER_H_
