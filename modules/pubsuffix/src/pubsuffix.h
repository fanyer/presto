/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _PUBSUFFIX_H_
#define _PUBSUFFIX_H_

#if defined PUBSUFFIX_ENABLED

#include "modules/updaters/xml_update.h"
#include "modules/pubsuffix/src/dns_regchk.h"

/** Retrieves and processes a public suffix file
 *	If necessary the handler will use a DNS lookup heuristic to check for an IP address.
 *
 *	This updater will use cached versions of the resource if available.
 *
 *	Process:
 *		- A update is requested for a specific TLD, with the orginal request servername as a reference
 *		- The if expired conditional loading start
 *		- When finished loading successfully, the file is verified and parsed, and each
 *			servername in the TLD is updated. 
 *		- If unsuccessful, the DNS fallback is used for the requesting servername only.
 *		- When operation is completed a message is posted to all who have tried to get a 
 *			suffix-status, who will then retry the request (they may have to do multiple retries)
 */
class PublicSuffix_Updater : public Link
{
private:
	/** The requesting servername/domain name */
	ServerName_Pointer domain_sn;
	/** The TLD being checked */
	ServerName_Pointer suffix;

	AutoDeleteHead suffix_list;
	OpString name;
	BOOL all_levels_are_TLDs;
	unsigned int levels;

public:
	/** Constructor */
	PublicSuffix_Updater(ServerName *tld, ServerName *checking_domain);
	/** Destructor */
	~PublicSuffix_Updater();

	/** Construct the updater object, and prepare the URL and callback message*/
	OP_STATUS Construct(OpMessage fin_msg);

	/** Construct the updater object using a fixed URL */
	OP_STATUS Construct(const OpStringC8 &fixed_url, OpMessage fin_msg);

	/** Process the file */
	virtual OP_STATUS ProcessFile();

	OP_STATUS ParseContent(XMLFragment &parser, AutoDeleteHead &suffix_list);
	OP_STATUS ParseFile(XMLFragment &parser);

	/** What is the TLD suffix we are testing for? */
	ServerName *GetSuffix(){return suffix;}

private:

	/** Process the file for the list of servers with the specific (multilevel) suffix */
	void ProcessSpecification(Head &servername_list, ServerName *suffix, unsigned int level, class SuffixList_item *first_item);
};
#endif

#endif // _PUBSUFFIX_H_
