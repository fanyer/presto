/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BIG5HKSCS_DECODER_H
#define BIG5HKSCS_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE
#include "modules/encodings/decoders/inputconverter.h"

class Big5HKSCStoUTF16Converter : public InputConverter
{
public:
	Big5HKSCStoUTF16Converter();
	virtual OP_STATUS Construct();
	virtual ~Big5HKSCStoUTF16Converter();
	virtual int Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return !m_prev_byte; }
#endif

private:
#ifndef ENC_BIG_HKSCS_TABLE
	UINT32 LookupHKSCS(unsigned char row, unsigned char cell);
#endif
#if defined ENC_BIG_HKSCS_TABLE || defined DOXYGEN_DOCUMENTATION
	long hkscs_tableoffset(unsigned char row, unsigned char cell);
	UINT32 *GenerateHKSCSTable();
#endif

	unsigned char m_prev_byte;
	const UINT16 *m_big5table;
	long m_big5table_length;
	uni_char m_surrogate;
#ifndef ENC_BIG_HKSCS_TABLE
	const UINT16 *m_hkscs_plane0, *m_hkscs_plane2;
	long m_hkscs_plane0_length, m_hkscs_plane2_length;
#endif
	const unsigned char *m_compat_table;
	long m_compat_table_length;

	Big5HKSCStoUTF16Converter(const Big5HKSCStoUTF16Converter&);
	Big5HKSCStoUTF16Converter& operator =(const Big5HKSCStoUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
#endif // BIG5HKSCS_DECODER_H
