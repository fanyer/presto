/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTF7_DECODER_H
#define UTF7_DECODER_H

#ifdef ENCODINGS_HAVE_UTF7
#include "modules/encodings/decoders/inputconverter.h"

/** Convert Unicode encoded as UTF-7 to UTF-16 */
class UTF7toUTF16Converter : public InputConverter
{
public:
	enum utf7_variant {
		STANDARD	///< UTF-7 as defined in RFC 2152
#ifdef ENCODINGS_HAVE_UTF7IMAP
	    , IMAP		///< Modified UTF-7 as defined in RFC 2060
#endif
	};

	UTF7toUTF16Converter(utf7_variant variant);
	virtual int Convert(const void *src, int len, void *dest,
	                    int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return m_state == ASCII; }
#endif

private:
	utf7_variant m_variant;
	unsigned int m_bits, m_valid_bits;
#ifdef ENCODINGS_HAVE_UTF7IMAP
	char m_escape;
#endif

	enum utf7_state { ASCII, BASE64_FIRST, BASE64 } m_state;
};

#endif // ENCODINGS_HAVE_UTF7
#endif // UTF7_DECODER_H
