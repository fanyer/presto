/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef INPUTCONVERTER_H
#define INPUTCONVERTER_H

#include "modules/encodings/charconverter.h"

#ifndef ENCODINGS_REGTEST
#include "modules/hardcore/unicode/unicode.h" // NOT_A_CHARACTER
#endif

/** Input converter interface */

class InputConverter : public CharConverter
{
public:
	/**
	 * Instantiates and returns a CharConverter object that can convert
	 * from the given encoding to the internal string encoding. This
	 * version uses the IANA charset identifier.
	 *
	 * @param charset Name of the encoding to convert from.
	 * @param obj A pointer to the created object will be inserted here.
	 *            The pointer will be set to NULL if an error occurs.
	 * @param allowUTF7 If FALSE (the default), a UTF-7 converter will never
	 *                  be returned (ERR_NOT_SUPPORTED will be returned
	 *                  instead). See bug #313069 for more information.
	 *
	 * @return A status value. ERR_OUT_OF_RANGE if charset is invalid,
	 *         ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if
	 *         support is disabled for this build, ERR_NO_MEMORY if
	 *         an OOM error occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverter(const char *charset,
	                                     InputConverter **obj,
	                                     BOOL allowUTF7 = FALSE);

#ifdef ENCODINGS_HAVE_MIB_SUPPORT
	/**
	 * Instantiates and returns a CharConverter object that can convert
	 * from the given encoding to the internal string encoding. This version
	 * uses the IANA charset MIB number (see RFC 3808) to identify the
	 * encoding.
	 *
	 * @param mibenum IANA charset MIB number of encoding.
	 * @param obj A pointer to the created object will be inserted here.
	 *            The pointer will be set to NULL if an error occurs.
	 * @param allowUTF7 If FALSE (the default), a UTF-7 converter will never
	 *                  be returned (ERR_NOT_SUPPORTED will be returned
	 *                  instead). See bug #313069 for more information.
	 *
	 * @return A status value. ERR_OUT_OF_RANGE if mibenum is invalid,
	 *         ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if
	 *         support is disabled for this build, ERR_NO_MEMORY if
	 *         an OOM error occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverter(int mibenum,
	                                     InputConverter **obj,
	                                     BOOL allowUTF7 = FALSE);
#endif

	/**
	 * Instantiates and returns a CharConverter object that can convert
	 * from the given encoding to the internal string encoding. This version
	 * uses the charset ID as determined by CharsetManager::GetCharsetID()
	 * to identify the encoding.
	 *
	 * @param id Charset ID identifying the encoding to convert from.
	 * @param obj A pointer to the created object will be inserted here.
	 *            The pointer will be set to NULL if an error occurs.
	 * @param allowUTF7 If FALSE (the default), a UTF-7 converter will never
	 *                  be returned (ERR_NOT_SUPPORTED will be returned
	 *                  instead). See bug #313069 for more information.
	 *
	 * @return A status value. ERR_OUT_OF_RANGE if charset ID is invalid,
	 *         ERR_NULL_POINTER if obj is NULL, ERR_NOT_SUPPORTED if
	 *         support is disabled for this build, ERR_NO_MEMORY if
	 *         an OOM error occurred. ERR if some other error occured.
	 */
	static OP_STATUS CreateCharConverterFromID(unsigned short id,
                                                   InputConverter **obj,
                                                   BOOL allowUTF7 = FALSE);

	virtual const char *GetDestinationCharacterSet();

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	/**
	 * Check if the decoder is in a valid ending state. If the decoder has
	 * consumed all input it has been presented and the internal state is
	 * returned to the initial one, this function will return TRUE.
	 *
	 * @return TRUE if the decoder is in a valid end state.
	 */
	virtual BOOL IsValidEndState() = 0;
#endif

protected:
	/**
	 * Handle invalid character in the input stream
	 *
	 * This is a convenience method collecting the handling of invalid
	 * characters in one place. It is to be called from the ::Convert()
	 * implementation in subclasses of this class.
	 *
	 * Unless actually_valid is given and TRUE, this method will increment
	 * m_num_invalid and set m_first_invalid_offset (if previously unset).
	 * If output_buffer is given and non-NULL, this method will also write
	 * a single character (NOT_A_CHARACTER/0xFFFD by default) to the given
	 * output buffer, before incrementing *output_buffer.
	 * The return value is the number of bytes written to output_buffer,
	 * i.e. 2 if output_buffer is given and non_NULL, else 0.
	 *
	 * @param output_offset The current BYTE offset into the destination
	 *        buffer of the Convert currently in progress.
	 *        IOW the offset relative to m_num_converted where the current
	 *        character would have been written if it was not invalid.
	 * @param output_buffer If given and non-NULL, write_char is written
	 *        to this buffer before the buffer pointer is incremented.
	 * @param write_char Which character to write to the output stream.
	 *        Defaults to NOT_A_CHARACTER/0xFFFD.
	 * @param actually_valid If given, and TRUE, m_num_invalid will NOT be
	 *        incremented (and m_first_invalid_offset NOT set); however the
	 *        rest of this method WILL be run. This can be used to write a
	 *        single (valid) character to the output buffer.
	 * @returns The number of bytes written to output_buffer, if given.
	 *          (i.e. 2 if output is given, 0 otherwise)
	 */
	inline int HandleInvalidChar(int output_offset,
	                             uni_char **output_buffer = NULL,
	                             uni_char write_char = NOT_A_CHARACTER,
	                             BOOL actually_valid = FALSE)
	{
		if (!actually_valid) {
			++m_num_invalid;
			if (m_first_invalid_offset == -1)
				m_first_invalid_offset = m_num_converted + (output_offset / 2);
		}
		if (output_buffer) {
			**output_buffer = write_char;
			(*output_buffer)++;
			return sizeof(uni_char);
		}
		return 0;
	}

private:
	static OP_STATUS CreateCharConverter_real(const char *realname,
	                                          InputConverter **obj,
	                                          BOOL allowUTF7 = FALSE);
};

#endif // INPUTCONVERTER_H
