/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/media/src/mediastate.h"

BOOL MediaState::Transition()
{
	OP_NEW_DBG("Transition", "MediaState");

	// increasing preload transitions
	if (coerced_preload < current_preload)
	{
		coerced_preload = current_preload;
		OP_DBG(("preload -> %s", coerced_preload.CStr()));
		return TRUE;
	}

	// networkState transitions
	BOOL force_loading = coerced_ready < pending_ready;
	if (force_loading && coerced_network == MediaNetwork::IDLE)
	{
		coerced_network = MediaNetwork::LOADING;
		return TRUE;
	}
	else if (coerced_network != current_network)
	{
#ifdef DEBUG_ENABLE_OPASSERT
# define TRANS_NETWORK(STATE1, STATE2) (coerced_network == MediaNetwork::STATE1 && current_network == MediaNetwork::STATE2)
		// networkState is controlled by the resource selection and
		// resource fetch algorithms, don't allow them to skip states.
		if (TRANS_NETWORK(EMPTY, NO_SOURCE) || TRANS_NETWORK(NO_SOURCE, EMPTY) ||
			TRANS_NETWORK(NO_SOURCE, LOADING) || TRANS_NETWORK(LOADING, NO_SOURCE))
			OP_ASSERT(coerced_ready == MediaReady::NOTHING);
		else if (TRANS_NETWORK(LOADING, IDLE))
			OP_ASSERT((coerced_preload == MediaPreload::NONE && coerced_ready == MediaReady::NOTHING) || coerced_preload > MediaPreload::NONE);
		else if (TRANS_NETWORK(IDLE, LOADING))
			OP_ASSERT(coerced_preload > MediaPreload::NONE);
		else
			OP_ASSERT(current_network == MediaNetwork::EMPTY);
# undef TRANS_NETWORK
#endif // DEBUG_ENABLE_OPASSERT

		// make sure to not oscillate between LOADING and IDLE
		if (!(force_loading && coerced_network == MediaNetwork::LOADING && current_network == MediaNetwork::IDLE))
		{
			coerced_network = current_network;
			OP_DBG(("network -> %s", coerced_network.CStr()));
			return TRUE;
		}
	}

	// increasing readyState transitions
	if (coerced_ready < current_ready)
	{
		OP_ASSERT(coerced_preload > MediaPreload::NONE);

		MediaReady clamped_ready = MIN(current_ready, pending_ready);
		if (coerced_ready < clamped_ready)
		{
			// increase step-wise
			++coerced_ready;
			OP_DBG(("ready -> %s", coerced_ready.CStr()));
			return TRUE;
		}
	}
	// decreasing readyState transitions
	else if (coerced_ready > current_ready)
	{
		// skip directly to NOTHING and CURRENT_DATA, otherwise decrease step-wise
		if (current_ready == MediaReady::NOTHING || current_ready == MediaReady::METADATA)
			coerced_ready = current_ready;
		else
			--coerced_ready;
		OP_DBG(("ready -> %s", coerced_ready.CStr()));
		return TRUE;
	}

	return FALSE;
}

#endif //MEDIA_HTML_SUPPORT
