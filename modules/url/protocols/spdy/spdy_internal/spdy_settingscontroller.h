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

#ifndef _SPDY_SETTINGSCONTROLLER_H_
#define _SPDY_SETTINGSCONTROLLER_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_framesproducer.h"

class SettingsFrameHeader;

class SpdySettingsController: public SpdyDataConsumer, public SpdyFramesProducer
{
	OpData buffer;
	OpData initialFrame;
	size_t leftToConsume;
	UINT32 maxConcurrentStreams;
	UINT32 uploadBandwidth;
	UINT32 downloadBandwidth;
	UINT32 roundTripTime;
	UINT32 currentCwnd;
	UINT32 downloadRetransRate;
	UINT32 initialWindowSize;
	SpdyVersion version;
	ServerName_Pointer serverName;
	unsigned short port;
	OpString8 hostport;
	BOOL privateMode;

	BOOL SetSetting(UINT32 id, UINT32 value);
public:
	SpdySettingsController(SpdyVersion version, ServerName_Pointer serverName, unsigned short port, BOOL privacyMode);

	void InitL();
	void SettingsFrameLoaded(const SettingsFrameHeader *settingsFrame);

	UINT32 GetMaxConcurrentStreams() { return maxConcurrentStreams; }
	UINT32 GetUploadBandwidth() { return uploadBandwidth; }
	UINT32 GetDownloadBandwidth() { return downloadBandwidth; }
	UINT32 GetRoundTripTime() { return roundTripTime; }
	UINT32 GetCurrentCwnd() { return currentCwnd; }
	UINT32 GetDownloadRetransRate() { return downloadRetransRate; }
	UINT32 GetInitialWindowSize() { return initialWindowSize; }

	virtual BOOL ConsumeDataL(OpData &data);
	virtual BOOL HasNextFrame() const;
	virtual void WriteNextFrameL(SpdyNetworkBuffer &target);
	virtual UINT8 GetPriority() const;
};


#endif // _SPDY_SETTINGSCONTROLLER_H_

