/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_JAPANESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/jis-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

/**
 * Convert from ISO-2022-JP encoding to shift-JIS encoding
 * @param s The two bytes to convert (input and output)
 */
static inline void jis2sjis(unsigned char *s)
{
	// Lifted from Lunde's CJKV book, p.418
	int rowOffset =  s[0] < 95 ? 112 : 176;
	int cellOffset= (s[0] % 2) ? (s[1] > 95 ? 32 : 31) : 126;
	s[0]  = ((s[0] + 1) >> 1) + rowOffset;
	s[1] += cellOffset;
}

UTF16toJISConverter::UTF16toJISConverter(enum encoding outputencoding)
	: m_maptable1(NULL), m_maptable2(NULL),
	  m_table1top(0), m_table2len(0),
	  m_jis0212_1(NULL), m_jis0212_2(NULL),
	  m_jis0212_1top(0), m_jis0212_2len(0),
	  m_cur_charset(ASCII),
	  m_my_encoding(outputencoding)
{
}

OP_STATUS UTF16toJISConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	// Load JIS 0208 tables
	long table1len = 0;
	m_maptable1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("jis-0208-rev-1", table1len));
	m_maptable2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("jis-0208-rev-2", m_table2len));

	// Determine which characters are supported by the tables
	m_table1top = 0x4E00 + table1len / 2;

	// Load JIS 0212 tables
	if (m_my_encoding == EUC_JP || m_my_encoding == ISO_2022_JP_1)
	{
		long jis0212_1len = 0;
		m_jis0212_1 =
			reinterpret_cast<const unsigned char *>(g_table_manager->Get("jis-0212-rev-1", jis0212_1len));
		m_jis0212_2 =
			reinterpret_cast<const unsigned char *>(g_table_manager->Get("jis-0212-rev-2", m_jis0212_2len));

		// Determine which characters are supported by the tables
		m_jis0212_1top = 0x4E00 + jis0212_1len / 2;
	}

	return m_maptable1 && m_maptable2 &&
		   (!(m_my_encoding == EUC_JP || m_my_encoding == ISO_2022_JP_1) || (m_jis0212_1 && m_jis0212_2))
		? OpStatus::OK : OpStatus::ERR;
}

UTF16toJISConverter::~UTF16toJISConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_maptable1);
		g_table_manager->Release(m_maptable2);

		if (m_jis0212_1 || m_jis0212_2)
		{
			g_table_manager->Release(m_jis0212_1);
			g_table_manager->Release(m_jis0212_2);
		}
	}
}

const char *UTF16toJISConverter::GetDestinationCharacterSet()
{
	switch (m_my_encoding)
	{
	case ISO_2022_JP:	return "iso-2022-jp";	break;
	case ISO_2022_JP_1:	return "iso-2022-jp-1";	break;
	case EUC_JP:		return "euc-jp";		break;
	case SHIFT_JIS:		return "shift_jis";		break;
	}

	OP_ASSERT(!"Unknown encoding");
	return NULL;
}

// Full-width character to use for undefined code points
// Full-width question mark
#define NOT_A_CHARACTER_JIS_KU		0x21
#define NOT_A_CHARACTER_JIS_TEN		0x29

