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
#include "modules/encodings/decoders/gbk-decoder.h"

GBKtoUTF16Converter::GBKtoUTF16Converter(gb_variant variant)
	: m_gbk_table(NULL), m_18030_table(NULL),
	  m_first_byte(0), m_second_byte(0), m_third_byte(0),
	  m_surrogate(0),
	  m_gbk_length(0), m_18030_length(0),
	  m_variant(variant)
{
}

OP_STATUS GBKtoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	m_gbk_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("gbk-table", m_gbk_length));
	m_gbk_length /= 2; // byte count to UINT16 count

#ifdef USE_ICU_DATA
	if (GB18030 == m_variant)
	{
		m_18030_table =
			reinterpret_cast<const UINT16 *>(g_table_manager->Get("gb18030-table", m_18030_length));
		m_18030_length /= 2 * 2; // byte count to UINT16 pair count
	}
#endif

	return m_gbk_table && (m_variant != GB18030 || m_18030_table) ? OpStatus::OK : OpStatus::ERR;
}

GBKtoUTF16Converter::~GBKtoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_gbk_table);
		if (GB18030 == m_variant)
		{
			g_table_manager->Release(m_18030_table);
		}
	}
}

int GBKtoUTF16Converter::Convert(const void *_src, int len, void *dest,
		   					       int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *src = reinterpret_cast<const unsigned char *>(_src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	if (m_surrogate)
	{
		*(output ++) = m_surrogate;
		written += sizeof (uni_char);
		m_surrogate = 0;
	}

	while (read < len && written < maxlen)
	{
		BOOL valid = FALSE;
		BOOL consume = TRUE;

		if (m_third_byte)
		{
			if (*src >= 0x30 && *src <= 0x39)
			{
				/* Valid trail byte of four-byte sequence in GB 18030.
				 *
				 * Calculate a linear sequence point inside the valid
				 * range of four-byte sequences [81,30,81,30] --
				 * [FE,39,FE,39] (represented as 0..1587599).
				 */
				unsigned int seqpoint =
					(((m_first_byte - 0x81) * 10 +
					  (m_second_byte - 0x30)) * 126 +
					 (m_third_byte - 0x81)) * 10 + *src - 0x30;

				/* [81308130] -- [8431A439] ([0..39419]) is used to encode
				 * the remaining BMP codepoints. */
				if (seqpoint <= 39419 && m_18030_table)
				{
					valid = TRUE;

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
					while (entries && ENCDATA16(*(p + 1)) <= seqpoint)
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

					// Convert sequence point to Unicode value
					uni_char cp = seqpoint - boundary + unicode;

					(*output ++) = cp;
					written += sizeof (uni_char);
				}
				/* [90,30,81,30] -- [E3,32,9A,35] (represented as
				 * 189000..1237575) is a linear encoding of Unicode planes
				 * 1 - 15 (U+10000 - U+10FFFF)
				 */
				else if (seqpoint >= 189000 && seqpoint <= 1237575)
				{
					valid = TRUE;

					/* Outside UTF-16 single codepoint area, create
					 * surrogate code units.
					 */
					UnicodePoint ucs4char = seqpoint - 189000 + 0x10000;
					uni_char high, low;
					Unicode::MakeSurrogate(ucs4char, high, low);

					(*output ++) = high;
					written += sizeof (uni_char);

					if (written == maxlen)
					{
						m_surrogate = low;
					}
					else
					{
						(*output ++) = low;
						written += sizeof (uni_char);
					}
				}
			}
			else
			{
				// Fourth byte was illegal, do not consume it
				consume = FALSE;
			}

			m_first_byte = m_second_byte = m_third_byte = 0;
		}
		else if (m_second_byte)
		{
			if (*src >= 0x81 && *src <= 0xFE)
			{
				// Valid third byte of four-byte sequence in GB 18030
				valid = TRUE;
				m_third_byte = *src;
			}
			else
			{
				// Invalid sequence; forget about prior bytes
				consume = FALSE;
				m_first_byte = m_second_byte = 0;
			}
		}
		else if (m_first_byte)
		{
			if (*src >= 0x30 && *src <= 0x39 && GB18030 == m_variant)
			{
				// Valid second byte of four-byte sequence in GB 18030
				valid = TRUE;
				m_second_byte = *src;
			}
			else
			{
				if (*src >= 0x40 && *src <= 0xFE)
				{
					int codepoint = (m_first_byte - 0x81) * 191 + (*src - 0x40);
					if (codepoint < m_gbk_length)
					{
						// Valid trail byte of two-byte sequence in GB2312/GBK/GB18030
						valid = TRUE;
						(*output++) = ENCDATA16(m_gbk_table[codepoint]);
						written += 2;
					}
				}
				else
				{
					// Second byte was illegal, do not consume it
					consume = FALSE;
				}

				m_first_byte = 0;
			}
		}
		else
		{
			if (*src <= 0x7F)
			{
				valid = TRUE;
				(*output++) = *src;
				written += 2;
			}
			else if (*src >= 0x81 && *src <= 0xFE)
			{
				// Valid lead byte, store for next iteration
				valid = TRUE;
				m_first_byte = *src;
			}
			else if (*src == 0x80 && GBK == m_variant)
			{
				/* Microsoft GBK puts a Euro sign at single-byte 80, contrary
				 * to all logic. GB18030 removes this mapping, and introduces
				 * it at A2A3. Since we use GB18030 based tables four output,
				 * this conversion is not round-trip safe.
				 */
				valid = TRUE;
				(*output++) = 0x20AC;
				written += 2;
			}
		}

		if (consume)
		{
			src ++;
			read ++;
		}

		if (!valid)
		{
			// Invalid char (or no tables for area)
			written += HandleInvalidChar(written, &output);
		}
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *GBKtoUTF16Converter::GetCharacterSet()
{
	switch (m_variant)
	{
	case GBK:
		return "gbk";

	case GB18030:
	default: // just in case
		return "gb18030";
	}
}

void GBKtoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_first_byte  = 0;
	m_second_byte = 0;
	m_third_byte  = 0;
	m_surrogate   = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
