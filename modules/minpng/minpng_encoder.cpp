/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MINPNG_ENCODER

#include "modules/zlib/zlib.h"

#include "modules/minpng/minpng.h"
#include "modules/minpng/minpng_encoder.h"

#define MINPNG_ENCODE_BUFFER_SIZE 64*1024

struct minpng_encoder_state
{
	enum {
		INITIAL,
		NEW_IDAT,
		IDAT,
		IEND,
		DONE
	} state;

	z_stream compress;

	UINT8 encode_buffer[MINPNG_ENCODE_BUFFER_SIZE];
	unsigned int encode_buffer_pos;
	unsigned int expected_scanline;
	BOOL resume_scanline;

	UINT8* scanline_data;

	void copy_scanline(const UINT32* data, unsigned int width, BOOL use_alpha);

	void append_to_buffer(UINT32 num);
	void append_to_buffer(const void* data, unsigned int size);

	minpng_encoder_state();
	~minpng_encoder_state();
};

minpng_encoder_state::minpng_encoder_state()
{
	state = INITIAL;
	encode_buffer_pos = 0;
	expected_scanline = 0;
	resume_scanline = FALSE;
	scanline_data = NULL;

	op_memset(&compress, 0, sizeof(compress));
}

minpng_encoder_state::~minpng_encoder_state()
{
	OP_DELETEA(scanline_data);
	deflateEnd(&compress);
}

void minpng_encoder_state::copy_scanline(const UINT32* data, unsigned int width, BOOL use_alpha)
{
	// FIXME: the best compression should be found and used on a per scanline basis
	scanline_data[0] = 0;
	if (use_alpha)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
#if defined(PLATFORM_COLOR_IS_RGBA)
			scanline_data[x*4+1] = data[x]&0xff; // r
			scanline_data[x*4+2] = (data[x]>>8)&0xff; // g
			scanline_data[x*4+3] = (data[x]>>16)&0xff; // b
			scanline_data[x*4+4] = data[x]>>24; // a
#else
			scanline_data[x*4+1] = (data[x]>>16)&0xff; // r
			scanline_data[x*4+2] = (data[x]>>8)&0xff; // g
			scanline_data[x*4+3] = data[x]&0xff; // b
			scanline_data[x*4+4] = data[x]>>24; // a
#endif //(PLATFORM_COLOR_IS_RGBA)
		}
	}
	else
	{
		for (unsigned int x = 0; x < width; ++x)
		{
#if defined(PLATFORM_COLOR_IS_RGBA)
			scanline_data[x*3+1] = data[x]&0xff; // r
			scanline_data[x*3+2] = (data[x]>>8)&0xff; // g
			scanline_data[x*3+3] = (data[x]>>16)&0xff; // b
#else
			scanline_data[x*3+1] = (data[x]>>16)&0xff; // r
			scanline_data[x*3+2] = (data[x]>>8)&0xff; // g
			scanline_data[x*3+3] = data[x]&0xff; // b
#endif //(PLATFORM_COLOR_IS_RGBA)
		}
	}
}


void minpng_encoder_state::append_to_buffer(UINT32 num)
{
#ifndef OPERA_BIG_ENDIAN
	num =  ((num&0xff000000)>>24) | ((num&0xff0000)>>8) | 
		((num&0xff00)<<8) | ((num&0xff)<<24);
#endif // !OPERA_BIG_ENDIAN
	append_to_buffer(&num, 4);
}

void minpng_encoder_state::append_to_buffer(const void* data, unsigned int size)
{
	OP_ASSERT(encode_buffer_pos + size <= MINPNG_ENCODE_BUFFER_SIZE);
	op_memcpy(encode_buffer+encode_buffer_pos, data, size);
	encode_buffer_pos += size;
}

