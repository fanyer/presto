/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/decoders/iso-8859-1-decoder.h"

int ISOLatin1toUTF16Converter::Convert(const void *src, int len, void *dest,
				                       int maxlen, int *read_p)
{
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	len = MIN(len, maxlen / 2);

	for (int i = len; i; -- i)
	{
		*(output ++) = static_cast<uni_char>(*(input ++));
	}

	*read_p = len;
	m_num_converted += len;
	return len * 2;
}

const char *ISOLatin1toUTF16Converter::GetCharacterSet()
{
	return m_is_x_user_defined ? "x-user-defined" : "iso-8859-1";
}
