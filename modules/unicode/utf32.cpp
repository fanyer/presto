/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef USE_UNICODE_UTF32

#include "modules/unicode/utf32.h"
#include "modules/encodings/encoders/encoder-utility.h"

int UTF32Decoder::Convert(const void *src, int len,
                          void *dest, int maxlen, int *read_ext)
{
	union utf32_union
	{
		UINT32 ucs;
		char bytes[4]; /* ARRAY OK 2011-08-22 peter */
	};
	int read = 0, written = 0;

	len &= ~3; // Make sure source size is always divisible by four
	maxlen &= ~1; // Make sure destination size is always even

	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);

	if (m_surrogate)
	{
		if (output)
		{
			*(output ++) = m_surrogate;
		}
		written += sizeof(uni_char);
		m_surrogate = 0;
	}

	while (read < len && written < maxlen)
	{
		utf32_union ucs;
#ifdef NEEDS_RISC_ALIGNMENT
		ucs.bytes[0] = *(input ++);
		ucs.bytes[1] = *(input ++);
		ucs.bytes[2] = *(input ++);
		ucs.bytes[3] = *(input ++);
#else
		ucs.ucs = *reinterpret_cast<const UINT32 *>(input);
		input += 4;
#endif

		read += 4;

		if (ucs.ucs < 0x10000UL)
		{
			// Character is within UTF-16's 16-bit range
			if (Unicode::IsSurrogate(ucs.ucs))
				*output = NOT_A_CHARACTER;  // Surrogates are not allowed in UTF-32
			else
				*output = static_cast<uni_char>(ucs.ucs);
			++output;
			written += sizeof(uni_char);
		}
		else if (ucs.ucs < 0x110000UL)
		{
			// UTF-16 supports this non-BMP range using surrogates
			uni_char high, low;
			Unicode::MakeSurrogate(ucs.ucs, high, low);

			*output = high;
			++output;
			written += sizeof(uni_char);

			if (written >= maxlen)
			{
				m_surrogate = low;
			}
			else
			{
				*output = low;
				++output;
				written += sizeof(uni_char);
			}
		}
		else
		{
			// Illegal UTF-32 sequence, and if UCS-4, something we can't
			// represent in UTF-16 anyway
			*output = NOT_A_CHARACTER;
			++output;
			written += sizeof(uni_char);
		}
	}

	*read_ext = read;
	return written;
}

void UTF32Decoder::Reset()
{
	m_surrogate = 0;
}

int UTF32Encoder::Convert(const void *src, int len, void *dest,
                          int maxlen, int *read_ext)
{
	int read = 0, written = 0;
	maxlen &= ~3; // Make sure destination size is always divisible by four
	len &= ~1; // Make sure source size is always even

	if (!maxlen)
	{
		*read_ext = 0;
		return 0;
	}

	UINT32 *output = reinterpret_cast<UINT32 *>(dest);

	if (m_add_bom)
	{
		*(output ++) = 0xFEFFUL; // Add BOM
		written += sizeof(UINT32);
		m_add_bom = FALSE;
	}

	const UINT16 *source= reinterpret_cast<const UINT16 *>(src);

	while (read < len && written < maxlen)
	{
		if (m_surrogate || Unicode::IsLowSurrogate(*source))
		{
			written += sizeof(UINT32);
			if (m_surrogate && Unicode::IsLowSurrogate(*source))
			{
				// Second half of a surrogate
				// Calculate UCS-4 character
				*(output ++) = Unicode::DecodeSurrogate(m_surrogate, *source);
			}
			else
			{
				// Either we have a high surrogate without a low surrogate,
				// or a low surrogate on its own. Anyway this is an illegal
				// UTF-16 sequence.
				*(output ++) = NOT_A_CHARACTER;
				if (m_surrogate)
				{
					// Avoid consuming the invalid code unit.
					m_surrogate = 0;
					continue;
				}
			}

			m_surrogate = 0;
		}
		else if (Unicode::IsHighSurrogate(*source))
		{
			// High surrogate area, should be followed by a low surrogate
			m_surrogate = *source;
		}
		else
		{ // up to U+FFFF
			*(output ++) = static_cast<UINT32>(*source);
			written += sizeof(UINT32);
		}

		source ++;
		read += sizeof(UINT16);
	}

	*read_ext = read;
	return written;
}

void UTF32Encoder::Reset(BOOL add_bom)
{
	m_surrogate = 0;
	m_add_bom = add_bom;
}

#endif // USE_UNICODE_UTF32
