/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#if defined(INTERNAL_BMP_SUPPORT) || defined(ASYNC_IMAGE_DECODERS_EMULATION)

#include "modules/img/src/imagedecoderbmp.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

/* constants for the biCompression field */
#define BMP_RGB        0	// supported
#define BMP_RLE8       1	// supported
#define BMP_RLE4       2	// supported
#define BMP_BITFIELDS  3	// supported

ImageDecoderBmp::ImageDecoderBmp()
{
	// null all pointers
	image_decoder_listener = NULL;
	decode = NULL;
	line_BGRA = NULL;
	col_map = NULL;

	width = 0;
	height = 0;
	bits_per_pixel = 0;
	last_decoded_line = 0;
	start_pixel = 0;
	is_rle_encoded = FALSE;
	is_upside_down = TRUE;
	total_buf_used = 0;
}

ImageDecoderBmp::~ImageDecoderBmp()
{
	if (line_BGRA != NULL)
	{
		OP_DELETEA(line_BGRA);
		line_BGRA = NULL;
	}

	if (col_map != NULL)
	{
		OP_DELETEA(col_map);
		col_map = NULL;
	}

	if (decode != NULL)
	{
		OP_DELETE(decode);
		decode = NULL;
	}
}

OP_STATUS ImageDecoderBmp::DecodeData(const BYTE* _data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	resendBytes = 0;
	BYTE* data = (BYTE*)_data;
	UINT32 buf_used = 0;

    if (!decode)
    {
		// create decode-struct to put header data in
		decode = OP_NEW(ImageBmpDecodeInfo, ());
		if (!decode)
			return OpStatus::ERR_NO_MEMORY;
		else
			CleanDecoder();
    }

	if (!decode->fileheader_loaded)
	{
		// read file header
		int read_bytes = ReadBmpFileHeader(data, numBytes);

		if (read_bytes == 0)
		{
			resendBytes = numBytes; // need more data
			return OpStatus::OK;
		}
		else
		{
			buf_used += read_bytes;
			total_buf_used += read_bytes;
		}

		decode->fileheader_loaded = TRUE;
	}

	if (!decode->infoheader_loaded)
	{
		// read info header
		int read_bytes = ReadBmpInfoHeader(data + buf_used, numBytes - buf_used);

		if (read_bytes == 0)
		{
			resendBytes = numBytes - buf_used; // needs more data
			return OpStatus::OK;
		}
		else
		{
			buf_used += read_bytes;
			total_buf_used += read_bytes;
		}

		if ((decode->bmi.bmh.biWidth * height) > (50 * 1024 * 1024) || decode->bmi.bmh.biWidth >= 32768 || height >= 32768)
		{
			return OpStatus::ERR;
		}
		if (width <= 0 || height <= 0)
		{
			return OpStatus::ERR;
		}

		// determine compression format (if any)
		if (decode->bmi.bmh.biCompression == BMP_RLE4 || decode->bmi.bmh.biCompression == BMP_RLE8)
			is_rle_encoded = TRUE;
		else if (decode->bmi.bmh.biCompression != BMP_BITFIELDS && decode->bmi.bmh.biCompression != BMP_RGB)
			return OpStatus::ERR; // unknown format!

		if (bits_per_pixel != 1 &&
			bits_per_pixel != 4 &&
			bits_per_pixel != 8 &&
			bits_per_pixel != 16 &&
			bits_per_pixel != 24 &&
			bits_per_pixel != 32)
		{
			return OpStatus::ERR; // illegal bits_per_pixel
		}

		if (is_rle_encoded && bits_per_pixel != 4 && bits_per_pixel != 8)
		{
			return OpStatus::ERR;
		}

		decode->infoheader_loaded = TRUE;

		if (bits_per_pixel == 16 || bits_per_pixel == 24 || bits_per_pixel == 32)
		{
			decode->colormap_loaded = TRUE;  // skip color map part for these depths

			// if 16bit -- must determine correct bitmasks for RGB
			if (bits_per_pixel == 16 && decode->bmi.bmh.biCompression == BMP_BITFIELDS)
			{
				// read RGB masks - OBS need to use memcpy here because of ARM data alignment
				for (int i = 0; i < 3; i++)
				{
					bitfields_mask[i] = (UINT32)data[buf_used] +
										(UINT32)(data[buf_used + 1] << 8) +
										(UINT32)(data[buf_used + 2] << 16) +
										(UINT32)(data[buf_used + 3] << 24);
					buf_used += 4;
				}
				total_buf_used += 12;
			}
			else if (bits_per_pixel == 16 && decode->bmi.bmh.biCompression == BMP_RGB)
			{
				// set default bitmasks
				bitfields_mask[0] = 0x7c00; // R
				bitfields_mask[1] = 0x03e0; // G
				bitfields_mask[2] = 0x001f; // B
			}
		}

		// tell listener header info
		image_decoder_listener->OnInitMainFrame(width, height);

		ImageFrameData image_frame_data;
		image_frame_data.rect.width = width;
		image_frame_data.rect.height = height;
		image_frame_data.interlaced = TRUE;
		image_frame_data.bits_per_pixel = bits_per_pixel;
#ifdef SUPPORT_ALPHA_IN_32BIT_BMP
		if (bits_per_pixel == 32)
			image_frame_data.alpha = TRUE;
#endif // SUPPORT_ALPHA_IN_32BIT_BMP
		image_frame_data.bottom_to_top = is_upside_down;
		image_decoder_listener->OnNewFrame(image_frame_data);

		// now, when wo know the size of picture -- allocate data line
		line_BGRA = OP_NEWA(UINT32, width);

		if (line_BGRA == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		last_decoded_line = is_upside_down ? height - 1 : 0;
	}

	if (!decode->colormap_loaded)
	{
		col_mapsize = 1<<bits_per_pixel;

		// check if biClrUsed is set to other than 0 (bmp not using a full palette)
		if (!decode->is_bitmapcoreinfo && (UINT32)decode->bmi.bmh.biClrUsed != 0)
		{
			if ((UINT32)decode->bmi.bmh.biClrUsed > col_mapsize)
				return OpStatus::ERR;
			col_mapsize = (UINT32)decode->bmi.bmh.biClrUsed;
		}

		UINT32 col_map_bytesize = col_mapsize * ((decode->is_bitmapcoreinfo) ? sizeof(RGBT) : sizeof(RGBQ));
		if (numBytes - buf_used < col_map_bytesize)
		{
			if (more)
			{
				resendBytes = numBytes - buf_used;
				return OpStatus::OK;
			}
			else
			{
				return OpStatus::ERR;
			}
		}

		// allocate color map
		col_map = OP_NEWA(RGBQ, col_mapsize);
		if (!col_map)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		for (UINT32 i=0; i < col_mapsize; i++)
		{
			col_map[i].rgbBlue = *(data + buf_used++);
			col_map[i].rgbGreen = *(data + buf_used++);
			col_map[i].rgbRed = *(data + buf_used++);
			if (!decode->is_bitmapcoreinfo)
				buf_used++;
		}
		if (!decode->is_bitmapcoreinfo)
		{
			total_buf_used += col_mapsize * 4;
		}
		else
		{
			total_buf_used += col_mapsize * 3;
		}
		decode->colormap_loaded = TRUE;
	}

	UINT32 bytes_left = numBytes - buf_used;

	if (total_buf_used + bytes_left <= decode->bmf.bfOffBits)
	{
		total_buf_used += bytes_left;
		return OpStatus::OK;
	}
	else if (total_buf_used < (INT32)decode->bmf.bfOffBits)
	{
		buf_used += decode->bmf.bfOffBits - total_buf_used;
		total_buf_used += bytes_left;
	}

	// Load bitmap data line by line

	if (is_rle_encoded)
		resendBytes = ReadRleData(data + buf_used, numBytes - buf_used, more);
	else
		resendBytes = ReadData(data + buf_used, numBytes - buf_used, more);
	return OpStatus::OK;
}

void ImageDecoderBmp::CleanDecoder()
{
	decode->is_bitmapcoreinfo = FALSE;
	decode->fileheader_loaded = FALSE;
	decode->infoheader_loaded = FALSE;
	decode->colormap_loaded = FALSE;
}

inline UINT32 from_little_endian(const BYTE *data, int size)
{
	UINT32 result = 0;
	for (int i=0; i<size; i++)
	{
		result |= *data++ << i*8;
	}
	return result;
}

UINT32 ImageDecoderBmp::ReadBmpFileHeader(BYTE* data, UINT32 num_bytes)
{
	int bytes_read = 0;

	if (num_bytes < 14) // check if enough data to read header
		return 0;

	decode->bmf.bfType = from_little_endian(data + bytes_read, 2);
	bytes_read += 2;
	decode->bmf.bfSize = from_little_endian(data + bytes_read, 4);
	bytes_read += 4;
	decode->bmf.bfReserved1 = from_little_endian(data + bytes_read, 2);
	bytes_read += 2;
	decode->bmf.bfReserved2 = from_little_endian(data + bytes_read, 2);
	bytes_read += 2;
    decode->bmf.bfOffBits = from_little_endian(data + bytes_read, 4);
    bytes_read += 4;

	return bytes_read;
}

UINT32 ImageDecoderBmp::ReadBmpInfoHeader(BYTE* data, UINT32 num_bytes)
{
	if (num_bytes < 16) // check if enough data to read coreheader
		return 0;

	int read_bytes = 0;

	decode->bmi.bmh.biSize = from_little_endian(data, 4);
	read_bytes += 4;

	if (decode->bmi.bmh.biSize < 40) // the size of the infoheader
	{
		op_memset(&decode->bmi.bmh, 0, sizeof(decode->bmi.bmh));

		//coreheader
		decode->is_bitmapcoreinfo = TRUE;

		decode->bmi.bmh.biWidth = from_little_endian(data + read_bytes, 2);
		read_bytes += 2;
		decode->bmi.bmh.biHeight = from_little_endian(data + read_bytes, 2);
		read_bytes += 2;
		decode->bmi.bmh.biPlanes = from_little_endian(data + read_bytes, 2);
		read_bytes += 2;
		decode->bmi.bmh.biBitCount = from_little_endian(data + read_bytes,2);
		read_bytes += 2;

		width = decode->bmi.bmh.biWidth;
		height = decode->bmi.bmh.biHeight;
		if (height < 0)
		{
			is_upside_down = FALSE;
			height = -height;
		}
		bits_per_pixel = decode->bmi.bmh.biBitCount;
	}
	else
	{
		if (num_bytes < 40) // check if enough data to read infoheader
			return 0;

		// infoheader
		decode->bmi.bmh.biWidth = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biHeight = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biPlanes = from_little_endian(data + read_bytes, 2);
		read_bytes += 2;
		decode->bmi.bmh.biBitCount = from_little_endian(data + read_bytes, 2);
		read_bytes += 2;
		decode->bmi.bmh.biCompression = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biSizeImage = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biXPelsPerMeter = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biYPelsPerMeter = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biClrUsed = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;
		decode->bmi.bmh.biClrImportant = from_little_endian(data + read_bytes, 4);
		read_bytes += 4;

		width = decode->bmi.bmh.biWidth;
		height = decode->bmi.bmh.biHeight;
		if (height < 0)
		{
			is_upside_down = FALSE;
			height = -height;
		}
		bits_per_pixel = decode->bmi.bmh.biBitCount;
	}

	return read_bytes;
}

void ImageDecoderBmp::SetPixel(UINT32 pixel, UINT16 color_index)
{
	// OP_ASSERT(pixel < width);
	if (pixel >= (UINT32)width)
		return;

	RGBQ* col;
	RGBQ black = { 0, 0, 0, 0 };
	if ( color_index < col_mapsize )
		col = &col_map[color_index];
	else
		col = &black;

#ifdef SUPPORT_ALPHA_IN_32BIT_BMP
	line_BGRA[pixel] = (col->rgbAlpha << 24) | (col->rgbRed << 16) | (col->rgbGreen << 8) | (col->rgbBlue);
#else // !SUPPORT_ALPHA_IN_32BIT_BMP
	line_BGRA[pixel] = 0xFF000000 | (col->rgbRed << 16) | (col->rgbGreen << 8) | (col->rgbBlue);
#endif // SUPPORT_ALPHA_IN_32BIT_BMP
}

INT32 ImageDecoderBmp::ReadRleData(BYTE* data, UINT32 num_bytes, BOOL more)
{
	UINT32 copy_w = 2; // we need at least two bytes to read anything
	int current_line = last_decoded_line;
	UINT32 bytes_read = 0;
	BYTE mask = 0; // to be able to handle different amount of bits per pixel
	if (bits_per_pixel == 1) mask = 0x01;
	if (bits_per_pixel == 4) mask = 0x0F;
	if (bits_per_pixel == 8) mask = 0xFF;
	int dest_pixel = start_pixel;
	int line_diff = is_upside_down ? -1 : 1;

	while (((INT32)(num_bytes - bytes_read) >= (INT32)copy_w) && (current_line >= 0))
	{
		// convert bitmap line to 32-bit data
		BYTE* line_data = data + bytes_read; // a pointer to current place in data

		while(/*dest_pixel < width &&*/ ((INT32)(num_bytes - bytes_read) >= 2))
		{
			// read two bytes from file
			BYTE byte1 = *line_data++;
			BYTE byte2 = *line_data++;
			bytes_read += 2;

			if (byte1 != 0) // byte1 tells number of pixels using color(s) in byte2
			{
				BYTE num_pixels = byte1;
				if (dest_pixel + num_pixels > width) // don't write data out of bounds
					num_pixels = width - dest_pixel;

				UINT8 color[2];
				color[1] = byte2&mask;
				color[0] = byte2>>(8-bits_per_pixel)&mask;

				BYTE col_num = 0; // used to switch between the two colors
				while(num_pixels-- > 0)
				{
					SetPixel(dest_pixel, color[col_num&0x01]);
					dest_pixel++; col_num++;
				}
			}
			else if (byte2 == 2) // delta move: following 2 bytes tells us where to move
			{
				// check if we have enough data
				if (num_bytes - bytes_read < 2)
				{
					start_pixel = dest_pixel;
					last_decoded_line = current_line;
					return (num_bytes - bytes_read) + 2; //keep last two bytes til next round
				}

				// remember x-pos
				UINT8 orig_pixel = dest_pixel;

				// read next bytes to determine where we're heading
				BYTE byte3 = *line_data++;
				BYTE byte4 = *line_data++;
				bytes_read += 2;

				// fill the rest of line with color=0 and signal finished line to listener
				while (byte4 > 0)
				{
					if (current_line >= 0 && current_line < height)
					{
						while (dest_pixel < width)
							SetPixel(dest_pixel++, 0);

						if (image_decoder_listener)
							image_decoder_listener->OnLineDecoded(line_BGRA, current_line, 1);
					}

					current_line += line_diff;
					dest_pixel = 0;
					byte4--;
				}

				// fill to orig_pixel + byte3
				byte3 += orig_pixel;
				while (dest_pixel < byte3)
				{
					SetPixel(dest_pixel++, 0);
				}
			}
			else if (byte2 == 0 || byte2 == 1) // newline or end_bmp
			{
				if (image_decoder_listener)
				{
					// make sure we never send uninitialized data to
					// image_decoder_listener - see security bug
					// CORE-16983
					while (dest_pixel < width)
						SetPixel(dest_pixel++, 0);
					image_decoder_listener->OnLineDecoded(line_BGRA, current_line, 1);
				}

				if (current_line == 0) // the picture is done
				{
					if (image_decoder_listener)
						image_decoder_listener->OnDecodingFinished();

					return 0;
				}

				current_line += line_diff;
				dest_pixel = 0;
			}
			else if (byte2 >= 3) // now we shall read byte2 number of pixels as is
			{
				int bytes_to_read = (int)op_ceil(((float)byte2 * bits_per_pixel) / 8);
				int fill_word = bytes_to_read%2; // all data is word aligned (we may have to step over one byte in the end)

				// check if we have enough data
				if (bytes_to_read + fill_word > (INT32)(num_bytes - bytes_read))
				{
					start_pixel = dest_pixel;
					last_decoded_line = current_line;
					return (num_bytes - bytes_read) + 2; //need more data (keep previous two bytes)
				}

				while (bytes_to_read-- > 0)
				{
					// read next byte to plot pixel(s)
					BYTE next_byte = *line_data++;
					bytes_read++;

					UINT8 color[2];
					color[1] = next_byte&mask;
					color[0] = next_byte>>(8-bits_per_pixel)&mask;

					for (int i = 0; i < (8-bits_per_pixel)/4 + 1; i++)
					{
						if (byte2-- > 0)
							SetPixel(dest_pixel++, color[i]);
					}

				}
				line_data += fill_word;
				bytes_read += fill_word;
			}
			start_pixel = dest_pixel;
		}
		dest_pixel = 0;
	}
	last_decoded_line = current_line;

	if (!more) // we will not get more data
	{
		if (image_decoder_listener)
			image_decoder_listener->OnDecodingFinished();

		return 0;
	}

	return num_bytes - bytes_read;
}

INT32 ImageDecoderBmp::ReadData(BYTE* data, UINT32 num_bytes,  BOOL more)
{
	UINT32 copy_w = (UINT32)(((long)width * bits_per_pixel + 7)/8);
	int bytes_read = 0;
	BYTE mask = 0; // to be able to handle different amount of bits per pixel
	if (bits_per_pixel == 1) mask = 0x01;
	if (bits_per_pixel == 4) mask = 0x0F;
	if (bits_per_pixel == 8) mask = 0xFF;

    UINT32 aligned_w = (copy_w + 3)/4*4;
	int current_line = last_decoded_line;
	int line_diff = is_upside_down ? -1 : 1;

	while ((num_bytes - bytes_read >= aligned_w) && (current_line >= 0) && (current_line <= height))
	{
		// convert bitmap line to 32-bit data
		BYTE* line_data = data + bytes_read; // a pointer to current place in data

		UINT32 dest_pixel = 0;
		int read_byte = 0;
		int bit_count = 8;

		while(dest_pixel < (UINT32)width)
		{
			if (bits_per_pixel <= 8)
			{
				int color = (line_data[read_byte]>>(bit_count - bits_per_pixel))&mask;

				SetPixel(dest_pixel++, color);

				bit_count -= bits_per_pixel;
				if (bit_count == 0)
				{
					read_byte++;
					bit_count = 8;
				}
			}
#ifdef SUPPORT_ALPHA_IN_32BIT_BMP
			else if (bits_per_pixel == 32)
			{
				line_BGRA[dest_pixel] = (line_data[read_byte + 3] << 24) | (line_data[read_byte + 2] << 16) | (line_data[read_byte + 1] << 8) | (line_data[read_byte]);

				read_byte += 4;
				dest_pixel++;
			}
#endif // SUPPORT_ALPHA_IN_32BIT_BMP
			else if (bits_per_pixel == 24 || bits_per_pixel == 32)
			{
				line_BGRA[dest_pixel] = 0xFF000000 | (line_data[read_byte + 2] << 16) | (line_data[read_byte + 1] << 8) | (line_data[read_byte]);

				read_byte += bits_per_pixel / 8;
				dest_pixel++;
			}
			else if (bits_per_pixel == 16)
			{
				UINT32 source = line_data[read_byte] | line_data[read_byte + 1]<<8; // read whole word
				UINT8 pix_blue = (source & bitfields_mask[2]) << 3;		//Blue
				UINT8 pix_green = ((source & bitfields_mask[1]) >>(5 + (bitfields_mask[1]!=0x03e0))) << 3;	//Green
				UINT8 pix_red = ((source & bitfields_mask[0]) >>(10 + (bitfields_mask[1]!=0x03e0))) << 3;	//Red

					// Added code to get "purer" colors, white should now be "FFFFFF" instead of "F8F8F8".
					// Quite noticable difference.
				pix_blue |= pix_blue >> 5;
				pix_green |= pix_green >> 5;
				pix_red |= pix_red >> 5;

				line_BGRA[dest_pixel] = 0xFF000000 | (pix_red << 16) | (pix_green << 8) | (pix_blue);

				read_byte += 2;
				dest_pixel++;
			}
		}
		if (image_decoder_listener)
				image_decoder_listener->OnLineDecoded(line_BGRA, current_line, 1);

		last_decoded_line = current_line;

		current_line += line_diff;
		bytes_read += aligned_w;
	}
	last_decoded_line = current_line;

	if (current_line < 0 || current_line >= height || !more) // we're done (or will not get more data anyway)
	{
		if (image_decoder_listener)
			image_decoder_listener->OnDecodingFinished();

		return 0;
	}

	return num_bytes - bytes_read;
}

void ImageDecoderBmp::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	image_decoder_listener = imageDecoderListener;
}

#endif // INTERNAL_BMP_SUPPORT
