/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef DNS_PREFETCHING

#include "modules/logdoc/dnsprefetcher.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

static const unsigned PREFETECHES_PER_BURST = 20;
static const double BURST_TIME = 1000;

DNSPrefetcher::DNSPrefetcher()
	: burst_start(0),
	  burst_prefetches(0),
	  registered_callback(FALSE),
	  posted_message(FALSE)
{
}

/* virtual */
DNSPrefetcher::~DNSPrefetcher()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void
DNSPrefetcher::Prefetch(URL url, DNSPrefetchType type)
{
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DNSPrefetching))
		return;

	if (type == DNS_PREFETCH_MOUSEOVER)
	{
		/* Always prefetch mouseover prefetches. */
		if (url.PrefetchDNS())
		{
			OP_NEW_DBG("DNSPrefetcher::Prefetch", "dns_prefetching");
			OP_DBG((UNI_L("MOUSEOVER url: %s"), url.GetAttribute(URL::KUniName).CStr()));
		}
		return;
	}

#ifdef LOGDOC_PREFETCH_PARSING_AND_DYNAMIC
	if (burst_prefetches >= PREFETECHES_PER_BURST)
	{
		double current_burst_time = g_op_time_info->GetRuntimeMS() - burst_start;
		if (current_burst_time < BURST_TIME)
		{
			PostMessage(BURST_TIME - current_burst_time);
			return;
		}
		else
		{
			burst_prefetches = 0;
			burst_start = 0;
		}
	}

	BOOL prefetched = url.PrefetchDNS();
	if (prefetched)
	{
		OP_NEW_DBG("DNSPrefetcher::Prefetch", "dns_prefetching");
		OP_DBG((UNI_L("PARSING url: %s"), url.GetAttribute(URL::KUniName).CStr()));

		if (burst_start == 0)
			burst_start = g_op_time_info->GetRuntimeMS();

		burst_prefetches++;
	}
#endif // LOGDOC_PREFETCH_PARSING_AND_DYNAMIC
}

/* virtual */ void
DNSPrefetcher::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_DNS_PREFETCH_START_BURST);
	burst_prefetches = 0;
	burst_start = 0;
	posted_message = FALSE;
}

void
DNSPrefetcher::PostMessage(double delay)
{
	if (posted_message)
		return;

	if (!registered_callback)
	{
		if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_DNS_PREFETCH_START_BURST, (MH_PARAM_1) this)))
			return;
		registered_callback = TRUE;
	}

	g_main_message_handler->PostDelayedMessage(MSG_DNS_PREFETCH_START_BURST, (MH_PARAM_1) this, 0, static_cast<unsigned long>(delay));
	posted_message = TRUE;
}

#endif // DNS_PREFETCHING
