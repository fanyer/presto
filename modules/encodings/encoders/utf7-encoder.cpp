/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef ENCODINGS_HAVE_UTF7

#ifndef ENCODINGS_REGTEST
# include "modules/url/url_enum.h" // Mime_EncodeTypes, needed for modules/url/encoder.h
# include "modules/formats/base64_decode.h" // Base64_decoding_table
#endif
#include "modules/encodings/encoders/utf7-encoder.h"

#ifndef ENCODINGS_HAVE_UTF7IMAP
# define m_escape '+'
#endif

#define UTF7_B 0x01 // UTF-7 set B (Base64 characters)
#define UTF7_D 0x02 // UTF-7 set D (directly encoded characters)
#define UTF7_O 0x04 // UTF-7 set O (optional direct characters)
#define UTF7_C 0x08 // ASCII control characters (<=32)

/* List of UTF-7 character classes */
const unsigned char UTF16toUTF7Converter::utf7_char_classes[0x80] =
{
	// 0-7
	UTF7_D, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C,
	// 8-15
	UTF7_C, UTF7_O, UTF7_O, UTF7_C, UTF7_C, UTF7_O, UTF7_C, UTF7_C,
	// 16-23
	UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C,
	// 24-31
	UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C, UTF7_C,
	// 32-39
	UTF7_D, UTF7_O, UTF7_O, UTF7_O, UTF7_O, UTF7_O, UTF7_O, UTF7_D,
	// 40-47
	UTF7_D, UTF7_D, UTF7_O, 0     , UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 48-55
	UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 56-63
	UTF7_D, UTF7_D, UTF7_D, UTF7_O, UTF7_O, UTF7_O, UTF7_O, UTF7_D,
	// 64-71
	UTF7_O, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 72-79
	UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 80-87
	UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 88-95
	UTF7_D, UTF7_D, UTF7_D, UTF7_O, 0     , UTF7_O, UTF7_O, UTF7_O,
	// 96-103
	UTF7_O, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 104-111
	UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 112-119
	UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D, UTF7_D,
	// 120-127
	UTF7_D, UTF7_D, UTF7_D, UTF7_O, UTF7_O, UTF7_O, 0,      UTF7_C
};

/**
 * Check if we allow this character to be sent clear (i.e not Base64 encoded)
 */
BOOL UTF16toUTF7Converter::AllowClear(uni_char c)
{
	switch (m_variant)
	{
#ifdef ENCODINGS_HAVE_UTF7IMAP
	case IMAP:
		// IMAP must not use base64 for ASCII
		if (c >= 0x20 && c < 0x7F)
			return TRUE;
		break;
#endif

	case STANDARD:
		if (c < 0x80 && utf7_char_classes[c] & (UTF7_D | UTF7_O | UTF7_C))
			return TRUE;
		break;

	case SAFE:
		if (c < 0x80 && utf7_char_classes[c] & UTF7_D)
			return TRUE;
		break;
	}

	return FALSE;
}

UTF16toUTF7Converter::UTF16toUTF7Converter(utf7_variant variant)
	: m_state(ASCII), m_variant(variant), m_valid_bits(0)
{
#ifdef ENCODINGS_HAVE_UTF7IMAP
	m_escape = (variant == IMAP) ? '&' : '+';
#endif
}


