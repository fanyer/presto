/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)

#ifndef ENCODINGS_REGTEST
# include "modules/util/handy.h" // ARRAY_SIZE
#endif
#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/dbcs-decoder.h"

DBCStoUTF16Converter::DBCStoUTF16Converter(const char *table_name,
                                           unsigned char first_low, unsigned char first_high,
                                           unsigned char second_low, unsigned char second_high,
                                           unsigned char second_low2, unsigned char second_high2)
	: m_map_table(NULL),
	  m_map_length(0),
	  m_prev_byte(0),
	  m_first_low(first_low), m_first_high(first_high),
	  m_second_low(second_low), m_second_high(second_high),
	  m_second_low2(second_low2), m_second_high2(second_high2)
{
	op_strlcpy(m_table_name, table_name, ARRAY_SIZE(m_table_name));
}

OP_STATUS DBCStoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	// Load mapping table
	m_map_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get(m_table_name, m_map_length));
	m_map_length /= 2; // must convert from byte count to UINT16 count

	return m_map_table ? OpStatus::OK : OpStatus::ERR;
}

DBCStoUTF16Converter::~DBCStoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_map_table);
	}
}

int DBCStoUTF16Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	while (read < len && written < maxlen)
	{
		if (m_prev_byte)
		{
			int codepoint = get_map_index(m_prev_byte, *input);
			m_prev_byte = 0;
			if (((*input >= m_second_low && *input <= m_second_high) ||
				 (*input >= m_second_low2 && *input <= m_second_high2)))
			{
				if (codepoint >= 0 && codepoint < m_map_length)
				{
					(*output ++) = ENCDATA16(m_map_table[codepoint]);
					written += 2;
				}
				else
				{
					// Well-formed but unknown, consume and emit U+FFFD
					written += HandleInvalidChar(written, &output);
				}
			}
			else
			{
				// Second byte was illegal, do not consume it
				written += HandleInvalidChar(written, &output);
				continue;
			}
		}
		else
		{
			if (*input <= 0x7F)
			{
				// ASCII
				(*output ++) = *input;
				written += 2;
			}
			else if (*input >= m_first_low && *input <= m_first_high)
			{
				// Valid lead byte, store for next iteration
				m_prev_byte = *input;
			}
			else
			{
				written += HandleInvalidChar(written, &output);
			}
		}

		input ++;
		read ++;
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

void DBCStoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_prev_byte = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
