/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/encodings/encoders/euc-tw-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

UTF16toEUCTWConverter::UTF16toEUCTWConverter()
	: m_maptable1(NULL), m_maptable2(NULL),
	  m_table1top(0), m_table2len(0)
{
}

OP_STATUS UTF16toEUCTWConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long table1len;

	m_maptable1 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("cns11643-rev-table-1", table1len));
	m_maptable2 =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("cns11643-rev-table-2", m_table2len));

	// Determine which characters are supported by the tables
	m_table1top = 0x4E00 + table1len / 2;

	return m_maptable1 && m_maptable2 ? OpStatus::OK : OpStatus::ERR;
}

UTF16toEUCTWConverter::~UTF16toEUCTWConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_maptable1);
		g_table_manager->Release(m_maptable2);
	}
}

int UTF16toEUCTWConverter::Convert(const void* src, int len, void* dest,
                                   int maxlen, int* read_p)
{
	int read = 0;
	int written = 0;
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	int utf16len = len / 2;

	char *output = static_cast<char *>(dest);

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
			char dbcsdata[2]; /* ARRAY OK 2009-01-27 johanh */
			if (*input >= 0x4E00 && *input < m_table1top)
			{
				dbcsdata[1] = m_maptable1[(*input - 0x4E00) * 2];
				dbcsdata[0] = m_maptable1[(*input - 0x4E00) * 2 + 1];
			}
			else
			{
				lookup_dbcs_table(m_maptable2, m_table2len, *input, dbcsdata);
			}

			if (dbcsdata[0] != 0 && dbcsdata[1] != 0)
			{
				// Convert into plane, row and cell data:
				// What we have is a little-endian number where the two
				// most significant bits are the plane (1 = plane 1, 2 =
				// plane 2, 3 = plane 14), the next seven bits is the row
				// and the seven least significant bits are the cell.
				// Since the row/cell is EUC encoded, we set their high
				// bits;
				unsigned int value = (static_cast<unsigned char>(dbcsdata[0]) << 8) |
				                      static_cast<unsigned char>(dbcsdata[1]);
				unsigned int plane = value >> 14;
				unsigned int row = ((value >> 7) & (0x7F)) | 0x80;
				unsigned int cell = (value & 0x7F) | 0x80;

				if (plane != 1)
				{
					// Four byte (EUC code set 2)
					if (written + 4 > maxlen)
					{
						// Don't split character
						*read_p = read * 2; // Counting bytes, not UTF-16 characters
						m_num_converted += read;
						return written;
					}

					output[0] = static_cast<char>(static_cast<unsigned char>(0x8E)); // SS2
					output[1] = static_cast<char>(static_cast<unsigned char>(0xA0 | (3 == plane ? 14 : plane)));
					output[2] = static_cast<char>(row);
					output[3] = static_cast<char>(cell);
					written += 4;
					output += 4;
				}
				else
				{
					// Two byte (EUC code set 1)

					if (written + 2 > maxlen)
					{
						// Don't split character
						break;
					}

					output[0] = static_cast<char>(row);
					output[1] = static_cast<char>(cell);
					written += 2;
					output += 2;
				}
			}
			else // Undefined code-point
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

const char *UTF16toEUCTWConverter::GetDestinationCharacterSet()
{
	return "euc-tw";
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
