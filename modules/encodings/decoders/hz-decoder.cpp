/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/hz-decoder.h"

HZtoUTF16Converter::HZtoUTF16Converter()
	: m_code_table(NULL),
	  m_prev_byte(0),
	  m_gb_mode(0),
	  m_table_length(0)
{
}

OP_STATUS HZtoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	m_code_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("gbk-table", m_table_length));
	m_table_length /= 2; // Adjust from byte count to UINT16 count

	return m_code_table ? OpStatus::OK : OpStatus::ERR;
}

HZtoUTF16Converter::~HZtoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_code_table);
	}
}

int HZtoUTF16Converter::Convert(const void *_src, int len, void *dest,
			   					int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *src = reinterpret_cast<const unsigned char *>(_src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	while (read < len && written < maxlen)
	{
		if (m_prev_byte)
		{
			if ('~' == m_prev_byte)
			{
				m_prev_byte = 0; // reset m_prev_byte

				if ('{' == *src)
				{
					// Switch to GB mode
					m_gb_mode = 1;
				}
				else if ('}' == *src)
				{
					// Switch to ASCII mode
					m_gb_mode = 0;
				}
				else if ('~' == *src)
				{
					// Literal tilde
					(*output++) = '~';
					written += 2;
				}
				// \n continues a line outputting nothing
				else if ('\n' != *src)
				{
					// Invalid escape sequence
					written += HandleInvalidChar(written, &output);
					continue;
				}
			}
			else
			{
				int codepoint = (m_prev_byte - 1) * 191 + (*src + 0x40);
				if (*src > 0x20 && *src < 0x7F && codepoint < m_table_length)
				{
					(*output++) = ENCDATA16(m_code_table[codepoint]);
					written += 2;
				}
				else if (0x0A == *src)
				{
					// Error condition: Line break in stream. Output it and
					// switch back to singlebyte mode.
					m_gb_mode = 0;
					written += HandleInvalidChar(written, &output, 0x0A);
				}
				else
				{
					written += HandleInvalidChar(written, &output);
				}
				m_prev_byte = 0;
			}
		}
		else if ('~' == *src)
		{
			// Escape character
			m_prev_byte = *src;
		}
		else if (*src & 0x80)
		{
			// High-bit characters are not allowed in a HZ data stream
			written += HandleInvalidChar(written, &output);
		}
		else if (m_gb_mode)
		{
			// Double-byte mode
			if (*src > 0x20 && *src < 0x7F)
			{
				m_prev_byte = *src;
			}
			else if (0x0A == *src)
			{
				// Error condition: Line break in stream. Output it and
				// switch back to singlebyte mode.
				m_gb_mode = 0;
				written += HandleInvalidChar(written, &output, 0x0A);
			}
			else
			{
				written += HandleInvalidChar(written, &output);
			}
		}
		else
		{
			// ASCII mode
			(*output++) = *src;
			written += 2;
		}

		src ++;
		read ++;
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *HZtoUTF16Converter::GetCharacterSet()
{
	return "hz-gb-2312";
}

void HZtoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_prev_byte = 0;
	m_gb_mode = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
