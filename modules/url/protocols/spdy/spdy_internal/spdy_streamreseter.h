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

#ifndef _SPDY_STREAMRESETER_H_
#define _SPDY_STREAMRESETER_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_framesproducer.h"

/**
 * This frames producer has one simple purpose - to cancel unwanted or invalid streams initiated by the server side.
 */
class SpdyStreamReseter: public SpdyFramesProducer
{
	OpData buffer;
	SpdyVersion version;
public:
	SpdyStreamReseter(SpdyVersion v): version(v) {} 
	void ResetStream(UINT32 streamId, UINT32 resetStatus);

	virtual BOOL HasNextFrame() const;
	virtual void WriteNextFrameL(SpdyNetworkBuffer &target);
	virtual UINT8 GetPriority() const;
};

#endif // _SPDY_STREAMRESETER_H_
