/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/img/decoderfactorygif.h"

#if defined(INTERNAL_GIF_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecodergif.h"

ImageDecoder* DecoderFactoryGif::CreateImageDecoder(ImageDecoderListener* listener)
{
	return ImageDecoderGif::Create(listener);
}

#endif // INTERNAL_GIF_SUPPORT

BOOL3 DecoderFactoryGif::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	// FIXME:IMG We might want to check for more data, if size is 0 or something
	if (len < 10)
	{
		return MAYBE;
	}

	// Get the logical dimensions
	width = (((UINT32)data[7]) << 8) | (data[6]);
	height = (((UINT32)data[9]) << 8) | (data[8]);

#ifndef ASYNC_IMAGE_DECODERS
	// Some gif's lies about their dimensions. Both IE and Mozilla ignores the logical dimensions if it is a
	// single frame gif. The dimensions of the first frame is used instead.
	peekwidth = 0;
	peekheight = 0;
	ImageDecoderGif* tmp = ImageDecoderGif::Create(this, FALSE);
	if (tmp == NULL)
	{
		return YES;
	}

	int resendBytes;
	if (OpStatus::IsError(tmp->DecodeData((unsigned char*)data, len, FALSE, resendBytes)))
	{
		OP_DELETE(tmp);
		return YES; // Just go ahead with the logical dimensions instead.
	}

	if (peekwidth > 0 && peekheight > 0)
	{
		OP_DELETE(tmp);
		width = peekwidth;
		height = peekheight;
		return YES;
	}

	OP_DELETE(tmp);

	return MAYBE;
#else
	return YES;
#endif
}

BOOL3 DecoderFactoryGif::CheckType(const UCHAR* data, INT32 len)
{
	char* src = (char*)data;

	if (len < 6)
	{
		return MAYBE;
	}

	if (op_strncmp(src, "GIF", 3) != 0 ||
		src[3] < '0' || src[3] > '9' ||
		src[4] < '0' || src[4] > '9' ||
		src[5] < 'A' || src[5] > 'z' )
	{
		return NO;
	}

	return YES;
}
