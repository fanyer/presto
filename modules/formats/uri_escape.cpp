/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef FORMATS_URI_ESCAPE_SUPPORT

#include "modules/unicode/unicode.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/encodings/decoders/inputconverter.h"

BOOL UriEscape::NeedEscape(uni_char ch, int escape_flags)
{
	if (ch == ' ' && (escape_flags & (UsePlusForSpace | UsePlusForSpace_DEF)) != 0)
		return FALSE;
	return ch <= 0xff && (need_escape_masks[ch] & escape_flags) != 0;
}

BOOL UriEscape::NeedModification(uni_char ch, int escape_flags)
{
	if (ch == ' ' && (escape_flags & (UsePlusForSpace | UsePlusForSpace_DEF)) != 0)
		return TRUE;
	return NeedEscape(ch, escape_flags);
}

int UriEscape::CountEscapes(const char* str, int escape_flags)
{
	int need_escape_count = 0;
	char ch;

	if (!str)
		return 0;

	while ((ch = *str++) != 0)
		if (NeedEscape(ch, escape_flags))
			need_escape_count++;

	return need_escape_count;
}

int UriEscape::CountEscapes(const uni_char* str, int escape_flags)
{
	int need_escape_count = 0;
	uni_char ch;

	if (!str)
		return 0;

	while ((ch = *str++) != 0)
		if (NeedEscape(ch, escape_flags))
			need_escape_count++;

	return need_escape_count;
}

int UriEscape::CountEscapes(const char* str, int len, int escape_flags)
{
	int need_escape_count = 0;

	if (!str)
		return 0;

	for (int i=0; i<len; i++)
		if (NeedEscape(str[i], escape_flags))
			need_escape_count++;

	return need_escape_count;
}

int UriEscape::CountEscapes(const uni_char* str, int len, int escape_flags)
{
	int need_escape_count = 0;

	if (!str)
		return 0;

	for (int i=0; i<len; i++)
		if (NeedEscape(str[i], escape_flags))
			need_escape_count++;

	return need_escape_count;
}

int UriEscape::GetEscapedLength(const char* str, int escape_flags)
{
	int need_escape_count = 0, length = 0;
	char ch;

	if (!str)
		return 0;

	while ((ch = *str++) != 0) {
		if (NeedEscape(ch, escape_flags))
			need_escape_count++;
		length++;
	}

	return length + need_escape_count * ((escape_flags & PrefixBackslashX) != 0 ? 3 : 2);
}

int UriEscape::GetEscapedLength(const uni_char* str, int escape_flags)
{
	int need_escape_count = 0, length = 0;
	uni_char ch;

	if (!str)
		return 0;

	while ((ch = *str++) != 0) {
		if (NeedEscape(ch, escape_flags))
			need_escape_count++;
		length++;
	}

	return length + need_escape_count * ((escape_flags & PrefixBackslashX) != 0 ? 3 : 2);
}

int UriEscape::GetEscapedLength(const char* str, int len, int escape_flags)
{
	return len + CountEscapes(str, len, escape_flags) * ((escape_flags & PrefixBackslashX) != 0 ? 3 : 2);
}

int UriEscape::GetEscapedLength(const uni_char* str, int len, int escape_flags)
{
	return len + CountEscapes(str, len, escape_flags) * ((escape_flags & PrefixBackslashX) != 0 ? 3 : 2);
}

int UriEscape::EscapeIfNeeded(char* dst, char ch, int escape_flags)
{
	char* d = dst;
	if (ch == ' ' && (escape_flags & (UsePlusForSpace | UsePlusForSpace_DEF)) != 0)
		*d++ = '+';
	else if (NeedEscape(ch, escape_flags))
	{
		if ((escape_flags & PrefixBackslashX) != 0)
		{
			*d++ = '\\';
			*d++ = 'x';
		}
		else if ((escape_flags & PrefixBackslash) != 0)
			*d++ = '\\';
		else
			*d++ = '%';
		*d++ = EscapeFirst(ch);
		*d++ = EscapeLast(ch);
	}
	else
		*d++ = ch;
	return (int)(d-dst);
}

int UriEscape::EscapeIfNeeded(uni_char* dst, uni_char ch, int escape_flags)
{
	uni_char* d = dst;
	if (ch == ' ' && (escape_flags & (UsePlusForSpace | UsePlusForSpace_DEF)) != 0)
		*d++ = '+';
	else if (NeedEscape(ch, escape_flags))
	{
		if ((escape_flags & PrefixBackslashX) != 0)
		{
			*d++ = '\\';
			*d++ = 'x';
		}
		else if ((escape_flags & PrefixBackslash) != 0)
			*d++ = '\\';
		else
			*d++ = '%';
		*d++ = EscapeFirst((char)ch);
		*d++ = EscapeLast((char)ch);
	}
	else
		*d++ = ch;
	return (int)(d-dst);
}

int UriEscape::Escape(char* dst, const char* src, int escape_flags)
{
	char ch, *d = dst;
	while ((ch = *src++) != 0)
		d += EscapeIfNeeded(d, ch, escape_flags);
	*d = '\0';
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, const char* src, int escape_flags)
{
	uni_char ch, *d = dst;
	while ((ch = (uni_char)(unsigned char)*src++) != 0)
		d += EscapeIfNeeded(d, ch, escape_flags);
	*d = '\0';
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, const uni_char* src, int escape_flags)
{
	uni_char ch, *d = dst;
	while ((ch = *src++) != 0)
		d += EscapeIfNeeded(d, ch, escape_flags);
	*d = '\0';
	return (int)(d-dst);
}

int UriEscape::Escape(char* dst, const char* src, int src_len, int escape_flags)
{
	char *d = dst;
	for (int i=0; i<src_len; i++)
		d += EscapeIfNeeded(d, src[i], escape_flags);
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, const char* src, int src_len, int escape_flags)
{
	uni_char *d = dst;
	for (int i=0; i<src_len; i++)
		d += EscapeIfNeeded(d, (uni_char)(unsigned char)src[i], escape_flags);
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, const uni_char* src, int src_len, int escape_flags)
{
	uni_char *d = dst;
	for (int i=0; i<src_len; i++)
		d += EscapeIfNeeded(d, src[i], escape_flags);
	return (int)(d-dst);
}

int UriEscape::Escape(char* dst, int dst_maxlen, const char* src, int src_len, int escape_flags, int* src_consumed)
{
	char ch, *d = dst;
	int i;
	for (i=0; i<src_len && d-dst<dst_maxlen; i++)
	{
		ch = src[i];
		if (NeedEscape(ch, escape_flags) &&
			d-dst + ((escape_flags & PrefixBackslashX) != 0 ? 4 : 3) > dst_maxlen)
			break;
		d += EscapeIfNeeded(d, ch, escape_flags);
	}
	if (src_consumed)
		*src_consumed = i;
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, int dst_maxlen, const char* src, int src_len, int escape_flags, int* src_consumed)
{
	uni_char ch, *d = dst;
	int i;
	for (i=0; i<src_len && d-dst<dst_maxlen; i++)
	{
		ch = (uni_char)(unsigned char)src[i];
		if (NeedEscape(ch, escape_flags) &&
			d-dst + ((escape_flags & PrefixBackslashX) != 0 ? 4 : 3) > dst_maxlen)
			break;
		d += EscapeIfNeeded(d, ch, escape_flags);
	}
	if (src_consumed)
		*src_consumed = i;
	return (int)(d-dst);
}

int UriEscape::Escape(uni_char* dst, int dst_maxlen, const uni_char* src, int src_len, int escape_flags, int* src_consumed)
{
	uni_char ch, *d = dst;
	int i;
	for (i=0; i<src_len && d-dst<dst_maxlen; i++)
	{
		ch = src[i];
		if (NeedEscape(ch, escape_flags) &&
			d-dst + ((escape_flags & PrefixBackslashX) != 0 ? 4 : 3) > dst_maxlen)
			break;
		d += EscapeIfNeeded(d, ch, escape_flags);
	}
	if (src_consumed)
		*src_consumed = i;
	return (int)(d-dst);
}

OP_STATUS UriEscape::AppendEscaped(OpString8& dst, char ch, int escape_flags)
{
	char escape[4]; /* ARRAY OK 2009-01-06 roarl */
	int escape_len = EscapeIfNeeded(escape, ch, escape_flags);
	return dst.Append(escape, escape_len);  
}

