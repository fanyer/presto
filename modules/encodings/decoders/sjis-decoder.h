/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SJIS_DECODER_H
#define SJIS_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
#include "modules/encodings/decoders/jis-decoder.h"

class SJIStoUTF16Converter : public JIStoUTF16Converter
{
public:
	SJIStoUTF16Converter() : JIStoUTF16Converter() {};
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_prev_byte; }
#endif

private:
	SJIStoUTF16Converter(const SJIStoUTF16Converter&);
	SJIStoUTF16Converter& operator =(const SJIStoUTF16Converter&);

};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
#endif // SJIS_DECODER_H
