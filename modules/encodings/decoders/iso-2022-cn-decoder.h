/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ISO_2022_CN_DECODER_H
#define ISO_2022_CN_DECODER_H

#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)
#include "modules/encodings/decoders/inputconverter.h"

class ISO2022toUTF16Converter : public InputConverter
{
public:
	enum encoding {
#ifdef ENCODINGS_HAVE_CHINESE
		ISO2022CN,		///< Use ISO-2022-CN encoding
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		ISO2022KR		///< Use ISO-2022-KR encoding
#endif
	};

	ISO2022toUTF16Converter(enum encoding);
	virtual OP_STATUS Construct();
	virtual ~ISO2022toUTF16Converter();
	virtual int  Convert(const void *src, int len, void *dest,
		int maxlen, int *read);
	virtual const char *GetCharacterSet();
	virtual void Reset();
#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
	virtual BOOL IsValidEndState() { return m_state == ascii; }
#endif

private:
	enum cn_charset
	{
		NONE,			///< No charset yet defined
#ifdef ENCODINGS_HAVE_CHINESE
		// -- ISO-2022-CN
		GB2312,			///< ISO IR-058: SO charset is GB 2312
		CNS_11643_1,	///< ISO IR-171: SO charset is CNS 11643 plane 1
		CNS_11643_2,	///< ISO IR-172: SS2 charset is CNS 11643 plane 2
#endif
#ifdef ENCODINGS_HAVE_KOREAN
		// -- ISO-2022-KR
		KSC_5601		///< ISO IR-149: SO charset is KS C 5601
#endif
	};
	enum iso2022_state
	{
		ascii, esc, escplus1, escplus2,
		lead, trail, ss2_lead, ss2_trail
	};

#ifdef ENCODINGS_HAVE_CHINESE
	const UINT16 *m_gbk_table, *m_cns_11643_table;
	long m_gbk_length, m_cns_11643_length;
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	const UINT16 *m_ksc_5601_table;
	long m_ksc_5601_length;
#endif	

	char m_prev_byte;
	cn_charset m_current_so_charset;
#ifdef ENCODINGS_HAVE_CHINESE
	cn_charset m_current_ss2_charset;
#endif
	iso2022_state m_state, m_last_known_state;
	encoding m_my_encoding;

	BOOL identify_charset(char esc1, char esc2);

	ISO2022toUTF16Converter(const ISO2022toUTF16Converter&);
	ISO2022toUTF16Converter& operator =(const ISO2022toUTF16Converter&);
};

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)
#endif // ISO_2022_CN_DECODER_H
