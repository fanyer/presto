/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && (defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_KOREAN)

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/hz-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

UTF16toDBCS7bitConverter::UTF16toDBCS7bitConverter(enum encoding outputencoding)
	: m_maptable1(NULL), m_maptable2(NULL),
	  m_table1start(0), m_table1top(0), m_table2len(0),
	  m_cur_charset(ASCII),
	  m_my_encoding(outputencoding),
#ifdef ENCODINGS_HAVE_KOREAN
	  m_iso2022_initialized(FALSE),
#endif
	  m_switch_sequence_length(0)
{
}

OP_STATUS UTF16toDBCS7bitConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long table1len;
	const char *table1 = NULL, *table2 = NULL;
	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case HZGB2312:
		table1 = "gbk-rev-table-1";
		table2 = "gbk-rev-table-2";
# ifdef ENCODINGS_HAVE_KOREAN
		m_iso2022_initialized = TRUE; // Doesn't require initialization
# endif
		m_switch_sequence_length = 2;
		m_table1start = 0x4E00;
		break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		table1 = "ksc5601-rev-table-1";
		table2 = "ksc5601-rev-table-2";
		m_switch_sequence_length = 1;
		m_table1start = 0xAC00;
		break;
#endif
	}

	m_maptable1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get(table1, table1len));
	m_maptable2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get(table2, m_table2len));

	// Determine which characters are supported by the tables
	m_table1top = m_table1start + table1len / 2;

	return m_maptable1 && m_maptable2 ? OpStatus::OK : OpStatus::ERR;
}

UTF16toDBCS7bitConverter::~UTF16toDBCS7bitConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_maptable1);
		g_table_manager->Release(m_maptable2);
	}
}

// Full-width character to use for undefined code points
// Full-width question mark (HZ)
#define NOT_A_CHARACTER_HZ_KU		0x23
#define NOT_A_CHARACTER_HZ_TEN		0x3F

// Full-width question mark (ISO-2022-KR)
#define NOT_A_CHARACTER_ISO2022KR_KU		0x23
#define NOT_A_CHARACTER_ISO2022KR_TEN		0x3F

// Switch sequences for ISO-2022-KR
#define ISO2022KR_SBCS 0x0F /* SI: Switch to ASCII mode */
#define ISO2022KR_DBCS 0x0E /* SO: Switch to DBCS mode */

