/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _BMP_SUPPORT_

#include "modules/img/decoderfactorybmp.h"

#if defined(INTERNAL_BMP_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderbmp.h"

ImageDecoder* DecoderFactoryBmp::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = OP_NEW(ImageDecoderBmp, ());
	if (image_decoder != NULL)
	{
		image_decoder->SetImageDecoderListener(listener);
	}
	return image_decoder;
}

#endif // INTERNAL_BMP_SUPPORT

BOOL3 DecoderFactoryBmp::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	if (len < 26)
	{
		return MAYBE;
	}

	UINT32 sizebuf = MakeUINT32(data[17], data[16], data[15], data[14]);

	if (sizebuf < 40)
	{
		width   = MakeUINT32(0, 0, data[19], data[18]);
		height  = MakeUINT32(0, 0, data[21], data[20]);
	}
	else
	{
		width   = MakeUINT32(data[21], data[20], data[19], data[18]);
		height  = MakeUINT32(data[25], data[24], data[23], data[22]);
		if (height < 0)
			height = -height;
	}

	if (width <= 0 || height <= 0)
	{
		return NO;
	}

	return YES;
}

BOOL3 DecoderFactoryBmp::CheckType(const UCHAR* data, INT32 len)
{
	if (len < 2)
	{
		return MAYBE;
	}
	if (data[0] == 'B' && data[1] == 'M')
	{
		return YES;
	}
	return NO;
}

#endif // _BMP_SUPPORT_
