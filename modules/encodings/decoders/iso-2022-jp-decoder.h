/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ISO_2022_JP_DECODER_H
#define ISO_2022_JP_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
#include "modules/encodings/decoders/jis-decoder.h"

class ISO2022JPtoUTF16Converter : public JIS0212toUTF16Converter
{
public:
	enum encoding {
		ISO_2022_JP,  ///< Use ISO-2022-JP encoding
		ISO_2022_JP_1 ///< Use ISO-2022-JP-1 encoding
	};

	ISO2022JPtoUTF16Converter(enum encoding);
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return m_state == ascii; }
#endif

private:
	enum iso_charset
	{
		ASCII,        ///< ordinary US-ASCII
		JIS_ROMAN,    ///< ISO IR-014: JIS-Roman (JIS X 0201 left half)
		JIS_KATAKANA, ///< ISO IR-013: JIS-Katakana (JIS X 0201 right half)
		JIS_0208,     ///< ISO IR-168: JIS X 0208:1997 or ISO IR-042: JIS X 0208:1978 (JIS C 6226-1978)
		JIS_0212,     ///< ISO IR-159: JIX X 0212:1990
		INCOMPLETE,   ///< Incomplete character designation (JIS X 0212 prefix)
		UNKNOWN
	};
	enum iso2022_state
	{
		ascii, singlebyte, lead, trail, esc, escplus1, escplus2
	};

	iso_charset   m_charset;
	iso2022_state m_state;
	encoding      m_my_encoding;

	iso_charset identify_charset(char esc1, char esc2);

private:
	ISO2022JPtoUTF16Converter(const ISO2022JPtoUTF16Converter&);
	ISO2022JPtoUTF16Converter& operator =(const ISO2022JPtoUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
#endif // ISO_2022_JP_DECODER_H
