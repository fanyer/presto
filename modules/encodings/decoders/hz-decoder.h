/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HZ_DECODER_H
#define HZ_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/decoders/inputconverter.h"

/**
 * A converter from the 8-bit-safe HZ-GB-2312 (or just HZ)
 * encoding to Unicode. The HZ encoding is based on the
 * GB 2312-80 character set.
 */
class HZtoUTF16Converter : public InputConverter
{
public:
	HZtoUTF16Converter();
	virtual OP_STATUS Construct();
	virtual ~HZtoUTF16Converter();
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_prev_byte && !m_gb_mode; }
#endif

private:
	const UINT16 *m_code_table;
	char m_prev_byte;
	int m_gb_mode;
	long m_table_length;

	HZtoUTF16Converter(const HZtoUTF16Converter&);
	HZtoUTF16Converter& operator =(const HZtoUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // HZ_DECODER_H
