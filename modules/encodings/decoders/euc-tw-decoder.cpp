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
#include "modules/encodings/decoders/euc-tw-decoder.h"

EUCTWtoUTF16Converter::EUCTWtoUTF16Converter()
	: m_cns_11643_table(NULL),
	  m_cns_11643_table_len(0),
	  m_lead(0), m_ss2(0),
	  m_plane(0)
{
}

OP_STATUS EUCTWtoUTF16Converter::Construct()
{
	if (OpStatus::IsError(this->InputConverter::Construct())) return OpStatus::ERR;

	m_cns_11643_table =
		reinterpret_cast<const UINT16 *>(g_table_manager->Get("cns11643-table", m_cns_11643_table_len));
	m_cns_11643_table_len /= 2; // Size was in bytes, we need it in uni_chars

	return m_cns_11643_table ? OpStatus::OK : OpStatus::ERR;
}

EUCTWtoUTF16Converter::~EUCTWtoUTF16Converter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_cns_11643_table);
	}
}

int EUCTWtoUTF16Converter::Convert(const void *_src, int len, void *dest,
		   					       int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	const unsigned char *src = reinterpret_cast<const unsigned char *>(_src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	maxlen &= ~1; // Make sure destination size is always even

	while (read < len && written < maxlen)
	{
		bool consume = true;

		if (m_lead)
		{
			// Expecting trail; this can be a two-byte code in plane 1
			// or a four-byte code in any plane; plane tells us that
			// (zero for two-byte code, non-zero 1-based for four-byte codes)

			// Convert EUC-TW into CNS 11643 row-cell (kuten) encoding
			// used by the mapping table.
			if (*src >= 0xA1 && *src <= 0xFE)
			{
				int codepoint =
					(m_plane ? m_plane - 1 : 0) * 8742 +
					(m_lead - 161) * 94 + (*src - 161);

				if (codepoint < m_cns_11643_table_len && m_lead < 0xFE && m_plane >= 0)
				{
					UINT16 cns11643char = ENCDATA16(m_cns_11643_table[codepoint]);
					written += HandleInvalidChar(written, &output, cns11643char, cns11643char != NOT_A_CHARACTER);
				}
				else
				{
					written += HandleInvalidChar(written, &output);
				}
			}
			else
			{
				// Byte was illegal, do not consume it
				written += HandleInvalidChar(written, &output);
				consume = false;
			}

			m_lead = 0;
			m_plane = 0;
			m_ss2 = 0;
		}
		else if (m_ss2)
		{
			if (m_plane)
			{
				// Expecting lead
				if (*src >= 0xA1 && *src <= 0xFE)
				{
					m_lead = *src;
				}
				else
				{
					// Byte was illegal, do not consume it
					m_ss2 = 0;
					m_lead = 0;
					m_plane = 0;
					written += HandleInvalidChar(written, &output);
					consume = false;
				}
			}
			else
			{
				// Expecting plane
				if (*src >= 0xA1 && *src <= 0xB0)
				{
					m_plane = *src - 0xA0;
					if (0x0E == m_plane)
					{
						m_plane = 3; // table compression
					}
					else if (m_plane >= 3)
					{
						m_plane = -1; // we only support plane 0, 1 and 14
					}
				}
				else // Invalid second/plane byte
				{
					m_ss2 = 0;
					m_lead = 0;
					m_plane = 0;
					written += HandleInvalidChar(written, &output);
					consume = false;
				}
			}
		}
		else
		{
			// Expecting lead, SS2 or single-byte
			if (*src <= 0x7F)
			{
				// This range should really use the KS Roman table, but
				// we don't because of the Yen vs. backslash problem
				(*output++) = *src;
				written += 2;
			}
			else if (142 == *src)
			{
				// Code set 2 is also CNS 11643, but with plane indicator byte.
				m_ss2 = 1;
			}
			else if (*src >= 0xA1 && *src <= 0xFE)
			{
				m_lead = *src;
			}
			else
			{
				written += HandleInvalidChar(written, &output);
			}
		}

		if (consume)
		{
			src ++;
			read ++;
		}
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *EUCTWtoUTF16Converter::GetCharacterSet()
{
	return "euc-tw";
}

void EUCTWtoUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	m_lead  = 0;
	m_ss2   = 0;
	m_plane = 0;
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
