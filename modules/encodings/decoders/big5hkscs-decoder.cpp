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
#include "modules/encodings/decoders/big5hkscs-decoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

Big5HKSCStoUTF16Converter::Big5HKSCStoUTF16Converter()
	: m_prev_byte(0),
	  m_big5table(NULL),
	  m_big5table_length(0),
	  m_surrogate(0),
#ifndef ENC_BIG_HKSCS_TABLE
	  m_hkscs_plane0(NULL),     m_hkscs_plane2(NULL),
	  m_hkscs_plane0_length(0), m_hkscs_plane2_length(0),
#endif
	m_compat_table(NULL),
	m_compat_table_length(0)
{
}

OP_STATUS Big5HKSCStoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

#ifdef USE_MOZTW_DATA
	m_big5table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("big5-table", m_big5table_length));
	m_big5table_length /= 2; // byte count to ushort count
#endif

#ifdef USE_HKSCS_DATA
	m_compat_table =
		reinterpret_cast<const unsigned char *>(g_table_manager->Get("hkscs-compat", m_compat_table_length));

#ifndef ENC_BIG_HKSCS_TABLE
	m_hkscs_plane0 =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("hkscs-plane-0", m_hkscs_plane0_length));
	m_hkscs_plane0_length /= 2; // byte count to ushort count
	m_hkscs_plane2 =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("hkscs-plane-2", m_hkscs_plane2_length));
	m_hkscs_plane2_length /= 2; // byte count to ushort count
#endif
#endif

	return m_big5table && m_compat_table
#ifndef ENC_BIG_HKSCS_TABLE
	    && m_hkscs_plane0 && m_hkscs_plane2
#endif
	  ? OpStatus::OK : OpStatus::ERR;
}

Big5HKSCStoUTF16Converter::~Big5HKSCStoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_big5table);
		g_table_manager->Release(m_compat_table);

#ifndef ENC_BIG_HKSCS_TABLE
		g_table_manager->Release(m_hkscs_plane0);
		g_table_manager->Release(m_hkscs_plane2);
#endif
	}
}

#ifndef ENC_BIG_HKSCS_TABLE
UINT32 Big5HKSCStoUTF16Converter::LookupHKSCS(unsigned char row, unsigned char cell)
{
	// Perform linear searches through the data tables to find the
	// corresponding Unicode codepoints.

	// Row/cell is stored with cell first (row/cell interpreted as a 32-bit
	// value, stored in little endian).
	UINT16 rowcell =
#if (defined OPERA_BIG_ENDIAN && !defined ENCODINGS_OPPOSITE_ENDIAN) || (!defined OPERA_BIG_ENDIAN && defined ENCODINGS_OPPOSITE_ENDIAN)
		(cell << 8) | row;
#else
		(row << 8) | cell;
#endif

	// Plane 0 table
	const UINT16 *current = m_hkscs_plane0;
	const UINT16 *end = m_hkscs_plane0 + m_hkscs_plane0_length;
	while (current < end)
	{
		// The table stores pairs of (unicode,hkscs) mappings
		if (current[1] == rowcell)
			return ENCDATA16(*current);
		current += 2;
	}

	// Plane 2 table
	current = m_hkscs_plane2;
	end = m_hkscs_plane2 + m_hkscs_plane2_length;
	while (current < end)
	{
		// The table stores pairs of (unicode-0x20000,hkscs) mappings
		if (current[1] == rowcell)
			return ENCDATA16(*current) + 0x20000;
		current += 2;
	}

	// Nothing found
	return 0;
}

#else // ENC_BIG_HKSCS_TABLE

#define HKSCS_TABLE_LENGTH 0x939E
long Big5HKSCStoUTF16Converter::hkscs_tableoffset(unsigned char row, unsigned char cell)
{
	// Only rows 87-A0, C6-C8 and F9-FE are used
	OP_ASSERT((row >= 0x87 && row <= 0xA0) || (row >= 0xC6 && row <= 0xC8) || (row >= 0xF9 && row <= 0xFE));
	if (row >= 0xF9)
	{ // F9..FE => A4..A9
		row -= 0x55;
	}
	else if (row >= 0xC6)
	{ // C6..C8 => A1..A3
		row -= 0x25;
	}

	// Only cells 40-7E and A1-FE are used
	OP_ASSERT((cell >= 0x40 && cell <= 0x7E) || (cell >= 0xA1 && cell <= 0xFE));
	if (cell >= 0xA1)
	{ // A1..FE => 7F..DC
		cell -= 0x22;
	}

	return (static_cast<long>(row) * 0xDD + cell) - 0x74CB; // 0000..939E
}

