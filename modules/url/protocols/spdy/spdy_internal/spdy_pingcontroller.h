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

#ifndef _SPDY_PINGCONTROLLER_H_
#define _SPDY_PINGCONTROLLER_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_framesproducer.h"
#include "modules/hardcore/timer/optimer.h"

class PingFrameHeader;
class SpdyFramesHandlerListener;

class SpdyPingController: public SpdyFramesProducer, public OpTimerListener
{
	OpData buffer;
	SpdyFramesHandlerListener *listener;
	UINT32 lastPingId;
	double lastPingTime;
	double lastRoundTripTime;
	UINT32 nextPingId;
	SpdyVersion version;
	OpTimer sendPingTimer;
	OpTimer pingTimeoutTimer;
	BOOL sendPingTimerSet;
	BOOL sendPingEnabled;

public:
	SpdyPingController(SpdyVersion v, UINT32 firstPingId);
	void SetListener(SpdyFramesHandlerListener *l) { listener = l; }

	void PingFrameLoaded(const PingFrameHeader *frameHeader);
	virtual BOOL HasNextFrame() const;
	virtual void WriteNextFrameL(SpdyNetworkBuffer &target);
	virtual UINT8 GetPriority() const;
	double GetLastRoundTripTime() const { return lastRoundTripTime; }
	virtual void OnTimeOut(OpTimer* timer);
};

#endif // _SPDY_PINGCONTROLLER_H_