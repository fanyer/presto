/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#include "core/pch.h"

#include "modules/img/src/imagedecodergif.h"
#include "modules/probetools/probepoints.h"

ImageDecoderGif::ImageDecoderGif() : global_palette(NULL),
									 global_palette_cols(0),
									 local_palette(NULL),
									 local_palette_cols(0),
									 global_width(0),
									 global_height(0),
									 global_colors(FALSE),
									 gif89a(FALSE),
									 listener(NULL),
									 lzw_decoder(NULL),
									 decode_lzw(FALSE),
									 gif_state(GIF_STATE_READ_HEADER),
									 frame_initialized(FALSE),
									 got_first_frame(FALSE),
									 got_graphic_extension(FALSE),
									 interlace_pass(0),
									 current_x_pos(0),
									 current_line(0),
									 line_buf(NULL)
{
}

ImageDecoderGif::~ImageDecoderGif()
{
	OP_DELETE(lzw_decoder);
	OP_DELETEA(local_palette);
	OP_DELETEA(global_palette);
	OP_DELETEA(line_buf);
}

ImageDecoderGif* ImageDecoderGif::Create(ImageDecoderListener* listener, BOOL decode_lzw)
{
	ImageDecoderGif* decoder = OP_NEW(ImageDecoderGif, ());
	if (decoder == NULL)
	{
		return NULL;
	}
	decoder->listener = listener;
	decoder->decode_lzw = decode_lzw;
	return decoder;
}

void ImageDecoderGif::SetImageDecoderListener(ImageDecoderListener* imageDecoderListener)
{
	listener = imageDecoderListener;
}

OP_STATUS ImageDecoderGif::DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL/* load_all = FALSE*/)
{
	OP_PROBE6(OP_PROBE_IMG_GIF_DECODE_DATA);
	OP_STATUS ret_val = OpStatus::OK;
	if (numBytes == 0 && !more)
	{
		resendBytes = 1; // Special hack to save footprint.
	}
	else
	{
		ret_val = DecodeDataInternal(data, numBytes, more, resendBytes);
	}
	RETURN_IF_ERROR(ret_val);
	if (!more && resendBytes >= 0)
	{
		if (gif_state != GIF_STATE_FINISHED)
		{
			if (!got_first_frame)
			{
				return OpStatus::ERR;
			}
			if (listener)
			{
				listener->OnDecodingFinished();
			}
			gif_state = GIF_STATE_FINISHED;
		}
		// Do not care about this error.
		resendBytes = 0;
		return OpStatus::OK;
	}
	return ret_val;
}

