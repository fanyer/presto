/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/datastructures/ZlibStream.h"
#include "modules/zlib/zlib.h"

ZlibStream::ZlibStream()
	: m_block(0)
	, m_stream(0)
{
}

ZlibStream::~ZlibStream()
{
	OP_DELETEA(m_block);
	OP_DELETE(m_stream);
}

OP_STATUS ZlibStream::AddData(const char* source, size_t length, BOOL flush)
{
	if (!m_stream)
		RETURN_IF_ERROR(Init());

	return AddDataInternal(source, length, flush);
}

OP_STATUS ZlibStream::Init()
{
	if (!m_block)
		m_block = OP_NEWA(char, BlockSize);

	m_stream = OP_NEW(z_stream, ());

	if (!m_block || !m_stream)
		return OpStatus::ERR_NO_MEMORY;

	m_stream->zalloc = Z_NULL;
	m_stream->zfree  = Z_NULL;
	m_stream->opaque = Z_NULL;

	return InitializeStream();
}

OP_STATUS ZlibStream::Flush(StreamBuffer<char>& output)
{
	RETURN_IF_ERROR(AddData("", 0, TRUE));

	output.TakeOver(m_buffer);

	return OpStatus::OK;
}

CompressZlibStream::~CompressZlibStream()
{
	deflateEnd(m_stream);
}

OP_STATUS CompressZlibStream::AddDataInternal(const char* source, size_t length, BOOL flush)
{
	// compress blocks of BlockSize bytes and add them to m_buffer until done
	m_stream->avail_in  = length;
	m_stream->next_in   = reinterpret_cast<Bytef*>(const_cast<char*>(source));
	int flush_param = flush ? Z_SYNC_FLUSH : Z_NO_FLUSH;

	do
	{
		m_stream->avail_out = BlockSize;
		m_stream->next_out  = reinterpret_cast<Bytef*>(m_block);

		if (deflate(m_stream, flush_param) == Z_STREAM_ERROR)
			return OpStatus::ERR;

		RETURN_IF_ERROR(m_buffer.Append(m_block, BlockSize - m_stream->avail_out));

	} while (m_stream->avail_out == 0);

	return OpStatus::OK;
}

OP_STATUS CompressZlibStream::InitializeStream()
{
	if (deflateInit2(m_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
		return OpStatus::ERR;

	return OpStatus::OK;
}

DecompressZlibStream::~DecompressZlibStream()
{
	inflateEnd(m_stream);
}

OP_STATUS DecompressZlibStream::AddDataInternal(const char* source, size_t length, BOOL flush)
{
	m_stream->avail_in = length;
	m_stream->next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(source));
	int flush_param = flush ? Z_SYNC_FLUSH : Z_NO_FLUSH;
	int zret;

	do
	{
		m_stream->avail_out = BlockSize;
		m_stream->next_out  = reinterpret_cast<Bytef*>(m_block);

		zret = inflate(m_stream, flush_param);
		switch (zret)
		{
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				return OpStatus::ERR;
		}

		RETURN_IF_ERROR(m_buffer.Append(m_block, BlockSize - m_stream->avail_out));

	} while ((zret == Z_OK && m_stream->avail_out == 0) || !(zret == Z_STREAM_END || zret < 0));

	return OpStatus::OK;
}

OP_STATUS DecompressZlibStream::InitializeStream()
{
	int rc = inflateInit2(m_stream, m_has_gzip_header ? 32+15 : -15);
	return rc == Z_OK ? OpStatus::OK : OpStatus::ERR;
}
