/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ISO_2022_CN_ENCODER_H
#define ISO_2022_CN_ENCODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/encoders/outputconverter.h"

/** Convert UTF-16 into ISO 2022-CN
 */
class UTF16toISO2022CNConverter : public OutputConverter
{
public:
	UTF16toISO2022CNConverter();
	virtual OP_STATUS Construct();
	virtual ~UTF16toISO2022CNConverter();

	virtual int Convert(const void *src, int len, void *dest, int maxlen,
	                    int *read);

	virtual const char *GetDestinationCharacterSet();
	virtual int ReturnToInitialState(void *dest);
	virtual int LongestSelfContainedSequenceForCharacter();
	virtual void Reset();

private:
	enum cn_charset {
		ASCII,			///< ordinary US-ASCII (single-byte)
		NONE = ASCII,	///< No SO charset defined
	    GB2312,			///< ISO IR-058: GB2312 mode (double-byte)
		CNS_11643_1,	///< ISO IR-171: CNS 11643 plane 1 mode (double-byte)

		// Tests for the switcher
		TRY_GB2312,
		TRY_CNS_11643,
		FAIL
	};

	cn_charset m_cur_charset, m_so_initialized;
	BOOL m_ss2_initialized; // ISO IR-172: CNS 11643 plane 2
	const unsigned char *m_gbk_table1, *m_gbk_table2,
	                    *m_cns11643_table1, *m_cns11643_table2;
	long m_gbk_table1top, m_gbk_table2len,
	     m_cns116431_table1top, m_cns1143_table2len;

	/** Helper to write encoding switcher codes */
	static int switch_charset(const char *, size_t, BOOL, char *, int);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // ISO_2022_CN_ENCODER_H
