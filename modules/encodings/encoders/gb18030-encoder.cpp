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
#include "modules/encodings/encoders/gb18030-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

UTF16toGB18030Converter::UTF16toGB18030Converter()
	: m_gbktable1(NULL), m_gbktable2(NULL),
	  m_18030_table(NULL),
	  m_table1top(0), m_table2len(0), m_18030_length(0),
	  m_surrogate(0)
{
}

OP_STATUS UTF16toGB18030Converter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long table1len;

	m_gbktable1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("gbk-rev-table-1", table1len));
	m_gbktable2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("gbk-rev-table-2", m_table2len));

#ifdef USE_ICU_DATA
	m_18030_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("gb18030-table", m_18030_length));
#endif

	// Determine which characters are supported by the tables
	m_table1top = 0x4E00 + table1len / 2;

	m_18030_length /= 2 * 2; // byte count to UINT16 pair count

	return m_gbktable1 && m_gbktable2 && m_18030_table ? OpStatus::OK : OpStatus::ERR;
}

UTF16toGB18030Converter::~UTF16toGB18030Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_gbktable1);
		g_table_manager->Release(m_gbktable2);
		g_table_manager->Release(m_18030_table);
	}
}

int UTF16toGB18030Converter::Convert(const void *src, int len, void *dest,
                                  int maxlen, int* read_p)
{
	int read = 0;
	int written = 0;
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	int utf16len = len / 2;

	char *output = reinterpret_cast<char *>(dest);

	while (read < utf16len && written < maxlen)
	{
		if (*input < 128)
		{
			// ASCII
			*(output ++) = static_cast<char>(*input);
			written ++;
		}
		else
		{
			char rowcell[2] = { 0, 0 };
			int seqpoint = -1;
			BOOL is_invalid = FALSE;
			UINT16 was_surrogate = 0;

			if (m_surrogate || Unicode::IsLowSurrogate(*input))
			{
				if (m_surrogate && Unicode::IsLowSurrogate(*input))
				{
					/* Second half of a surrogate. Calculate Unicode codepoint. */
					UnicodePoint ucs4char = Unicode::DecodeSurrogate(m_surrogate, *input);

					/* [90,30,81,30] -- [E3,32,9A,35] (represented as
					 * 189000..1237575) is a linear encoding of Unicode
					 * planes 1 - 15 (U+10000 - U+10FFFF)
					 */
					seqpoint = ucs4char - 0x10000 + 189000;
				}
				else
				{
					is_invalid = TRUE;
				}
				was_surrogate = m_surrogate;
				m_surrogate = 0;
			}
			else if (Unicode::IsHighSurrogate(*input))
			{
				// High surrogate area, should be followed by a low surrogate
				m_surrogate = *input;
			}
			else
			{
				// Check for standard GBK codepoint
				if (*input >= 0x4E00 && *input < m_table1top)
				{
					rowcell[1] = m_gbktable1[(*input - 0x4E00) * 2];
					rowcell[0] = m_gbktable1[(*input - 0x4E00) * 2 + 1];
				}
				else
				{
					lookup_dbcs_table(m_gbktable2, m_table2len, *input, rowcell);
				}

				m_surrogate = 0;
			}

			if (rowcell[0] != 0 && rowcell[1] != 0)
			{
				if (written + 2 > maxlen)
				{
					// Don't split character
					m_surrogate = was_surrogate; // Reset
					break;
				}

				output[0] = rowcell[0];
				output[1] = rowcell[1];
				written += 2;
				output += 2;
			}
			else if (!m_surrogate && !is_invalid)
			{
				if (written + 4 > maxlen)
				{
					// Don't split character
					m_surrogate = was_surrogate; // Reset
					break;
				}

				/* Find the linear sequence point inside the valid
				 * range of four-byte sequences [81,30,81,30] --
				 * [FE,39,FE,39] (represented as 0..1587599), unless
				 * we calculated one above (for non-BMP characters)
				 */
				if (-1 == seqpoint && m_18030_table &&
				    (*input < 0xE000 || *input >= 0xF900 /* ignore PUA */))
				{
					/* Get Unicode offset from table. The table contains pairs
					 * (codepoint, seqpoint) where "codepoint" is a Unicode
					 * codepoint and "seqpoint" is the sequence point as
					 * calculated above from which there exists a linear growing
					 * interval. There are 206 such intervals in the BMP area,
					 * which maps to the range [81,30,81,30] -- [84,31,A4,39]
					 * (represented as 0..39419).
					 */
					const UINT16 *p = m_18030_table;
					long entries = m_18030_length;
					uni_char boundary = 0, unicode = 0;
					while (entries && ENCDATA16(*p) <= *input)
					{
						/* Iterate until we find the lower bound for this
						 * sequence point.
						 */
						unicode = *(p ++); // First Unicode codepoint
						boundary= *(p ++); // Sequence point
#ifdef ENCODINGS_OPPOSITE_ENDIAN
						unicode = ENCDATA16(unicode);
						boundary= ENCDATA16(boundary);
#endif
						entries --;
					}

					// Convert Unicode value to sequence point
					seqpoint = *input - unicode + boundary;
				}

				if (seqpoint != -1)
				{
					/* Convert sequence point to a valid four-byte sequence
					 * inside the interval [81,30,81,30] -- [FE,39,FE,39].
					 * Note that only the ranges [81,30,81,30] -- [84,31,A4,39]
					 * (for the parts of BMP not mapped by GBK) and
					 * [90,30,81,30] -- [E3,32,9A,35] (non-BMP) are actually
					 * used.
					 */
					unsigned char b1 = seqpoint / 10 / 126 / 10;
					seqpoint -= b1 * 10 * 126 * 10;
					unsigned char b2 = seqpoint / 10 / 126;
					seqpoint -= b2 * 10 * 126;
					unsigned char b3 = seqpoint / 10;
					unsigned char b4 = seqpoint - b3 * 10;

					*(output ++) = b1 + 0x81;
					*(output ++) = b2 + 0x30;
					*(output ++) = b3 + 0x81;
					*(output ++) = b4 + 0x30;
					written += 4;
				}
				else
				{
					// Not much to do without the table
					is_invalid = TRUE;
				}
			}

			if (is_invalid)
			{
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				if (!CannotRepresent(*input, read, &output, &written, maxlen))
					break;
#else
				*(output ++) = '?';
				written ++;
				CannotRepresent(*input, read);
#endif
			}
		}

		read ++; // Counting UTF-16 characters, not bytes
		input ++;
	}

	*read_p = read * 2; // Counting bytes, not UTF-16 characters
	m_num_converted += read;
	return written;
}

const char *UTF16toGB18030Converter::GetDestinationCharacterSet()
{
	return "gb18030";
}

void UTF16toGB18030Converter::Reset()
{
	this->OutputConverter::Reset();
	m_surrogate = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
