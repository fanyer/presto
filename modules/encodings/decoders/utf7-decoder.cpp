/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef ENCODINGS_HAVE_UTF7

#ifndef ENCODINGS_REGTEST
# include "modules/url/url_enum.h" // Mime_EncodeTypes, needed for modules/url/encoder.h
# include "modules/formats/base64_decode.h" // Base64_decoding_table
#endif
#include "modules/encodings/decoders/utf7-decoder.h"

#ifndef ENCODINGS_HAVE_UTF7IMAP
# define m_escape '+'
#endif

UTF7toUTF16Converter::UTF7toUTF16Converter(utf7_variant variant)
	: m_variant(variant), m_state(ASCII)
{
#ifdef ENCODINGS_HAVE_UTF7IMAP
	m_escape = (variant == IMAP) ? '&' : '+';
#endif
}

int UTF7toUTF16Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read_ext)
{
	const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	uni_char *output = reinterpret_cast<uni_char *>(dest);
	int read = 0, written = 0;
	maxlen &= ~1; // Make sure destination size is always even

	while (read < len && written < maxlen)
	{
		if (*input & 0x80)
		{
			// No high-bit characters allowed in stream
			written += HandleInvalidChar(written, &output);
		}
		else
		{
			long decoded = -1; // -1 means no valid data yet
			switch (m_state)
			{
			case ASCII:
				if (m_escape == *input)
				{
					m_state = BASE64_FIRST;
					m_bits = 0; // Clear base64 bitfield
					m_valid_bits = 0; // No valid bits
				}
				else
				{
					// Allow everything else (not very strict)
					decoded = *input;
				}
				break;

			case BASE64_FIRST:
				if ('-' == *input)
				{
					// The sequence "+-" encodes "+"
					decoded = m_escape;
				}
				// Fall through

			case BASE64:
				if ('-' == *input)
				{
					// Switch state and absorb
					m_state = ASCII;
					if (m_bits != 0)
					{
						// It is not allowed to have "dangling" bits in
						// the stream
						if (-1 == decoded)
						{
							decoded = NOT_A_CHARACTER;
						}
						HandleInvalidChar(written);
					}
				}
				else
				{
					unsigned char base64_char = *input;
#ifdef ENCODINGS_HAVE_UTF7IMAP
					if (IMAP == m_variant)
					{
						// IMAP uses , instead of / in the Base64 alphabet.
						switch (base64_char)
						{
						case ',': base64_char = '/'; break;
						case '/': base64_char =   0; break; // Flags as invalid
						}
					}
#endif
					unsigned char base64_decoded = Base64_decoding_table[base64_char];
					if (base64_decoded >= 64)
					{
						// Termination character or non-base64 character:
						// Switch state and keep character as-is.
						m_state = ASCII;
						decoded = *input;
						if (m_bits != 0)
						{
							// It is not allowed to have "dangling" bits in
							// the stream
							HandleInvalidChar(written);
						}
					}
					else
					{
						m_bits = (m_bits << 6) | base64_decoded;
						m_valid_bits += 6;
						m_state = BASE64;
					}
				}
				break;
			}

			if (decoded == -1 && m_valid_bits >= 16)
			{
				// We have decoded a UTF-16 codepoint
				m_valid_bits -= 16;
				decoded = m_bits >> m_valid_bits;

				// Mask away what we have left
				unsigned int mask_out = 0x0000FFFF;
				mask_out <<= m_valid_bits;
				m_bits &= ~mask_out;
			}

			// Add output if we got anything
			if (decoded != -1)
			{
				*(output ++) = static_cast<uni_char>(decoded);
				written += sizeof (uni_char);
			}
		}
		// We always consume input
		input ++;
		read ++;
	}

	*read_ext = read;
	m_num_converted += written / sizeof (uni_char);
	return written;
}

const char *UTF7toUTF16Converter::GetCharacterSet()
{
	return "utf-7";
}

void UTF7toUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	// Keep variant
	m_state = ASCII;
}

#endif // ENCODINGS_HAVE_UTF7