OP_STATUS ImageDecoderGif::DecodeDataInternal(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes)
{
	resendBytes = 0;
	int curr_pos = 0;

	int frame_count_this_round = 0;
	while (TRUE)
	{
		if (more && frame_count_this_round > 1)
		{
			// Only decode maximum of one frame each run. Otherwise we could lock in here too long.
			resendBytes = numBytes - curr_pos;
			return OpStatus::OK;
		}

		switch (gif_state)
		{
		case GIF_STATE_READ_HEADER:
			{
				if (numBytes - curr_pos < 6)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}

				if (op_strncmp("GIF8", (const char*)&data[curr_pos], 4) != 0
					|| (data[curr_pos + 4] != '7' && data[curr_pos + 4] != '9')
					|| data[curr_pos + 5] != 'a')
				{
					return OpStatus::ERR;
				}
				gif89a = (data[curr_pos + 4] == '9');
				curr_pos += 6;
				gif_state = GIF_STATE_READ_LOGICAL_SCREEN;
				break;
			}
		case GIF_STATE_READ_LOGICAL_SCREEN:
			{
				if (numBytes - curr_pos < 7)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				global_colors = !!(data[curr_pos + 4] & 0x80);
				global_width = data[curr_pos] + (data[curr_pos + 1] << 8);
				global_height = data[curr_pos + 2] + (data[curr_pos + 3] << 8);
				// Skip bg_color_index  (data[curr_pos + 5]);
				global_palette_cols = data[curr_pos + 4] & 0x07;
				curr_pos += 7;
				gif_state = global_colors ? GIF_STATE_READ_GLOBAL_COLORS : GIF_STATE_READ_DATA;
				break;
			}
		case GIF_STATE_READ_GLOBAL_COLORS:
			{
				int bytes_to_copy = (1 << (global_palette_cols + 1)) * 3; // Move to a function if needed.
				if (numBytes - curr_pos < bytes_to_copy)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				global_palette = OP_NEWA(UINT8, bytes_to_copy);
				if (global_palette == NULL)
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				op_memcpy(global_palette, (const char*)&data[curr_pos], bytes_to_copy);
				curr_pos += bytes_to_copy;
				gif_state = GIF_STATE_READ_DATA;
				break;
			}
		case GIF_STATE_READ_DATA:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				switch (data[curr_pos])
				{
				case GIF_EXTENSION:
					{
						gif_state = GIF_STATE_EXTENSION;
						break;
					}
				case GIF_IMAGE_DESCRIPTOR:
					{
						gif_state = GIF_STATE_IMAGE_DESCRIPTOR;
						break;
					}
				case GIF_TRAILER:
					{
						gif_state = GIF_STATE_TRAILER;
						break;
					}
				case 0:
					{
						// Should not happen, but will happen anyway.
						// This is a bug fix moved from old gif decoder. Violation of gif specification.
						break;
					}
				default:
					{
						return OpStatus::ERR;
						break;
					}
				}
				curr_pos++;
				break;
			}
		case GIF_STATE_EXTENSION:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				switch (data[curr_pos])
				{
				case GIF_COMMENT:
					{
						gif_state = GIF_STATE_COMMENT_EXTENSION;
						break;
					}
				case GIF_GRAPHICAL_CONTROL:
					{
						gif_state = GIF_STATE_GRAPHICAL_CONTROL_EXTENSION;
						break;
					}
				case GIF_APPLICATION:
					{
						gif_state = GIF_STATE_APPLICATION_EXTENSION;
						break;
					}
				default:
					{
						gif_state = GIF_STATE_SKIP_EXTENSION;
						break;
					}
				}
				curr_pos++;
				break;
			}
		case GIF_STATE_COMMENT_EXTENSION:
			{
				gif_state = GIF_STATE_SKIP_BLOCKS;
				break;
			}
		case GIF_STATE_GRAPHICAL_CONTROL_EXTENSION:
			{
				if (numBytes - curr_pos < 6)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				if (data[curr_pos] != 4 || data[curr_pos + 5] != 0)
				{
					return OpStatus::ERR;
				}

				// If we get two graphic control extensions in a row,
				// we will use the information from the second one.
				// This is a violation of the gif specification,
				// but there are street gifs out there that will
				// not follow the standard.

				int disposal_method = (data[curr_pos + 1] >> 2) & 0x03;
				switch (disposal_method)
				{
				case 2:
					frame_data.disposal_method = DISPOSAL_METHOD_RESTORE_BACKGROUND;
					break;
				case 3:
					frame_data.disposal_method = DISPOSAL_METHOD_RESTORE_PREVIOUS;
					break;
				default:
					frame_data.disposal_method = DISPOSAL_METHOD_DO_NOT_DISPOSE;
					break;
				}
				frame_data.transparent = !!(data[curr_pos + 1] & 0x01);
				frame_data.duration = data[curr_pos + 2] + (data[curr_pos + 3] << 8);
#ifdef IMG_FULL_SPEED_GIF
				if (frame_data.duration <= 1)
					frame_data.duration = 10;
#else
				frame_data.duration = MAX(frame_data.duration, 10);
#endif
				frame_data.transparent_index = data[curr_pos + 4];
				got_graphic_extension = TRUE;

				InitializeFrame();
				curr_pos += 6;
				gif_state = GIF_STATE_READ_DATA;
				break;
			}
		case GIF_STATE_APPLICATION_EXTENSION:
			{
				if (numBytes - curr_pos < 13)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				if (op_strncmp((const char*)&data[curr_pos + 1], "NETSCAPE2.0", 11) == 0 && data[curr_pos + 12] == 3)
				{
					curr_pos += 13;
					gif_state = GIF_STATE_NETSCAPE_EXTENSION;
				}
				else
				{
					gif_state = GIF_STATE_SKIP_EXTENSION;
				}
				break;
			}
		case GIF_STATE_NETSCAPE_EXTENSION:
			{
				if (numBytes - curr_pos < 3)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				if (data[curr_pos] == 1)
				{
					int nr_of_repeats = data[curr_pos + 1] + (data[curr_pos + 2] << 8);
					if (listener != NULL)
					{
						listener->OnAnimationInfo(nr_of_repeats);
					}
				}
				curr_pos += 3;
				gif_state = GIF_STATE_SKIP_BLOCKS;
				break;
			}
		case GIF_STATE_SKIP_EXTENSION:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				int block_size = data[curr_pos];
				if ((numBytes - curr_pos) < (block_size + 1))
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				curr_pos += block_size + 1;
				gif_state = GIF_STATE_SKIP_BLOCKS;
				break;
			}
		case GIF_STATE_SKIP_BLOCKS:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				int block_size = data[curr_pos];
				if (block_size == 0)
				{
					curr_pos++;
					gif_state = GIF_STATE_READ_DATA;
				}
				else
				{
					if ((numBytes - curr_pos) < (block_size + 1))
					{
						OP_ASSERT(numBytes - curr_pos >= 0);
						resendBytes = numBytes - curr_pos;
						return OpStatus::OK;
					}
					curr_pos += block_size + 1;
				}
				break;
			}
		case GIF_STATE_IMAGE_DESCRIPTOR:
			{
				if (numBytes - curr_pos < 9)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				if (!got_graphic_extension)
				{
					frame_data.transparent = FALSE;
					frame_data.transparent_index = 0;
				}
				frame_data.rect.x = data[curr_pos] + (data[curr_pos + 1] << 8);
				frame_data.rect.y = data[curr_pos + 2] + (data[curr_pos + 3] << 8);
				frame_data.rect.width = data[curr_pos + 4] + (data[curr_pos + 5] << 8);
				frame_data.rect.height = data[curr_pos + 6] + (data[curr_pos + 7] << 8);
				int packed_fields = data[curr_pos + 8];
				BOOL has_local_colors = !!(packed_fields >> 7);
				frame_data.pal_is_shared = !has_local_colors;
				frame_data.interlaced = !!((packed_fields >> 6) & 0x01);
				local_palette_cols = packed_fields & 0x07;
				if (!got_first_frame)
				{
					BOOL continue_decoding = TRUE;
					RETURN_IF_ERROR(SignalMainFrame(&continue_decoding));
					got_first_frame = TRUE;
					if (!continue_decoding)
					{
						gif_state = GIF_STATE_FINISHED;
						break;
					}
				}
				InitializeFrame();
				if (has_local_colors)
				{
					gif_state = GIF_STATE_LOCAL_COLORS;
				}
				else
				{
					RETURN_IF_ERROR(SignalNewFrame());
					gif_state = GIF_STATE_IMAGE_DATA_START;
				}
				curr_pos += 9;
				frame_count_this_round++;
				break;
			}
		case GIF_STATE_LOCAL_COLORS:
			{
				int bytes_to_copy = (1 << (local_palette_cols + 1)) * 3;
				if (numBytes - curr_pos < bytes_to_copy)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				local_palette = OP_NEWA(UINT8, bytes_to_copy);
				if (local_palette == NULL)
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				op_memcpy(local_palette, (const char*)&data[curr_pos], bytes_to_copy);
				RETURN_IF_ERROR(SignalNewFrame());
				curr_pos += bytes_to_copy;
				gif_state = GIF_STATE_IMAGE_DATA_START;
				break;
			}
		case GIF_STATE_IMAGE_DATA_START:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				if (decode_lzw && data[curr_pos] >= 2 && data[curr_pos] <= 8)
				{
					if (lzw_decoder == NULL)
					{
						lzw_decoder = LzwDecoder::Create(this);
						if (lzw_decoder == NULL)
						{
							return OpStatus::ERR_NO_MEMORY;
						}
					}
					int num_cols = local_palette ? local_palette_cols : global_palette_cols;
					lzw_decoder->Reset(num_cols, data[curr_pos], frame_data.transparent_index);
					gif_state = GIF_STATE_IMAGE_DATA;
				}
				else
				{
					if (frame_data.rect.width == 1 && frame_data.rect.height == 1 && decode_lzw && data[curr_pos] == 1)
					{
						UINT8 code[1];
						code[0] = frame_data.transparent ? frame_data.transparent_index : 0;
						OnCodesDecoded(code, 1);
					}
					gif_state = GIF_STATE_SKIP_BLOCKS;
				}
				curr_pos++;
				break;
			}
		case GIF_STATE_IMAGE_DATA:
			{
				if (numBytes - curr_pos < 1)
				{
					OP_ASSERT(numBytes - curr_pos >= 0);
					resendBytes = numBytes - curr_pos;
					return OpStatus::OK;
				}
				int block_size = data[curr_pos];
				if (block_size == 0)
				{
					curr_pos++;
					gif_state = GIF_STATE_READ_DATA;
				}
				else
				{
					if ((numBytes - curr_pos) < (block_size + 1))
					{
						OP_ASSERT(numBytes - curr_pos >= 0);
						resendBytes = numBytes - curr_pos;
						return OpStatus::OK;
					}
					RETURN_IF_ERROR(lzw_decoder->DecodeData(&data[curr_pos + 1], block_size, more));
					curr_pos += block_size + 1;
				}
				break;
			}
		case GIF_STATE_TRAILER:
			{
				if (listener)
				{
					if (!got_first_frame)
					{
						BOOL continue_decoding = TRUE;
						RETURN_IF_ERROR(SignalMainFrame(&continue_decoding));
					}
					listener->OnDecodingFinished();
				}
				gif_state = GIF_STATE_FINISHED;
				break;
			}
		case GIF_STATE_FINISHED:
			{
				return OpStatus::OK;
				break;
			}
		default:
			{
				OP_ASSERT(FALSE);
				return OpStatus::ERR;
				break;
			}
		}
	}
}