int UTF16toJISConverter::Convert(const void *src, int len, void *dest,
                                 int maxlen, int *read_p)
{
	// Conversion table to convert half-width katakana to full-width
	static const uni_char fwkatakana[0xFF9F - 0xFF61 + 1] =
	{
		0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3,
		0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
		0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
		0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
		0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
		0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
		0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
		0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3,
		// The following two should be 0x3099 and 0x309A but since
		// they are not available in JIS-0208, we use the non-combining
		// forms instead. A true conversion should combine forms before
		// converting
		0x309B, 0x309C
	};

	int read = 0;
	int written = 0;
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	int utf16len = len / 2;

	char *output = reinterpret_cast<char *>(dest);

	BOOL is_jis0212;
	while (read < utf16len && written < maxlen)
	{
		if (*input < 128)
		{
			// ASCII
			if ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding)
			    && m_cur_charset != ASCII)
			{
				if (written + 4 > maxlen)
				{
					// Don't split escape code
					goto leave;
				}
				int extra = ReturnToInitialState(output);
				written += extra;
				output += extra;
			}
			*(output ++) = static_cast<char>(*input);
			written ++;
		}
		else
		{
			is_jis0212 = FALSE;
			char rowcell[2]; /* ARRAY OK 2009-01-27 johanh */
			if (*input >= 0x4E00 && *input < m_table1top)
			{
				rowcell[1] = m_maptable1[(*input - 0x4E00) * 2];
				rowcell[0] = m_maptable1[(*input - 0x4E00) * 2 + 1];
			}
			else if (*input >= 0xFF61 && *input <= 0xFF9F)
			{
				// Handling of the dreaded (and dreadful) half-width katakana
				rowcell[0] = 0; // Signal SBCS mode (overridden for non-Shift-JIS)

				switch (m_my_encoding)
				{
				case EUC_JP:
					// map into A1-DF range; add 8E as prefix
					rowcell[0] = static_cast<char>(static_cast<unsigned char>(0x8E));
					/* Fall through */
				case SHIFT_JIS:
					// map into A1-DF range (which goes straight out)
					rowcell[1] = *input - 0xFF61 + 0xA1;
					break;

				case ISO_2022_JP:
				case ISO_2022_JP_1:
					// Half-width to double-width conversion
					// We must do it for ISO 2022-JP, which does not support
					// half-width katakana at all.
					// This is a simplification; according to Lunde (p.429), we
					// should combine diacritics with their corresponding
					// characters here, but since Unicode declare the
					// de-composed variants as equal, and half-width text is
					// quite rare anyway, we don't do that. However, since
					// JIS-0208 does not contain the combining full-width forms
					// of U+FF9E and U+FF9F, these will not be handled
					// correctly (compare with table above)
 					lookup_dbcs_table(m_maptable2, m_table2len,
					                  fwkatakana[*input - 0xFF61], rowcell);
					break;
				}
			}
			else if (NOT_A_CHARACTER == *input)
			{
				rowcell[0] = 0;
				rowcell[1] = 0;
			}
			else
			{
				lookup_dbcs_table(m_maptable2, m_table2len, *input, rowcell);
			}

			// Check if we need to look it up in the JIS 0212 table
			if (m_jis0212_1 && m_jis0212_2 && rowcell[0] == 0 && rowcell[1] == 0)
			{
				if (*input >= 0x4E00 && *input < m_jis0212_1top)
				{
					rowcell[1] = m_jis0212_1[(*input - 0x4E00) * 2];
					rowcell[0] = m_jis0212_1[(*input - 0x4E00) * 2 + 1];
				}
				else
				{
					lookup_dbcs_table(m_jis0212_2, m_jis0212_2len, *input, rowcell);
				}
				if (rowcell[0] != 0 && rowcell[1] != 0)
				{
					if (EUC_JP == m_my_encoding)
					{
						if (written + 3 > maxlen)
						{
							// Don't split character
							goto leave;
						}
						*(output ++) = static_cast<char>(static_cast<unsigned char>(0x8F)); // Code set 3 prefix
						written ++;
					}
					else if (m_cur_charset != JIS_0212)
					{
						if ((read > 0) && (written + 6 > maxlen))
						{
							// Don't split escape code
							goto leave;
						}
						*(output ++) = 0x1B;
						*(output ++) = '$';
						*(output ++) = '(';
						*(output ++) = 'D';
						written += 4;
						m_cur_charset = JIS_0212;
						is_jis0212 = TRUE;
					}
				}
			}

redo:
			if (rowcell[0] != 0 && rowcell[1] != 0)
			{
				if ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding)
				    && (!is_jis0212 && m_cur_charset != JIS_0208))
				{
					if ((read > 0) && (written + 5 > maxlen))
					{
						// Don't split escape code
						goto leave;
					}
					*(output ++) = 0x1B;
					*(output ++) = '$';
					*(output ++) = 'B';
					written += 3;
					m_cur_charset = JIS_0208;
				}
				else if (written + 2 > maxlen)
				{
					// Don't split character
					goto leave;
				}

				output[0] = rowcell[0];
				output[1] = rowcell[1];

				// Adjust for EUC-JP or Shift-JIS
				switch (m_my_encoding)
				{
				case SHIFT_JIS:
					jis2sjis((unsigned char *) output);
					break;

				case EUC_JP:
					output[0] |= 0x80;
					output[1] |= 0x80;
					break;

				default:
					break;
				}

				// Advance pointers and counters
				written += 2;
				output += 2;
			}
			else if (rowcell[0] == 0 && rowcell[1] != 0) 
			{
				// this means we're outputting a single byte. Used for
				// half-width katakana in Shift-JIS and for JIS-Roman
				// specials (see next if clause).
				*(output ++) = rowcell[1];
				written++;
			}
			else if (*input == 0xA5 || *input == 0x203E)
			{
				// Bug#214734: The narrow Yen character and overline
				// characters are only available in the JIS-Roman
				// character set.
				
				switch (m_my_encoding)
				{
				case ISO_2022_JP:
				case ISO_2022_JP_1:
					// ISO 2022-JP does support JIS-Roman through a
					// character set switch.
					if (m_cur_charset != JIS_ROMAN)
					{
						if (written + 4 > maxlen)
						{
							// Don't split escape code
							goto leave;
						}
						*(output ++) = 0x1B;
						*(output ++) = '(';
						*(output ++) = 'J';
						written += 3;
						m_cur_charset = JIS_ROMAN;
					}
					/* Fall through */
				case SHIFT_JIS:
					// Shift-JIS is normally JIS-Roman based, and other
					// browsers do output these as single-byte codes.
					rowcell[1] = static_cast<char>((*input == 0xA5) ? 0x5C : 0x7E);
					break;

				case EUC_JP:
					// EUC-JP is never JIS-Roman based, so convert these
					// to their full-width representations.
					// U+FFE5 FULLWIDTH YEN SIGN - JIS+216F
					// U+FFE3 FULLWIDTH MACRON - JIS+2131
					rowcell[0] = 0x21;
					rowcell[1] = static_cast<char>((*input == 0xA5) ? 0x6F : 0x31);
					break;
				}

				// Output
				goto redo;
			}
			else // Undefined code-point
			{
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				// Must switch to ASCII mode to store the entity
				if ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding)
					&& m_entity_encoding && ASCII != m_cur_charset)
				{
					if (written + 4 > maxlen)
					{
						// Don't split escape code
						goto leave;
					}
					int extra = ReturnToInitialState(output);
					written += extra;
					output += extra;
				}
				static const char iso2022jp_not_a_character[] =
					{ NOT_A_CHARACTER_JIS_KU, NOT_A_CHARACTER_JIS_TEN,  0 };
				if (!CannotRepresent(*input, read, &output, &written, maxlen,
				                     ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding) && ASCII != m_cur_charset)
					                   ? iso2022jp_not_a_character
					                   : NULL))
				{
					goto leave;
				}
