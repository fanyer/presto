/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/decoders/utf16-decoder.h"

// ==== UTF-16 any byte order -> UTF-16  ===================================

#ifdef NEEDS_RISC_ALIGNMENT
union utf16_union
{
	uni_char utf16;
	char bytes[2]; /* ARRAY OK 2012-04-24 peter */
};
#endif

UTF16toUTF16Converter::UTF16toUTF16Converter()
	: m_utf16converter(NULL)
{
}

UTF16toUTF16Converter::~UTF16toUTF16Converter()
{
	if (m_utf16converter) OP_DELETE(m_utf16converter);
}

int UTF16toUTF16Converter::Convert(const void *src, int len, void *dest,
								   int maxlen, int *read)
{
	if (!len || (!m_utf16converter && len < 2) || maxlen < 2)
	{
		// No input to convert
		*read = 0;
		return 0;
	}

	if (!m_utf16converter)
	{
		const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
		char *output = reinterpret_cast<char *>(dest);

		int skip = 0;
		// Need to figure out byte order first
#ifdef NEEDS_RISC_ALIGNMENT
		utf16_union bom_u;
		bom_u.bytes[0] = input[0];
		bom_u.bytes[1] = input[1];
		uni_char bom = bom_u.utf16;
#else
		uni_char bom = *reinterpret_cast<const uni_char *>(src);
#endif

		BOOL native_endian;
		if (0xFFFE == bom)
		{
			// Opposite endian
			native_endian = FALSE;
			skip = 2;
		}
		else if (0xFEFF == bom)
		{
			// Native endian
			native_endian = TRUE;
			skip = 2;
		}
		else
		{
			/* No BOM specified; RFC 2781 states that the contents "SHOULD
			 * be interpreted as big-endian". The consensus among the other
			 * browsers seems, however, to be to use little-endian (see
			 * CORE-43500). But first we try to be smart: HTML documents
			 * usually start with markup, which is always in the ASCII
			 * range, so we try to see if the first 10 UTF-16 characters
			 * all fall in the Basic Latin range (ASCII) for UTF-16LE,
			 * otherwise we assume UTF-16BE. */
			int bytes_to_test = (len > 20) ? 20 : len;
			BOOL probable_big = TRUE;

			for (int i = 0; i < bytes_to_test && probable_big; i += 2)
			{
				if (input[i] != 0 || input[i + 1] >= 0x80)
				{
					probable_big = FALSE;
				}
			}

			/* If we probed big-endian content, use that, otherwise
			 * fall back to little-endian. */
			native_endian =
#ifdef OPERA_BIG_ENDIAN
				probable_big;
#else
				!probable_big;
#endif
		}

		if (native_endian)
		{
			m_utf16converter = OP_NEW(IdentityConverter, ());
		}
		else
		{
			m_utf16converter = OP_NEW(ByteSwapConverter, ());
		}
		if (m_utf16converter && OpStatus::IsError(m_utf16converter->Construct()))
		{
			OP_DELETE(m_utf16converter);
			m_utf16converter = NULL;
		}
		if (m_utf16converter == NULL)
			return -1;

		input += skip; len -= skip;
		int written = m_utf16converter->Convert(input, len, output, maxlen, read);
		(*read) += skip;
		m_num_converted += written / sizeof (uni_char);
		m_num_invalid = m_utf16converter->GetNumberOfInvalid();
		if (m_first_invalid_offset == -1)
			m_first_invalid_offset = m_utf16converter->GetFirstInvalidOffset();
		return written;
	}
	else
	{
		int written = m_utf16converter->Convert(src, len, dest, maxlen, read);
		m_num_converted += written / sizeof (uni_char);
		m_num_invalid = m_utf16converter->GetNumberOfInvalid();
		if (m_first_invalid_offset == -1)
			m_first_invalid_offset = m_utf16converter->GetFirstInvalidOffset();
		return written;
	}
}

#ifdef ENCODINGS_HAVE_CHECK_ENDSTATE
BOOL UTF16toUTF16Converter::IsValidEndState()
{
	if (m_utf16converter)
	{
		return m_utf16converter->IsValidEndState();
	}
	else
	{
		// An unlabeled UTF-16 document is not valid without indication of
		// byte order.
		return FALSE;
	}
}
#endif

const char *UTF16toUTF16Converter::GetCharacterSet()
{
	return "utf-16";
}

void UTF16toUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	if (m_utf16converter) OP_DELETE(m_utf16converter);
	m_utf16converter = NULL;
}

// ==== UTF-16 opposite endian -> UTF-16  ================================

int ByteSwapConverter::Convert(const void *src, int len, void *dest, int maxlen,
			       int *read)
{
	if (len > maxlen) len = maxlen;
	len &= ~1; // Make sure destination size is always even

	const char *input = reinterpret_cast<const char *>(src);
	char *output = reinterpret_cast<char *>(dest);

	for (int i = 0; i < len; i += 2)
	{
		output[i] = input[i + 1];
		output[i + 1] = input[i];
	}
	*read = len;
	m_num_converted += len / sizeof (uni_char);
	return len;
}

const char *ByteSwapConverter::GetCharacterSet()
{
	return "utf-16";
}

// ==== UTF-16 machine endian -> UTF-16  =================================
// This conversion differs from the identity conversion in that we know
// what encoding we are converting from and to.

const char *UTF16NativetoUTF16Converter::GetCharacterSet()
{
	return "utf-16";
}

const char *UTF16NativetoUTF16Converter::GetDestinationCharacterSet()
{
	return "utf-16";
}
