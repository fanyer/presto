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

#include "modules/url/protocols/spdy/spdy_internal/spdy_streamreseter.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_frameheaders.h"

void SpdyStreamReseter::ResetStream(UINT32 streamId, UINT32 resetStatus)
{
	ResetStreamFrameHeader rsfh(version, streamId, static_cast<SpdyResetStatus>(resetStatus));
	buffer.AppendCopyData(reinterpret_cast<char*>(&rsfh), ResetStreamFrameHeader::GetSize());
}

BOOL SpdyStreamReseter::HasNextFrame() const
{
	return !buffer.IsEmpty();
}

void SpdyStreamReseter::WriteNextFrameL(SpdyNetworkBuffer &target)
{
	target.AppendCopyDataL(buffer);
	buffer.Clear();
}

UINT8 SpdyStreamReseter::GetPriority() const
{
	return 0xff;
}

#endif // USE_SPDY