OP_STATUS UriEscape::AppendEscaped(OpString&  dst, uni_char ch, int escape_flags)
{
	uni_char escape[4]; /* ARRAY OK 2009-01-06 roarl */
	int escape_len = EscapeIfNeeded(escape, ch, escape_flags);
	return dst.Append(escape, escape_len);  
}

OP_STATUS UriEscape::AppendEscaped(OpString8& dst, const char* src, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	Escape(dst.CStr()+dst_len, src, escape_flags);
	return OpStatus::OK;
}

OP_STATUS UriEscape::AppendEscaped(OpString& dst, const char* src, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	Escape(dst.CStr()+dst_len, src, escape_flags);
	return OpStatus::OK;
}

OP_STATUS UriEscape::AppendEscaped(OpString&  dst, const uni_char* src, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	Escape(dst.CStr()+dst_len, src, escape_flags);
	return OpStatus::OK;
}

OP_STATUS UriEscape::AppendEscaped(OpString8& dst, const char* src, int src_len, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, src_len, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	char* d_str = dst.CStr();
	dst_len += Escape(d_str+dst_len, src, src_len, escape_flags);
	d_str[dst_len] = 0;
	return OpStatus::OK;
}

OP_STATUS UriEscape::AppendEscaped(OpString& dst, const char* src, int src_len, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, src_len, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	uni_char* d_str = dst.CStr();
	dst_len += Escape(d_str+dst_len, src, src_len, escape_flags);
	d_str[dst_len] = 0;
	return OpStatus::OK;
}

OP_STATUS UriEscape::AppendEscaped(OpString&  dst, const uni_char* src, int src_len, int escape_flags)
{
	int escaped_len = GetEscapedLength(src, src_len, escape_flags);
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+escaped_len+1)) return OpStatus::ERR_NO_MEMORY;
	uni_char* d_str = dst.CStr();
	dst_len += Escape(d_str+dst_len, src, src_len, escape_flags);
	d_str[dst_len] = 0;
	return OpStatus::OK;
}

// To change the need-escape mask values, modify the much more explicit
// code in Test_GetNeedEscapeMasks in uri_escape.ot, and look at
// the output from the selftest
const int UriEscape::need_escape_masks[256] = {
	0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,
	0x05001,0x05002,0x05002,0x05001,0x05001,0x05002,0x05001,0x05001,
	0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,
	0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,0x05001,
	0x25c80,0x00880,0x25c80,0x03890,0x01880,0x078a0,0x03980,0x04880,
	0x04880,0x04880,0x04080,0x03880,0x05880,0x00000,0x00000,0x05004,
	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x05a80,0x07a80,0x25c80,0x058c0,0x25c80,0x05980,
	0x05880,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x05880,0x05888,0x05880,0x01c80,0x00000,
	0x21c80,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,0x00000,
	0x00000,0x00000,0x00000,0x01c80,0x01c80,0x01c80,0x00880,0x05001,
	0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,
	0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,
	0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,
	0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,0x0d000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
	0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,0x15000,
};

int UriUnescape::Unescape(char* dst, const char* src, int unescape_flags)
{
	UriUnescapeIterator it(src, unescape_flags);
	int new_len = 0;
	while (it.More())
		dst[new_len++] = it.Next();
	dst[new_len] = '\0';
	return new_len;
}

int UriUnescape::Unescape(uni_char* dst, const char* src, int unescape_flags)
{
	UriUnescapeIterator it(src, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&dst[new_len], it.NextUni());
	dst[new_len] = '\0';
	return new_len;
}

int UriUnescape::Unescape(uni_char* dst, const uni_char* src, int unescape_flags)
{
	UriUnescapeIterator_Uni it(src, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&dst[new_len], it.Next());
	dst[new_len] = '\0';
	return new_len;
}

int UriUnescape::Unescape(char* dst, const char* src, int src_len, int unescape_flags)
{
	UriUnescapeIterator_N it(src, src_len, unescape_flags);
	int new_len = 0;
	while (it.More())
		dst[new_len++] = it.Next();
	dst[new_len] = '\0';
	return new_len;
}

int UriUnescape::Unescape(uni_char* dst, const char* src, int src_len, int unescape_flags)
{
	UriUnescapeIterator_N it(src, src_len, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&dst[new_len], it.NextUni());
	dst[new_len] = '\0';
	return new_len;
}

