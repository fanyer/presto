/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#if defined(INTERNAL_ICO_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

// http://www.dcs.ed.ac.uk/home/mxr/gfx/2d/BMP.txt

#include "modules/img/src/imagedecoderico.h"

ImageDecoderIco::ImageDecoderIco()
	: image_decoder_listener(NULL),
	  width(0),
	  height(0),
	  bits_per_pixel(0),
	  col_mapsize(0),
	  decode(0),
	  img_to_decode(0),
	  decoding_finished(FALSE),
	  is_alpha(FALSE)
{
}

ImageDecoderIco::~ImageDecoderIco()
{
	if (decode != NULL)
	{
		if (decode->icEntries)
		{
			OP_DELETEA(decode->icEntries);
		}
		OP_DELETE(decode);
	}
}

static inline UINT16 MakeUINT16(UCHAR c0, UCHAR c1)
{
	return
		c0 << 8 |
		c1;
}

static BOOL IsNonBlank16By16Entry(ICO_DIRENTRY* dir_entry)
{
	return dir_entry->bWidth == 16 && dir_entry->bHeight == 16 && dir_entry->bColorCount > 1;
}

OP_STATUS ImageDecoderIco::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	OP_STATUS need_more_status = more ? OpStatus::OK : OpStatus::ERR;

	resendBytes = 0;
	if (decoding_finished)
	{
		return OpStatus::OK;
	}

	UINT32 buf_used = 0;

    // We need a decode-structure to hold the data
	if (!decode)
	{
		decode = OP_NEW(ImageIcoDecodeInfo, ());
		if (!decode)
			return OpStatus::ERR_NO_MEMORY;
		else
			CleanDecoder();
	}

	if (!decode->header_loaded)
	{
		// read file header
		int read_bytes = ReadIcoFileHeader(data, numBytes);

		if (read_bytes == 0)
		{
			resendBytes = numBytes; // need more data
			return need_more_status;
		}
		else
			buf_used += read_bytes;

		if (decode->header.idCount == 0)
			return OpStatus::ERR;

		decode->header_loaded = TRUE;
	}

	if (!decode->entries_loaded)
	{
		// allocate mem for all entries
		if (!decode->icEntries)
		{
			decode->icEntries = OP_NEWA(ICO_DIRENTRY, decode->header.idCount);
			if (!decode->icEntries)
				return OpStatus::ERR_NO_MEMORY;
		}

		// read entries
		int read_bytes = ReadIcoEntries(data + buf_used, numBytes - buf_used);

		if (read_bytes == 0)
		{
			resendBytes = numBytes; // need more data
			return need_more_status;
		}
		else
			buf_used += read_bytes;

		decode->entries_loaded = TRUE;

		// chose image to decode - this should be somewhere else
		img_to_decode = 0;
		ICO_DIRENTRY *current_ico = &decode->icEntries[0];

		int i = 1;
		while (i < decode->header.idCount)
		{
			ICO_DIRENTRY *candidate = &decode->icEntries[i];

			// Find the entry with the highest bit/color count. Prefer non-blank 16 by 16 entries if available.
			
			if ( (!IsNonBlank16By16Entry(current_ico) || IsNonBlank16By16Entry(candidate)) &&
				 (candidate->bColorCount >= current_ico->bColorCount || candidate->wBitCount >= current_ico->wBitCount))
			{
				img_to_decode = i;
				current_ico = candidate;
			}

			i++;
		}

		width = current_ico->bWidth;
		height = current_ico->bHeight;

		if (width && height)
		{
			if (image_decoder_listener != NULL)
			{
				image_decoder_listener->OnInitMainFrame(width, height);
			}
		}
		else
		{
			return OpStatus::ERR;
		}
	}

	UINT32 bytes_in_image = 0;

	if (!decode->reached_offset)
	{
		// fast forward to correct data point... (depending of img_to_decode)
		ICO_DIRENTRY *current_ico = &decode->icEntries[img_to_decode];
		bytes_in_image = current_ico->dwBytesInRes;
		if (current_ico->dwImageOffset + bytes_in_image > (UINT32)numBytes)
		{
			resendBytes = numBytes;
			return need_more_status;
		}
		else
			buf_used = current_ico->dwImageOffset;
	}

	// can we trust bytesinres?

	decode->reached_offset = TRUE;

	// FIXME: check dimensions and color depth again!

	const unsigned char* ptr = data + buf_used;

	if (bytes_in_image < 4)
	{
		return OpStatus::ERR;
	}

	UINT32 biSize = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);	//biSize

	if (biSize > bytes_in_image)
		return OpStatus::ERR;

	UINT32 image_bytes_left = bytes_in_image - biSize;

	buf_used += biSize;

	ptr += 4+4+4+2; // skip biSize, biWidth, biHeight, biPlanes
	bits_per_pixel = MakeUINT16(ptr[1], ptr[0]);

	switch (bits_per_pixel)
	{
		case 1:
		case 2:
		case 4:
		case 8:
		case 24:
		case 32:
			break;
		default:
			return OpStatus::ERR;
	}

	ptr += 2+4+4+4+4; // skip biBitCount, biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter
	UINT32 biClrUsed = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);

	col_mapsize = biClrUsed ? biClrUsed : (bits_per_pixel > 8 ? 0 : 1 << bits_per_pixel);

	ICO_DIRENTRY *current_ico = &decode->icEntries[img_to_decode];
	is_alpha = (current_ico->wPlanes >= 1 || current_ico->wBitCount == 0) && bits_per_pixel == 32;

	// ok, now we're ready to fetch icon data...

	ImageFrameData image_frame_data;
	image_frame_data.rect.width = width;
	image_frame_data.rect.height = height;
	image_frame_data.alpha = is_alpha;
	image_frame_data.transparent = bits_per_pixel != 32;
	image_frame_data.bits_per_pixel = bits_per_pixel;
	image_frame_data.bottom_to_top = TRUE;
	if (image_decoder_listener != NULL)
	{
		image_decoder_listener->OnNewFrame(image_frame_data);
	}

	// iterate through all bytes in source pixel data
	const UINT8* src = data + buf_used;

	if (bits_per_pixel <= 8)
		RETURN_IF_ERROR(ReadIndexed(src, image_bytes_left));
	else if (bits_per_pixel == 32)
		RETURN_IF_ERROR(ReadRaw32(src, image_bytes_left));
	else
		RETURN_IF_ERROR(ReadRaw(src, image_bytes_left));

	if (image_decoder_listener != NULL)
	{
		image_decoder_listener->OnDecodingFinished();
	}

	decoding_finished = TRUE;

	return OpStatus::OK;
}

