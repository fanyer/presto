/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef OPDATA_DEBUG

#include "modules/opdata/src/DebugViewFormatter.h"
#include "modules/opdata/UniString.h" // Need access to IsUniCharAligned()

/* virtual */ size_t
DebugViewEscapedByteFormatter::FormatData(char *dst, size_t dst_len, const char *src, size_t src_len)
{
	OP_ASSERT(dst && dst_len && src && src_len);
	char *p = dst;
	const char *q = src;
	bool prev_escaped = false;
	while (p < dst + dst_len && q < src + src_len)
	{
		if ((*q == '\'' || *q == '\\') && p + 2 < dst + dst_len)
		{
			*(p++) = '\\';
			*(p++) = *q;
			prev_escaped = false;
		}
		else if (IsPrintableAscii(*q) &&
			!(prev_escaped && op_isxdigit(*q)))
		{
			*(p++) = *q;
			prev_escaped = false;
		}
		else if (p + 4 < dst + dst_len)
		{
			p = FormatPrefix(p);
			p = FormatByte(p, *q);
			prev_escaped = true;
		}
		else
		{
			/*
			 * If we get here, the given dst_len was insufficient
			 * for holding the debug-formatted data. This is a bug
			 * and should be fixed. However, in the meantime, we can
			 * at least '\0'-terminate the string (if there is room)
			 * to let the user know (by seeing a debug string that
			 * ends prematurely) that not all of the data is present
			 * in the string.
			 */
			if (p > dst && p == dst + dst_len)
				p--; // Backtrack one byte to fit '\0'.
			if (p < dst + dst_len)
				*(p++) = '\0';
			break;
		}
		q++;
	}
	return p - dst;
}

/* virtual */ size_t
DebugViewEscapedUniCharFormatter::NeedLength(size_t input_length)
{
	OP_ASSERT(UniString::IsUniCharAligned(input_length));
	// 2 bytes in => "\x####" == 6 bytes out
	return 6 * UNICODE_DOWNSIZE(input_length);
}

/* virtual */ size_t
DebugViewEscapedUniCharFormatter::FormatData(char *dst, size_t dst_len, const char *src, size_t src_len)
{
	OP_ASSERT(dst && dst_len && src && src_len);
	OP_ASSERT(UniString::IsUniCharAligned(src));
	OP_ASSERT(UniString::IsUniCharAligned(src_len));
	char *p = dst;
	const uni_char *q = reinterpret_cast<const uni_char *>(src);
	const uni_char * const q_end = q + UNICODE_DOWNSIZE(src_len);
	bool prev_escaped = false;
	while (p < dst + dst_len && q < q_end)
	{
		if ((*q == '\'' || *q == '\\') && p + 2 < dst + dst_len)
		{
			*(p++) = '\\';
			*(p++) = static_cast<char>(*q);
			prev_escaped = false;
		}
		else if (IsPrintableAscii(*q) &&
			!(prev_escaped && op_isxdigit(*q)))
		{
			*(p++) = static_cast<char>(*q);
			prev_escaped = false;
		}
		else if (p + 6 < dst + dst_len)
		{
			p = FormatPrefix(p);
			p = FormatByte(p, (*q >> 8) & 0xff);
			p = FormatByte(p, (*q) & 0xff);
			prev_escaped = true;
		}
		else
		{
			/*
			 * If we get here, the given dst_len was insufficient
			 * for holding the debug-formatted data. This is a bug
			 * and should be fixed. However, in the meantime, we can
			 * at least '\0'-terminate the string (if there is room)
			 * to let the user know (by seeing a debug string that
			 * ends prematurely) that not all of the data is present
			 * in the string.
			 */
			if (p > dst && p == dst + dst_len)
				p--; // Backtrack one byte to fit '\0'.
			if (p < dst + dst_len)
				*(p++) = '\0';
			break;
		}
		q++;
	}
	return p - dst;
}

#endif // OPDATA_DEBUG
