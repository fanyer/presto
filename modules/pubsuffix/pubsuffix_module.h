/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_PUBSUFFIX_MODULE_H
#define MODULES_PUBSUFFIX_MODULE_H

#if defined PUBSUFFIX_ENABLED

#include "modules/hardcore/opera/module.h"

class AutoFetch_Manager;

/** Module object. Includes all global infrastructure for the public suffix module 
 *	
 *	Public Suffix is a list over all domains that can be considered registry like, that is,
 *	similar to a TLD, as well as some that are not, when all other domains at the same hierarchical
 *	level are such domains.
 *
 *	The list is stored as separate, digitally signed files on certs.opera.com (the same
 *	server used for root certificate updates) in a XML format defined by an Internet draft 
 *	published by Opera.
 *
 *	When loaded, all active ServerName objects in the TLD will be tagged with the category 
 *	of hostname that they are, TLD, registry, domain, or host. "Domains" are names immediately 
 *	below a registry domain, or a TLD, if there are no second level registries.
 *
 *	The API in this module should not be accessed directly, but through the asynchronous ServerName API,
 *	which require the client to register a callback listener if the module have to go online to fetch 
 *	the information first.
 */
class PubsuffixModule : public OperaModule
{
private:
	/** List of domains already checked; no need to check them again */
	AutoDeleteHead checked_domains;

#ifdef PUBSUFFIX_OVERRIDE_UPDATE
	/** List of domains that are not going to be checked using the normal URLs, and the URLs to use instead */
	AutoDeleteHead predef_domains;
#endif

	/** List of active updaters */
	AutoDeleteHead updaters;

	/** Update manager */
	OP_STATUS LoadPubSuffixState();

public:
	/** Constructor */
	PubsuffixModule();

	/** Destructor */
	virtual ~PubsuffixModule(){};

	/** Initialization */
	virtual void InitL(const OperaInitInfo& info);

	/** Clean up the module object */
	virtual void Destroy();

	/** Check the specificed domain */
	OP_STATUS CheckDomain(ServerName *domain);

	/** Have this TLD been checked ? */
	BOOL HaveCheckedDomain(const OpStringC8 &tld);

	/** Set that we have checked this TLD */
	void  SetHaveCheckedDomain(const OpStringC8 &tld);

#ifdef PUBSUFFIX_OVERRIDE_UPDATE
	/** Add a URL override for a TLD */
	OP_STATUS AddUpdateOverride(const OpStringC8 &tld, const OpStringC8 &url);
#endif
};

#define PUBSUFFIX_MODULE_REQUIRED

#define g_pubsuf_api				(&(g_opera->pubsuffix_module))
#define g_pubsuffix_context			g_opera->pubsuffix_module.m_pubsuffix_context

#endif // PUBSUFFIX_ENABLED
#endif // !MODULES_PUBSUFFIX_MODULE_H
