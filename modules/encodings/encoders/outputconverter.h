/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OUTPUTCONVERTER_H
#define OUTPUTCONVERTER_H

#include "modules/encodings/charconverter.h"

/** Output converter interface */
class OutputConverter : public CharConverter
{
public:
	/**
	 * Instantiates and returns a CharConverter object that can convert
	 * from the internal string encoding to the given encoding. This
	 * version uses the IANA charset identifier.
	 *
	 * If you are implementing your own conversion system (that is, have
	 * FEATURE_TABLEMANAGER disabled, you will need to implement this
	 * method on your own. Please note that there are several encoders
	 * that are available without the TableManager that you should make
	 * use of in your own implementation.
	 *
	 * @param charset Name of encoding.
	 * @param obj
	 *    A pointer to the created object will be inserted here.
	 *    The pointer will be set to NULL if an error occurs.
	 * @param iscanon
	 *    TRUE if name is already in canonized form (i.e if you have
	 *    already called CharsetManager::Canonize()).
	 * @param allownuls
	 *    TRUE if export media can handle octet streams with nul octets.
	 *    If FALSE, UTF-8 will be used if UTF-16 is requested.
	 *    You need to query the returned CharConverter for its encoding.
	 * @return
	 *    A status value. ERR_OUT_OF_RANGE if charset is invalid,
	 *    ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if support
	 *    is disabled for this build, ERR_NO_MEMORY if an OOM error
	 *    occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverter(const char *charset,
	                                     OutputConverter **obj,
	                                     BOOL iscanon = FALSE,
	                                     BOOL allownuls = FALSE);

	/**
	 * @overload
	 * @param charset Name of encoding.
	 * @param obj
	 *    A pointer to the created object will be inserted here.
	 *    The pointer will be set to NULL if an error occurs.
	 * @param iscanon
	 *    TRUE if name is already in canonized form (i.e if you have
	 *    already called CharsetManager::Canonize()).
	 * @param allownuls
	 *    TRUE if export media can handle octet streams with nul octets.
	 *    If FALSE, UTF-8 will be used if UTF-16 is requested.
	 *    You need to query the returned CharConverter for its encoding.
	 * @return
	 *    A status value. ERR_OUT_OF_RANGE if charset is invalid,
	 *    ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if support
	 *    is disabled for this build, ERR_NO_MEMORY if an OOM error
	 *    occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverter(const uni_char *charset,
	                                     OutputConverter **obj,
	                                     BOOL iscanon = FALSE,
	                                     BOOL allownuls = FALSE);

	/**
	 * Instantiates and returns a CharConverter object that can convert
	 * from the internal string encoding to the given encoding. This
	 * version uses a charset ID as determined by
	 * CharsetManager::GetCharsetID().
	 *
	 * @param charsetID Charset ID identifying encoding.
	 * @param obj
	 *    A pointer to the created object will be inserted here.
	 *    The pointer will be set to NULL if an error occurs.
	 * @param allownuls
	 *    TRUE if export media can handle octet streams with nul octets.
	 *    If FALSE, UTF-8 will be used if UTF-16 is requested.
	 *    You need to query the returned CharConverter for its encoding.
	 * @return
	 *    A status value. ERR_OUT_OF_RANGE if charset is invalid,
	 *    ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if support
	 *    is disabled for this build, ERR_NO_MEMORY if an OOM error
	 *    occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverterFromID(unsigned short id,
	                                     OutputConverter **obj,
	                                     BOOL allownuls = FALSE);

	virtual const char *GetCharacterSet();
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
	OutputConverter();
#endif

	/**
	 * Reset the encoder to initial state, writing any escape sequence or
	 * similar necessary to make the current converted data self-contained.
	 *
	 * The output buffer must have at least the number of bytes available
	 * returned by calling this function with a NULL parameter. This number
	 * is always smaller than the number returned by
	 * LongestSelfContainedSequenceForCharacter(). The number of bytes
	 * that are required may vary according to the current state of the
	 * encoder.
	 *
	 * The internal state of the encoder is changed when this method is
	 * called with a non-NULL parameter.
	 *
	 * This method will NOT flush a surrogate code unit (non-BMP codepoint),
	 * if such a code unit has been consumed as the last code unit, nor
	 * will it reset the invalid character counter. To do this, call
	 * Reset() after calling this method.
	 *
	 * @param dest A buffer to write to. Specifying NULL will just return
	 *             the number of bytes necessary.
	 * @return The number of bytes (that would be) written to the output
	 *         buffer to make the current string self-contained.
	 */
	virtual int ReturnToInitialState(void *dest) = 0;

	/**
	 * Reset the encoder to initial state without writing any data to the
	 * output stream. Also remove any information about invalid characters
	 * and any unconverted data in the buffer.
	 *
	 * The mode set by EnableEntityEncoding() is not changed.
	 *
	 * NB! If you call Reset during conversion, you may end up with invalid
	 * output, only use this method if you are actually starting over. For
	 * a more graceful reset operation, please use ReturnToInitialState().
	 */
	virtual void Reset();

	/**
	 * Check space required (worst case) to add one UTF-16 code unit to the
	 * output stream, and then call ReturnToInitialState(). This value may
	 * vary according to the current state of the encoder.
	 *
	 * The number returned is (obviously) always larger than the number
	 * returned by ReturnToInitialState(NULL).
	 */
	virtual int LongestSelfContainedSequenceForCharacter() = 0;

#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	/**
	 * Retrieve a string describing characters in the input that could not
	 * be represented in the output. The character string is limited to
	 * represent the 16 first unconvertible characters.
	 *
	 * @return Unconvertible characters.
	 */
	inline const uni_char *GetInvalidCharacters() const
	{ return m_invalid_characters; }
#endif

#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	/**
	 * Enable conversion of characters not available in the output encoding
	 * as numerical entities. This is not strictly according to the HTML
	 * specification, but is used by MSIE and accepted by most web based
	 * forums. The default is not to use entities.
	 *
	 * @param enable TRUE to allow entities.
	 */
	inline void EnableEntityEncoding(BOOL enable)
	{ m_entity_encoding = enable; }
#endif

protected:
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
	BOOL m_entity_encoding;
	BOOL CannotRepresent(const uni_char ch, int input_offset,
	                     char **dest, int *written, int maxlen,
	                     const char *substitute = NULL);
#endif
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
	void CannotRepresent(const uni_char ch, int input_offset);
#else
	inline void CannotRepresent(const uni_char, int input_offset)
	{
		++m_num_invalid;
		if (m_first_invalid_offset == -1)
			m_first_invalid_offset = m_num_converted + input_offset;
	}
#endif

private:
#ifdef ENCODINGS_HAVE_SPECIFY_INVALID
	uni_char m_invalid_characters[17];	///< Unconvertible characters. /* ARRAY OK 2009-03-02 johanh - boundary checked */
#endif
#if defined ENCODINGS_HAVE_SPECIFY_INVALID || defined ENCODINGS_HAVE_ENTITY_ENCODING
	uni_char m_invalid_high_surrogate;	///< Half of an unconvertible surrogate.
#endif
};

#endif // OUTPUTCONVERTER_H
