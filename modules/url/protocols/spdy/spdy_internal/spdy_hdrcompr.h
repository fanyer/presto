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

#ifndef _SPDY_HDRCOMPR_H_
#define _SPDY_HDRCOMPR_H_

#include "modules/zlib/zlib.h"
#include "modules/url/protocols/spdy/spdy_protocol.h"

class SpdyCompressionProvider
{
	SpdyVersion version;
	BOOL secureMode;
	z_stream compressor;
	z_stream decompressor;

	static OP_STATUS ZlibErrorToOpStatus(int error);
public:
	SpdyCompressionProvider(SpdyVersion version, BOOL secureMode);
	~SpdyCompressionProvider();
	OP_STATUS Init();
	OP_STATUS Compress(int flushType = Z_SYNC_FLUSH);
	OP_STATUS Decompress();

	z_stream& GetCompressionStream();
	z_stream& GetDecompressionStream();
};

#endif // _SPDY_HDRCOMPR_H_