UINT32 *Big5HKSCStoUTF16Converter::GenerateHKSCSTable()
{
	// For "unlimited memory devices" the table is deallocated in
	// EncodingsModule::Destroy()
	UINT32 *hkscs_table =
		g_opera->encodings_module.m_hkscs_table = OP_NEWA(UINT32, HKSCS_TABLE_LENGTH);
	if (NULL == hkscs_table) return NULL;

	op_memset(hkscs_table, 0, HKSCS_TABLE_LENGTH * sizeof (UINT32));

	long hkscs_plane0_length;
	const UINT16 *hkscs_plane0 =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("hkscs-plane-0", hkscs_plane0_length));

	if (hkscs_plane0)
	{
		const UINT16 *p = hkscs_plane0;

		while (hkscs_plane0_length)
		{
			// The table stores pairs of (unicode,hkscs) mappings
			UINT16 ucs = *(p ++);
#ifdef ENCODINGS_OPPOSITE_ENDIAN
			ucs = ENCDATA16(ucs);
#endif
			unsigned char *q = (unsigned char *) p ++;
			unsigned char cell = *(q ++);   // Cell is first byte
			unsigned char row  = *q;        // Row is second byte

			long offset = hkscs_tableoffset(row, cell);
			if (offset >= 0 && offset < HKSCS_TABLE_LENGTH)
			{
				hkscs_table[offset] = ucs;
			}

			hkscs_plane0_length -= 4;
		}

		g_table_manager->Release(hkscs_plane0);
	}

	long hkscs_plane2_length;
	const UINT16 *hkscs_plane2 =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("hkscs-plane-2", hkscs_plane2_length));

	if (hkscs_plane2)
	{
		const UINT16 *p = hkscs_plane2;

		while (hkscs_plane2_length)
		{
			// The table stores pairs of (unicode - 0x20000,hkscs) mappings
			UINT16 ucs = *(p ++);
#ifdef ENCODINGS_OPPOSITE_ENDIAN
			ucs = ENCDATA16(ucs);
#endif
			unsigned char *q = (unsigned char *) p ++;
			unsigned char cell = *(q ++);   // Cell is first byte
			unsigned char row  = *q;        // Row is second byte

			long offset = hkscs_tableoffset(row, cell);
			if (offset >= 0 && offset < HKSCS_TABLE_LENGTH)
			{
				// Store the plane 2 value here, convert to surrogate only
				// when requested
				hkscs_table[offset] = ucs + 0x20000;
			}

			hkscs_plane2_length -= 4;
		}

		g_table_manager->Release(hkscs_plane2);
	}

	return hkscs_table;
}
#endif // else !ENC_BIG_HKSCS_TABLE

int Big5HKSCStoUTF16Converter::Convert(
	const void *_src, int len, void *dest, int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *src = reinterpret_cast<const unsigned char *>(_src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

#ifdef ENC_BIG_HKSCS_TABLE
	UINT32 *hkscs_table = g_opera->encodings_module.m_hkscs_table;
#endif

	if (m_surrogate)
	{
		*(output ++) = m_surrogate;
		written += sizeof (uni_char);
		m_surrogate = 0;
	}

	while (read < len && written < maxlen)
	{
		if (m_prev_byte)
		{
			if ((*src >= 0x40 && *src <= 0x7E) ||
			    (*src >= 0xA1 && *src <= 0xFE))
			{
				unsigned char this_byte = *src;
redolookup:
				// Valid trail byte
				if (m_prev_byte < 0xA1 ||
				    (m_prev_byte == 0xC6 && this_byte >= 0xA1) ||
				    m_prev_byte == 0xC7 || m_prev_byte == 0xC8 ||
				    (m_prev_byte == 0xF9 && this_byte >= 0xD6) ||
				    m_prev_byte >= 0xFA)
				{
					// HKSCS extension
#ifdef ENC_BIG_HKSCS_TABLE
					if (NULL == hkscs_table)
					{
						// First seen extended character - generate the 147K
						// mapping table
						hkscs_table = GenerateHKSCSTable();
					}
					if (hkscs_table)
					{
						// Mapping table is generated
						long codepoint = hkscs_tableoffset(m_prev_byte, this_byte);
						if (codepoint >= 0 && codepoint < HKSCS_TABLE_LENGTH)
						{
							// This should always be true, but we test just to
							// be sure
							UINT32 ucs = hkscs_table[codepoint];
#else
							// If ENC_BIG_HKSCS_TABLE is disabled, we do a linear
							// search for the codepoint. Inefficient, yes,
							// but only for the HKSCS extended characters, and
							// MUCH less memory hungry.
							UINT32 ucs = LookupHKSCS(m_prev_byte, this_byte);
#endif // ENC_BIG_HKSCS_TABLE
							if (0 == ucs)
							{
								// Check for compatibility mapping
								char newcp[2]; /* ARRAY OK 2009-01-27 johanh */
								lookup_dbcs_table(m_compat_table, m_compat_table_length,
								                  (m_prev_byte << 8) | this_byte, newcp);
								if (newcp[0] && newcp[1])
								{
									// This compatibility point is remapped.
									// So what we need to do is to redo the
									// lookup. This is the ugly way of doing
									// it:
									m_prev_byte = newcp[0];
									this_byte = newcp[1];
									goto redolookup;
								}
								written += HandleInvalidChar(written, &output);
							}
							else if (ucs < 0x10000)
							{
								(*output++) = static_cast<uni_char>(ucs);
								written += sizeof (uni_char);
							}
							else
							{
								// Outside BMP - we need to create surrogates
								uni_char high, low;
								Unicode::MakeSurrogate(ucs, high, low);
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
#ifdef ENC_BIG_HKSCS_TABLE
						}
					}
					else
					{
						// Not much we can do without the table
						(*output++) = NOT_A_CHARACTER;
						written += sizeof (uni_char);
					}
#endif // ENC_BIG_HKSCS_TABLE
				}
				else
				{
					// Standard Big5
					int codepoint = (m_prev_byte - 0xA1) * 191 + (this_byte - 0x40);
					if (codepoint < m_big5table_length)
					{
						UINT16 big5char = ENCDATA16(m_big5table[codepoint]);
						written += HandleInvalidChar(written, &output, big5char, big5char != NOT_A_CHARACTER);
					}
					else
					{
						written += HandleInvalidChar(written, &output);
					}
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
			if (*src <= 0x7F)
			{
				(*output++) = *src;
				written += sizeof (uni_char);
			}
			else if (*src >= 0x87 && *src <= 0xFE)
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

const char *Big5HKSCStoUTF16Converter::GetCharacterSet()
{
	return "big5-hkscs";
}

void Big5HKSCStoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_prev_byte = 0;
	m_surrogate = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
