/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#ifdef INTERNAL_WBMP_SUPPORT

#include "modules/img/src/imagedecoderwbmp.h"

ImageDecoderWbmp::ImageDecoderWbmp()
{
	image_decoder_listener = NULL;

	wbmp_inField = wType;
	wbmp_type = 0;
	wbmp_fixheader = 0;
	wbmp_width = 0;
	wbmp_height = 0;
	start_pixel = 0;
	error = FALSE;

	bits_per_pixel = 0;
	header_loaded = FALSE;
	line_BGRA = NULL;
	last_decoded_line = 0;
}

ImageDecoderWbmp::~ImageDecoderWbmp()
{
	if (line_BGRA)
		OP_DELETEA(line_BGRA);
}

OP_STATUS ImageDecoderWbmp::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	resendBytes = 0;
	UINT32 buf_used = 0;

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
					error = TRUE;
					return OpStatus::OK;
				}
				wbmp_inField = wFixHeader;
			}
			break;

		case wFixHeader:
			if (*data & 0x80)
			{
				// Continues in next byte
				wbmp_fixheader = (wbmp_fixheader | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				wbmp_fixheader |= *data;
				wbmp_inField = wWidth;
				// NOTE: WBMP allows for ExtHeader, but its size is always
				// zero in level 0, so it is presently ignored.
			}
			break;

		case wWidth:
			if (*data & 0x80)
			{
				// Continues in next byte
				wbmp_width = (wbmp_width | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				wbmp_width |= *data;
				wbmp_inField = wHeight;
			}
			break;

		case wHeight:
			if (*data & 0x80)
			{
				// Continues in next byte
				wbmp_height = (wbmp_height | (*data & 0x7F)) << 7;
			}
			else
			{
				// Last byte
				wbmp_height |= *data;
				header_loaded = TRUE;
			}
			break;
		}

		data++;
		buf_used++;

		if (buf_used >= (UINT32)numBytes)
		{
			return OpStatus::OK;
		}

		if (header_loaded)
		{
			if (wbmp_width <= 0 || wbmp_width >= 65535 || wbmp_height <= 0 || wbmp_height >= 65535)
			{
				return OpStatus::ERR;
			}
			bits_per_pixel = 1;

			// allocate data line
			line_BGRA = OP_NEWA(UINT32, wbmp_width);
			if (line_BGRA == NULL)
				return OpStatus::ERR_NO_MEMORY;

			// tell listener header info
			if (image_decoder_listener)
			{
				image_decoder_listener->OnInitMainFrame(wbmp_width, wbmp_height);
				ImageFrameData image_frame_data;
				image_frame_data.rect.width = wbmp_width;
				image_frame_data.rect.height = wbmp_height;
				image_frame_data.transparent = TRUE;
				image_frame_data.bits_per_pixel = bits_per_pixel;
				image_decoder_listener->OnNewFrame(image_frame_data);
			}
		}
	}

	// Load bitmap data
	// start_pixel indicates where to start in last_decoded_line
	UINT32 current_pixel = start_pixel;
	UINT32 current_line = last_decoded_line;
	UINT32 bytewidth = wbmp_width / 8;
	if (bytewidth * 8 != wbmp_width) bytewidth ++;

	while ((buf_used < (UINT32)numBytes) && (current_line < wbmp_height))
	{
		while ((current_pixel < wbmp_width) && (buf_used < (UINT32)numBytes))
		{
			BYTE next_byte = *(data);
			for (int src_bit = 7; src_bit >= 0; src_bit--)
			{
				if (current_pixel == wbmp_width)
					break;

				UINT8 *dest_row = (UINT8*)&line_BGRA[current_pixel];

				if ((next_byte>>src_bit)&0x01)
				{
					*((UINT32*)dest_row) = 0; // 'white' pixels are transparent
				}
				else
				{
					*((UINT32*)dest_row) = 0xFF000000;
				}
				current_pixel++;
			}
			buf_used++;
			data++;
		}

		// check if a whole line was decoded -- if so, tell listener
		if (current_pixel == wbmp_width)
		{
			// signal decoded line
			if (image_decoder_listener)
				image_decoder_listener->OnLineDecoded(line_BGRA, current_line, 1);

			current_pixel = 0;
			start_pixel = 0;
			current_line++;
			last_decoded_line = current_line;
		}
		else
		{
			start_pixel = current_pixel;
			last_decoded_line = current_line;
		}
	}

	if (current_line == wbmp_height || !more)
	{
		last_decoded_line = current_line;

		if (image_decoder_listener)
			image_decoder_listener->OnDecodingFinished();

		return OpStatus::OK;
	}

	resendBytes = numBytes - buf_used;
	return OpStatus::OK;
}

void ImageDecoderWbmp::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	image_decoder_listener = imageDecoderListener;
}

#endif // INTERNAL_WBMP_SUPPORT
