/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTF8_DECODER_H
#define UTF8_DECODER_H

#include "modules/encodings/decoders/inputconverter.h"
#include "modules/unicode/utf8.h"

/** Convert Unicode encoded as UTF-8 to UTF-16 */
class UTF8toUTF16Converter : public InputConverter,
	                         private UTF8Decoder
{
public:
	UTF8toUTF16Converter();
	virtual int Convert(const void *src, int len, void *dest,
	                    int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return this->UTF8Decoder::IsValidEndState(); }
#endif

protected:
	virtual int HandleInvalid(int output_offset)
	{
		// Route from UTF8Decoder to InputConverter
		return HandleInvalidChar(output_offset);
	}

	virtual void AddConverted(int num_converted)
	{
		// Route from UTF8Decoder to InputConverter
		m_num_converted += num_converted;
	}

private:
	UTF8toUTF16Converter(const UTF8toUTF16Converter&);
	UTF8toUTF16Converter& operator =(const UTF8toUTF16Converter&);
};

#endif // UTF8_DECODER_H
