/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file OpTrustInfoResolver.h
 */

#ifndef OPTRUST_INFO_RESOLVER_H
# define OPTRUST_INFO_RESOLVER_H

# ifdef TRUST_INFO_RESOLVER
#  include "modules/pi/network/OpHostResolver.h"
class OpTrustInfoResolverListener
{
public:
	virtual ~OpTrustInfoResolverListener(){}
	virtual void OnTrustInfoResolveError() = 0;
	virtual void OnTrustInfoResolved() = 0;
};

class OpTrustInformationResolver
{
public:

	virtual ~OpTrustInformationResolver(){}

	virtual OP_STATUS	DownloadTrustInformation(OpWindowCommander * target_windowcommander) = 0;
};

# endif // TRUST_INFO_RESOLVER
#endif // OPTRUST_INFO_RESOLVER_H
