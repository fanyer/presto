/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JIS_ENCODER_H
#define JIS_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into ISO-2022-JP, ISO-2022-JP-1, EUC-JP or Shift-JIS
 */
class UTF16toJISConverter : public OutputConverter
{
public:
	enum encoding {
		ISO_2022_JP,	///< Use ISO-2022-JP encoding
		ISO_2022_JP_1,	///< Use ISO-2022-JP-1 encoding
		EUC_JP,			///< Use EUC-JP encoding
		SHIFT_JIS		///< Use Shift-JIS encoding
	};

	UTF16toJISConverter(enum encoding);
	virtual OP_STATUS Construct();
	virtual ~UTF16toJISConverter();
	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *dest);
	virtual int LongestSelfContainedSequenceForCharacter();
	virtual void Reset();

private:
	enum iso_charset {
		ASCII,		///< ordinary US-ASCII
		JIS_ROMAN,	///< ISO IR-014: JIS-Roman (JIS X 0201 left half)
		JIS_0208,	///< ISO IR-168: JIS X 0208:1997
		JIS_0212	///< ISO IR-159: JIX X 0212:1990
	};

	const unsigned char *m_maptable1, *m_maptable2;
	long m_table1top, m_table2len;

	const unsigned char *m_jis0212_1, *m_jis0212_2;
	long m_jis0212_1top, m_jis0212_2len;

	iso_charset m_cur_charset;
	enum encoding m_my_encoding;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
#endif // JIS_ENCODER_H
