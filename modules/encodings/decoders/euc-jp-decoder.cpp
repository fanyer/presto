/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/decoders/euc-jp-decoder.h"

EUCJPtoUTF16Converter::EUCJPtoUTF16Converter()
	: JIS0212toUTF16Converter(),
	  m_prev_byte2(0)
{
}

int EUCJPtoUTF16Converter::Convert(const void *_src, int len, void *dest,
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
			if (0x8E == m_prev_byte)
			{
				m_prev_byte = 0;
				/* Code set 2 is the JIS X 0201 set of half-width katakana.
				 * The range [A1..DF] are encoded in Unicode at codepoints
				 * [FF61..FF9F] in the same order. */
				if (*src >= 0xA1 && *src <= 0xDF)
				{
					(*output++) = *src - 0xA1 + 0xFF61;
					written += 2;
				}
				else
				{
					// Second byte was illegal, do not consume it
					written += HandleInvalidChar(written, &output);
					continue;
				}
			}
			else if (0x8F == m_prev_byte)
			{
				/* Code set 3 is JIS X 0212-1990. Seems not to be very
				 * common on the web, but is more common in e-mail. Not
				 * supported by Internet Explorer on Windows. Supported by
				 * most Unix browsers due to fonts supporting it, and
				 * support has also been added to Mozilla/Firefox on
				 * Windows. */
				if (m_prev_byte2)
				{
					int codepoint = (m_prev_byte2 - 0xA1) * 94 + (*src - 0xA1);
					m_prev_byte = 0;
					m_prev_byte2 = 0;
					if (*src >= 0xA1 && *src <= 0xFE)
					{
						if (codepoint < m_jis0212_length)
						{
							UINT16 jis0212char = ENCDATA16(m_jis_0212_table[codepoint]);
							written += HandleInvalidChar(written, &output, jis0212char, jis0212char != NOT_A_CHARACTER);
						}
						else
						{
							// Well-formed but unknown, consume and emit U+FFFD
							written += HandleInvalidChar(written, &output);
						}
					}
					else
					{
						// Third byte was illegal, do not consume it
						written += HandleInvalidChar(written, &output);
						continue;
					}
				}
				else if (*src >= 0xA1 && *src <= 0xFE)
				{
					m_prev_byte2 = *src;
				}
				else
				{
					// Second byte was illegal, do not consume it
					m_prev_byte = 0;
					written += HandleInvalidChar(written, &output);
					continue;
				}
			}
			else
			{
				/* Convert EUC-JP into JIS 0208 row-cell (kuten) encoding
				 * used by the mapping table. See Lunde pp.414 for an
				 * explanation. */
				int codepoint = (m_prev_byte - 0xA1) * 94 + (*src - 0xA1);
				m_prev_byte = 0;

				if (*src >= 0xA1 && *src <= 0xFE)
				{
					if (codepoint < m_jis0208_length)
					{
						UINT16 jis0208char = ENCDATA16(m_jis_0208_table[codepoint]);
						written += HandleInvalidChar(written, &output, jis0208char, jis0208char != NOT_A_CHARACTER);
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
		}
		else
		{
			if (*src <= 0x7F)
			{
				(*output++) = *src;
				written += 2;
			}
			else if ((*src >= 0xA1 && *src <= 0xFE) ||
			         *src == 0x8E || *src == 0x8F)
			{
				m_prev_byte = *src;
			}
			else
			{
				// Valid lead byte, store for next iteration
				written += HandleInvalidChar(written, &output);
			}
		}

		src ++;
		read ++;
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *EUCJPtoUTF16Converter::GetCharacterSet()
{
	return "euc-jp";
}

void EUCJPtoUTF16Converter::Reset()
{
	this->JIS0212toUTF16Converter::Reset();
	m_prev_byte2 = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
