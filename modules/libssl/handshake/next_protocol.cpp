/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** @author Kajetan Switalski kswitalski@opera.com
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/next_protocol.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

SSL_NextProtocol_st::SSL_NextProtocol_st()
{
	AddItem(selected_protocol);
	AddItem(padding);
}

void SSL_NextProtocol_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	SSL_varvector8 spdy2, spdy3, http;
	spdy2.Append("spdy/2");
	spdy3.Append("spdy/3");
	http.Append("http/1.1");

#ifdef USE_SPDY
	uint32 idx;
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSpdy3, pending->server_info->GetServerName()) && pending->next_protocols_available.Contain(spdy3, idx))
	{
		selected_protocol = spdy3;
		pending->selected_next_protocol = NP_SPDY3;
	}
	else if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSpdy2, pending->server_info->GetServerName()) && pending->next_protocols_available.Contain(spdy2, idx))
	{
		selected_protocol = spdy2;
		pending->selected_next_protocol = NP_SPDY2;
	}
	else
#endif // USE_SPDY
	{
		selected_protocol = http;
		pending->selected_next_protocol = NP_HTTP;
	}

	padding.Resize(32 - ((selected_protocol.GetLength() + 2) % 32));
}

#endif // relevant support
