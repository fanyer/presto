/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef ENCODINGS_HAVE_TABLE_DRIVEN

#ifndef ENCODINGS_REGTEST
# include "modules/util/handy.h" // ARRAY_SIZE
#endif
#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/sbcs-decoder.h"

SingleBytetoUTF16Converter::SingleBytetoUTF16Converter(const char *charset, const char *tablename)
	: ISOLatin1toUTF16Converter(),
	  m_table_size(0),
	  m_codepoints(NULL)
{
	Init(charset, tablename);
}

SingleBytetoUTF16Converter::SingleBytetoUTF16Converter(const char *tablename)
	: ISOLatin1toUTF16Converter(),
	  m_table_size(0),
	  m_codepoints(NULL)
{
	Init(tablename, tablename);
}

void SingleBytetoUTF16Converter::Init(const char *charset, const char *tablename)
{
	op_strlcpy(m_charset, charset, ARRAY_SIZE(m_charset));
	op_strlcpy(m_tablename, tablename, ARRAY_SIZE(m_tablename));
}

OP_STATUS SingleBytetoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->ISOLatin1toUTF16Converter::Construct())) return OpStatus::ERR;

	m_codepoints =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get(m_tablename, m_table_size));
	if (m_table_size != 256 && m_table_size != 512)
	{
		// Unknown table size!
		g_table_manager->Release(m_codepoints);
		m_codepoints = NULL;
	}

	return m_codepoints ? OpStatus::OK : OpStatus::ERR;
}

SingleBytetoUTF16Converter::~SingleBytetoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_codepoints);
	}
}

int SingleBytetoUTF16Converter::Convert(const void *src, int len, void *dest,
					int maxlen, int *read_p)
{
	if (!m_codepoints)
	{
		// Fall back to ISO 8859-1 if no tables were loaded
		return ISOLatin1toUTF16Converter::Convert(src, len, dest, maxlen, read_p);
	}

	uni_char *output = reinterpret_cast<uni_char *>(dest);
	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	len = MIN(len, maxlen / 2);

	if (256 == m_table_size)
	{
		// ASCII based encoding
		for (int i = len; i; -- i)
		{
			if (*input & 0x80)
			{
#ifdef ENCODINGS_OPPOSITE_ENDIAN
				*(output ++) = ENCDATA16(m_codepoints[*input & 0x7F]);
				++ input;
#else
				*(output ++) = m_codepoints[*(input ++) & 0x7F];
#endif
			}
			else
			{
				*(output ++) = static_cast<UINT16>(*(input ++));
			}
		}
	}
	else
	{
		// Non-ASCII based encoding, for example VISCII
		for (int i = len; i; -- i)
		{
#ifdef ENCODINGS_OPPOSITE_ENDIAN
			*(output ++) = ENCDATA16(m_codepoints[*input]);
			++ input;
#else
			*(output ++) = m_codepoints[*(input ++)];
#endif
		}
	}

	*read_p = len;
	m_num_converted += len;
	return len * 2;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN
