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
#include "modules/encodings/decoders/iso-2022-jp-decoder.h"

ISO2022JPtoUTF16Converter::ISO2022JPtoUTF16Converter(enum encoding inputencoding)
	: JIS0212toUTF16Converter(),
	  m_charset(ASCII),
	  m_state(ascii),
	  m_my_encoding(inputencoding)
{
}

ISO2022JPtoUTF16Converter::iso_charset ISO2022JPtoUTF16Converter::identify_charset(char esc1, char esc2)
{
	if (esc1 == '(' && esc2 == 'B')
	{
		return ASCII;
	}
	else if (esc1 == '(' && esc2 == 'I')
	{
		return JIS_KATAKANA;
	}
	else if (esc1 == '(' && esc2 == 'J')
	{
		return JIS_ROMAN;
	}
	else if (esc1 == '$' && (esc2 == 'B' || esc2 == '@'))
	{
		/* Treat JIS X 0208:1997 and JIS X 0208:1978 (JIS C 6226:1978) as if
		 * they were the same, but in reality that is not 100 % true. JIS X
		 * 0208:1978 is an earlier version of JIS X 0208:1997, and there are
		 * some exceedingly subtle differences between them (different
		 * versions of some characters have exchanged places and some glyph
		 * shapes are subtly altered). These differences are documented on
		 * pages 918-922 of Lunde's CJKV book. Reading those pages can
		 * seriously endanger your mental health. Since treating the two
		 * character sets as one and the same did not yield a bug report for
		 * the first decade, it seems that we can live with the assumption.
		 */
		return JIS_0208;
	}
	else if (esc1 == '$' && esc2 == '(')
	{
		return INCOMPLETE;
	}
	else
	{
		return UNKNOWN;
	}
}

int ISO2022JPtoUTF16Converter::Convert(const void *src, int len, void *dest,
                                       int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);

	while (read < len && written < maxlen)
	{
		switch (m_state)
		{
		case lead:
			// Double-byte mode; lead expected
			if (*input < 0x20)
			{
				// C0 control character.
				if (0x1B == *input)
				{
					// Begin escape sequence.
					m_state = esc;
					m_prev_byte = *input;
				}
				else
				{
					// Anything else is an error.
					m_state = ascii;
					written += HandleInvalidChar(written, &output, *input);
				}
			}
			else
			{
				m_prev_byte = *input;
				m_state = trail;
			}
			input ++;
			read ++;
			break;

		case trail:
		{
			/* Double-byte mode; trail expected.
			 * In the 0xFF x 0xFF character square, JIS X 0208 only uses
			 * character positions in the 0x21 - 0x7E (columns) and 0x21 -
			 * 0x74 (rows) area. We exploit this to shrink the table by a
			 * factor of about 10. This is the reason for the 33 and 94
			 * numbers below. We start at row 33 and column 33, and each row
			 * has 94 characters in it. The if-test is there to ensure that
			 * we don't try to read outside the mapping table. */
			int codepoint = (m_prev_byte - 33) * 94 + (*input - 33);
			m_state = lead;

			if (m_prev_byte < 0x21 || m_prev_byte > 0x7C ||
			    *input < 0x21 || *input > 0x7E ||
				codepoint >= (m_charset == JIS_0212 ? m_jis0212_length : m_jis0208_length))
			{
				written += HandleInvalidChar(written, &output);
				if (*input < 0x20)
				{
					/* C0 control character where trail byte was expected:
					 * flag a decoding error and switch to ASCII. */
					m_state = *input == 0x1B ? esc : ascii;
				}
			}
			else
			{
				UINT16 utf16 = ENCDATA16((m_charset == JIS_0212 ? m_jis_0212_table : m_jis_0208_table)[codepoint]);
				written += HandleInvalidChar(written, &output, utf16, utf16 != NOT_A_CHARACTER);
			}
			input ++;
			read ++;
			break;
		}

		case ascii:
			// Single-byte ASCII mode
			while (read < len && written < maxlen)
			{
				// Copy as much as possible in this inner loop
				unsigned char ch = *(input ++);
				read ++;
				if (27 == ch)
				{
					m_state = esc;
					break;
				}
				else
				{
					(*output++) = static_cast<UINT16>(ch);
					written += 2;
				}
			}
			break;

		case singlebyte:
			// Single-byte JIS-ROMAN or JIS-KATAKANA mode
			switch (m_charset)
			{
			default:
					OP_ASSERT(!"Internal error, invalid ISO-2002-JP SB state.");
			case JIS_ROMAN:
				while (read < len && written < maxlen)
				{
					// Copy as much as possible in this inner loop
					unsigned char ch = *(input ++);
					read ++;
					switch (ch)
					{
					case 0x1B: m_state = esc; goto done_jisroman; /* Mode switch */
					case 0x5C: (*output++) = 0x00A5; break; /* YEN SIGN */
					case 0x7E: (*output++) = 0x203E; break; /* OVERLINE */
					default:   (*output++) = ch < 0x80 ? ch
					                                   : NOT_A_CHARACTER;
					}
					written += 2;
				}
done_jisroman:
				break;
			case JIS_KATAKANA:
				while (read < len && written < maxlen)
				{
					// Copy as much as possible in this inner loop
					unsigned char ch = *(input ++);
					read ++;
					if (0x1B == ch)
					{
						m_state = esc;
						break;
					}
					else
					{
						(*output++) =
							(ch >= 0x21 && ch <= 0x5F) ? ch + 0xFF40
							                           : NOT_A_CHARACTER;
						written += 2;
					}
				}
				break;
			}
			break;

		case esc:
			// ESC found, first identifier expected
			m_prev_byte = *input;
			if (m_prev_byte == '(' || m_prev_byte == '$')
			{
				m_state = escplus1;
			}
			else
			{
				// Unrecognized escape sequence: don't consume
				written += HandleInvalidChar(written, &output);
				m_state = ascii;
				break;
			}
			input ++;
			read ++;
			break;

		case escplus1:
			// ESC and first found, second identifier expected
			m_charset = identify_charset(m_prev_byte, *input);
			if (JIS_0208 == m_charset)
			{
				m_state = lead;
			}
			else if (ASCII == m_charset)
			{
				m_state = ascii;
			}
			else if (INCOMPLETE == m_charset)
			{
				m_state = escplus2;
			}
			else if (JIS_ROMAN == m_charset || JIS_KATAKANA == m_charset)
			{
				m_state = singlebyte;
			}
			else
			{
				// Unrecognized escape sequence: don't consume
				written += HandleInvalidChar(written, &output);
				m_state = ascii;
				break;
			}
			input ++;
			read ++;
			break;

		case escplus2:
			// ESC and the two first bytes of the JIS X 0212 designator
			// found, third byte expected
			if ('D' == *input)
			{
				m_charset = JIS_0212;
				m_state = lead;
			}
			else
			{
				// Unrecognized escape sequence: don't consume
				written += HandleInvalidChar(written, &output);
				m_state = ascii;
				break;
			}
			read ++;
			input ++;
			break;
		}
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *ISO2022JPtoUTF16Converter::GetCharacterSet()
{
	return m_my_encoding == ISO_2022_JP ? "iso-2022-jp" : "iso-2022-jp-1";
}

void ISO2022JPtoUTF16Converter::Reset()
{
	this->JIS0212toUTF16Converter::Reset();
	m_charset = ASCII;
	m_state   = ascii;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE
