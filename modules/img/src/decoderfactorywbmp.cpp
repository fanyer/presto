/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef WBMP_SUPPORT

#include "modules/img/decoderfactorywbmp.h"

#ifdef INTERNAL_WBMP_SUPPORT
#include "modules/img/src/imagedecoderwbmp.h"
#endif // INTERNAL_WBMP_SUPPORT

enum WbmpIsDecoding
{
	wType,		///< In image type information
	wFixHeader,	///< In WBMP fixed header
	wWidth,		///< In image width information
	wHeight		///< In image height information
};

#ifdef INTERNAL_WBMP_SUPPORT
ImageDecoder* DecoderFactoryWbmp::CreateImageDecoder(ImageDecoderListener* listener)
{
	ImageDecoder* image_decoder = OP_NEW(ImageDecoderWbmp, ());
	if (image_decoder != NULL)
	{
		image_decoder->SetImageDecoderListener(listener);
	}
	return image_decoder;
}

#endif // INTERNAL_WBMP_SUPPORT

BOOL3 DecoderFactoryWbmp::CheckSize(const UCHAR* data, INT32 len, INT32& width, INT32& height)
{
	UINT32 buf_used = 0;
	BOOL header_loaded = FALSE;
	WbmpIsDecoding wbmp_inField = wType;
	UINT32	wbmp_type = 0;
	width = 0;
	height = 0;

	// Load header
	while (!header_loaded)
	{
		switch (wbmp_inField)
		{
		case wType:
			if (*data & 0x80)
			{
				// Continues in next byte
				wbmp_type = (wbmp_type | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				wbmp_type |= *data;

				// We only support WBMP level 0
				if (wbmp_type != 0)
				{
					return NO;
				}
				wbmp_inField = wFixHeader;
			}
			break;

		case wFixHeader:
			if (*data & 0x80)
			{
				// Continues in next byte
			}
			else
			{
				// Last byte
				wbmp_inField = wWidth;
				// NOTE: WBMP allows for ExtHeader, but its size is always
				// zero in level 0, so it is presently ignored.
			}
			break;

		case wWidth:
			if (*data & 0x80)
			{
				// Continues in next byte
				width = (width | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				width |= *data;
				wbmp_inField = wHeight;
			}
			break;

		case wHeight:
			if (*data & 0x80)
			{
				// Continues in next byte
				height = (height | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				height |= *data;
				header_loaded = TRUE;
			}
			break;
		}
		
		data++;
		buf_used++;

		if (buf_used >= (UINT32)len)
		{
			return MAYBE;
		}

	}
	return (width > 0 && height > 0) ? YES : NO; // FIXME:KILSMO
}

BOOL3 DecoderFactoryWbmp::CheckType(const UCHAR* data, INT32 len)
{
	if (len < 8)
	{
		return MAYBE;
	}
	if (data[0] == 0x00 && data[1] == 0x00 && data[2] != 0x00)
	{
		if(data[2] == 1 && data[3] == 0) // this is an .ICO
		{
			return NO;
		}
		INT32 width, height;
		if(CheckSize(data, 8, width, height) == YES)
		{
			return YES;
		}
	}
	return NO;
}

#endif // WBMP_SUPPORT
