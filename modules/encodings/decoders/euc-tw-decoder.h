/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EUC_TW_DECODER_H
#define EUC_TW_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/decoders/inputconverter.h"

class EUCTWtoUTF16Converter : public InputConverter
{
public:
	EUCTWtoUTF16Converter();
	virtual OP_STATUS Construct();
	virtual ~EUCTWtoUTF16Converter();
	virtual int Convert(const void *src, int len, void *dest,
	                    int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_lead && !m_ss2; }
#endif

private:
	const unsigned short *m_cns_11643_table;
	long m_cns_11643_table_len;
	unsigned char m_lead, m_ss2;
	signed char m_plane;

	EUCTWtoUTF16Converter(const EUCTWtoUTF16Converter&);
	EUCTWtoUTF16Converter& operator =(const EUCTWtoUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // EUC_TW_DECODER_H