int UTF16toDBCS7bitConverter::Convert(const void *src, int len, void *dest,
                                      int maxlen, int *read_p)
{
	int read = 0;
	int written = 0;
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	int utf16len = len / 2;

	char *output = reinterpret_cast<char *>(dest);

#ifdef ENCODINGS_HAVE_KOREAN
	if (!m_iso2022_initialized)
	{
		// Need to output the designator sequence first of all
		OP_ASSERT(ISO2022KR == m_my_encoding);
		if (maxlen < 4)
		{
			// Doesn't fit -- give up
			*read_p = 0;
			return 0;
		}
		*(output ++) = 0x1B;
		*(output ++) = '$';
		*(output ++) = ')';
		*(output ++) = 'C';
		written += 4;
		m_iso2022_initialized = TRUE;
	}
#endif

	while (read < utf16len && written < maxlen)
	{
		if (*input < 128)
		{
			// ASCII
			if (m_cur_charset != ASCII)
			{
				if (written + 1 + m_switch_sequence_length > maxlen)
				{
					// Don't split escape code
					goto leave;
				}
				int extra = ReturnToInitialState(output);
				output += extra;
				written += extra;
			}
#ifdef ENCODINGS_HAVE_CHINESE
			if ('~' == *input && HZGB2312 == m_my_encoding)
			{
				if (written + 2 > maxlen)
				{
					// Don't split escape code
					goto leave;
				}
				*(output ++) = '~';
				written ++;
			}
#endif
			*(output ++) = static_cast<char>(*input);
			written ++;
		}
		else
		{
			char rowcell[2]; /* ARRAY OK 2009-01-27 johanh */
			if (*input >= m_table1start && *input < m_table1top)
			{
				rowcell[1] = m_maptable1[(*input - m_table1start) * 2];
				rowcell[0] = m_maptable1[(*input - m_table1start) * 2 + 1];
			}
			else
			{
				lookup_dbcs_table(m_maptable2, m_table2len, *input, rowcell);
			}

			// Since we're using a extended EUC tables (GBK/EUC-KR), we have
			// codepoints outside the 7-bit DBCS encoding ranges. We treat
			// those as undefined.
			if ((unsigned char) rowcell[0] > 0xA0 &&
			    (unsigned char) rowcell[0] < 0xFE &&
			    (unsigned char) rowcell[1] > 0xA0)
			{
				// This codepoint is valid in 7-bit DBCS (but might be
				// undefined in out encoding, but we ignore that case)

				if (m_cur_charset != DBCS)
				{
					if (written + m_switch_sequence_length + 2 > maxlen)
					{
						// Don't split escape code
						goto leave;
					}

					switch (m_my_encoding)
					{
#ifdef ENCODINGS_HAVE_CHINESE
					case HZGB2312:
						*(output ++) = '~';
						*(output ++) = '{';
						written += 2;
						break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
					case ISO2022KR:
						*(output ++) = ISO2022KR_DBCS;
						written ++;
						break;
#endif
					}

					m_cur_charset = DBCS;
				}
				else if (written + 2 > maxlen)
				{
					// Don't split character
					goto leave;
				}

				// Our table is in EUC encoding, so we strip the high-bit
				output[0] = rowcell[0] & 0x7F;
				output[1] = rowcell[1] & 0x7F;

				// Advance pointers and counters
				written += 2;
				output += 2;
			}
			else // Undefined code-point
			{
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				// Must switch to ASCII mode to store the entity
				if (m_entity_encoding && ASCII != m_cur_charset)
				{
					if (written + m_switch_sequence_length > maxlen)
					{
						// Don't split escape code
						goto leave;
					}
					int extra = ReturnToInitialState(output);
					written += extra;
					output += extra;
				}

				// String representing an unknown character in DBCS encoding.
				// Exploit the fact that the fullwidth question mark is
				// encoded identical the same way in GB2312 and KSC5601.
				OP_ASSERT(NOT_A_CHARACTER_HZ_KU  == NOT_A_CHARACTER_ISO2022KR_KU);
				OP_ASSERT(NOT_A_CHARACTER_HZ_TEN == NOT_A_CHARACTER_ISO2022KR_TEN);
				static const char hz_not_a_character[] =
					{ NOT_A_CHARACTER_HZ_KU, NOT_A_CHARACTER_HZ_TEN,  0 };

				if (!CannotRepresent(*input, read, &output, &written, maxlen,
				                     ASCII == m_cur_charset ? NULL : hz_not_a_character))
				{
					goto leave;
				}
#else
				CannotRepresent(*input, read);

				// When everything else fails: cry
				if (ASCII != m_cur_charset)
				{
					if (written + 2 > maxlen)
					{
						// Don't split character
						goto leave;
					}

					// Don't bother switching modes; exploit the fact that the
					// fullwidth question mark has the same encoding in our two
					// target encodings
					OP_ASSERT(NOT_A_CHARACTER_HZ_KU  == NOT_A_CHARACTER_ISO2022KR_KU);
					OP_ASSERT(NOT_A_CHARACTER_HZ_TEN == NOT_A_CHARACTER_ISO2022KR_TEN);
					*(output ++) = NOT_A_CHARACTER_HZ_KU;
					*(output ++) = NOT_A_CHARACTER_HZ_TEN;

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

	// In these encodings we want to switch back to ASCII at end of line,
	// so if we have read all the input bytes and have space left in the
	// output buffer, we switch back. For HZ, we do a "hard" return, but
	// for ISO-2022-KR we only output a SI
	switch (m_my_encoding)
	{
#ifdef ENCODINGS_HAVE_CHINESE
	case HZGB2312:
		if (read == utf16len && written + 2 <= maxlen)
		{
			written += ReturnToInitialState(output);
			// output pointer is now invalid
		}
		break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		if (read == utf16len && ASCII != m_cur_charset && written < maxlen)
		{
			*(output ++) = ISO2022KR_SBCS;
			m_cur_charset = ASCII;
			written ++;
		}
		break;
#endif
	}

leave:
	*read_p = read * 2; // Counting bytes, not UTF-16 characters
	m_num_converted += read;
	return written;
}	

const char *UTF16toDBCS7bitConverter::GetDestinationCharacterSet()
{
	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case HZGB2312:
		return "hz-gb-2312";
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		return "iso-2022-kr";
#endif
	}
}

int UTF16toDBCS7bitConverter::ReturnToInitialState(void *dest)
{
	switch (m_cur_charset)
	{
	case ASCII:
		return 0;

	case DBCS:
	default:
		switch (m_my_encoding)
		{
		default:
#ifdef ENCODINGS_HAVE_CHINESE
		case HZGB2312:
			if (dest)
			{
				char *output = reinterpret_cast<char *>(dest);
				*(output ++) = '~';
				*(output ++) = '}';
				m_cur_charset = ASCII;
			}
			return 2;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
		case ISO2022KR:
			if (dest)
			{
				char *output = reinterpret_cast<char *>(dest);
				*output = ISO2022KR_SBCS;
				m_cur_charset = ASCII;
				m_iso2022_initialized = FALSE;
			}
			return 1;
#endif
		}
	}
}

void UTF16toDBCS7bitConverter::Reset()
{
	this->OutputConverter::Reset();
	m_cur_charset = ASCII;
#ifdef ENCODINGS_HAVE_KOREAN
	m_iso2022_initialized =
# ifdef ENCODINGS_HAVE_CHINESE
		(m_my_encoding == HZGB2312)
			? TRUE
			:
# endif
			  FALSE;
#endif
}

int UTF16toDBCS7bitConverter::LongestSelfContainedSequenceForCharacter()
{
	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case HZGB2312:
		switch (m_cur_charset)
		{
		case ASCII:
		default:
			// Worst case: One GB2312 character
			// Switch to GB2312:     2 bytes
			// Output one character: 2 bytes
			// Switch to ASCII:      2 bytes
			return 2 + 2 + 2;

		case DBCS:
			// Worst case: One GB2312 character
			// Output one character: 2 bytes
			// Switch to ASCII:      2 bytes
			return 2 + 2;
		}
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case ISO2022KR:
		if (!m_iso2022_initialized)
		{
			// Worst case: Designator plus one KSC5601 character
			// Designator:           4 bytes
			// Switch to KSC5601:    1 byte
			// Output one character: 2 bytes
			// Switch to ASCII:      1 byte
			return 4 + 1 + 2 + 1;
		}

		switch (m_cur_charset)
		{
		case ASCII:
		default:
			// Worst case: One KSC5601 character
			// Switch to KSC5601:    1 byte
			// Output one character: 2 bytes
			// Switch to ASCII:      1 byte
			return 1 + 2 + 1;

		case DBCS:
			// Worst case: One KSC5601 character
			// Output one character: 2 bytes
			// Switch to ASCII:      1 byte
			return 2 + 1;
		}
#endif
	}
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
