/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UNICODE_UTF8_H
#define MODULES_UNICODE_UTF8_H

/** Convert Unicode encoded as UTF-8 to UTF-16 */
class UTF8Decoder
{
public:
	UTF8Decoder();

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * len bytes and write no more than maxlen bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to different buffers.
	 *
	 * Convert() maintains state between calls, and uses the same state
	 * variable as Measure(). If you are switching between the two on
	 * the same UTF8Decoder object, make sure to call Reset() before
	 * switching.
	 *
	 * @param src The buffer to convert from.
	 * @param len The amount of data in the src buffer (in bytes).
	 * @param dest The buffer to convert to.
	 * @param maxlen The size of the dest buffer (in bytes).
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes written to dest; -1 on error
	 *         (failed to allocate memory).
	 */
	int Convert(const void *src, int len, void *dest, int maxlen, int *read);

	/**
	 * Read bytes from src and count how long the resulting UTF-16
	 * string would be, but read no more than len bytes, and measure no
	 * more than maxlen bytes of output. Return the number of bytes that
	 * would be written if Convert() was called, and set read to the
	 * number of bytes read.
	 *
	 * Measure() maintains state between calls, and uses the same state
	 * variable as Convert(). If you are switching between the two on
	 * the same UTF8Decoder object, make sure to call Reset() before
	 * switching.
	 *
	 * @param src The buffer to measure from.
	 * @param len The amount of data in the src buffer (in bytes).
	 * @param maxlen The size of the dest buffer (in bytes).
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes that would be written to dest;
	 *         -1 on error (failed to allocate memory).
	 */
	int Measure(const void *src, int len, int maxlen = INT_MAX, int *read = NULL);

	/**
	 * Reset the decoder to initial state without writing any data
	 * to the output stream. Also remove any information about invalid
	 * characters and any unconverted data in the buffer.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over.
	 */
	void Reset();

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	/**
	 * Check if the decoder is in a valid ending state. If the decoder has
	 * consumed all input it has been presented and the internal state is
	 * returned to the initial one, this function will return TRUE.
	 *
	 * @return TRUE if the decoder is in a valid end state.
	 */
	BOOL IsValidEndState() { return -1 == m_charSplit; }
#endif

protected:
	/** For use by the InputConverter variant. */
	virtual int HandleInvalid(int) { return 0; }
	/** For use by the InputConverter variant. */
	virtual void AddConverted(int) {}

private:
	int m_charSplit;    ///< Bytes left in UTF8 character, -1 if in ASCII
	int m_sequence;     ///< Type of current UTF8 sequence (total length - 2)
	UINT32 m_ucs;       ///< Current ucs character being decoded
	UINT16 m_surrogate; ///< Surrogate left to insert in next iteration

	UTF8Decoder(const UTF8Decoder&);
	UTF8Decoder& operator =(const UTF8Decoder&);
};

class UTF8Encoder
{
public:
	UTF8Encoder() : m_surrogate(0) {}

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * len bytes and write no more than maxlen bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to different buffers.
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
	 * @return The number of bytes written to dest; -1 on error
	 *         (failed to allocate memory).
	 */
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	/**
	 * Calculate size needed (in bytes) to accomodate this string.
	 *
	 * Measure() maintains state between calls, and uses the same state
	 * variable as Convert(). If you are switching between the two on
	 * the same UTF8Encoder object, make sure to call Reset() before
	 * switching.
	 *
	 * @param src Source string (UTF-16 characters).
	 * @param len Length of source string (in bytes).
	 * @param maxlen The maximum number of output bytes to consider.
	 * @param read Output parameter returning the number of bytes
	 *             read from src.
	 * @return The number of bytes that would be written to dest.
	 */
	int Measure(const void *src, int len, int maxlen = INT_MAX, int *read = NULL);

	/**
	 * Check space required (worst case) to add one UTF-16 code unit to the
	 * output stream, and then call ReturnToInitialState(). This value may
	 * vary according to the current state of the encoder.
	 *
	 * The number returned is (obviously) always larger than the number
	 * returned by ReturnToInitialState(NULL).
	 */
	int LongestSelfContainedSequenceForCharacter();

	/**
	 * Reset the decoder to initial state without writing any data
	 * to the output stream. Also remove any information about invalid
	 * characters and any unconverted data in the buffer.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over.
	 */
	void Reset();

protected:
	/** For use by the OutputConverter variant. */
	virtual void AddConverted(int) {}

private:
	UINT16 m_surrogate; ///< Stored surrogate value
};

#endif // MODULES_UNICODE_UTF8_H
