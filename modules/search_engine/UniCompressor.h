/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef UNICOMPRESSOR_H
#define UNICOMPRESSOR_H

#include "modules/search_engine/TypeDescriptor.h" // For the CHECK_RESULT macro

/**
 * @brief Very fast algorithm to compress and decompress unicode text.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * The compression/decompression speed is similar to the world's fastest compression, LZO.
 */
class UniCompressor : public NonCopyable
{
public:
	UniCompressor(void) {m_dict = NULL;}
	~UniCompressor(void) {FreeCompDict();}

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
	 * @param dst 3 * uni_strlen(src) * sizeof(uni_char) / 2 + 6 B must be available
	 * @param src unicode text ended by 0
	 * @return length of output data stored in dst
	 */
	unsigned Compress(unsigned char *dst, const uni_char *src);

	/**
	 * length of original data should be kept to allocate a sufficient buffer for decompression
	 * @param dst buffer for original text, text is terminated by 0
	 * @param src compressed data
	 * @param len length of src
	 * @return length of uncompressed text without the terminating 0
	 */
	unsigned Decompress(uni_char *dst, const unsigned char *src, unsigned len);

	/**
	 * @return the original length of the compressed data excluding the terminating 0
	 */
	unsigned Length(const unsigned char *src);

protected:
	inline unsigned char *OutputLiteral(unsigned char *op, const uni_char *sp, const uni_char *cp);
	inline unsigned char *OutputMatch(unsigned char *op, unsigned length, unsigned short offset);
	UINT32 *m_dict;
};

#endif  // UNICOMPRESSOR_H

