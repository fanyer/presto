/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JIS_DECODER_H
#define JIS_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
#include "modules/encodings/decoders/inputconverter.h"

/** Abstract super-class for decoder for JIS 0208 based encodings. */
class JIStoUTF16Converter : public InputConverter
{
public:
	JIStoUTF16Converter();
	virtual OP_STATUS Construct();
	virtual ~JIStoUTF16Converter();
	virtual void Reset();

protected:
	const UINT16 *m_jis_0208_table;
	long          m_jis0208_length;
	unsigned char m_prev_byte;
};

/** Abstract super-class for decoder for JIS 0208+0212 based encodings. */
class JIS0212toUTF16Converter : public JIStoUTF16Converter
{
public:
	JIS0212toUTF16Converter();
	virtual OP_STATUS Construct();
	virtual ~JIS0212toUTF16Converter();

protected:
	const UINT16 *m_jis_0212_table;
	long m_jis0212_length;
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
#endif // JIS_DECODER_H