OP_STATUS
ImageDecoderIco::ReadRaw32(const unsigned char* src, UINT32 bytes_in_image)
{
	if (width * height * 4 > bytes_in_image)
	{
		return OpStatus::ERR;
	}

	// we keep one line at a time of unpacked pixeldata
	UINT8* line = OP_NEWA(UINT8, width * 4);
	if (line == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// iterate through all lines
	for (UINT32 y = 0; y < height; y++)
	{
		for (UINT32 x = 0; x < width; x++)
		{
			UINT8* dst = line + x * 4;

			if (is_alpha)
				*(UINT32 *)dst = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
			else
				*(UINT32 *)dst = (0xffu << 24) | (src[2] << 16) | (src[1] << 8) | src[0];

			// next src byte
			src += 4;
		}

		// we have a line
		if (image_decoder_listener != NULL)
		{
			// image is upside down
			image_decoder_listener->OnLineDecoded(line, height - 1 - y, 1);
		}
	}

	OP_DELETEA(line);

	return OpStatus::OK;
}

OP_STATUS
ImageDecoderIco::ReadRaw(const unsigned char* src, UINT32 bytes_in_image)
{
	UINT32 real_width_src = ((bits_per_pixel * width + 31) / 32) * 4;
	UINT32 real_width_and = ((width + 31) / 32) * 4;

	if ((real_width_src + real_width_and) * height > bytes_in_image)
	{
		return OpStatus::ERR;
	}

	// we keep one line at a time of unpacked pixeldata
	UINT8* line = OP_NEWA(UINT8, width * 4);
	if (line == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// start of and-mask (transparency mask)
	const UINT8* src_and = src + (real_width_src * height);

	// iterate through all lines
	for (UINT32 y = 0; y < height; y++)
	{
		const UINT8 *next_and = src_and + real_width_and;
		const UINT8 *next_src = src + real_width_src;

		UINT32 and_bits=0;
		UINT8 src_and_byte=0;

		for (UINT32 x = 0; x < width; x++, src+=3)
		{
			if (and_bits == 0)
			{
				src_and_byte = *src_and++;
				and_bits = 8;
			}

			// the bit is set if pixel is transparent
			// image code expects 0x00 for transparency and 0xff for opaque
			BOOL is_transparent  = !!(src_and_byte & 0x80);
			src_and_byte <<= 1;
			and_bits--;

			UINT8* dst = line + x * 4;

			if (is_transparent)
			{
				*(UINT32 *)dst = 0;
			}
			else
			{
				*(UINT32 *)dst = (255u << 24) | (src[2] << 16) |
					(src[1] << 8) | src[0];
			}
		}

		// we have a line
		if (image_decoder_listener != NULL)
		{
			// image is upside down
			image_decoder_listener->OnLineDecoded(line, height - 1 - y, 1);
		}

		src = next_src;
		src_and = next_and;
	}

	OP_DELETEA(line);

	return OpStatus::OK;
}


OP_STATUS
ImageDecoderIco::ReadIndexed(const unsigned char* src, UINT32 bytes_in_image)
{
	UINT32 real_width_src = ((bits_per_pixel * width + 31) / 32) * 4;
	UINT32 real_width_and = ((width + 31) / 32) * 4;

	if ((real_width_src + real_width_and) * height + 4 * col_mapsize > bytes_in_image)
	{
		return OpStatus::ERR;
	}

	// we keep one line at a time of unpacked pixeldata
	UINT8* line = OP_NEWA(UINT8, width * 4);
	if (line == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// col_map is BGRABGRABGRA
	const UINT8* col_map = src;

	src += 4 * col_mapsize;

	// start of and-mask (transparency mask)
	const UINT8* src_and = src + (real_width_src * height);

	// iterate through all lines
	for (UINT32 y = 0; y < height; y++)
	{
		const UINT8 *next_and = src_and + real_width_and;
		const UINT8 *next_src = src + real_width_src;

		UINT32 src_bits=0, and_bits=0;
		UINT8 src_byte=0, src_and_byte=0;

		for (UINT32 x = 0; x < width; x++)
		{
			if (and_bits == 0)
			{
				src_and_byte = *src_and++;
				and_bits = 8;
			}
			if (src_bits == 0)
			{
				src_byte = *src++;
				src_bits = 8;
			}

			UINT8 dst_index = (src_byte >> (8 - bits_per_pixel));
			src_byte <<= bits_per_pixel;
			src_bits -= bits_per_pixel;

				// the bit is set if pixel is transparent
				// image code expects 0x00 for transparency and 0xff for opaque
			BOOL is_transparent = !!(src_and_byte & 0x80);
			src_and_byte <<= 1;
			and_bits--;

			UINT8* dst = line + x * 4;

 			const UINT8 black[] = { 0, 0, 0, 0 };
 			const UINT8* color = dst_index < col_mapsize ? col_map + dst_index * 4 : black;

			if (is_transparent)
			{
				*((UINT32*)dst) = 0;
			}
			else
			{
				*((UINT32*)dst) = (255u << 24) | (color[2] << 16) | (color[1] << 8) | color[0];
			}
		}

		// we have a line
		if (image_decoder_listener != NULL)
		{
			// image is upside down
			image_decoder_listener->OnLineDecoded(line, height - 1 - y, 1);
		}

		src = next_src;
		src_and = next_and;
	}

	OP_DELETEA(line);

	return OpStatus::OK;
}

void ImageDecoderIco::CleanDecoder()
{
	decode->header_loaded = FALSE;
	decode->entries_loaded = FALSE;
	decode->reached_offset = FALSE;
	decode->icEntries = NULL;
}

UINT32 ImageDecoderIco::ReadIcoFileHeader(const BYTE* data, UINT32 num_bytes)
{
	// pwfix: 4??
	if (num_bytes < 6) // check if enough data to start to read header
		return 0;

	ICO_HEADER *hd = &decode->header;

	hd->idReserved = data[0] | (data[1] << 8);
	data += 2;
	hd->idType = data[0] | (data[1] << 8);
	data += 2;
	hd->idCount = data[0] | (data[1] << 8);

	return 6;
}

UINT32 ImageDecoderIco::ReadIcoEntries(const BYTE* data, UINT32 num_bytes)
{
	UINT32 count = decode->header.idCount;

	if (num_bytes < count * 16)
		return 0; // wait until we can read all ICONDIR-entries

	for (UINT32 img_num = 0; img_num < count; img_num++)
	{
		ICO_DIRENTRY *icodir = &decode->icEntries[img_num];
		icodir->bWidth = *data++;
		icodir->bHeight = *data++;
		icodir->bColorCount = *data++;
		if(icodir->bColorCount == 0)
			icodir->bColorCount = 255;
		icodir->bReserved = *data++;
		icodir->wPlanes = data[0] | (data[1] << 8);
		data += 2;
		icodir->wBitCount = data[0] | (data[1] << 8);
		data += 2;
		icodir->dwBytesInRes = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		data += 4;
		icodir->dwImageOffset = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		data += 4;
	}

	return count * 16;
}

void ImageDecoderIco::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	image_decoder_listener = imageDecoderListener;
}

#endif // INTERNAL_ICO_SUPPORT
