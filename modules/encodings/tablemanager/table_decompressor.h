/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Per Hedbor
*/

#if !defined TABLE_DECOMPRESSOR_H && defined TABLEMANAGER_COMPRESSED_TABLES
#define TABLE_DECOMPRESSOR_H

#include "modules/zlib/zlib.h"

/** 
 * Decompress deflate and delta-encoded tables.
 * Only used from the internal tablemanager functions, never used by users.
 *
 * The tables are stored as deflate-compressed delta-encoded 24-bit
 * signed network-byte-order integers. The decompression will generate
 * an equal number of 16bit unsigned host-byte-order integers.
 */
class TableDecompressor
{
public:
	TableDecompressor();
	~TableDecompressor();

	/**
	 * Initialize decompressor object. Object MUST be initialized before
	 * the decompress session, and the object should be used only once.
	 *
	 * @return Status of the operation
	 */
	OP_STATUS Init();

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * src_len bytes and write no more than dest_len bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to different buffers.
	 *
	 * @param src The buffer to convert from.
	 * @param src_len The amount of data in the src buffer (in bytes).
	 * @param dest The buffer to convert to.
	 * @param dest_len The size of the dest buffer (in bytes).
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes written to dest; -1 on error 
	 *         (failed to allocate memory).
	 */
	int Decompress(const UINT8 *src, int src_len, UINT8 *dest, int dest_len, int *read);

private:
	inline int Undelta(UINT8 *dest, const UINT8 *src, int len, int cur);

public:
	z_stream m_stream;
	UINT8 *m_work_area;
	int m_work_area_len;

	int m_delta; //Preious Undelta value
	int m_left; //Number of bytes in m_work_area from last (incomplete) Decompress call
};
#endif
