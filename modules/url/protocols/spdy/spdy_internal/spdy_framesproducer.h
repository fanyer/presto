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

#ifndef _SPDY_FRAMESPRODUCER_H_
#define _SPDY_FRAMESPRODUCER_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_networkbuffer.h"

/**
 * A base class for all classes that can produce SPDY frames
 */
class SpdyFramesProducer: public ListElement<SpdyFramesProducer>
{
public:
	virtual ~SpdyFramesProducer() { Out(); }

	virtual BOOL HasNextFrame() const = 0;
	virtual void WriteNextFrameL(SpdyNetworkBuffer &target) = 0;
	virtual UINT8 GetPriority() const = 0;
};


#endif // _SPDY_FRAMESPRODUCER_H_