int UriUnescape::Unescape(uni_char* dst, const uni_char* src, int src_len, int unescape_flags)
{
	UriUnescapeIterator_Uni_N it(src, src_len, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&dst[new_len], it.Next());
	dst[new_len] = '\0';
	return new_len;
}

OP_STATUS UriUnescape::UnescapeJavascriptURL(OpString& dest, const char* str)
{
	InputConverter *decoder;
	RETURN_IF_ERROR(InputConverter::CreateCharConverter("utf-8", &decoder));
	OpAutoPtr<InputConverter> anchor_decoder(decoder);

	if (!dest.Reserve(op_strlen(str)))
		return OpStatus::ERR_NO_MEMORY;

	uni_char enc_buffer[2]; /* ARRAY OK 2012-05-02 sof */

	const char *previous = str;
	uni_char surrogate = NOT_A_CHARACTER;

	UriUnescapeIterator it(str, All);
	while (it.More())
	{
		char ch = it.Next();
		int processed_bytes;
		int written;
		if ((written = decoder->Convert(&ch, 1, enc_buffer, ARRAY_SIZE(enc_buffer) * sizeof(uni_char), &processed_bytes)) == -1)
			return OpStatus::ERR_NO_MEMORY;

		if (written >= static_cast<int>(sizeof(uni_char)) && enc_buffer[0] == NOT_A_CHARACTER)
		{
			unsigned count = it.Current() - previous;
			/* Check if due to an invalid sequence, or a literal NOT_A_CHARACTER. */
			if (!(count == 3 && static_cast<unsigned char>(previous[0]) == 0xef && static_cast<unsigned char>(previous[1]) == 0xbf && static_cast<unsigned char>(previous[2]) == 0xbd) &&
			    !(count == 9 && op_strnicmp(previous, "%ef%bf%bd", 9) == 0))
			{
				UriUnescapeIterator_N inner(previous, count, All);
				while (inner.More())
				{
					char i_ch = inner.Next();
					uni_char expanded = static_cast<uni_char>(static_cast<unsigned char>(i_ch));
					RETURN_IF_ERROR(dest.Append(&expanded, 1));
				}

				previous = it.Current();
				continue;
			}
			else
				previous = it.Current();
		}
		else if (written == sizeof(uni_char) && Unicode::IsHighSurrogate(enc_buffer[0]))
		{
			/* A surrogate pairing was started..keep track of this should the paired
			   one turn out invalid. */
			surrogate = enc_buffer[0];
			continue;
		}
		else if (written == sizeof(uni_char) && Unicode::IsLowSurrogate(enc_buffer[0]) && surrogate != NOT_A_CHARACTER)
		{
			/* Matched pair - output the delayed high part. */
			RETURN_IF_ERROR(dest.Append(&surrogate, 1));

			surrogate = NOT_A_CHARACTER;
		}
		else if (written == sizeof(uni_char))
			previous = it.Current();

		if (written > 0)
			RETURN_IF_ERROR(dest.Append(enc_buffer, written / sizeof(uni_char)));
	}

	/* Flush out incomplete or invalid sequences. */
	if (it.Current() > previous)
	{
		unsigned count = it.Current() - previous;
		UriUnescapeIterator_N inner(previous, count, All);
		while (inner.More())
		{
			char i_ch = inner.Next();
			uni_char expanded = static_cast<uni_char>(static_cast<unsigned char>(i_ch));
			RETURN_IF_ERROR(dest.Append(&expanded, 1));
		}
	}
	return OpStatus::OK;
}

int UriUnescape::ReplaceChars(char* str, int unescape_flags)
{
	UriUnescapeIterator it(str, unescape_flags);
	int new_len = 0;
	while (it.More())
		str[new_len++] = it.Next();
	str[new_len] = '\0';
	return new_len;
}

int UriUnescape::ReplaceChars(uni_char* str, int unescape_flags)
{
	UriUnescapeIterator_Uni it(str, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&str[new_len], it.Next());
	str[new_len] = '\0';
	return new_len;
}

void UriUnescape::ReplaceChars(char* str, int& length, int unescape_flags)
{
	UriUnescapeIterator_N it(str, length, unescape_flags);
	int new_len = 0;
	while (it.More())
		str[new_len++] = it.Next();
	length = new_len;
}

void UriUnescape::ReplaceChars(uni_char* str, int& length, int unescape_flags)
{
	UriUnescapeIterator_Uni_N it(str, length, unescape_flags);
	int new_len = 0;
	while (it.More())
		new_len += Unicode::WriteUnicodePoint(&str[new_len], it.Next());
	length = new_len;
}

