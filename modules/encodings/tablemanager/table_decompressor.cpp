/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Per Hedbor
*/
#include "core/pch.h"

#ifdef TABLEMANAGER_COMPRESSED_TABLES
#include "modules/zlib/zlib.h"
#include "modules/encodings/tablemanager/table_decompressor.h"


TableDecompressor::TableDecompressor()
: m_work_area(NULL),
  m_work_area_len(0),
  m_delta(0),
  m_left(0)
{
}

TableDecompressor::~TableDecompressor()
{
	inflateEnd(&m_stream);
	OP_DELETEA(m_work_area);
}

OP_STATUS TableDecompressor::Init()
{
	op_memset(&m_stream, 0, sizeof(m_stream));

	if (inflateInit(&m_stream) != Z_OK)
		return OpStatus::ERR_NO_MEMORY;

	m_stream.next_in = 0;
	m_stream.avail_in = 0;
	m_stream.next_out = 0;
	m_stream.avail_out = 0;

	m_delta = 0;
	m_left = 0;

	return OpStatus::OK;
}


// Avoid power-of-two to make malloc overhead less serious for mallocs
// that try to page-align large allocations.
#define INPUT_BUFFER 242
#define OUTPUT_BUFFER 756

/**
 * Read bytes from src and write them to dest, but read no more than
 * src_len bytes and write no more than dest_len bytes. Return the number
 * of bytes written into dest, and set read to the number of bytes
 * read. The src and dest pointers must point to different buffers.
 */
int TableDecompressor::Decompress(const UINT8 *src, int src_len, UINT8 *dest, int dest_len, int *read)
{
	int err = Z_OK;
	int written = 0;

#ifdef ENC_MEM_CONSERVING_DECOMPRESSION
	// All in one go. This uses less memory when the table size is 680
	// bytes or smaller. This is true for most eight-bit encodings, whose
	// tables tend to be exactly 512 bytes large.
	//
	// Arithmetics somewhat unlogically done on the right side instead
	// of the left side operand to get a compile-time constant.
	if (0 && dest_len <= ((INPUT_BUFFER + OUTPUT_BUFFER) / 3) << 1)
	{
		if (!m_work_area)
		{
			m_work_area_len = (dest_len * 3) >> 1;
			m_work_area = OP_NEWA(UINT8, m_work_area_len);
			if (!m_work_area)
				return -1;
		}

		m_stream.next_in = reinterpret_cast<Bytef *>(const_cast<UINT8 *>(src));
		m_stream.avail_in = src_len;
		m_stream.next_out = reinterpret_cast<Bytef *>(m_work_area);
		m_stream.avail_out = m_work_area_len;

		err = inflate(&m_stream, Z_FINISH);

		// If m_stream.avail_out is not 0, we did not get enough data
		// after decompression. This should only happen if we have
		// illegal content in the encoding.bin file.
		// 
		// Same goes for err not beeing Z_STREAM_END
		if (err != Z_STREAM_END || m_stream.avail_out > 0)
			return -1;

		// Ignore return value (next delta start).
		// Here we never uncompress more than will fit in destination.
		// All in one go.
		Undelta(dest, m_work_area, dest_len >> 1, 0);

		written = dest_len;

		if (read)
			*read = src_len;
	}
	else
#endif // ENC_MEM_CONSERVING_DECOMPRESSION
	{
		// Streaming decompression.
		// Uses less memory for large tables, but always more CPU.
		if (!m_work_area)
		{
			m_work_area_len = OUTPUT_BUFFER + 2;
			m_work_area = OP_NEWA(UINT8, m_work_area_len);
			if (!m_work_area)
				return -1;
		}

		m_stream.next_in = reinterpret_cast<Bytef *>(const_cast<UINT8 *>(src));
		m_stream.avail_in = src_len;
		m_stream.next_out = reinterpret_cast<Bytef *>(m_work_area + m_left);
		m_stream.avail_out = m_work_area_len - 2;

		// Decompress the data.
		err = inflate(&m_stream, 0);
		if (err < 0)		// Z_OK, Z_STREAM_END are larger than 0
			return -1;

		if (read)
			*read = src_len - m_stream.avail_in;

		// Undo the delta compression of the decompressed data and
		// copy it to the destination buffer.
		int sz = (OUTPUT_BUFFER - m_stream.avail_out);
		int nelems = sz / 3;

		m_left = sz % 3;

		if ((nelems << 1) > dest_len)
			return -1;

		m_delta = Undelta(dest, m_work_area, nelems, m_delta);
		written += nelems << 1;

		// Experiments show that it's faster to always do the copying.
		m_work_area[0] = m_work_area[nelems * 3];
		m_work_area[1] = m_work_area[nelems * 3 + 1];
	}

	return written;
}

/**
 * Undo the delta compression. Each code-point is represented by the
 * difference from the previous codepoint, encoded as a 24-bit signed
 * integer.
 *
 * If we were 100% certain that 'dest' is aligned on an even address
 * (we probably are) we could do the writing faster using an array of
 * shorts.
 *
 * The 24-bit values are always in NBO.
 */
inline int TableDecompressor::Undelta(UINT8 *dest, const UINT8 *src, int len, int cur)
{
	int next;
	int i;
	for (i = 0; i < len; i++)
	{
		next = *src++;

		next = (next << 8) | *src++;
		next = (next << 8) | *src++;
		if (next > 0x7fffff)	// negative.
			next -= 0x1000000;
		cur += next;
#ifdef NEEDS_RISC_ALIGNMENT
# ifdef OPERA_BIG_ENDIAN
		*dest++ = cur >> 8;
		*dest++ = cur & 255;
# else
		*dest++ = cur & 255;
		*dest++ = cur >> 8;
# endif	 // OPERA_BIG_ENDIAN
#else
		*(UINT16 *) dest = cur;
		dest += 2;
#endif // NEEDS_RISC_ALIGNMENT
	}
	return cur;
}

#endif // TABLEMANAGER_COMPRESSED_TABLES
