/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DBCS_ENCODER_H
#define DBCS_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into simple row-cell encoding
 */
class UTF16toDBCSConverter : public OutputConverter
{
public:
	enum encoding {
#ifdef ENCODINGS_HAVE_CHINESE
		BIG5,			///< Use Big5 encoding
		GBK,			///< Use GBK (>= GB2312 = EUC-CN) encoding
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		EUCKR			///< Use EUC-KR (= KSC 5 601) encoding
#endif
	};

	UTF16toDBCSConverter(enum encoding);
	virtual OP_STATUS Construct();
	virtual ~UTF16toDBCSConverter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *) { return 0; };
	virtual int LongestSelfContainedSequenceForCharacter() { return 2; };

private:
	const unsigned char *m_maptable1, *m_maptable2;
	long m_table1start, m_table1top, m_table2len;
	enum encoding m_my_encoding;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
#endif // DBCS_ENCODER_H
