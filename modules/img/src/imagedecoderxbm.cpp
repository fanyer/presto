/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#if defined(INTERNAL_XBM_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderxbm.h"

#include "modules/util/str.h"

ImageDecoderXbm::ImageDecoderXbm()
{
	image_decoder_listener = NULL;
	header_loaded = FALSE;
	start_pixel = 0;
	last_decoded_line = 0;
	width = 0;
	height = 0;
	row_BGRA = NULL;
}

ImageDecoderXbm::~ImageDecoderXbm()
{
	if (row_BGRA)
		OP_DELETEA(row_BGRA);
}

OP_STATUS ImageDecoderXbm::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	resendBytes = 0;
	int buf_used = 0;

	while (!header_loaded)
	{
		int line_len = GetNextLine((const unsigned char*)data + buf_used, numBytes - buf_used);
		if (!line_len)
		{
			if (more)
			{
				resendBytes = numBytes - buf_used;
			}
			return OpStatus::OK;
		}

		const unsigned char* start_dim = strnstr((const unsigned char*)data + buf_used, line_len-1, (const unsigned char*) WidthString);
		if (start_dim)
		{
			start_dim += op_strlen(WidthString);
			width = op_atoi((const char*) start_dim);
		}
		else
		{
			start_dim = strnstr((const unsigned char*)data + buf_used, line_len-1, (const unsigned char*) HeightString);
			if (start_dim)
			{
				start_dim += op_strlen(HeightString);
				height = op_atoi((const char*) start_dim);
			}
		}

		buf_used += line_len;

		if (width && height)
		{
			header_loaded = TRUE;

			if (image_decoder_listener != NULL)
			{
				if (width <= 0 || width >= 65536 || height <= 0 || height >= 65536)
				{
					return OpStatus::ERR;
				}
				if (!image_decoder_listener->OnInitMainFrame(width, height))
					return OpStatus::OK;

				ImageFrameData image_frame_data;
				image_frame_data.rect.width = width;
				image_frame_data.rect.height = height;
				image_frame_data.interlaced = TRUE;
				image_frame_data.bits_per_pixel = 1;
				image_frame_data.transparent = TRUE;
				image_decoder_listener->OnNewFrame(image_frame_data);
			}
			// alloc a line to write line data into
			row_BGRA = OP_NEWA(UINT32, width);
			if (!row_BGRA)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	// Load bitmap data
	// start_pixel indicates where to start in last_decoded_line
	BYTE src_bit = 0;
	UINT32 target_pixel = start_pixel;
	UINT32 current_line = last_decoded_line;

	while (current_line < height)
	{
		BYTE next_byte;
		while ((target_pixel < width) && GetNextInt((const unsigned char*)data, numBytes, buf_used, next_byte))
		{
			for (src_bit = 0; (src_bit < 8) && (target_pixel < width); src_bit++)
			{
				UINT8 *dest_row = (UINT8*)&row_BGRA[target_pixel];

				if ((next_byte>>src_bit)&0x01)
				{
					*((UINT32*)dest_row) = 0xFF000000;
				}
				else
				{
					// transparent
					dest_row[0] = dest_row[1] = dest_row[2] = dest_row[3] = 0x00;
				}

				target_pixel++;
			}
		}
		if (target_pixel < width)
		{
			// need more data
			start_pixel = target_pixel;
			last_decoded_line = current_line;
			resendBytes = numBytes - buf_used;
			return OpStatus::OK;
		}
		else
		{
			target_pixel = 0;
			if (image_decoder_listener)
				image_decoder_listener->OnLineDecoded(row_BGRA, current_line, 1);

			current_line++;
		}
	}

	if (current_line == height || !more)
	{
		// Tells us that we are ready parsing.
		if (image_decoder_listener != NULL)
			image_decoder_listener->OnDecodingFinished();
	}

	return OpStatus::OK;
}

int ImageDecoderXbm::GetNextLine(const unsigned char* buf, int buf_len)
{
	const unsigned char* tmp = buf;
	int i;
	for (i=0; i<buf_len; i++)
	{
		if (*tmp == '\r' || *tmp == '\n')
			break;
		tmp++;
	}
	if (i < buf_len)
	{
		// fount '\r' or '\n'
		return i+1;
	}
	else
		return 0;
}

BOOL ImageDecoderXbm::GetNextInt(const unsigned char* buf, int buf_len, int& buf_used, BYTE& next_byte)
{
	BOOL ok = FALSE;
	// check if enough data
	if ((buf_len - buf_used) < 5)
		return FALSE;

	const unsigned char* tmp = strnstr(buf + buf_used, (buf_len - buf_used - 1), (unsigned char*) "0x");
	if (tmp)
	{
		if ((buf_len - (tmp - buf)) < 5)
			return FALSE;

		buf_used = tmp - buf;

		tmp += 2;
		if (op_isxdigit(*tmp) && op_isxdigit(*(tmp+1)))
		{
			next_byte = (op_isdigit(*tmp)) ? *tmp-'0' : (op_isupper(*tmp)) ? *tmp-'A'+10 : *tmp-'a'+10;
			next_byte *= 16;
			tmp++;
			next_byte += (op_isdigit(*tmp)) ? *tmp-'0' : (op_isupper(*tmp)) ? *tmp-'A'+10 : *tmp-'a'+10;
			ok = TRUE;
		}

		buf_used += 4;
	}
	else if (buf_len-buf_used > 4)
		buf_used = buf_len-4;

	return ok;
}

void ImageDecoderXbm::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	image_decoder_listener = imageDecoderListener;
}

#endif // INTERNAL_XBM_SUPPORT
