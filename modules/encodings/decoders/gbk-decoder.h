/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GBK_DECODER_H
#define GBK_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/decoders/inputconverter.h"

/**
 * A converter from the GBK and GB18030 encodings to Unicode. GBK is
 * an extension of EUC-CN based on a never-published
 * annex to GB 13000.1-93, according to Lunde (p. 170).
 * GB18030 is an extension to EUC-CN which is backwards compatible,
 * while encoding *all* of Unicode.
 */
class GBKtoUTF16Converter : public InputConverter
{
public:
	enum gb_variant { GBK, GB18030 };

	GBKtoUTF16Converter(gb_variant variant);
	virtual OP_STATUS Construct();
	virtual ~GBKtoUTF16Converter();
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_first_byte; }
#endif

private:

	GBKtoUTF16Converter(const GBKtoUTF16Converter&);
	GBKtoUTF16Converter& operator =(const GBKtoUTF16Converter&);

	const UINT16 *m_gbk_table, *m_18030_table;
	unsigned char m_first_byte, m_second_byte, m_third_byte;
	UINT16 m_surrogate;
	long m_gbk_length, m_18030_length;
	gb_variant m_variant;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // GBK_DECODER_H
