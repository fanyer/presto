/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#include "core/pch.h"

#ifdef USE_SPDY

#include "modules/url/protocols/spdy/spdy_internal/spdy_frameheaders.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_pingcontroller.h"
#include "modules/url/protocols/spdy/spdy_protocol.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

SpdyPingController::SpdyPingController(SpdyVersion v, UINT32 firstPingId):
	listener(NULL), 
	lastPingId(0),
	lastPingTime(0.),
	lastRoundTripTime(0.),
	nextPingId(firstPingId),
	version(v),
	sendPingTimerSet(FALSE)
{
	sendPingEnabled = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyPingEnabled);
	if (sendPingEnabled)
	{
		sendPingTimer.SetTimerListener(this);
		pingTimeoutTimer.SetTimerListener(this);
	}
}

BOOL SpdyPingController::HasNextFrame() const
{
	return !buffer.IsEmpty() || (sendPingEnabled && !sendPingTimerSet);
}

void SpdyPingController::OnTimeOut(OpTimer* timer)
{
	if (timer == &pingTimeoutTimer)
		listener->OnPingTimeout();
	else
	{
		PingFrameHeader pfh(version, lastPingId = nextPingId);
		if (OpStatus::IsSuccess(buffer.AppendCopyData(reinterpret_cast<const char *>(&pfh), PingFrameHeader::GetSize())))
		{
			lastPingTime = g_op_time_info->GetRuntimeMS();
			nextPingId += 2;
			sendPingTimer.Start(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyPingTimeout) * 1000 * 2);
			sendPingTimerSet = TRUE;
			pingTimeoutTimer.Start(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SpdyPingTimeout) * 1000);
			listener->OnHasDataToSend();
		}
	}
}

void SpdyPingController::WriteNextFrameL(SpdyNetworkBuffer &target)
{
	if (sendPingEnabled && !sendPingTimerSet)
		OnTimeOut(&sendPingTimer);
	target.AppendCopyDataL(buffer);
	buffer.Clear();
}

void SpdyPingController::PingFrameLoaded(const PingFrameHeader *frameHeader)
{
	if (frameHeader->GetId() % 2 == nextPingId % 2)
	{
		double currentTime = g_op_time_info->GetRuntimeMS();
		OP_ASSERT(lastPingId == frameHeader->GetId());
		lastRoundTripTime = currentTime - lastPingTime;
		pingTimeoutTimer.Stop();
	}
	else
	{
		if (OpStatus::IsSuccess(buffer.AppendCopyData(reinterpret_cast<const char *>(frameHeader), PingFrameHeader::GetSize())))
			listener->OnHasDataToSend();
	}
}

UINT8 SpdyPingController::GetPriority() const
{
	return 0xff; // ping has always the highest priority
}

#endif // USE_SPDY
