/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPMEDIASOURCEIMPL_H
#define OPMEDIASOURCEIMPL_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/windowcommander/OpMediaSource.h"
#include "modules/media/src/mediasource.h"

class OpMediaSourceImpl :
	public OpMediaSource,
	public MediaSourceClient
{
public:
	OpMediaSourceImpl(MediaSource* source);
	virtual ~OpMediaSourceImpl();

	// OpMediaSource
	virtual const char* GetContentType() { return m_source->GetContentType(); }
	virtual BOOL IsSeekable() { return m_source->IsSeekable(); }
	virtual OpFileLength GetTotalBytes() { return m_source->GetTotalBytes(); }
	virtual OP_STATUS GetBufferedBytes(const OpMediaByteRanges** ranges) { return m_source->GetBufferedBytes(ranges); }
	virtual OP_STATUS Read(void* buffer, OpFileLength offset, OpFileLength size);
	virtual OP_STATUS Preload(OpFileLength offset, OpFileLength size);

private:
	MediaSource* m_source;
};

#endif // MEDIA_PLAYER_SUPPORT

#endif // OPMEDIASOURCEIMPL_H