OP_STATUS ImageDecoderGif::SignalMainFrame(BOOL* continue_decoding)
{
	OP_ASSERT(continue_decoding != NULL);
	*continue_decoding = TRUE;
	BOOL has_position = frame_data.rect.x > 0 || frame_data.rect.y > 0;
	if (frame_data.rect.width != 0 && frame_data.rect.height != 0)
	{
		if (global_width == frame_data.rect.width && global_height == frame_data.rect.height)
		{
			frame_data.rect.x = 0;
			frame_data.rect.y = 0;
		}
		else if (has_position &&
				 global_width == frame_data.rect.width + frame_data.rect.x &&
				 global_height == frame_data.rect.height + frame_data.rect.y)
		{
			if (!gif89a)
			{
				frame_data.rect.x = 0;
				frame_data.rect.y = 0;
				global_width = frame_data.rect.width;
				global_height = frame_data.rect.height;
			}
		}
		else
		{
			if (has_position)
			{
				if (frame_data.rect.width + frame_data.rect.x > global_width ||
					frame_data.rect.height + frame_data.rect.y > global_height)
				{
					global_width = frame_data.rect.width;
					global_height = frame_data.rect.height;
				}
			}
			else
			{
				if (!gif89a || frame_data.rect.width > global_width || frame_data.rect.height > global_height)
				{
					global_width = frame_data.rect.width;
					global_height = frame_data.rect.height;
				}
			}
		}
	}
	if (global_width == 0 || global_height == 0)
	{
		return OpStatus::ERR;
	}
	if (listener != NULL)
	{
		*continue_decoding = listener->OnInitMainFrame(global_width, global_height);
	}
	return OpStatus::OK;
}

