/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/compress.h"

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "modules/zlib/zlib.h"

/***********************************************************************************
 ** Compress data
 **
 ** Compressor::Compress
 ** @param src Data to be compressed
 ** @param dest Compressed data (output)
 ***********************************************************************************/
OP_STATUS Compressor::Compress(const char* src, size_t length, StreamBuffer<char>& dest)
{
	OP_STATUS ret = OpStatus::OK;
	z_stream  strm;
	size_t	  bufsize = (size_t)(length * 1.001 + 12);
	OpString8 tempbuf;

	if (!tempbuf.Reserve(bufsize))
		return OpStatus::ERR_NO_MEMORY;

	// allocate deflate state
	strm.zalloc = Z_NULL;
	strm.zfree  = Z_NULL;
	strm.opaque = Z_NULL;
	if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
		return OpStatus::ERR;

	// compress until we're done
	strm.avail_in  = length;
	strm.next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(src));

	do
	{
		strm.avail_out = bufsize;
		strm.next_out  = reinterpret_cast<Bytef*>(tempbuf.CStr());

		deflate(&strm, Z_FINISH);

		ret = dest.Append(tempbuf.CStr(), bufsize - strm.avail_out);
		if (OpStatus::IsError(ret))
			break;

	} while (strm.avail_out == 0);

	// Cleanup
	deflateEnd(&strm);

	return ret;
}


/***********************************************************************************
 ** Decompress data previously compressed with Compress()
 **
 ** Compressor::Decompress
 ** @param src Compressed data
 ** @param dest Decompressed data (output)
 ***********************************************************************************/
OP_STATUS Compressor::Decompress(const char* src, size_t length, StreamBuffer<char>& dest)
{
	static const size_t bufsize = 131072;

	OP_STATUS ret = OpStatus::OK;
	z_stream  strm;
	OpString8 tempbuf;
	int		  zret;

	if (!tempbuf.Reserve(bufsize))
		return OpStatus::ERR_NO_MEMORY;

	// allocate inflate state
	strm.zalloc   = Z_NULL;
	strm.zfree    = Z_NULL;
	strm.opaque   = Z_NULL;
	strm.avail_in = length;
	strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(src));
	if (inflateInit(&strm) != Z_OK)
		return OpStatus::ERR;

	// decompress until we're done
	do
	{
		strm.avail_out = bufsize;
		strm.next_out  = reinterpret_cast<Bytef*>(tempbuf.CStr());

		zret = inflate(&strm, Z_NO_FLUSH);
		switch (zret)
		{
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
			case Z_BUF_ERROR:
				ret = OpStatus::ERR;
		}

		if (OpStatus::IsError(ret) ||
				  OpStatus::IsError(ret = dest.Append(tempbuf.CStr(), bufsize - strm.avail_out)))
			break;

	} while ((zret == Z_OK && strm.avail_out == 0) || zret != Z_STREAM_END);

	// Cleanup
	inflateEnd(&strm);

	return ret;
}

#endif // M2_SUPPORT
