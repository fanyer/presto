/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HZ_ENCODER_H
#define HZ_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into HZ-GB2312 or ISO-2022-KR
 */
class UTF16toDBCS7bitConverter : public OutputConverter
{
public:
	enum encoding {
#ifdef ENCODINGS_HAVE_CHINESE
		HZGB2312,		///< Use HZ-GB2312 encoding
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		ISO2022KR		///< Use ISO-2022-KR encoding
#endif
	};

	UTF16toDBCS7bitConverter(enum encoding);
	virtual OP_STATUS Construct();
	virtual ~UTF16toDBCS7bitConverter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *dest);
	virtual int LongestSelfContainedSequenceForCharacter();
	virtual void Reset();

private:
	enum dbcs_charset {
		ASCII,		///< ordinary US-ASCII (single-byte)
		DBCS		///< Double-byte (GB2312/KSC5601)
	};

	const unsigned char *m_maptable1, *m_maptable2;
	long m_table1start, m_table1top, m_table2len;
	dbcs_charset m_cur_charset;
	encoding m_my_encoding;
#ifdef ENCODINGS_HAVE_KOREAN
	BOOL m_iso2022_initialized;
#endif
	int m_switch_sequence_length;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
#endif // HZ_ENCODER_H
