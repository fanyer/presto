/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DNSPREFETCHER_H
#define DNSPREFETCHER_H

#ifdef DNS_PREFETCHING

#include "modules/logdoc/logdocenum.h"

/**
 * Prefetch servername resolutions for links while parsing and when hovering
 * links. During parsing the number of prefetches are limited.
 */
class DNSPrefetcher
	: public MessageObject
{
public:
	DNSPrefetcher();
	virtual ~DNSPrefetcher();

	/**
	 * Perform a DNS prefetch of the servername for a URL.
	 *
	 * @param url The URL.
	 * @param type The source of the prefetch.
	 */
	void Prefetch(URL url, DNSPrefetchType type);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	void PostMessage(double delay);

	double burst_start;
	unsigned burst_prefetches;
	BOOL registered_callback, posted_message;
};

#endif // DNS_PREFETCHING
#endif // DNSPREFETCHER_H
