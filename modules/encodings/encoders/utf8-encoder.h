/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTF8_ENCODER_H
#define UTF8_ENCODER_H

#include "modules/encodings/encoders/outputconverter.h"
#include "modules/unicode/utf8.h"

class UTF16toUTF8Converter : public OutputConverter,
	                         private UTF8Encoder
{
public:
	UTF16toUTF8Converter() {}

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();

	/**
	 * Calculate size needed (in bytes) to accomodate this string.
	 *
	 * BytesNeeded() maintains state between calls, and uses the same state
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
	int BytesNeeded(const void *src, int bytesize, int maxlen = INT_MAX, int *read = NULL)
	{ return this->UTF8Encoder::Measure(src, bytesize, maxlen, read); }

	virtual int ReturnToInitialState(void *) { return 0; }
	virtual int LongestSelfContainedSequenceForCharacter()
	{ return this->UTF8Encoder::LongestSelfContainedSequenceForCharacter(); }
	virtual void Reset();

protected:
	virtual void AddConverted(int num_converted)
	{
		// Route from UTF8Encoder to OutputConverter
		m_num_converted += num_converted;
	}
};

#endif // UTF8_ENCODER_H
