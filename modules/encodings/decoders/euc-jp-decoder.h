/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EUC_JP_DECODER_H
#define EUC_JP_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
#include "modules/encodings/decoders/jis-decoder.h"

class EUCJPtoUTF16Converter : public JIS0212toUTF16Converter
{
public:
	EUCJPtoUTF16Converter();
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_prev_byte; }
#endif

private:
	unsigned char   m_prev_byte2;

	EUCJPtoUTF16Converter(const EUCJPtoUTF16Converter&);
	EUCJPtoUTF16Converter& operator =(const EUCJPtoUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
#endif // EUC_JP_DECODER_H
