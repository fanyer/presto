/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ZLIB_STREAM_H
#define ZLIB_STREAM_H

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"

typedef struct z_stream_s z_stream;

/** @brief Stream class used to compress or decompress data with 'deflate' (zlib)
  * Depending on use, use CompressZlibStream or DecompressZlibStream
  *
  * e.g.
  * CompressZlibStream stream;
  * stream.AddData(uncompressed, uncompressed_length)
  * stream.Flush(compressed);
  */
class ZlibStream
{
public:
	ZlibStream();
	virtual ~ZlibStream();

	/** Add input data to this stream
	  * @param source Data to add
	  * @param length Length of data to add
	  */
	OP_STATUS AddData(const char* source, size_t length) { return AddData(source, length, FALSE); }

	/** Flush the output of this stream to a buffer
	  * @param output Where to flush output to
	  */
	OP_STATUS Flush(StreamBuffer<char>& output);

protected:
	OP_STATUS AddData(const char* source, size_t length, BOOL flush);
	OP_STATUS Init();
	virtual OP_STATUS AddDataInternal(const char* source, size_t length, BOOL flush) = 0;
	virtual OP_STATUS InitializeStream() = 0;

	static const size_t BlockSize = 1024;

	char* m_block;
	StreamBuffer<char> m_buffer;
	z_stream* m_stream;
};

class CompressZlibStream : public ZlibStream
{
public:
	virtual ~CompressZlibStream();

protected:
	virtual OP_STATUS AddDataInternal(const char* source, size_t length, BOOL flush);
	virtual OP_STATUS InitializeStream();
};

class DecompressZlibStream : public ZlibStream
{
public:
	DecompressZlibStream(BOOL has_gzip_header = FALSE)
		: ZlibStream(), m_has_gzip_header(has_gzip_header) {}

	virtual ~DecompressZlibStream();

protected:
	virtual OP_STATUS AddDataInternal(const char* source, size_t length, BOOL flush);
	virtual OP_STATUS InitializeStream();
private:
	BOOL m_has_gzip_header;
};

#endif // ZLIB_STREAM_H
