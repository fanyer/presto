/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ICO_SUPPORT

#include "modules/img/decoderfactoryico.h"

#if defined(INTERNAL_ICO_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderico.h"

ImageDecoder* DecoderFactoryIco::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = OP_NEW(ImageDecoderIco, ());
	if (image_decoder != NULL)
	{
		image_decoder->SetImageDecoderListener(listener);
	}
	return image_decoder;
}

#endif // INTERNAL_ICO_SUPPORT

BOOL3 DecoderFactoryIco::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	// FIXME:IMG We might want to check for more data, if size is 0 or something
	if (len < 10)
	{
		return MAYBE;
	}

#ifndef ASYNC_IMAGE_DECODERS
	peekwidth = 0;
	peekheight = 0;
	ImageDecoderIco tmp;
	tmp.SetImageDecoderListener(this);
	int resendBytes;
	OP_STATUS ret_val = tmp.DecodeData((unsigned char*)data, len, TRUE, resendBytes);

	if (peekwidth == 0 && peekheight == 0)
		return OpStatus::IsError(ret_val) ? NO : MAYBE;

	width = peekwidth;
	height = peekheight;

	return YES;
#else
	// We don't know which of the entries the external decoder will use. We guess the first one.
	UINT16 count = ((UINT16*)data)[2];
	if (count > 0)
	{
		const UCHAR* entry = &data[6];
		width = entry[0];
		height = entry[1];
	}
	else
	{
		// No entries. Probably a invalid ico-file.
		width = 16;
		height = 16;
	}
	return YES;
#endif
}

BOOL3 DecoderFactoryIco::CheckType(const UCHAR* data, INT32 len)
{
	if (len < 4)
	{
		return MAYBE;
	}
	if (op_memcmp(data, "\0\0\1\0", 4) == 0)
	{
		return YES;
	}
	return NO;
}

#ifndef ASYNC_IMAGE_DECODERS
BOOL DecoderFactoryIco::OnInitMainFrame(INT32 width, INT32 height)
{
	peekwidth = width;
	peekheight = height;
	return FALSE;
}
#endif

#endif // ICO_SUPPORT
