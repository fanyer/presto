/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _JPG_SUPPORT_

#include "modules/img/decoderfactoryjpg.h"

#if defined(INTERNAL_JPG_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderjpg.h"

extern ImageDecoder* create_jpeg_decoder();

ImageDecoder* DecoderFactoryJpg::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = create_jpeg_decoder();
	if (image_decoder != NULL)
	{
		image_decoder->SetImageDecoderListener(listener);
	}
	return image_decoder;
}

#endif // INTERNAL_JPG_SUPPORT

#if defined (USE_JAYPEG) && !defined (ASYNC_IMAGE_DECODERS)
#include "modules/jaypeg/jaydecoder.h"
#include "modules/jaypeg/jayimage.h"

class JaySize : public JayImage
{
public:
	JaySize() : valid(FALSE){}
	int init(int width, int height, int numComponents, BOOL progressive){
		w = width;
		h = height;
		valid = TRUE;
		// Return an error to abort decoding
		return JAYPEG_ERR;
	}
	void scanlineReady(int scanline, const unsigned char *imgdata){}
	int w, h;
	BOOL valid;
};

BOOL3 DecoderFactoryJpg::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	JaySize size;
	JayDecoder decoder;
	int result = decoder.init(data, len, &size, FALSE);
	if (result == JAYPEG_ERR || result == JAYPEG_ERR_NO_MEMORY)
		return NO;
	if (result == JAYPEG_NOT_ENOUGH_DATA)
		return MAYBE;
	result = decoder.decode(data, len);
	if (size.valid)
	{
		width = size.w;
		height = size.h;
		return YES;
	}
	if (result == JAYPEG_ERR || result == JAYPEG_ERR_NO_MEMORY)
		return NO;
	// not enough data
	return MAYBE;
}

BOOL3 DecoderFactoryJpg::CheckType(const UCHAR* data, INT32 len)
{
	JayDecoder decoder;
	int result = decoder.init(data, len, NULL, FALSE);
	if (result == JAYPEG_OK)
		return YES;
	if (result == JAYPEG_ERR || result == JAYPEG_ERR_NO_MEMORY)
		return NO;
	// not enough data
	return MAYBE;
}

#endif // USE_JAYPEG && !ASYNC_IMAGE_DECODERS

#ifdef USE_SYSTEM_JPEG

BOOL3 DecoderFactoryJpg::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	// we need 8 bytes look-a-head
	const UCHAR* data_end = data + len - 7;
	
	while (data < data_end)
	{
		// find marker
		while ((data < data_end) && (*data != 0xff)) 
			data++;

		// skip duplicates
		while ((data < data_end) && (*data == 0xff)) 
			data++;

		// not enough data in buffer!
		if (data > data_end)
			return MAYBE;

		// soi marker (start of image)
		if (*data == 0xd8)
		{
			continue;
		}
		
		// a sof marker is anything of
		// 0xc0 - 0xc3, 0xc5-0x7, 0xc9-0xcb, 0xcd-0xcf
		if ((*data >= 0xc0) &&
			(*data <= 0xcf) &&
			(*data != 0xc4) &&
			(*data != 0xc8) &&
			(*data != 0xcc))
		{
			height = (data[4] << 8) | (data[5]);
			width = (data[6] << 8) | (data[7]);
			return YES;
		}
		else
		{
			// skip data for all other types of markers
			UINT32 length = (data[1] << 8) | (data[2]);
			data += (length + 1);
		}
	}

	return MAYBE;
}

BOOL3 DecoderFactoryJpg::CheckType(const UCHAR* data, INT32 len)
{
	if (len < 2)
	{
		return MAYBE;
	}
	OP_ASSERT(data != NULL);

	char* src = (char*)data;

	if (((unsigned char*)src)[0] == 0xff &&
		((unsigned char*)src)[1] == 0xd8)
	{
		return YES;
	}
	
	return NO;
}

#endif // USE_SYSTEM_JPEG

#endif // _JPG_SUPPORT_
