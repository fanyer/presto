/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef BINCOMPRESSOR_H
#define BINCOMPRESSOR_H

#include "modules/search_engine/TypeDescriptor.h" // For the CHECK_RESULT macro

/**
 * @brief Very fast algorithm to compress and decompress arbitrary data. UniCompressor is more efective for Unicode text.
 * @author Pavel Studeny <pavels@opera.com>
 */
class BinCompressor : public NonCopyable
{
public:
	BinCompressor(void) {m_dict = NULL;}
	~BinCompressor(void) {FreeCompDict();}

	/**
	 * init internal ditionary, only needed for compression
	 */
	CHECK_RESULT(OP_STATUS InitCompDict(void));

	/**
	 * free internal dictionary
	 */
	void FreeCompDict(void) {if (m_dict != NULL) OP_DELETEA(m_dict); m_dict = NULL;}

	/**
	 * compress a block of unicode text
	 * @param dst 69 * len / 68 + 8 B must be available
	 * @param src data to compress
	 * @param len length of src in bytes
	 * @return length of output data stored in dst
	 */
	unsigned Compress(unsigned char *dst, const void *src, unsigned len);

	/**
	 * length of original data should be kept to allocate a sufficient buffer for decompression
	 * @param dst buffer for decompressed data
	 * @param src compressed data
	 * @param len length of src in bytes
	 * @return length of uncompressed data
	 */
	unsigned Decompress(void *dst, const unsigned char *src, unsigned len);

	/**
	 * @return the original length of the compressed data
	 */
	unsigned Length(const unsigned char *src);

protected:
	inline unsigned char *OutputLiteral(unsigned char *op, const unsigned char *sp, const unsigned char *cp);
	inline unsigned char *OutputMatch(unsigned char *op, unsigned length, unsigned short offset);
	UINT32 *m_dict;
};

#endif  // BINCOMPRESSOR_H