PngEncRes::Result minpng_encode(struct PngEncFeeder* input, struct PngEncRes* res)
{
	minpng_encoder_state* s;
	UINT32 crc;
	int zerr;
	res->data_size = 0;
	res->data = NULL;
	if (!input->state)
	{
		input->state = OP_NEW(minpng_encoder_state, ());
		if (!input->state)
			return PngEncRes::OOM_ERROR;
	}
	s = (minpng_encoder_state*)input->state;

	switch (s->state)
	{
	case minpng_encoder_state::INITIAL: // Initialize the encoder
	{
		if (input->compressionLevel > Z_BEST_COMPRESSION)
			input->compressionLevel = Z_BEST_COMPRESSION;
		if (input->compressionLevel < 0)
			input->compressionLevel = 0;

		zerr = deflateInit(&s->compress, input->compressionLevel);
		if (zerr == Z_MEM_ERROR)
			return PngEncRes::OOM_ERROR;
		else if (zerr != Z_OK)
			return PngEncRes::ILLEGAL_DATA;

		// write headers to the decode buffer
		// HEADER
		s->append_to_buffer("\211\120\116\107\r\n\32\n", 8);

		// IHDR
		s->append_to_buffer(13);
		s->append_to_buffer("IHDR", 4);
		s->append_to_buffer(input->xsize);
		s->append_to_buffer(input->ysize);
		if (input->has_alpha)
		{
			s->append_to_buffer("\10\6\0\0\0", 5);
		}
		else
		{
			s->append_to_buffer("\10\2\0\0\0", 5);
		}
		crc = crc32(0, s->encode_buffer+12, 17);
		s->append_to_buffer(crc);

		s->state = minpng_encoder_state::NEW_IDAT;
		s->resume_scanline = FALSE;
		res->data = s->encode_buffer;
		res->data_size = s->encode_buffer_pos;
		s->encode_buffer_pos = 0;
		OP_DELETEA(s->scanline_data);
		const UINT32 size = input->xsize*(input->has_alpha?4:3)+1;
		s->scanline_data = OP_NEWA(UINT8, size); // add one to store filter type too
		if (!s->scanline_data)
			return PngEncRes::OOM_ERROR;
		return PngEncRes::AGAIN;
	}
	case minpng_encoder_state::NEW_IDAT: // Start a new idat chunk
		s->compress.next_out = s->encode_buffer+8;
		s->compress.avail_out = MINPNG_ENCODE_BUFFER_SIZE-12;
		s->state = minpng_encoder_state::IDAT;
		// fall-through
	case minpng_encoder_state::IDAT: // Continue with an already started chunk
		// Make sure we got the correct scanline
		if (s->expected_scanline != input->scanline)
			return PngEncRes::ILLEGAL_DATA;

		if (!s->resume_scanline)
		{
			s->copy_scanline(input->scanline_data, input->xsize, !!input->has_alpha);

			s->compress.next_in = s->scanline_data;
			s->compress.avail_in = input->xsize*(input->has_alpha?4:3)+1;
		}
		// compress some data
		zerr = deflate(&s->compress, input->scanline==input->ysize-1 ? Z_FINISH : Z_NO_FLUSH);
		if (zerr == Z_MEM_ERROR)
			return PngEncRes::OOM_ERROR;
		else if (zerr != Z_OK && zerr != Z_STREAM_END)
			return PngEncRes::ILLEGAL_DATA;
		if (s->compress.avail_out == 0 || zerr == Z_STREAM_END)
		{
			unsigned int chunk_size = MINPNG_ENCODE_BUFFER_SIZE-s->compress.avail_out;
			// flush the chunk and start a new
			s->state = minpng_encoder_state::NEW_IDAT;
			res->data = s->encode_buffer;
			res->data_size = chunk_size;
			s->encode_buffer_pos = 0;
			s->append_to_buffer(chunk_size-12); // don't include crc, size or type
			s->append_to_buffer("IDAT", 4);
			crc = crc32(0, s->encode_buffer+4, chunk_size-8);
			s->encode_buffer_pos = chunk_size-4;
			s->append_to_buffer(crc);
			if (input->scanline==input->ysize-1 && !s->compress.avail_in) // all data of the last scanline has been processed
			{
				if (zerr == Z_STREAM_END)
					s->state = minpng_encoder_state::IEND;
				else
					s->resume_scanline = TRUE;
				return PngEncRes::AGAIN;
			}
		}
		if (s->compress.avail_in)
		{
			// there was not enough space to encode the entire scanline, so try again later
			s->resume_scanline = TRUE;
			return PngEncRes::AGAIN;
		}
		s->resume_scanline = FALSE;
		++s->expected_scanline;
		return PngEncRes::NEED_MORE;
	case minpng_encoder_state::IEND:
		s->encode_buffer_pos = 0;
		s->append_to_buffer(0);
		s->append_to_buffer("IEND", 4);
		crc = crc32(0, s->encode_buffer+4, 4);
		s->append_to_buffer(crc);
		res->data = s->encode_buffer;
		res->data_size = s->encode_buffer_pos;
		s->state = minpng_encoder_state::DONE;
		// fall-through
	case minpng_encoder_state::DONE:
	default:
		return PngEncRes::OK;
	}
}

void minpng_init_encoder_feeder(struct PngEncFeeder* res)
{
	op_memset(res, 0, sizeof(PngEncFeeder));
	res->compressionLevel = 8;
}

void minpng_clear_encoder_feeder(struct PngEncFeeder* res)
{
	OP_DELETE((minpng_encoder_state *)res->state);
	res->state = NULL;
}

void minpng_init_encoder_result( struct PngEncRes *res )
{
	op_memset(res, 0, sizeof(PngEncRes));
}

void minpng_clear_encoder_result(struct PngEncRes* res)
{
	minpng_init_encoder_result( res );
}

#endif // MINPNG_ENCODER

