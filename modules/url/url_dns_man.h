/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jonny Rein Eriksen
**
*/

#ifndef _URL_DNS_MAN_H_
#define _URL_DNS_MAN_H_

#include "modules/pi/network/OpHostResolver.h"
#include "modules/url/url_sn.h"

struct ResolverListItem : public Link
{
	ResolverListItem():
		  resolver(NULL)
		, server_name(NULL)
#if defined(OPERA_PERFORMANCE) || defined(SCOPE_RESOURCE_MANAGER)
		, prefetch(FALSE)
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
		, notifying_listeners(FALSE)
	{}

	~ResolverListItem()
	{
		OP_DELETE(resolver);
	}

	OpHostResolver *resolver;
	ServerName_Pointer server_name;
	AutoDeleteHead resolver_listener_list;
#if defined(OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
	BOOL prefetch;
	double time_dns_request_started;
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
	BOOL notifying_listeners;
};

struct ResolverListenerListItem: public Link
{
	OpHostResolverListener *object;
};

/** This class handles all DNS resolving in Opera.
 *
 */
class UrlDNSManager: public OpHostResolverListener
{
	/** Constructor */
	UrlDNSManager();
	/** Construct for the given name */
	OP_STATUS Construct();
	ResolverListItem *FindResolverListItem(OpHostResolver* aHostResolver);

	Head	resolver_list;

public:
	/**
	 *	Creates a UrlDNSManager object.
	 *	
	 */
	static UrlDNSManager *CreateL();

	/** Destructor */
	virtual ~UrlDNSManager();

	void PreDestructStep();

	OP_STATUS Resolve(ServerName *sn, OpHostResolverListener *listener
#if defined(OPERA_PERFORMANCE) || defined (SCOPE_RESOURCE_MANAGER)
		, BOOL prefetch = TRUE
#endif // OPERA_PERFORMANCE || SCOPE_RESOURCE_MANAGER
		);

#ifdef SCOPE_RESOURCE_MANAGER
	/**
	 *	Returns milliseconds since DNS prefetch started.
	 *	Will be zero if no prefetching was done.
	 *
	 */
	double GetPrefetchTimeSpent(const ServerName* server_name);
#endif //SCOPE_RESOURCE_MANAGER

	void RemoveListener(ServerName *server_name, OpHostResolverListener *object);

	// From OpHostResolverListener
	virtual void OnHostResolved(OpHostResolver* aHostResolver);
	virtual void OnHostResolverError(OpHostResolver* aHostResolver, OpHostResolver::Error aError);
};

#endif //_URL_DNS_MAN_H_
