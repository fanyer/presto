/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef UTF7_ENCODER_H
#define UTF7_ENCODER_H

#ifdef ENCODINGS_HAVE_UTF7
#include "modules/encodings/encoders/outputconverter.h"

class UTF16toUTF7Converter : public OutputConverter
{
public:
	enum utf7_variant {
		STANDARD,	///< UTF-7 as defined in RFC 2152
		SAFE		///< Like STANDARD, but encodes unsafe characters
#ifdef ENCODINGS_HAVE_UTF7IMAP
		, IMAP		///< Modified UTF-7 as defined in RFC 2060
#endif
	};

	UTF16toUTF7Converter(utf7_variant variant);

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *dest);
	virtual int LongestSelfContainedSequenceForCharacter();
	virtual void Reset();

private:
	enum utf7_state { ASCII, BASE64, BASE64_EXIT, ESCAPE } m_state;
	utf7_variant m_variant;
	unsigned int m_bits, m_valid_bits;
#ifdef ENCODINGS_HAVE_UTF7IMAP
	char m_escape;
#endif

	BOOL AllowClear(uni_char c);
	static const unsigned char utf7_char_classes[0x80]; /* ARRAY OK 2012-02-15 eddy */
};

#endif // ENCODINGS_HAVE_UTF7
#endif // UTF7_ENCODER_H