int UTF16toUTF7Converter::Convert(const void* src, int len, void* dest,
                                  int maxlen, int* read_ext)
{
	const UINT16 *input = reinterpret_cast<const UINT16 *>(src);
	char *output = reinterpret_cast<char *>(dest);
	len &= ~1; // Make sure source size is always even

	int read = 0, written = 0;

	while (written < maxlen)
	{
		if (m_valid_bits >= 6)
		{
			// Convert to Base64
			char base64_char = Base64_Encoding_chars[(m_bits >> (m_valid_bits - 6)) & 0x3F];
#ifdef ENCODINGS_HAVE_UTF7IMAP
			if ('/' == base64_char && IMAP == m_variant) base64_char = ',';
#endif
			*(output ++) = base64_char;
			written ++;

			// Remove encoded bits
			unsigned int mask_out = 0x0000FFFF;
			m_valid_bits -= 6;
			mask_out <<= m_valid_bits;
			m_bits &= ~mask_out;

			// Maybe consume input
			if (m_valid_bits < 6 && m_state != BASE64_EXIT)
			{
				read ++;
				input ++;
			}
		}
		else if (len == read * 2)
		{
			// We have consumed all input, but we try to empty any base64 bits
			// left before we exit
			BOOL quitnow = FALSE;
			switch (m_state)
			{
			case BASE64:
				if (m_valid_bits)
				{
					// Need to pad with zeros to output the last pattern
					m_bits <<= 6;
					m_valid_bits += 6;
				}
				m_state = BASE64_EXIT;
				break;

			case BASE64_EXIT:
				// Switch to ASCII mode
				*(output ++) = '-';
				written ++;
				m_state = ASCII;
				break;

			default:
				quitnow = TRUE;
			}

			// Exit loop
			if (quitnow) break;
		}
		else
		{
			switch (m_state)
			{
			case ASCII:
				if (m_escape == *input)
				{
					m_state = ESCAPE;
					*(output ++) = m_escape;
					written ++;
					// Character not consumed yet
				}
				else if (!*input || AllowClear(*input))
				{
					*(output ++) = static_cast<char>(*(input ++));
					read ++;
					written ++;
				}
				else
				{
					// Switch to base64 mode
					*(output ++) = m_escape;
					written ++;
					m_state = BASE64;
					m_bits = 0;
					m_valid_bits = 0;
				}
				break;

			case BASE64: // Encode UTF-16 character
				if (!*input || AllowClear(*input))
				{
					if (m_valid_bits)
					{
						// Need to pad with zeros to output the last pattern
						m_bits <<= 6;
						m_valid_bits += 6;
					}
					m_state = BASE64_EXIT;
				}
				else
				{
					m_bits = (m_bits << 16) | *input;
					m_valid_bits += 16;
				}
				break;

			case BASE64_EXIT: // Exit from base64
				// Switch to ASCII mode
				OP_ASSERT(0 == m_bits);
				*(output ++) = '-';
				written ++;
				m_state = ASCII;
				break;

			case ESCAPE: // Escaping the escape character
				*(output ++) = '-';
				read ++;
				input ++;
				written ++;
				m_state = ASCII;
				break;
			}
		}
	}

	*read_ext = read * 2;
	m_num_converted += read;
	return written;
}

const char *UTF16toUTF7Converter::GetDestinationCharacterSet()
{
	return "utf-7";
}

int UTF16toUTF7Converter::ReturnToInitialState(void *dest)
{
	int upperbound = 0;

	switch (m_state)
	{
	case ASCII:
	default:
		// We're already in initial state
		return 0;

	case ESCAPE:
		// One byte needed to return to initial state
		upperbound = 1;
		break;

	case BASE64_EXIT:
	case BASE64:
		// Need to output m_valid_bits in Base64 mode and then
		// switch back to the initial state
		upperbound = m_valid_bits / 6 + 1 + 1; // may be 1 byte too high
		break;
	}

	if (dest)
	{
		int read;
		int written =
			upperbound ? Convert(UNI_L(""), 0, dest, upperbound, &read) : 0;
		OP_ASSERT(m_state == ASCII);
		return written;
	}
	else
	{
		return upperbound;
	}
}

void UTF16toUTF7Converter::Reset()
{
	this->OutputConverter::Reset();
	m_state = ASCII;
	m_valid_bits = 0;
}


int UTF16toUTF7Converter::LongestSelfContainedSequenceForCharacter()
{
	switch (m_state)
	{
	case ASCII:
	default:
		// Worst case: Switch to Base64 mode and encode one character
		// Escape character: 1 byte
		// Encode 16 bits:   3 bytes (2 2/3)
		// Switch to ASCII:  1 byte
		return 1 + 3 + 1;

	case ESCAPE:
		// Worst case is outputting the escaped character and then doing
		// the above routine
		return 1 + 1 + 3 + 1;

	case BASE64_EXIT:
	case BASE64:
		// Worst case: Output another UTF-16 character and switch to
		// ASCII mode.
		// Encode 16 bits:  2 2/3 bytes
		// Switch to ASCII: 1 byte
		if (m_valid_bits <= 2)
		{
			// Only 1/3 of the Base64 byte is used, so our 16 bits makes
			// it exactly 3 bytes
			return 3 + 1;
		}
		else
		{
			// More than 1/3 of the Base64 byte is used, so we need to
			// encode those in ceil(m_valid_bits/6) bytes first
			return m_valid_bits / 6 + 1 + 3 + 1; // may be 1 byte too high
		}
	}
}

#endif // ENCODINGS_HAVE_UTF7