int UriUnescape::strcmp(const char* s1, const char* s2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(::op_strcmp(s1, s2) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they match
	UriUnescapeIterator i1(s1, unescape_flags);
	UriUnescapeIterator i2(s2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
		if ((diff = (int)(unsigned char)i1.Next() - (int)(unsigned char)i2.Next()) != 0)
			return diff;
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::strcmp(const uni_char* s1, const uni_char* s2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(::uni_strcmp(s1, s2) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they match
	UriUnescapeIterator_Uni i1(s1, unescape_flags);
	UriUnescapeIterator_Uni i2(s2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
		if ((diff = (int)i1.Next() - (int)i2.Next()) != 0)
			return diff;
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::stricmp(const char* s1, const char* s2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(::op_stricmp(s1, s2) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they
	// match case-insensitively
	UriUnescapeIterator i1(s1, unescape_flags);
	UriUnescapeIterator i2(s2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
		if ((diff = op_tolower((unsigned char)i1.Next()) - op_tolower((unsigned char)i2.Next())) != 0)
			return diff;
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::stricmp(const uni_char* s1, const uni_char* s2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(::uni_stricmp(s1, s2) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they
	// match case-insensitively
	UriUnescapeIterator_Uni i1(s1, unescape_flags);
	UriUnescapeIterator_Uni i2(s2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
		if ((diff = op_tolower(i1.Next()) - op_tolower(i2.Next())) != 0)
			return diff;
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::strncmp(const char* s1, int n1, const char* s2, int n2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(n1 == n2 && ::op_strncmp(s1, s2, n1) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they match
	UriUnescapeIterator_N i1(s1, n1, unescape_flags);
	UriUnescapeIterator_N i2(s2, n2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
	{
		int ch1 = (unsigned char)i1.Next();
		int ch2 = (unsigned char)i2.Next();
		if (ch1 == 0 && ch2 == 0)
			return 0;
		if ((diff = ch1 - ch2) != 0)
			return diff;
	}
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::strncmp(const uni_char* s1, int n1, const uni_char* s2, int n2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(n1 == n2 && ::uni_strncmp(s1, s2, n1) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they match
	UriUnescapeIterator_Uni_N i1(s1, n1, unescape_flags);
	UriUnescapeIterator_Uni_N i2(s2, n2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
	{
		int ch1 = i1.Next();
		int ch2 = i2.Next();
		if (ch1 == 0 && ch2 == 0)
			return 0;
		if ((diff = ch1 - ch2) != 0)
			return diff;
	}
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::strnicmp(const char* s1, int n1, const char* s2, int n2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(n1 == n2 && ::op_strnicmp(s1, s2, n1) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they
	// match case-insensitively
	UriUnescapeIterator_N i1(s1, n1, unescape_flags);
	UriUnescapeIterator_N i2(s2, n2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
	{
		int ch1 = op_tolower((unsigned char)i1.Next());
		int ch2 = op_tolower((unsigned char)i2.Next());
		if (ch1 == 0 && ch2 == 0)
			return 0;
		if ((diff = ch1 - ch2) != 0)
			return diff;
	}
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

int UriUnescape::strnicmp(const uni_char* s1, int n1, const uni_char* s2, int n2, int unescape_flags)
{
	// Quick compare; check if they are already matching case, without unescaping
	if(n1 == n2 && ::uni_strnicmp(s1, s2, n1) == 0)
		return 0;

	// If we did not get a case-sensitive match, unescape and check if they
	// match case-insensitively
	UriUnescapeIterator_Uni_N i1(s1, n1, unescape_flags);
	UriUnescapeIterator_Uni_N i2(s2, n2, unescape_flags);
	int diff;
	while (i1.More() && i2.More())
	{
		int ch1 = op_tolower(i1.Next());
		int ch2 = op_tolower(i2.Next());
		if (ch1 == 0 && ch2 == 0)
			return 0;
		if ((diff = ch1 - ch2) != 0)
			return diff;
	}
	return (i1.More() ? 1 : (i2.More() ? -1 : 0));
}

BOOL UriUnescape::isstrprefix(const char* prefix, const char* s, int unescape_flags)
{
	UriUnescapeIterator i1(prefix, unescape_flags);
	UriUnescapeIterator i2(s, unescape_flags);
	while (i1.More() && i2.More())
		if (i1.Next() != i2.Next())
			return FALSE;
	return (i1.More() ? FALSE : TRUE);
}

BOOL UriUnescape::isstrprefix(const uni_char* prefix, const uni_char* s, int unescape_flags)
{
	UriUnescapeIterator_Uni i1(prefix, unescape_flags);
	UriUnescapeIterator_Uni i2(s, unescape_flags);
	while (i1.More() && i2.More())
		if (i1.Next() != i2.Next())
			return FALSE;
	return (i1.More() ? FALSE : TRUE);
}

BOOL UriUnescape::ContainsValidEscapedUtf8(const char* str, int len)
{
	BOOL illegal_escaping_found, utf8_escaping_found;
	if (len == KAll) {
		UriUnescapeIterator it(str, UriUnescape::ConvertUtf8);
		while (it.More())
			it.NextUni();
		it.GetStatus(&illegal_escaping_found, &utf8_escaping_found);
	}
	else
	{
		UriUnescapeIterator_N it(str, len, UriUnescape::ConvertUtf8);
		while (it.More())
			it.NextUni();
		it.GetStatus(&illegal_escaping_found, &utf8_escaping_found);
	}
	return utf8_escaping_found && !illegal_escaping_found;
}

BOOL UriUnescape::ContainsValidEscapedUtf8(const uni_char* str, int len)
{
	BOOL illegal_escaping_found, utf8_escaping_found;
	if (len == KAll) {
		UriUnescapeIterator_Uni it(str, UriUnescape::ConvertUtf8);
		while (it.More())
			it.Next();
		it.GetStatus(&illegal_escaping_found, &utf8_escaping_found);
	}
	else
	{
		UriUnescapeIterator_Uni_N it(str, len, UriUnescape::ConvertUtf8);
		while (it.More())
			it.Next();
		it.GetStatus(&illegal_escaping_found, &utf8_escaping_found);
	}
	return utf8_escaping_found && !illegal_escaping_found;
}

// To change unescape-exception mask values, modify the much more explicit
// code in Test_GetUnescapeExceptionMasks in uri_escape.ot, and look at
// the output from the selftest
const unsigned char UriUnescapeIteratorBase::unescape_exception_masks[256] = {
	0x21,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x01,0x02,0x02,0x02,0x02,0x02,0x01,0x01,
	0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x01,0x01,0x01,0x04,0x01,0x01,0x01,0x01,
	0x08,0x00,0x20,0x20,0x20,0x20,0x20,0x00,
	0x00,0x00,0x00,0x20,0x20,0x00,0x00,0x20,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x00,0x00,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,
	0x90,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
};

BOOL UriUnescapeIteratorBase::IsExcepted(uni_char ch)
{
	return (ch == 0 && IsNullTerminated()) || (unescape_exception_masks[ch] & m_unescape_flags) != 0;
}

UnicodePoint UriUnescapeIteratorBase::UnescapeAndAdvance(uni_char current, BOOL output_is_8bit)
{
	if (current == '+' && (m_unescape_flags & UriUnescape::TranslatePlusToSpace) != 0)
		return ' ';
#if PATHSEPCHAR != '/'
	if (current == '/' && (m_unescape_flags & UriUnescape::TranslatePathSepChar) != 0)
		return PATHSEPCHAR;
#endif
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	if (current == '|' && (m_unescape_flags & UriUnescape::TranslateDriveChar) != 0)
		return ':';
#endif
	// Stop unescaping when finding a query marker (bug #49350)
	if (current == '?' && (m_unescape_flags & UriUnescape::StopAfterQuery) != 0)
		m_in_query = TRUE;

	if (Unicode::IsHighSurrogate(current))
	{
		uni_char next;
		if (m_length > 1 && Unicode::IsLowSurrogate(next = Get(0)))
		{
			// Decode a surrogate pair
			Advance(1);
			return Unicode::DecodeSurrogate(current, next);
		}
		// Any other use of surrogates are illegal, just return unmodified
		return current;
	}

	if (current != '%' || m_in_query)
		return current;

	if (m_unescape_prevent_count != 0)
	{
		m_unescape_prevent_count--;
		return current;
	}

	uni_char c1, c2, c3, c4;
	if (m_length < 2 || !uni_isxdigit(c1 = Get(0)) || !uni_isxdigit(c2 = Get(1)))
	{
		m_illegal_escaping_found = TRUE;
		return current;
	}
	unsigned char first = (unsigned char)UriUnescape::Decode((char)c1, (char)c2);

	if (first < 0x80 || m_unescape_enforce_count != 0 ||
		(m_unescape_flags & UriUnescape::ConvertUtf8) == 0)
	{
		if (m_unescape_enforce_count != 0)
			m_unescape_enforce_count--;
		else if (IsExcepted(first))
			return current;
		Advance(2);
		return first;
	}

	// At this point, it *should* be UTF-8
	if (m_length >= 5 && Get(2) == '%' &&
		uni_isxdigit(c1 = Get(3)) && uni_isxdigit(c2 = Get(4)))
	{
		unsigned char second = (unsigned char)UriUnescape::Decode((char)c1, (char)c2);
		UnicodePoint decoded = ((first & 0x1F) << 6) | (second & 0x3F);

		if ((first & 0xE0) == 0xC0 && (second & 0xC0) == 0x80 && decoded >= 0x80)
		{
			m_utf8_escaping_found = TRUE;
			if (output_is_8bit)
			{
				m_unescape_enforce_count = 1; // Make sure the next one is unescaped
				Advance(2);
				return first;
			}
			Advance(5);
			return decoded;
		}
		else if ((first & 0xF0) == 0xE0 && m_length >= 8 && Get(5) == '%' &&
				 uni_isxdigit(c1 = Get(6)) && uni_isxdigit(c2 = Get(7)))
		{
			unsigned char third  = (unsigned char)UriUnescape::Decode((char)c1, (char)c2);
			decoded = ((first & 0x1F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);

			if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80 && decoded >= 0x800)
			{
				m_utf8_escaping_found = TRUE;
				if (((m_unescape_flags & UriUnescape::ExceptUnsafeHigh) == 0 || (
					decoded != 0x115F &&  decoded != 0x1160 && decoded != 0x3164 && // Filler characters that should not be displayed unencoded due to spoofing issues
					decoded != 0x202E && // Right to left override should not be displayed. See bug 319181.
					!(decoded >= 0x2000 && decoded <= 0x200B) && decoded != 0x3000 // Spaces
				   )))
				{
					if (output_is_8bit)
					{
						m_unescape_enforce_count = 2; // Make sure the next two are unescaped
						Advance(2);
						return first;
					}
					Advance(8);
					return decoded;
				}
				else
				{
					m_unescape_prevent_count = 2; // Make sure the next two are not unescaped
					return current;
				}
			}
		}
		else if ((first & 0xF8) == 0xF0 && m_length >= 11 && Get(5) == '%' &&
		         uni_isxdigit(c1 = Get(6)) && uni_isxdigit(c2 = Get(7)) &&
		         Get(8) == '%' && uni_isxdigit(c3 = Get(9)) &&
				 uni_isxdigit(c4 = Get(10)))
		{
			unsigned char third  = (unsigned char)UriUnescape::Decode((char)c1, (char)c2);
			unsigned char fourth = (unsigned char)UriUnescape::Decode((char)c3, (char)c4);
			decoded = ((first & 0x0F) << 18) | ((second & 0x3F) << 12) |
			          ((third & 0x3F) << 6)  |  (fourth & 0x3F);

			if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80 &&
			    (fourth & 0xC0) == 0x80 && decoded >= 0x10000)
			{
				m_utf8_escaping_found = TRUE;
				if (output_is_8bit)
				{
					m_unescape_enforce_count = 3; // Make sure the next three are unescaped
					Advance(3);
					return first;
				}
				Advance(11);
				return decoded;
			}
			else
			{
				m_unescape_prevent_count = 3; // Make sure the next three are not unescaped
				return current;
			}
        }
	}

	m_illegal_escaping_found = TRUE;
	return current;
}

int UriUnescapeIteratorBase::OptimizedFlags(int unescape_flags)
{
	if ((unescape_flags & UriUnescape::ConvertUtf8IfPref) != 0 &&
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseUTF8Urls) != 0)
		unescape_flags |= UriUnescape::ConvertUtf8;
	return unescape_flags;
}

#endif // FORMATS_URI_ESCAPE_SUPPORT
