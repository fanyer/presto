/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/unicode/utf8.h"

/** Length of UTF-8 sequences for initial byte */
static const UINT8 utf8_len[256] =
{
	/* 00-0F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 10-1F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 20-2F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 30-3F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 40-4F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 50-5F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 60-6F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 70-7F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	/* 80-8F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
	/* 90-9F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
	/* A0-AF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
	/* B0-BF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
	/* C0-CF */	0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	/* D0-DF */	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	/* E0-EF */	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	/* F0-FF */	4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0
};

/** Bit masks for first UTF-8 byte */
static const UINT8 utf8_mask[3] =
{
	0x1F, 0x0F, 0x07
};

/** Low boundary of UTF-8 sequences */
static const UINT32 low_boundary[3] =
{
	0x80, 0x800, 0x10000
};

UTF8Decoder::UTF8Decoder()
	: m_charSplit(-1), m_surrogate(0)
{
}

int UTF8Decoder::Convert(const void *src, int len, void *dest, int maxlen,
                         int *read_ext)
{
	OP_ASSERT(dest != NULL);

	register const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	const unsigned char *input_start = input;
	const unsigned char *input_end = input_start + len;

	maxlen &= ~1; // Make sure destination size is always even
	if (!maxlen)
	{
		if (read_ext) *read_ext = 0;
		return 0;
	}

	register uni_char *output = reinterpret_cast<uni_char *>(dest);
	uni_char *output_start = output;
	uni_char *output_end =
		reinterpret_cast<uni_char *>(reinterpret_cast<char *>(dest) + maxlen);

	int written = 0;

	if (m_surrogate)
	{
		*(output ++) = m_surrogate;
		m_surrogate = 0;
	}

	// Work on the input buffer one byte at a time, and output when
	// we have a completed Unicode character

	// Decode the UTF-8 string; duplicates Measure(), decoupling
	// them is a speed optimization.
	while (input < input_end && output < output_end)
	{
		register unsigned char current = *input;

		// Check if we are in the middle of an escape (this works
		// across buffers)
		if (m_charSplit >= 0)
		{
			if (m_charSplit > 0 && (current & 0xC0) == 0x80)
			{
				// If current is 10xxxxxx, we continue to shift bits.
				m_ucs <<= 6;
				m_ucs |= current & 0x3F;
				++ input;
				-- m_charSplit;
			}
			else if (m_charSplit > 0)
			{
				// If current is not 10xxxxxx and we expected more input,
				// this is an illegal character, so we trash it and continue.
				HandleInvalid(static_cast<int>(output - output_start) * 2);
				*(output ++) = NOT_A_CHARACTER;
				m_charSplit = -1;
			}

			if (0 == m_charSplit)
			{
				// We are finished. We do not consume any more characters
				// until next iteration.
				if (m_ucs < low_boundary[m_sequence] || Unicode::IsSurrogate(m_ucs))
				{
					// Overlong UTF-8 sequences and surrogates are ill-formed,
					// and must not be interpreted as valid characters
					HandleInvalid(static_cast<int>(output - output_start) * 2);
					*output = NOT_A_CHARACTER;
				}
				else if (m_ucs < 0x10000)
				{
					// BMP is mapped to 16-bit units
					*output = static_cast<uni_char>(m_ucs);
				}
				else if (m_ucs <= 0x10FFFF)
				{
					// UTF-16 supports this non-BMP range using surrogates
					uni_char high, low;
					Unicode::MakeSurrogate(m_ucs, high, low);

					*(output ++) = high;

					if (output == output_end)
					{
						m_surrogate = low;
						m_charSplit = -1;
						written = static_cast<int>(output - output_start) * sizeof (uni_char);
						if (read_ext) *read_ext = static_cast<int>(input - input_start);
						AddConverted(written / sizeof (uni_char));
						return written;
					}

					*output = low;
				}
				else
				{
					// Character outside Unicode
					HandleInvalid(static_cast<int>(output - output_start) * 2);
					*output = NOT_A_CHARACTER;
				}

				++ output;
				-- m_charSplit; // = -1
			}
		}
		else
		{
			switch (utf8_len[current])
			{
			case 1:
				// This is a US-ASCII character
				*(output ++) = static_cast<uni_char>(*input);
				written += sizeof (uni_char);
				break;

			// UTF-8 escapes all have high-bit set
			case 2: // 110xxxxx 10xxxxxx
			case 3: // 1110xxxx 10xxxxxx 10xxxxxx
			case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				m_charSplit = utf8_len[current] - 1;
				m_sequence = m_charSplit - 1;
				m_ucs = current & utf8_mask[m_sequence];
				break;

			case 0: // Illegal UTF-8 (includes obsolete 5/6 byte forms)
				HandleInvalid(static_cast<int>(output - output_start) * 2);
				*(output ++) = NOT_A_CHARACTER;
				break;
			}
			++ input;
		}
	}

	// Calculate number of bytes written
	written = static_cast<int>(output - output_start) * sizeof (uni_char);

	if (read_ext) *read_ext = static_cast<int>(input - input_start);
	AddConverted(written / sizeof (uni_char));
	return written;
}

