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
#include "modules/encodings/encoders/big5hkscs-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

UTF16toBig5HKSCSConverter::UTF16toBig5HKSCSConverter()
	: m_big5table1(NULL), m_big5table2(NULL),
	  m_hkscs_plane0(NULL), m_hkscs_plane2(NULL),
	  m_table1top(0), m_table2len(0), m_plane0len(0), m_plane2len(0),
	  m_surrogate(0)
{
}

OP_STATUS UTF16toBig5HKSCSConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long table1len;

#ifdef USE_MOZTW_DATA
	m_big5table1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("big5-rev-table-1", table1len));
	m_big5table2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("big5-rev-table-2", m_table2len));
#endif
#ifdef USE_HKSCS_DATA
	m_hkscs_plane0 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("hkscs-plane-0", m_plane0len));
	m_hkscs_plane2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("hkscs-plane-2", m_plane2len));
#endif

	// Determine which characters are supported by the tables
	m_table1top = 0x4E00 + table1len / 2;

	return m_big5table1 && m_big5table2 && m_hkscs_plane0 && m_hkscs_plane2 ? OpStatus::OK : OpStatus::ERR;
}

UTF16toBig5HKSCSConverter::~UTF16toBig5HKSCSConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_big5table1);
		g_table_manager->Release(m_big5table2);
		g_table_manager->Release(m_hkscs_plane0);
		g_table_manager->Release(m_hkscs_plane2);
	}
}

int UTF16toBig5HKSCSConverter::Convert(const void* src, int len, void* dest,
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
			*(output ++) = char(*input);
			written ++;
		}
		else
		{
			char rowcell[2] = { 0, 0 };

			if (m_surrogate || Unicode::IsLowSurrogate(*input))
			{
				UINT32 ucs4char = static_cast<UINT32>(-1);
				if (m_surrogate && Unicode::IsLowSurrogate(*input))
				{
					// Second half of a surrogate
					// Calculate UCS-4 character
					ucs4char = Unicode::DecodeSurrogate(m_surrogate, *input);
					if (ucs4char >= 0x20000 && ucs4char <= 0x2FFFF)
					{
						lookup_dbcs_table(m_hkscs_plane2, m_plane2len,
						                  static_cast<UINT16>(ucs4char - 0x20000),
						                  rowcell);
					}
				}

				if (!rowcell[0] && !rowcell[1])
				{
					// Unrecognized or invalid character
					if (ucs4char != static_cast<UINT32>(-1))
					{
						// Valid but unknown, record the first half of the
						// surrogate pair as invalid, the second half will
						// be recorded below
						CannotRepresent(m_surrogate, read);
					}
					m_surrogate = 0;
				}
			}
			else if (Unicode::IsHighSurrogate(*input))
			{
				// High surrogate area, should be followed by a low surrogate
				m_surrogate = *input;
			}
			else
			{
				// Check for HKSCS codepoint
				lookup_dbcs_table(m_hkscs_plane0, m_plane0len, *input, rowcell);

				if (0 == rowcell[0])
				{
					// Check for standard Big5 codepoint
					if (*input >= 0x4E00 && *input < m_table1top)
					{
						rowcell[1] = m_big5table1[(*input - 0x4E00) * 2];
						rowcell[0] = m_big5table1[(*input - 0x4E00) * 2 + 1];
					}
					else
					{
						lookup_dbcs_table(m_big5table2, m_table2len, *input, rowcell);
					}
				}
				m_surrogate = 0;
			}

			if (rowcell[0] != 0 && rowcell[1] != 0)
			{
				if (written + 2 > maxlen)
				{
					// Don't split character
					break;
				}
				m_surrogate = 0;

				output[0] = rowcell[0];
				output[1] = rowcell[1];
				written += 2;
				output += 2;
			}
			else if (!m_surrogate)
			{
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
				if (!CannotRepresent(*input, read, &output, &written, maxlen))
					break;
#else
				*(output ++) = NOT_A_CHARACTER_ASCII;
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

const char *UTF16toBig5HKSCSConverter::GetDestinationCharacterSet()
{
	return "big5-hkscs";
}

void UTF16toBig5HKSCSConverter::Reset()
{
	this->OutputConverter::Reset();
	m_surrogate = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
