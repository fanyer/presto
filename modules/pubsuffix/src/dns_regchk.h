/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _DNS_REGCHK_H_
#define _DNS_REGCHK_H_

// DNS Registry-like domain check (fallback from public suffix)

#include "modules/util/simset.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"

class Comm;
class ServerName;

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"

/** This class does a heuristic check for the specified domain name using 
 *	either DNS, if direct network is available, or a HTTP HEAD request, if only
 *	a proxy connection is available
 *
 *	This code is based on the code previously located in cookies/url_cmi.cpp
 */
class DNS_RegistryCheck_Handler : public ProtocolComm
{
private:
	/** The name we are checking for */
	ServerName_Pointer domain_sn;

	/** The handler used for the DNS lookup; no connection is established */
	Comm *lookup;

	/** The URL used for proxied HTTP HEAD requests */
	URL_InUse lookup_url;

	/** Got message from proxy saying a direct access is needed */
	BOOL force_direct_lookup;

private:
	/** Initialization */
	void InternalInit();

public:
	
	/** Constructor */
	DNS_RegistryCheck_Handler(ServerName *sn);

	/** Destructor */
	virtual ~DNS_RegistryCheck_Handler();
	
	/** Reset the checker; used during destruction */
	void Clear();

	/** Set the necessary callbacks for the parent/owner */
	virtual OP_STATUS SetCallbacks(MessageObject* master, MessageObject* parent);

	/** Handle callbacks */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Start the lookup process */
	BOOL Start_Lookup();
};

#endif // _DNS_REGCHK_H_