int UTF8Decoder::Measure(const void *src, int len, int maxlen, int *read_ext)
{
	register const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	const unsigned char *input_start = input;
	const unsigned char *input_end = input_start + len;

	maxlen &= ~1; // Make sure destination size is always even
	if (!maxlen)
	{
		if (read_ext) *read_ext = 0;
		return 0;
	}

	int written = 0;

	if (m_surrogate)
	{
		written = sizeof (uni_char);
		m_surrogate = 0;
	}

	// Just count the number of bytes needed; duplicates Convert() above,
	// decoupling them is a speed optimization.
	while (input < input_end && written < maxlen)
	{
		register unsigned char current = *input;

		// Check if we are in the middle of an escape (this works
		// across buffers)
		if (m_charSplit >= 0)
		{
			if (m_charSplit > 0 && (current & 0xC0) == 0x80)
			{
				// If current is 10xxxxxx, we continue to shift bits.
				++ input;
				-- m_charSplit;
			}
			else if (m_charSplit > 0)
			{
				// If current is not 10xxxxxx and we expected more input,
				// this is an illegal character, so we trash it and continue.
				HandleInvalid(written);
				m_charSplit = -1;
			}

			if (0 == m_charSplit)
			{
				// We are finished. We do not consume any more characters
				// until next iteration.
				if (low_boundary[m_sequence] > 0xFFFF)
				{
					// Surrogate
					// Overestimates characters outside Unicode (>=0x110000)
					written += 2 * sizeof (uni_char);
				}
				else
				{
					written += sizeof (uni_char);
				}
				-- m_charSplit; // = -1
			}
		}
		else
		{
			switch (utf8_len[current])
			{
			case 1:
				// This is a US-ASCII character
				written += sizeof (uni_char);
				break;

			// UTF-8 escapes all have high-bit set
			case 2: // 110xxxxx 10xxxxxx
			case 3: // 1110xxxx 10xxxxxx 10xxxxxx
			case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				m_charSplit = utf8_len[current] - 1;
				m_sequence = m_charSplit - 1;
				break;

			case 0: // Illegal UTF-8 (includes obsolete 5/6 byte forms)
				HandleInvalid(written);
				written += sizeof (uni_char);
				break;
			}
			++ input;
		}
	}

	if (read_ext) *read_ext = static_cast<int>(input - input_start);
	AddConverted(written / sizeof (uni_char));
	return written;
}

void UTF8Decoder::Reset()
{
	m_charSplit = -1; // UTF-8 octects left to decode
	m_surrogate = 0; // Left-over surrogate to insert into stream
}

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

