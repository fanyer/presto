/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef COMPRESS_H
#define COMPRESS_H

template<class T> class StreamBuffer;

namespace Compressor
{
	/** Compress data
	 * @param src Data to be compressed
	 * @param length Length of src
	 * @param dest Compressed data (output)
	 */
	OP_STATUS Compress(const char* src, size_t length, StreamBuffer<char>& dest);

	/** Decompress data previously compressed with Compress()
	 * @param src Compressed data
	 * @param length Length of src
	 * @param dest Decompressed data (output)
	 */
	OP_STATUS Decompress(const char* src, size_t length, StreamBuffer<char>& dest);
};

#endif // COMPRESS_H
