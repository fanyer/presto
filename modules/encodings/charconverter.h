/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file charconverter.h
 *
 * Character encoding converter interface.
 */

#ifndef CHARCONVERTER_H
#define CHARCONVERTER_H

#ifndef ENCODINGS_REGTEST
#include "modules/hardcore/base/opstatus.h"
#endif

// ==== CONVERTER INTERFACE   ==============================================

/**
 * A CharConverter converts text from one encoding to another. This is the
 * abstract base class of all the converters used to convert to and from
 * Unicode.
 */
class CharConverter
{
public:

	/** First-phase constructor for converter */
	CharConverter() : m_num_converted(0), m_num_invalid(0), m_first_invalid_offset(-1) {};

	/**
	 * Second-phase constructor for converter.
	 *
	 * This second-phase constructor is used by subclasses to allocate memory,
	 * and do other things that can possibly fail, and thus cannot be done in
	 * the first-phase constructor.
	 *
	 * @return OpStatus::OK on success, otherwise some kind of OpStatus
	 *         error (currently only OpStatus::ERR (even on OOM) although
	 *         this should change). Unless OpStatus::OK is returned, calling
	 *         any method on this object (other than its destructor) results
	 *         in undefined behaviour.
	 */
	virtual OP_STATUS Construct() { return OpStatus::OK; };

	/** Destructor for converter */
	virtual ~CharConverter() {};

	/**
	 * Read bytes from src and write them to dest, but read no more than
	 * len bytes and write no more than maxlen bytes. Return the number
	 * of bytes written into dest, and set read to the number of bytes
	 * read. The src and dest pointers must point to different buffers.
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
	                    int *read) = 0;

	/** Return name of character set being decoded, or NULL if unknown */
	virtual const char *GetCharacterSet() = 0;
	/** Return name of character set being encoded, or NULL if unknown */
	virtual const char *GetDestinationCharacterSet() = 0;
	/** Return number of unconvertable codepoints in input */
	int GetNumberOfInvalid() { return m_num_invalid; }
	/** Return the character offset (in UTF-16 codepoints) of the first
	 *  unconvertable codepoint in input, or -1 if no unconvertable
	 *  codepoints have been encountered
	 */
	int GetFirstInvalidOffset() { return m_first_invalid_offset; }

	/**
	 * Reset the encoder/decoder to initial state without writing any data
	 * to the output stream. Also remove any information about invalid
	 * characters and any unconverted data in the buffer.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over.
	 */
	virtual void Reset() { m_num_converted = 0; m_num_invalid = 0; m_first_invalid_offset = -1; }

protected:
	int m_num_converted; ///< Number of UTF-16 characters converted so far, not including current call to Convert().
	int m_num_invalid; ///< Number of unconvertable codepoints in input
	int m_first_invalid_offset; ///< Character offset of first unconvertable codepoint in input

private:
	CharConverter(const CharConverter&);
	CharConverter& operator =(const CharConverter&);

};

#endif // CHARCONVERTER_H