OP_STATUS ImageDecoderGif::SignalNewFrame()
{
	if (local_palette)
	{
		frame_data.pal = local_palette;
		frame_data.num_colors = 1 << (local_palette_cols + 1);
		frame_data.bits_per_pixel = local_palette_cols + 1;
		OP_ASSERT(!frame_data.pal_is_shared);
	}
	else
	{
		frame_data.pal = global_palette;
		if (global_palette)
		{
			frame_data.num_colors = 1 << (global_palette_cols + 1);
		}
		else
		{
			frame_data.num_colors = 0;
		}
		frame_data.bits_per_pixel = global_palette_cols + 1;
		OP_ASSERT(frame_data.pal_is_shared);
	}

#ifdef SUPPORT_INDEXED_OPBITMAP
	line_buf = OP_NEWA(UINT8, frame_data.rect.width);
#else
	line_buf = OP_NEWA(UINT32, frame_data.rect.width);
#endif // SUPPORT_INDEXED_OPBITMAP

	if (line_buf == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if (listener != NULL)
	{
		ImageFrameData frame_data_copy = frame_data;
#ifndef SUPPORT_INDEXED_OPBITMAP
		frame_data_copy.pal = NULL;
#endif // !SUPPORT_INDEXED_OPBITMAP
		listener->OnNewFrame(frame_data_copy);
	}
	frame_data.disposal_method = DISPOSAL_METHOD_DO_NOT_DISPOSE;
	frame_data.duration = 10;
	got_graphic_extension = FALSE;
	interlace_pass = 0;
	current_x_pos = 0;
	current_line = 0;
	frame_initialized = FALSE;
	return OpStatus::OK;
}

void ImageDecoderGif::InitializeFrame()
{
	if (!frame_initialized)
	{
		OP_DELETEA(local_palette);
		local_palette = NULL;
		OP_DELETEA(line_buf);
		line_buf = NULL;
		frame_initialized = TRUE;
	}
}

void ImageDecoderGif::OnCodesDecoded(const UINT8* codes, int nr_of_codes)
{
	int curr_pos = 0;
	if (current_line >= frame_data.rect.height)
	{
		// No more data can fit.
		// Should not happen, but happens anyway (specially interlaced 1*1 pixel transparent images).
		return;
	}
	// line_buf needs to be added to the class, and initialized.
	OP_ASSERT(nr_of_codes > 0);
	while (nr_of_codes - curr_pos > 0)
	{
		int codes_to_copy = MIN(nr_of_codes - curr_pos, frame_data.rect.width - current_x_pos);
		OP_ASSERT(codes_to_copy > 0);
#ifdef SUPPORT_INDEXED_OPBITMAP
		op_memcpy(&line_buf[current_x_pos], &codes[curr_pos], codes_to_copy);
#else
		if (frame_data.pal != NULL)
		{
			for (int i = 0; i < codes_to_copy; i++)
			{
				int code = codes[curr_pos + i];
				if (frame_data.transparent && code == frame_data.transparent_index)
				{
					line_buf[current_x_pos + i] = 0x00;
				}
				else if (code < frame_data.num_colors)
				{
					UINT8* col = &frame_data.pal[code * 3];
					line_buf[current_x_pos + i] = 0xff000000 | (col[0] << 16) | (col[1] << 8) | col[2];
				}
				else
				{
					line_buf[current_x_pos + i] = 0xff000000;
				}
			}
		}
#endif // SUPPORT_INDEXED_OPBITMAP
		current_x_pos += codes_to_copy;
		curr_pos += codes_to_copy;
		OP_ASSERT(current_x_pos <= frame_data.rect.width);
		if (current_x_pos >= frame_data.rect.width)
		{
			// Copy line.
			int line_height = 1;
			int line_distance = 1;
			int next_start_line = frame_data.rect.height;
			if (frame_data.interlaced)
			{
				OP_ASSERT(interlace_pass >= 0 && interlace_pass <= 3);
				switch (interlace_pass)
				{
				case 0:
					line_height = 8;
					line_distance = 8;
					next_start_line = 4;
					break;
				case 1:
					line_height = 4;
					line_distance = 8;
					next_start_line = 2;
					break;
				case 2:
					line_height = 2;
					line_distance = 4;
					next_start_line = 1;
					break;
				case 3:
					line_distance = 2;
					break;
				default:
					OP_ASSERT(FALSE); // Should not happen.
					break;
				}
			}
			if (listener != NULL && frame_data.pal != NULL)
			{
				listener->OnLineDecoded(line_buf, current_line, line_height);
			}
			current_x_pos = 0;
			current_line += line_distance;
			if (current_line >= frame_data.rect.height)
			{
				current_line = next_start_line;
				if (!frame_data.interlaced)
				{
					return;
				}
				else
				{
					interlace_pass++;
					while (interlace_pass < 3 && current_line >= frame_data.rect.height)
					{
						interlace_pass++;
						current_line = 4 - interlace_pass;
						OP_ASSERT(current_line == 2 || current_line == 1);
						OP_ASSERT(interlace_pass == 2 || interlace_pass == 3);
					}
				}
			}
		}
	}
}

#ifdef SELFTEST

void ImageDecoderGif::InitTestingCodesDecoded(int width, int height, BOOL interlaced)
{
	frame_data.rect.x = 0;
	frame_data.rect.y = 0;
	frame_data.rect.width = width;
	frame_data.rect.height = height;
	frame_data.interlaced = interlaced;
	local_palette = OP_NEWA(UINT8, height * 3);
	SignalNewFrame();
}

#endif // SELFTEST


/////////////////////////////////////////////////
// LzwDecoder                                  //
/////////////////////////////////////////////////

LzwDecoder::LzwDecoder() : code_decoder(NULL),
						   nr_of_rest_bits(0),
						   rest_bits(0)
{
}

LzwDecoder::~LzwDecoder()
{
	OP_DELETE(code_decoder);
}

LzwDecoder* LzwDecoder::Create(LzwListener* listener)
{
	LzwDecoder* decoder = OP_NEW(LzwDecoder, ());
	if (decoder == NULL)
	{
		return NULL;
	}
	decoder->code_decoder = LzwCodeDecoder::Create(listener);
	if (decoder->code_decoder == NULL)
	{
		OP_DELETE(decoder);
		return NULL;
	}
	return decoder;
}

OP_STATUS LzwDecoder::DecodeData(const UINT8* data, int len, BOOL more)
{
	int curr_pos = 0;
	int num_bits = code_decoder->GetCodeSize();
	OP_ASSERT(num_bits <= 12 && num_bits > 0);
	while (curr_pos < len)
	{
		while (nr_of_rest_bits <= 24 && curr_pos < len)
		{
			rest_bits |= (data[curr_pos] << nr_of_rest_bits);
			nr_of_rest_bits += 8;
			curr_pos++;
		}
		while (num_bits <= nr_of_rest_bits)
		{
			short code = rest_bits & ((1 << num_bits) - 1);
			rest_bits >>= num_bits;
			nr_of_rest_bits -= num_bits;
			RETURN_IF_ERROR(code_decoder->DecodeCode(code));
			num_bits = code_decoder->GetCodeSize();
		}
	}
	return OpStatus::OK;
}

void LzwDecoder::Reset(int num_cols, int occupied_codes, int transparent_index)
{
	nr_of_rest_bits = 0;
	rest_bits = 0;
	code_decoder->Reset(num_cols, occupied_codes, transparent_index);
}

/////////////////////////////////////////////////
// LzwCodeDecoder                              //
/////////////////////////////////////////////////

LzwCodeDecoder::LzwCodeDecoder() : table(NULL),
								   old_code(0),
								   reached_end(FALSE),
								   decoding_started(FALSE)
{
}

LzwCodeDecoder* LzwCodeDecoder::Create(LzwListener* listener)
{
	LzwCodeDecoder* decoder = OP_NEW(LzwCodeDecoder, ());
	if (decoder == NULL)
	{
		return NULL;
	}
	decoder->table = OP_NEW(LzwStringTable, (listener));
	if (decoder->table == NULL)
	{
		OP_DELETE(decoder);
		return NULL;
	}
	return decoder;
}

LzwCodeDecoder::~LzwCodeDecoder()
{
	OP_DELETE(table);
}

void LzwCodeDecoder::Reset(int num_cols, int occupied_codes, int transparent_index)
{
	old_code = 0;
	reached_end = FALSE;
	decoding_started = FALSE;
	table->Reset(num_cols, occupied_codes, transparent_index);
}

int LzwCodeDecoder::GetCodeSize()
{
	return table->GetCodeSize();
}

OP_STATUS LzwCodeDecoder::DecodeCode(short code)
{
	OP_PROBE4(OP_PROBE_IMG_LZWCODEDECODER_DECODECODE);
	if (reached_end)
	{
		return OpStatus::OK;
	}

	LzwCodeType code_type = table->CodeType(code);

	OP_ASSERT(table->GetNrOfCodes() <= LZW_TABLE_SIZE);

	if (!decoding_started)
	{
		switch (code_type)
		{
		case LZW_CODE:
			table->OutputString(code);
			old_code = code;
			decoding_started = TRUE;
			break;
		case LZW_CLEAR_CODE:
			table->Clear();
			break;
		case LZW_END_CODE:
			reached_end = TRUE;
			break;
		default:
			OP_ASSERT(code_type == LZW_OUT_OF_BOUNDS_CODE);
			return OpStatus::ERR;
		}
	}
	else
	{
		switch (code_type)
		{
		case LZW_CODE:
			{
				if (code >= table->GetNrOfCodes())
				{
					UINT8 first_char;
					RETURN_IF_ERROR(table->GetFirstCharacter(old_code, first_char));
					RETURN_IF_ERROR(table->AddString(old_code, first_char));
					table->OutputString(code);
				}
				else
				{
					UINT8 first_char;
					RETURN_IF_ERROR(table->GetFirstCharacter(code, first_char));
					table->OutputString(code);
					RETURN_IF_ERROR(table->AddString(old_code, first_char));
				}
				old_code = code;
				break;
			}
		case LZW_CLEAR_CODE:
			{
				table->Clear();
				decoding_started = FALSE;
				break;
			}
		case LZW_END_CODE:
			{
				reached_end = TRUE;
				break;
			}
		default:
			{
				OP_ASSERT(code_type == LZW_OUT_OF_BOUNDS_CODE);
				return OpStatus::ERR;
			}
		}
	}
	return OpStatus::OK;
}

/////////////////////////////////////////////////
// LzwStringTable                              //
/////////////////////////////////////////////////

LzwStringTable::LzwStringTable(LzwListener* listener) : listener(listener),
														num_cols(0),
														first_code(0),
														next_code(0),
														code_size(0)
{
}

void LzwStringTable::Reset(int num_cols, int first_code, int transparent_index)
{
	OP_ASSERT(num_cols >= 0 && num_cols <= 7);
	OP_ASSERT(first_code >= 2 && first_code <= 8);
	this->num_cols = 1 << (num_cols + 1);
	this->first_code = 1 << first_code;
	this->zero_value = transparent_index;
	Clear();
}

void LzwStringTable::Clear()
{
	SetNextCode(first_code + 2);
	for (int i = 0; i < first_code; i++)
	{
		codes[i].parent = LZW_NO_PARENT;
		codes[i].data = i;
	}
	for (int j = next_code; j < LZW_TABLE_SIZE; j++)
	{
		codes[j].parent = LZW_NO_PARENT;
		codes[j].data = (UINT8) zero_value;
	}
	codes[first_code].parent = LZW_CLEAR;
	codes[first_code + 1].parent = LZW_END;
}

OP_STATUS LzwStringTable::AddString(short prefix, unsigned char suffix)
{
	OP_ASSERT(CodeType(prefix) == LZW_CODE);
	OP_ASSERT(0 <= next_code && next_code <= LZW_TABLE_SIZE);
	if (next_code == LZW_TABLE_SIZE)
	{
		return OpStatus::OK;
	}
	if (next_code > LZW_TABLE_SIZE ||
	    next_code == prefix)
	{
		return OpStatus::ERR;
	}

	short code_loop = prefix;
	while (0 <= code_loop && code_loop < LZW_TABLE_SIZE)
	{
		code_loop = codes[code_loop].parent;
		if (code_loop == next_code)
		{
			return OpStatus::ERR;
		}
	}

	codes[next_code].data = suffix;
	codes[next_code].parent = prefix;
	IncNextCode();
	return OpStatus::OK;
}

LzwCodeType LzwStringTable::CodeType(short code) const
{
	OP_ASSERT(code < LZW_TABLE_SIZE);
	if (code < 0 || code >= LZW_TABLE_SIZE)
	{
		return LZW_OUT_OF_BOUNDS_CODE;
	}
	short parent = codes[code].parent;
	switch (parent)
	{
	case LZW_CLEAR:
		return LZW_CLEAR_CODE;
	case LZW_END:
		return LZW_END_CODE;
	default:
		if ((0 <= parent && parent < LZW_TABLE_SIZE) || parent == LZW_NO_PARENT)
		{
			return LZW_CODE;
		}
		else
		{
			OP_ASSERT(FALSE); // Should never happen!
			return LZW_OUT_OF_BOUNDS_CODE;
		}
		break;
	}
}

void LzwStringTable::OutputString(short code)
{
	OP_PROBE4(OP_PROBE_IMG_LZWSTRINGTABLE_OUTPUTSTRING);
	OP_ASSERT(CodeType(code) == LZW_CODE);

	int pos = LZW_TABLE_SIZE-1;
	int len = 1;
	UINT8 code_data = codes[code].data;
	code = codes[code].parent;
	if (code_data < num_cols)
	{
		code_buf[pos] = code_data;
	}
	else
	{
		code_buf[pos] = (UINT8) zero_value;
	}
	--pos;
	while (code != LZW_NO_PARENT)
	{
		OP_ASSERT(CodeType(code) == LZW_CODE);
		code_data = codes[code].data;
		code = codes[code].parent;
		if (code_data < num_cols)
		{
			code_buf[pos] = code_data;
		}
		else
		{
			code_buf[pos] = (UINT8) zero_value;
		}
		--pos;
		++len;
		OP_ASSERT(len <= LZW_TABLE_SIZE);
	}
	OP_ASSERT(code == LZW_NO_PARENT);
	listener->OnCodesDecoded(code_buf+LZW_TABLE_SIZE-len, len);
}

OP_STATUS LzwStringTable::GetFirstCharacter(short code, UINT8& result)
{
	unsigned int depth = GetNrOfCodes() + 1;

	OP_ASSERT(CodeType(code) == LZW_CODE);
	while (!IsRoot(code))
	{
		if (!--depth)
			return OpStatus::ERR;

		Parent(code);
	}
	result = codes[code].data;
	return OpStatus::OK;
}

void LzwStringTable::SetNextCode(short code)
{
	next_code = code;
	short temp = next_code;
	code_size = 1;
	temp >>= 1;
	while (temp > 0)
	{
		temp >>= 1;
		code_size++;
	}
}

void LzwStringTable::IncNextCode()
{
	next_code++;
	switch (next_code)
	{
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
	case 256:
	case 512:
	case 1024:
	case 2048:
		code_size++;
		break;
	default:
		// Do not change code size.
		break;
	}
}
