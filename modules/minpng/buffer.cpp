/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef _PNG_SUPPORT_

#include "modules/zlib/zlib.h"
#include "modules/minpng/minpng.h"
#include "modules/minpng/png_int.h"

/****************************** gzipbuffer code ************************************/

int minpng_zbuffer::read(unsigned char *data, int bytes)
{
	int ret;
	if (eof)
	{
		input.clear();
		return Eof;
	}

	d_stream.next_in = input.ptr();
	d_stream.avail_in = input.size();
	d_stream.next_out = data;
	d_stream.avail_out = bytes;

	if ((ret = inflate(&d_stream, Z_SYNC_FLUSH)) < 0 && ret != Z_BUF_ERROR) // Need to take better care of oom and other errors.
	{
		if (ret == Z_MEM_ERROR)
			return Oom;
		else
			return IllegalData;
	}

	if (ret == Z_STREAM_END)
		eof = 1;
	input.consume(input.size() - d_stream.avail_in);
	return bytes - d_stream.avail_out;
}

minpng_zbuffer::~minpng_zbuffer()
{
	inflateEnd( &d_stream );
}

PngRes::Result minpng_zbuffer::init()
{
	int ret = inflateInit(&d_stream); // Need to take care of more values here.
	if (ret == Z_MEM_ERROR)
		return PngRes::OOM_ERROR;
	return PngRes::OK;
}

PngRes::Result minpng_zbuffer::re_init()
{
	inflateEnd( &d_stream );
	input.clear();
	eof = 0;
	op_memset(&d_stream, 0, sizeof(z_stream));
	int ret = inflateInit(&d_stream); // Need to take care of more values here.
	if (ret == Z_MEM_ERROR)
		return PngRes::OOM_ERROR;
	return PngRes::OK;
}

minpng_zbuffer::minpng_zbuffer()
{
	eof = 0;
	op_memset(&d_stream, 0, sizeof(z_stream));
}

/****************************** Buffer code ************************************/

int minpng_buffer::append(int sz, const unsigned char* add)
{
	if (!sz)
		return 0;
	OP_ASSERT(add);

	if (available >= sz) // The data will fit.
	{
		op_memcpy(data + len + off, add, sz);
		available -= sz;
		len += sz;

		OP_ASSERT(allocated >= len + off);
		OP_ASSERT(available >= 0);
		OP_ASSERT(available == allocated - (len + off));
		return 0;
    }

	if (off + available > sz) // Data will fit if we move the old data first
	{
		if (len)
			op_memmove(data, data + off, len );
		op_memcpy(data + len, add, sz);
		off = 0;
		len += sz;
		available = allocated - len;
		return 0;
	}

	// Need to allocate more memory. Double size by default.
	int alloc = MAX(sz + len + 15, allocated * 2);
	unsigned char *ndata = OP_NEWA(unsigned char, alloc);

	if (!ndata)
		return 1;

	if (len)
		op_memcpy(ndata, data + off, len);

	OP_DELETEA(data);
	data = ndata;

	off = 0;
	allocated = alloc;
	available = alloc - len - sz;
	op_memcpy(data + len, add, sz);
	len += sz;
	return 0;
}

unsigned char* minpng_buffer::ptr()
{
	if (!len)
		return NULL;
	OP_ASSERT(off >= 0);
	OP_ASSERT(len > 0);
	OP_ASSERT(allocated >= len + off);
	OP_ASSERT(available == allocated - (len + off));
	return data + off;
}

void minpng_buffer::consume(int bytes)
{
	OP_ASSERT(bytes <= len);
	len -= bytes;
	off += bytes;
}

void minpng_buffer::clear()
{
	OP_DELETEA(data);
	release();
}

void minpng_buffer::release()
{
	data = NULL;
	off = len = allocated = available = 0;
}

#endif // _PNG_SUPPORT_
