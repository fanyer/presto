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

#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_hdrcompr.h"


BOOL SpdyDevNullConsumer::ConsumeDataL(OpData &data)
{
	size_t dataToRead = MIN(dataToConsume, data.Length());
	data.Consume(dataToRead);
	dataToConsume -= dataToRead;
	return dataToConsume != 0;
}

BOOL SpdyDecompressingConsumer::ConsumeDataL(OpData &data)
{
	size_t dataToRead = MIN(dataToConsume, data.Length());
	size_t newDataToConsume = dataToConsume - dataToRead;

	char *compressedChunkBuff = reinterpret_cast<char*>(g_memory_manager->GetTempBuf());
	size_t compressedChunkBuffSize = g_memory_manager->GetTempBufLen();
	z_stream &decompressor = compressionProvider->GetDecompressionStream();

	while (dataToRead>0)
	{
		size_t compressedChunkLength = data.CopyInto(compressedChunkBuff, MIN(dataToRead, compressedChunkBuffSize));
		dataToRead -= compressedChunkLength;
		data.Consume(compressedChunkLength);

		decompressor.next_in = reinterpret_cast<Bytef*>(compressedChunkBuff);
		decompressor.avail_in = compressedChunkLength;

		while (decompressor.avail_in > 0)
		{
			char *outputBuffer = static_cast<char*>(g_memory_manager->GetTempBuf2k());
			size_t outputBufferSize = g_memory_manager->GetTempBuf2kLen();

			decompressor.next_out = reinterpret_cast<Bytef*>(outputBuffer);
			decompressor.avail_out = outputBufferSize;
			LEAVE_IF_ERROR(compressionProvider->Decompress());
		}
	}

	dataToConsume = newDataToConsume;
	return dataToConsume != 0;
}

#endif // USE_SPDY

