/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined ENCODINGS_HAVE_CHINESE

#include "modules/encodings/decoders/big5-decoder.h"

Big5toUTF16Converter::Big5toUTF16Converter()
	: DBCStoUTF16Converter(
#ifdef USE_MOZTW_DATA
	"big5-table",
#else
	"",
#endif
	 0xA1, 0xFE, 0x40, 0x7E, 0xA1, 0xFE)
{
}

int Big5toUTF16Converter::get_map_index(unsigned char first, unsigned char second)
{
	return (first - 0xA1) * 191 + (second - 0x40);
}

const char *Big5toUTF16Converter::GetCharacterSet()
{
	return "big5";
}

#endif // ENCODINGS_HAVE_TABLE_DRIVEN && ENCODINGS_HAVE_CHINESE
