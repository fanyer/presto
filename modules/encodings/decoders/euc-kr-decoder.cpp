/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_KOREAN

#include "modules/encodings/decoders/euc-kr-decoder.h"

EUCKRtoUTF16Converter::EUCKRtoUTF16Converter()
	: DBCStoUTF16Converter("ksc5601-table", 0x81, 0xFE, 0x41, 0x7A, 0x81, 0xFE)
{
}

int EUCKRtoUTF16Converter::get_map_index(unsigned char first, unsigned char second)
{
	int codepoint = -1;
	if (first < 0xC7)
	{
		// Lead bytes 0x81 -- 0xC6 allow trail bytes
		// 0x41 -- 0x5A, 0x61 -- 0x7A, 0x81 -- 0xFE
		int rowidx = (26 + 26 + 126) * (first - 0x81);
		if (second >= 0x41 && second <= 0x5A)
		{
			codepoint = rowidx + (second - 0x41); // 26 characters
		}
		else if (second >= 0x61 && second <= 0x7A)
		{
			codepoint = rowidx + 26 + second - 0x61; // 26 characters
		}
		else if (second >= 0x81 && second <= 0xFE)
		{
			codepoint = rowidx + 26 + 26 + second - 0x81; // 126 characters
		}
	}
	else
	{
		// Lead bytes 0xC7 -- 0xFD allow trail bytes 0xA1 -- 0xFE
		// For these leads, only these trails are stored, to minimize
		// table size
		if (second >= 0xA1 && second <= 0xFE)
		{
			codepoint = (26 + 26 + 126) * (0xC7 - 0x81) +
					    (first - 0xC7) * 94 + (second - 0xA1);
		}
	}

	return codepoint;
}

const char *EUCKRtoUTF16Converter::GetCharacterSet()
{
	return "euc-kr";
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_KOREAN
