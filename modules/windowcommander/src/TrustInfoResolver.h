/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file TrustInfoResolver.h
 */

#ifndef TRUST_INFO_RESOLVER_H_
# define TRUST_INFO_RESOLVER_H_

# ifdef TRUST_INFO_RESOLVER
#  include "modules/pi/network/OpHostResolver.h"
#  include "modules/windowcommander/OpTrustInfoResolver.h"

class OpWindowCommander;
class AuthenticationInformation;
class OpTrustInfoResolverListener;
class WindowCommander;

class TrustInformationResolver : public OpHostResolverListener, public OpTrustInformationResolver
{
	OpTrustInfoResolverListener		* m_listener;
	OpHostResolver 					* m_host_resolver;
	WindowCommander					* m_target_windowcommander; // where trust info will be displayed
	URL								  m_server_url;
public:
				TrustInformationResolver(OpTrustInfoResolverListener * listener)
				:	m_listener (listener),
					m_host_resolver(NULL),
					m_target_windowcommander(NULL){}

				virtual ~TrustInformationResolver();

	OP_STATUS	DownloadTrustInformation(OpWindowCommander * target_windowcommander);

	void		SetServerURL(URL url) { m_server_url = url; }

	// OpHostResolverListener API implementation
	void		OnHostResolved(OpHostResolver* host_resolver);
	void		OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);
private:
	OP_STATUS	OpenTrustInformation(BOOL is_local);

};
# endif // TRUST_INFO_RESOLVER
#endif /*TRUST_INFO_RESOLVER_H_*/
