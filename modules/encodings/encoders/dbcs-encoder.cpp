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
#include "modules/encodings/encoders/dbcs-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

UTF16toDBCSConverter::UTF16toDBCSConverter(enum encoding outputencoding)
	: m_maptable1(NULL), m_maptable2(NULL),
	  m_table1start(0), m_table1top(0), m_table2len(0),
	  m_my_encoding(outputencoding)
{
}

OP_STATUS UTF16toDBCSConverter::Construct()
{
	if (OpStatus::IsError(this->OutputConverter::Construct())) return OpStatus::ERR;

	long table1len;
	const char *table1 = NULL, *table2 = NULL;

#ifdef ENCODINGS_HAVE_CHINESE
	m_table1start = 0x4E00;
#endif
	switch (m_my_encoding)
	{
	default:
#ifdef ENCODINGS_HAVE_CHINESE
	case BIG5:
#ifdef USE_MOZTW_DATA
		table1 = "big5-rev-table-1";
		table2 = "big5-rev-table-2";
#endif
		break;

	case GBK:
		table1 = "gbk-rev-table-1";
		table2 = "gbk-rev-table-2";
		break;
#endif

#ifdef ENCODINGS_HAVE_KOREAN
	case EUCKR:
		table1 = "ksc5601-rev-table-1";
		table2 = "ksc5601-rev-table-2";
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

UTF16toDBCSConverter::~UTF16toDBCSConverter()
{
	if (g_table_manager)
	{
		g_table_manager->Release(m_maptable1);
		g_table_manager->Release(m_maptable2);
	}
}

const char *UTF16toDBCSConverter::GetDestinationCharacterSet()
{
	switch (m_my_encoding)
	{
#ifdef ENCODINGS_HAVE_CHINESE
	case BIG5:		return "big5";		break;
	case GBK:		return "gbk";		break;
#endif
#ifdef ENCODINGS_HAVE_KOREAN
	case EUCKR:		return "euc-kr";	break;
#endif
	}

	OP_ASSERT(!"Unknown encoding");
	return NULL;
}

int UTF16toDBCSConverter::Convert(const void* src, int len, void* dest,
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

			if (rowcell[0] != 0 && rowcell[1] != 0)
			{
				if (written + 2 > maxlen)
				{
					// Don't split character
					break;
				}

				output[0] = rowcell[0];
				output[1] = rowcell[1];
				written += 2;
				output += 2;
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

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && (ENCODINGS_HAVE_CHINESE || ENCODINGS_HAVE_KOREAN)
