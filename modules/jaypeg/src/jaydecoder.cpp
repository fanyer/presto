/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(_JPG_SUPPORT_) && defined (USE_JAYPEG)

#include "modules/jaypeg/jaydecoder.h"
#include "modules/jaypeg/jaypeg.h"

#include "modules/jaypeg/src/jayformatdecoder.h"
#include "modules/jaypeg/src/jayjp2decoder.h"

#include "modules/jaypeg/src/jayjfifdecoder.h"
#include "modules/jaypeg/src/jayjfifmarkers.h"

#include "modules/jaypeg/jayimage.h"

JayDecoder::JayDecoder() : decoder(NULL)
{}

JayDecoder::~JayDecoder()
{
	OP_DELETE(decoder);
}

#ifdef JAYPEG_JP2_SUPPORT
static const unsigned char jp2_id[] = {
	0x00,
	0x00,
	0x00,
	0x0c,

	0x6a,
	0x50,
	0x20,
	0x20,

	0x0d,
	0x0a,
	0x87,
	0x0a
};
#endif // JAYPEG_JP2_SUPPORT
int JayDecoder::init(const unsigned char *buffer, unsigned int bufferlen, JayImage *img, BOOL decode_data)
{
	OP_ASSERT(!decoder);
#ifdef JAYPEG_JFIF_SUPPORT
	if (bufferlen < 2)
		return JAYPEG_NOT_ENOUGH_DATA;
	if (buffer[0] == 0xff && buffer[1] == JAYPEG_JFIF_MARKER_SOI)
	{
		// if no output image is specified, only check if initialization would be ok
		if (!img)
			return JAYPEG_OK;
		decoder = OP_NEW(JayJFIFDecoder, (decode_data));
		if (!decoder)
		{
			return JAYPEG_ERR_NO_MEMORY;
		}
		((JayJFIFDecoder*)decoder)->init(img);
	}
#endif
#ifdef JAYPEG_JP2_SUPPORT
	if (!decoder)
	{
		if (bufferlen < 12)
			return JAYPEG_NOT_ENOUGH_DATA;
		bool valid_jp2 = true;
		for (unsigned int i = 0; i < 12; ++i)
		{
			if (buffer[i] != jp2_id[i])
				valid_jp2 = false;
		}
		if (valid_jp2){
			// if no output image is specified, only check if initialization would be ok
			if (!img)
				return JAYPEG_OK;
			decoder = OP_NEW(JayJP2Decoder, (decode_data));
			if (!decoder)
			{
				return JAYPEG_ERR_NO_MEMORY;
			}
		}
	}
#endif
	if (!decoder)
	{
		return JAYPEG_ERR;
	}
	return JAYPEG_OK;
}

int JayDecoder::decode(const unsigned char *buffer, unsigned int bufferlen)
{
	// decoder must be initialized
	if (!decoder)
	{
		return JAYPEG_ERR;
	}
	stream.initData(buffer, bufferlen);
	int res = decoder->decode(&stream);
	if (res == JAYPEG_NOT_ENOUGH_DATA)
		return stream.getLength();
	return res;
}

void JayDecoder::flushProgressive()
{
	if (decoder)
		decoder->flushProgressive();
}

BOOL JayDecoder::isDone()
{
	if (decoder)
		return decoder->isDone();
	return FALSE;
}

BOOL JayDecoder::isFlushed()
{
	if (decoder)
		return decoder->isFlushed();
	return FALSE;
}

#endif

