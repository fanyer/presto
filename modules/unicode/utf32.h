/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined MODULES_UTIL_UNICODE_UTF32_H && defined(USE_UNICODE_UTF32)
#define MODULES_UTIL_UNICODE_UTF32_H

/**
 * Convert UTF-32 into UTF-16 (native byte order). Any initial byte-order
 * mark (BOM) in the input buffer will be converted into a UTF-16
 * byte-order mark.
 */
class UTF32Decoder
{
public:
	UTF32Decoder()
		: m_surrogate(0)
		{};

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * len bytes and write no more than maxlen bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to non-overlapping buffers.
	 *
	 * Convert() maintains state between calls, and uses the same state
	 * variable as Measure(). If you are switching between the two on
	 * the same UTF8Encoder object, make sure to call Reset() before
	 * switching.
	 *
	 * @param src The buffer to convert from.
	 * @param len The amount of data in the src buffer (in bytes).
	 * @param dest The buffer to convert to.
	 * @param maxlen The size of the dest buffer (in bytes).
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes written to dest; -1 on error.
	 */
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	/**
	 * Reset the encoder to initial state without writing any data
	 * to the output stream. Also remove any information about invalid
	 * characters and any unconverted data in the buffer.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over.
	 */
	virtual void Reset();

private:
	UINT16 m_surrogate;	///< Surrogate left to insert in next iteration
};



/**
 * Convert UTF-16 into UTF-32 (native byte order). Does not insert a
 * byte-order mark (BOM) to the output buffer unless explicitly
 * requested.
 */
class UTF32Encoder
{
public:
	/**
	 * Constructor.
	 *
	 * @param add_bom Add a byte-order mark (BOM) to the beginning of
	 *  the output stream.
	 */
	UTF32Encoder(BOOL add_bom)
		: m_add_bom(add_bom)
		, m_surrogate(0)
		{};

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * len bytes and write no more than maxlen bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to non-overlapping buffers.
	 *
	 * Convert() maintains state between calls, and uses the same state
	 * variable as Measure(). If you are switching between the two on
	 * the same UTF8Encoder object, make sure to call Reset() before
	 * switching.
	 *
	 * @param src The buffer to convert from.
	 * @param len The amount of data in the src buffer (in bytes).
	 * @param dest The buffer to convert to.
	 * @param maxlen The size of the dest buffer (in bytes).
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes written to dest; -1 on error.
	 */
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	/**
	 * Reset the decoder to initial state without writing any data
	 * to the output stream. Also remove any information about invalid
	 * characters and any unconverted data in the buffer.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over.
	 *
	 * @param add_bom TRUE if a new byter-order mark (BOM) should be inserted.
	 */
	virtual void Reset(BOOL add_bom);

private:
	BOOL m_add_bom;
	UINT16 m_surrogate; ///< Stored surrogate value
};


#endif // MODULES_UTIL_UNICODE_UTF32_H