#else
				CannotRepresent(*input, read);

				// When everything else fails: cry
				if ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding)
				    && ASCII != m_cur_charset)
				{
					if (written + 2 > maxlen)
					{
						// Don't split character
						goto leave;
					}

					// Don't bother switching modes
					*(output ++) = NOT_A_CHARACTER_JIS_KU;
					*(output ++) = NOT_A_CHARACTER_JIS_TEN;

					written += 2;
				}
				else
				{
					// Don't waste an ideographic character
					*(output ++) = NOT_A_CHARACTER_ASCII;
					written ++;
				}
#endif
			}
		}

		read ++; // Counting UTF-16 characters, not bytes
		input ++;
	}

	// In ISO-2022-JP, we want to switch back to ASCII at end of line,
	// so if we have read all the input bytes and have space left in the
	// output buffer, we switch back.
	if (read == utf16len && written + 3 <= maxlen)
	{
		written += ReturnToInitialState(output);
		// output pointer is now invalid
	}

leave:
	*read_p = read * 2; // Counting bytes, not UTF-16 characters
	m_num_converted += read;
	return written;
}	

int UTF16toJISConverter::ReturnToInitialState(void *dest)
{
	if ((ISO_2022_JP == m_my_encoding || ISO_2022_JP_1 == m_my_encoding)
	    && ASCII != m_cur_charset)
	{
		// ISO-2022-JP must be switched into ASCII mode
		if (dest)
		{
			char *output = reinterpret_cast<char *>(dest);
			*(output ++) = 0x1B;
			*(output ++) = '(';
			*(output ++) = 'B';
			m_cur_charset = ASCII;
		}
		return 3;
	}

	// In all other cases, there's nothing to be done
	return 0;
}

void UTF16toJISConverter::Reset()
{
	this->OutputConverter::Reset();
	m_cur_charset = ASCII;
}

int UTF16toJISConverter::LongestSelfContainedSequenceForCharacter()
{
	switch (m_my_encoding)
	{
	case SHIFT_JIS:
		// Longest sequence is two bytes
		return 2;

	case EUC_JP:
		// Longest sequence is one code JIS 0212 character
		// Indicate code set 3:  1 byte
		// Output one character: 2 bytes
		return 3;

	case ISO_2022_JP:
		switch (m_cur_charset)
		{
		case ASCII:
		case JIS_ROMAN:
			// Worst case: One character in JIS 0208 charset:
			// Switch to codeset:    3 bytes
			// Output one character: 2 bytes
			// Switch to ASCII:      3 bytes
			return 3 + 2 + 3;

		case JIS_0208:
		default:
			// Worst case: One character in JIS 0208 charset:
			// Output one character: 2 bytes
			// Switch to ASCII:      3 bytes
			return 2 + 3;

			// ASCII character requires 3 + 1 bytes
		}

	case ISO_2022_JP_1:
	default:
		switch (m_cur_charset)
		{
		case ASCII:
		case JIS_0208:
		case JIS_ROMAN:
			// Worst case: One character in JIS 0212 charset:
			// Switch to codeset:    4 bytes
			// Output one character: 2 bytes
			// Switch to ASCII:      3 bytes
			return 4 + 2 + 3;

		case JIS_0212:
		default:
			// Worst case: One character in JIS 0208 charset:
			// Switch to codeset:    3 bytes
			// Output one character: 2 bytes
			// Switch to ASCII:      3 bytes
			return 3 + 2 + 3;

			// ASCII character requires 3 + 1 bytes
		}
	}
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_JAPANESE
