/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/encoders/utf16-encoder.h"

int UTF16toUTF16OutConverter::Convert(const void *src, int len, void *dest,
                                      int maxlen, int *read_ext)
{
	if (m_firstcall)
	{
		if (maxlen < 2)
		{
			// No space to add BOM
			*read_ext = 0;
			return 0;
		}

		// Add a BOM to the output string
		UINT16 *output = reinterpret_cast<UINT16 *>(dest);
		*(output ++) = 0xFEFF;
		m_firstcall = FALSE;

		// Do the rest of the buffer
		return Convert(src, len, output, maxlen - sizeof (UINT16), read_ext)
		       + sizeof (UINT16);
	}

	// Just copy
	if (len > maxlen) len = maxlen;
	op_memcpy(dest, src, len);
	*read_ext = len;
	m_num_converted += len / 2;
	return len;
}

int UTF16toUTF16OutConverter::ReturnToInitialState(void *)
{
	// Add a BOM to the next buffer as well
	m_firstcall = TRUE;
	return 0;
}

void UTF16toUTF16OutConverter::Reset()
{
	this->OutputConverter::Reset();
	m_firstcall = TRUE;
}

const char *UTF16toUTF16OutConverter::GetDestinationCharacterSet()
{
	return "utf-16";
}