int UTF8Encoder::Convert(const void *src, int len, void *dest,
                         int maxlen, int *read_ext)
{
	int read = 0, written = 0;

	const UINT16 *source = reinterpret_cast<const UINT16 *>(src);
	char *output = reinterpret_cast<char *>(dest);
	len &= ~1; // Make sure source size is always even

	while (read < len && written < maxlen)
	{
		if (m_surrogate || Unicode::IsLowSurrogate(*source))
		{
			if (m_surrogate && Unicode::IsLowSurrogate(*source))
			{
				if (written + 4 > maxlen)
					break;

				// Second half of a surrogate
				// Calculate UCS-4 character
				UINT32 ucs4char = Unicode::DecodeSurrogate(m_surrogate, *source);

				// U+10000 - U+10FFFF
				written += 4;
				(*output++) = static_cast<char>(0xF0 | ((ucs4char & 0x1C0000) >> 18));
				(*output++) = static_cast<char>(0x80 | ((ucs4char &  0x3F000) >> 12));
				(*output++) = static_cast<char>(0x80 | ((ucs4char &    0xFC0) >>  6));
				(*output++) = static_cast<char>(0x80 |  (ucs4char &     0x3F)       );

			}
			else
			{
				// Either we have a high surrogate without a low surrogate,
				// or a low surrogate on its own. Anyway this is an illegal
				// UTF-16 sequence.
				if (written + 3 > maxlen)
					break;

				written += 3;
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xEF)); // U+FFFD - replacement character
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xBF));
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xBD));

				if (m_surrogate)
				{
					// Avoid consuming the invalid code unit.
					m_surrogate = 0;
					continue;
				}
			}

			m_surrogate = 0;
		}
		else if (*source < 128)
		{
			(*output++) = static_cast<char>(*source);
			written ++;
		}
		else if (*source < 2048)
		{
			if (written + 2 > maxlen)
				break;

			written += 2;
			(*output++) = static_cast<char>(0xC0 | ((*source & 0x07C0) >> 6));
			(*output++) = static_cast<char>(0x80 | (*source & 0x003F));
		}
		else if (Unicode::IsHighSurrogate(*source))
		{
			// High surrogate area, should be followed by a low surrogate
			m_surrogate = *source;
		}
		else
		{ // up to U+FFFF
			if (written + 3 > maxlen)
				break;

			written += 3;
			(*output++) = static_cast<char>(0xE0 | ((*source & 0xF000) >> 12));
			(*output++) = static_cast<char>(0x80 | ((*source & 0x0FC0) >> 6));
			(*output++) = static_cast<char>(0x80 | (*source & 0x003F));
		}

		// Advance read pointer
		source++;
		read += 2;
	}

	*read_ext = read;
	AddConverted(read);
	return written;
}

int UTF8Encoder::Measure(const void *src, int len, int maxlen, int *read_ext)
{
	int read = 0, outlength = 0;
	const UINT16 *source= reinterpret_cast<const UINT16 *>(src);

	while (len > 0 && outlength < maxlen)
	{
		if (m_surrogate || Unicode::IsLowSurrogate(*source))
		{
			if (m_surrogate && Unicode::IsLowSurrogate(*source))
			{
				// U+10000 - U+10FFFF
				if (outlength + 4 > maxlen)
					break;
				outlength += 4;
			}
			else
			{
				// Replacement character
				if (outlength + 3 > maxlen)
					break;
				outlength += 3;
				if (m_surrogate)
				{
					// Avoid consuming the invalid code unit.
					m_surrogate = 0;
					continue;
				}
			}
			m_surrogate = 0;
		}
		else if (*source<128)
		{
			outlength ++;
		}
		else if (*source<2048)
		{
			if (outlength + 2 > maxlen)
				break;
			outlength += 2;
		}
		else if (Unicode::IsHighSurrogate(*source))
		{
			// High surrogate area, should be followed by a low surrogate
			m_surrogate = *source;
		}
		else
		{ // up to U+FFFF
			if (outlength + 3 > maxlen)
				break;
			outlength += 3;
		}

		source++;
		read += 2;
		len -= 2;
	}

	if (read_ext)
		*read_ext = read;
	return outlength;
}

int UTF8Encoder::LongestSelfContainedSequenceForCharacter()
{
	return m_surrogate ? 4 : 3;
}

void UTF8Encoder::Reset()
{
	m_surrogate = 0;
}
