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
#include "modules/encodings/decoders/sjis-decoder.h"

int SJIStoUTF16Converter::Convert(const void *_src, int len, void *dest,
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
			// Check for valid trail byte
			if (m_jis0208_length &&
			    ((*src >= 0x40 && *src <= 0x7E) ||
			     (*src >= 0x80 && *src <= 0xFC)))
			{
				/* Calculate offset in JIS 0208 table, taking into account the
				 * non-contiguity of both lead ([81..9F], [E0..EF]) and trail
				 * bytes ([40..7E], [80..FC]). Each lead byte encodes two rows
				 * of 94 (= 188) characters in the JIS X 0208 table. */
				int lead_offset = m_prev_byte >= 0xA0 ? 0xC1 : 0x81;
				int offset = *src >= 128 ? 0x41 : 0x40;
				int codepoint = (m_prev_byte - lead_offset) * 188 + *src - offset;
				if (codepoint < m_jis0208_length)
				{
					UINT16 jis0208char =
						ENCDATA16(m_jis_0208_table[codepoint]);
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
				m_prev_byte = 0;
				continue;
			}

			m_prev_byte = 0;
		}
		else
		{
			if (*src <= 0x7E)
			{
				// This range should really use the jis_roman_table, but
				// we don't because of the Yen vs. backslash problem
				(*output++) = *src;
				written += 2;
			}
			else if (*src >= 0xA1 && *src <= 0xDF)
			{
				/* The range [A1..DF] encodes half-width katakana in accordance
				 * to the JIS X 0201 table. These characters are encoded in
				 * Unicode at codepoints [FF61..FF9F] in the same order. */
				(*output++) = *src - 0xA1 + 0xFF61;
				written += 2;
			}
			else if ((*src >= 0x81 && *src <= 0x9F) ||
			         (*src >= 0xE0 && *src <= 0xFC))
			{
				// Valid lead byte, store for next iteration
				m_prev_byte = *src;
			}
			else
			{
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

const char *SJIStoUTF16Converter::GetCharacterSet()
{
	return "shift_jis";
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
