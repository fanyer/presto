/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/encoders/iso-8859-1-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

const char *UTF16toISOLatin1Converter::GetDestinationCharacterSet()
{
	if (m_high == 128) return "us-ascii";
	if (m_is_x_user_defined) return "x-user-defined";
	return "iso-8859-1";
}

char UTF16toISOLatin1Converter::lookup(uni_char)
{
	return 0;
}

int UTF16toISOLatin1Converter::Convert(const void *src, int len, void* dest,
                                       int maxlen, int *read_p)
{
	const uni_char *input = reinterpret_cast<const uni_char *>(src);
	int utf16len = len / 2;
	int written = 0;

	char *output = reinterpret_cast<char *>(dest);
	int read = 0;

	char target_character;

	while (read < utf16len && written < maxlen)
	{
		if (*input < m_high)
		{
			*(output ++) = static_cast<char>(*(input ++));
			++ written;
		}
		else if (*input >= 0xFF00 && *input <= 0xFF5E)
		{
			// Full-width ASCII
			*(output ++) = static_cast<char>((*(input ++) & 0xFF) + 0x20);
			++ written;
		}
		else if (0 != (target_character = lookup(*input)))
		{
			*(output ++) = target_character;
			++ written;
			++ input;
		}
		else
		{
			// Note: need some more logic here to fold look-alike characters
			// to their best local representations
#ifdef ENCODINGS_HAVE_ENTITY_ENCODING
			if (!CannotRepresent(*(input ++), read, &output, &written, maxlen))
				break;
#else
			*(output ++) = NOT_A_CHARACTER_ASCII;
			CannotRepresent(*(input ++), read);
			++ written;
#endif
		}
		++ read;
	}

	*read_p = read * 2;
	m_num_converted += read;
	return written;
}
