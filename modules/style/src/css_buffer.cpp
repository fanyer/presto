/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#include "modules/style/src/css_buffer.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/html5namemapper.h"
#include "modules/logdoc/html.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/src/css_selector.h"
#include "modules/style/src/css_properties.h"
#include "modules/style/src/css_values.h"

uni_char*
CSS_Buffer::CopyAndRemoveEscapes(uni_char* dst, const uni_char* src, unsigned int nchars)
{
	while (nchars > 0)
	{
		if (*src == '\\')
		{
			if (nchars > 1)
			{
				switch (src[1])
				{
				case '\r':
					if (nchars > 2 && src[2] == '\n')
					{
						src++;
						nchars--;
					}
					// fallthrough
				case '\n':
				case '\f':
					src += 2;
					nchars -= 2;
					continue;
					break;
				default:
					break;
				}
			}
			int ch = 0;
			int i = 0;
			while (--nchars > 0 && i < 6)
			{
				if (*++src >= '0' && *src <= '9')
					ch = (ch*16)+(*src-'0');
				else if (*src >= 'A' && *src <= 'F')
					ch = (ch*16)+(*src-'A'+10);
				else if (*src >= 'a' && *src <= 'f')
					ch = (ch*16)+(*src-'a'+10);
				else
					break;
				i++;
			}
			if (nchars == 0 || i == 6)
				src++;
			if (nchars > 0)
			{
				if (i == 0)
				{
					ch = *src++;
					nchars--;
				}
				else if (nchars > 0)
				{
					switch (src[0])
					{
					case '\r':
						if (nchars > 1 && src[1] == '\n')
						{
							src++;
							nchars--;
						}
						// fallthrough
					case '\n':
					case '\f':
					case '\t':
					case ' ':
						src++;
						nchars--;
					}
				}
			}

			if (Unicode::IsSurrogate(ch))
				*dst++ = 0xFFFD;
			else
				dst += Unicode::WriteUnicodePoint(dst, ch);
		}
		else
		{
			*dst++ = *src++;
			nchars--;
		}
	}
	*dst = 0;
	return dst;
}

uni_char*
CSS_Buffer::GetString(uni_char* ret_str, unsigned int start_pos, unsigned int length, BOOL unescape)
{
	OP_ASSERT(ret_str);
	uni_char* dst = ret_str;

	unsigned int bidx = 0;
	unsigned int pbase = 0;

	OP_ASSERT(start_pos+length <= Length());

	while (bidx < buf_cnt && start_pos > pbase+len[bidx])
		pbase += len[bidx++];

	OP_ASSERT(bidx < buf_cnt);

	if (bidx < buf_cnt)
	{
		const uni_char* cp = buf[bidx]+(start_pos-pbase);

		unsigned int tmp_len = len[bidx]-(start_pos-pbase);
		if (tmp_len > length)
			tmp_len = length;
		if (unescape)
			dst = CopyAndRemoveEscapes(dst, cp, tmp_len);
		else
			dst = uni_strncpy(dst, cp, tmp_len) + tmp_len;
		length -= tmp_len;

		while (length > 0 && ++bidx < buf_cnt)
		{
			cp = buf[bidx];
			if (len[bidx] > length)
				tmp_len = length;
			else
				tmp_len = len[bidx];
			if (unescape)
				dst = CopyAndRemoveEscapes(dst, cp, tmp_len);
			else
				dst = uni_strncpy(dst, cp, tmp_len) + tmp_len;
			length -= tmp_len;
		}
	}

	*dst = 0;
	return dst;
}

uni_char*
CSS_Buffer::GetString(unsigned int start_pos, unsigned int length, BOOL unescape)
{
	uni_char* ret_str = OP_NEWA(uni_char, length+1);
	if (ret_str)
		GetString(ret_str, start_pos, length, unescape);
	return ret_str;
}

uni_char*
CSS_Buffer::GetNameAndVal(unsigned int name_start, unsigned int name_len, unsigned int val_start, unsigned int val_len)
{
	uni_char* ret_str = OP_NEWA(uni_char, name_len+val_len+2);
	if (ret_str)
	{
		GetString(ret_str, name_start, name_len);
		// adjust name_len to the length used after escapes are replaced.
		name_len = uni_strlen(ret_str);
		GetString(ret_str+name_len+1, val_start, val_len);
	}
	return ret_str;
}

uni_char*
CSS_Buffer::GetSkinString(unsigned int start_pos, unsigned int length)
{
	length += 2; // make room for "s:" prefix.
	uni_char* buf = OP_NEWA(uni_char, length+1);
	uni_char* ret = buf;
	if (buf)
	{
		*buf++ = 's';
		*buf++ = ':';
		length -= 2;
		GetString(buf, start_pos, length);
	}
	return ret;
}

CSSValue
CSS_Buffer::GetValueSymbol(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(CSS_VALUE_NAME_MAX_SIZE)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(CSS_VALUE_NAME_MAX_SIZE))
	{
		GetString(ident, start_pos, length);
		return CSS_GetKeyword(ident, uni_strlen(ident));
	}
	else
		return CSS_VALUE_UNKNOWN;
}

short
CSS_Buffer::GetPropertySymbol(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(CSS_PROPERTY_NAME_MAX_SIZE)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(CSS_PROPERTY_NAME_MAX_SIZE))
	{
		GetString(ident, start_pos, length);
		length = uni_strlen(ident);
		return GetCSS_Property(ident, length);
	}
	else
		return -1;
}

unsigned short
CSS_Buffer::GetPseudoSymbol(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(13)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(13))
	{
		GetString(ident, start_pos, length);
		return CSS_GetPseudoClass(ident, uni_strlen(ident));
	}
	else
		return PSEUDO_CLASS_UNKNOWN;
}

short
CSS_Buffer::GetPseudoPage(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(5)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(5))
	{
		GetString(ident, start_pos, length);
		return CSS_GetPseudoPage(ident, uni_strlen(ident));
	}
	else
		return PSEUDO_PAGE_UNKNOWN;
}

CSS_MediaType
CSS_Buffer::GetMediaType(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(10)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(10))
	{
		GetString(ident, start_pos, length);
		return ::GetMediaType(ident, uni_strlen(ident), FALSE);
	}
	else
		return CSS_MEDIA_TYPE_UNKNOWN;
}

int
CSS_Buffer::GetTag(unsigned int start_pos, unsigned int length, NS_Type ns_type, BOOL case_sensitive, BOOL any_namespace)
{
	if (length > tag_buf_len - 1)
	{
		uni_char* new_buf = OP_NEWA(uni_char, length + 1);
		if (!new_buf)
			return Markup::HTE_UNKNOWN;

		if (tag_buf != orig_tag_buf)
			OP_DELETEA(tag_buf);
		tag_buf = new_buf;
		tag_buf_len = length + 1;
	}

	GetString(tag_buf, start_pos, length);
	if (any_namespace)
		return g_html5_name_mapper->GetTypeFromNameAmbiguous(tag_buf, case_sensitive);
	else
		return HTM_Lex::GetElementType(tag_buf, ns_type, case_sensitive);
}

int
CSS_Buffer::GetHtmlAttr(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(Markup::kMaxAttributeNameLength)+1]; /* ARRAY OK 2011-06-27 stighal */

	if (length <= CSS_ESCAPED_IDENT_LEN(Markup::kMaxAttributeNameLength))
	{
		GetString(ident, start_pos, length);
		int attr = g_html5_name_mapper->GetAttrTypeFromName(ident, FALSE, Markup::HTML);
		return attr;
	}
	else
		return Markup::HA_XML;
}

COLORREF
CSS_Buffer::GetNamedColorIndex(unsigned int start_pos, unsigned int length)
{
	COLORREF color = USE_DEFAULT_COLOR;
	uni_char ident[CSS_ESCAPED_IDENT_LEN(COLOR_MAX_SIZE)+1]; /* ARRAY OK 2009-02-05 rune */
	if (length <= CSS_ESCAPED_IDENT_LEN(COLOR_MAX_SIZE))
	{
		GetString(ident, start_pos, length);
		color = HTM_Lex::GetColIdxByName(ident, uni_strlen(ident));
		if ((color & CSS_COLOR_KEYWORD_TYPE_x_color) == USE_DEFAULT_COLOR)
		{
			// hack to allow hexcolors with missing hash '#'.
			if (length == 3 || length == 6)
				color = GetColor(start_pos, length);
		}
	}
	return color;
}

URL
CSS_Buffer::GetURLL(const URL& base_url, unsigned int start_pos, unsigned int length)
{
	uni_char* str = GetString(start_pos, length);

	if (!str)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	ANCHOR_ARRAY(uni_char, str);

	URL resolved_url = g_url_api->GetURL(base_url, str);
	if (resolved_url.Type() == URL_JAVASCRIPT)
		return URL();
	else
		return resolved_url;
}

CSS_MediaFeature
CSS_Buffer::GetMediaFeature(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(MEDIA_FEATURE_NAME_MAX_SIZE)+1]; /* ARRAY OK 2009-02-05 rune */

	if (length <= CSS_ESCAPED_IDENT_LEN(MEDIA_FEATURE_NAME_MAX_SIZE))
	{
		GetString(ident, start_pos, length);
		return ::GetMediaFeature(ident, uni_strlen(ident));
	}
	return MEDIA_FEATURE_unknown;
}

CSS_WebFont::Format
CSS_Buffer::GetFontFormat(unsigned int start_pos, unsigned int length)
{
	if (length == 3 || length == 4 || length == 8 || length == 12 || length == 17)
	{
		uni_char format_str[18]; /* ARRAY OK 2008-04-02 rune */
		GetString(format_str, start_pos, length);

		CSS_WebFont::Format format = CSS_WebFont::FORMAT_UNSUPPORTED;
		switch (length)
		{
		case 3:
			if (uni_stri_eq(format_str, "SVG"))
				format = CSS_WebFont::FORMAT_SVG;
			break;
		case 4:
			if (uni_stri_eq(format_str, "WOFF"))
				format = CSS_WebFont::FORMAT_WOFF;
			break;
		case 8:
			if (uni_stri_eq(format_str, "TRUETYPE"))
				format = CSS_WebFont::FORMAT_TRUETYPE;
			else if (uni_stri_eq(format_str, "OPENTYPE"))
				format = CSS_WebFont::FORMAT_OPENTYPE;
			break;
		case 12:
			if (uni_stri_eq(format_str, "TRUETYPE-AAT"))
				format = CSS_WebFont::FORMAT_TRUETYPE_AAT;
			break;
		case 17:
			if (uni_stri_eq(format_str, "EMBEDDED-OPENTYPE"))
				format = CSS_WebFont::FORMAT_EMBEDDED_OPENTYPE;
			break;
		}

		if (g_webfont_manager->SupportsFormat(format))
			return format;
	}
	return CSS_WebFont::FORMAT_UNSUPPORTED;
}

#ifdef CSS_ERROR_SUPPORT
uni_char*
CSS_Buffer::GetLineString(unsigned int start_pos, unsigned int max_len)
{
	if (PushState())
	{
		SetCurrentPos(start_pos);
		int c(0);
		unsigned int i(0);
		while (i++ < max_len && (c = GetNextChar()) != 0 && c != '\n' && c != '\r')
			;
		int end_pos = CurrentPos();
		if (c == '\n' || c == '\r')
			--end_pos;
		RestoreState();
		return GetString(start_pos, end_pos-start_pos, FALSE);
	}
	else
		return NULL;
}
#endif // CSS_ERROR_SUPPORT

#ifdef SAVE_SUPPORT

void
CSS_Buffer::WriteToStreamL(UnicodeFileOutputStream* stream, unsigned int start_pos, int length)
{
	if (PushState())
	{
		unsigned int remaining_length;
		if (length >= 0)
			remaining_length = length;
		else
			remaining_length = Length()-start_pos;

		SetCurrentPos(start_pos);
		while (remaining_length > 0 && BytesLeft() > 0)
		{
			if (bytes_left == 0)
			{
				// Force switching to next buffer.
				GetNextChar();
				UnGetChar();
			}
			int write_len = remaining_length > bytes_left ? bytes_left : remaining_length;
			remaining_length -= write_len;

			stream->WriteStringL(in_buffer, write_len);

			EatChars(write_len);
		}
		RestoreState();
	}
}

#endif // SAVE_SUPPORT

#ifdef CSS_ANIMATIONS
double
CSS_Buffer::GetKeyframePosition(unsigned int start_pos, unsigned int length)
{
	uni_char ident[CSS_ESCAPED_IDENT_LEN(4)+1]; /* ARRAY OK 2011-09-09 davve */

	if (length <= CSS_ESCAPED_IDENT_LEN(4))
	{
		GetString(ident, start_pos, length);
		return CSS_GetKeyframePosition(ident, uni_strlen(ident));
	}
	else
		return -1;
}
#endif // CSS_ANIMATIONS

#ifdef LIMITED_FOOTPRINT_DEVICE

CSS_Buffer::~CSS_Buffer()
{
	if (tag_buf != orig_tag_buf)
		OP_DELETEA(tag_buf);

	OP_DELETEA(len);
	OP_DELETEA(buf);
}

BOOL CSS_Buffer::AllocBufferArrays(unsigned int count)
{
	if (count > 0)
	{
		len = OP_NEWA(unsigned int, count);
		if (!len)
			return FALSE;
		buf = OP_NEWA(const uni_char*, count);
		if (!buf)
		{
			OP_DELETEA(len);
			len = NULL;
			return FALSE;
		}
	}
	return (len && buf);
}

void CSS_Buffer::AddBuffer(const uni_char* buffer, unsigned int length)
{
	if (length > 0 && buffer!=NULL)
	{
		if (buf_cnt == 0)
		{
			in_buffer = buffer;
			bytes_left = length;
		}
		buf[buf_cnt] = buffer;
		len[buf_cnt++] = length;
		total_len += length;
	}
}

BOOL CSS_Buffer::PushState()
{
	if (stack_idx < STACK_SIZE)
	{
		stored_bleft[stack_idx] = bytes_left;
		stored_buf[stack_idx] = in_buffer;
		stored_pos_base[stack_idx] = pos_base;
		stored_buf_idx[stack_idx++] = buf_idx;
		return TRUE;
	}
	else
		return FALSE;
}

void CSS_Buffer::PopState()
{
	if (stack_idx > 0)
		--stack_idx;
}

BOOL CSS_Buffer::RestoreState()
{
	if (stack_idx > 0)
	{
		bytes_left = stored_bleft[--stack_idx];
		in_buffer = stored_buf[stack_idx];
		pos_base = stored_pos_base[stack_idx];
		buf_idx = stored_buf_idx[stack_idx];
		return TRUE;
	}
	else
		return FALSE;
}

unsigned int CSS_Buffer::BytesLeft()
{
	return buf_cnt > 0 ? total_len - (pos_base + len[buf_idx]-bytes_left) : 0;
}

uni_char CSS_Buffer::LookAhead(unsigned int n)
{
	if (bytes_left > n)
		return in_buffer[n];

	unsigned int la_cnt = bytes_left;
	for (unsigned int i=buf_idx+1; i<buf_cnt; i++)
	{
		if (la_cnt+len[i] > n)
			return buf[i][n-la_cnt];
		la_cnt += len[i];
	}
	return 0;
}

uni_char CSS_Buffer::GetNextChar()
{
	if (bytes_left > 0)
	{
		--bytes_left;
		return *in_buffer++;
	}
	else if (buf_idx < buf_cnt-1)
	{
		pos_base += len[buf_idx];
		in_buffer = buf[++buf_idx];
		bytes_left = len[buf_idx]-1;
		return *in_buffer++;
	}
	else
		return 0;
}

void CSS_Buffer::UnGetChar()
{
	if (bytes_left < len[buf_idx])
	{
		bytes_left++;
		--in_buffer;
	}
	else if (buf_idx > 0)
	{
		bytes_left = 1;
		buf_idx--;
		in_buffer = buf[buf_idx]+(len[buf_idx]-1);
		pos_base -= len[buf_idx];
	}
}

void CSS_Buffer::EatChars(unsigned int cnt)
{
	while (cnt > bytes_left && buf_idx < buf_cnt-1)
	{
		pos_base += len[buf_idx];
		cnt -= bytes_left;
		bytes_left = len[++buf_idx];
		in_buffer = buf[buf_idx];
	}

	if (cnt > bytes_left)
		bytes_left = 0;
	else
	{
		bytes_left -= cnt;
		in_buffer += cnt;
	}
}

void CSS_Buffer::SetCurrentPos(unsigned int new_pos)
{
	buf_idx = 0;
	pos_base = 0;
	while (new_pos > pos_base+len[buf_idx] && buf_idx < buf_cnt-1)
		pos_base += len[buf_idx++];

	if (buf_idx < buf_cnt)
	{
		in_buffer = buf[buf_idx]+new_pos-pos_base;
		bytes_left = len[buf_idx]-(new_pos-pos_base);
	}
	else
	{
		in_buffer = buf[buf_idx]+len[buf_idx];
		bytes_left = 0;
	}
}

COLORREF CSS_Buffer::GetColor(unsigned int start_pos, unsigned int length)
{
	if (length >= 3 && length <= CSS_ESCAPED_IDENT_LEN(6))
	{
		uni_char color[CSS_ESCAPED_IDENT_LEN(6)+1]; /* ARRAY OK 2009-02-05 rune */

		GetString(color, start_pos, length);
		size_t color_len = uni_strlen(color);
		if (color_len == 3 || color_len == 6)
		{
			for (size_t i=0; i<color_len; i++)
				if (!uni_isxdigit(color[i]))
					return USE_DEFAULT_COLOR;

			int r, g, b;
			int n=color_len/3;
			r = HexToInt(color, n);
			g = HexToInt(color+n, n);
			b = HexToInt(color+2*n, n);
			if (color_len == 3)
			{
				r*=0x11;
				g*=0x11;
				b*=0x11;
			}
			return OP_RGB(r, g, b);
		}
	}
	return USE_DEFAULT_COLOR;
}

void CSS_Buffer::GetNChars(uni_char* dst, unsigned int count)
{
	if (dst)
		for (unsigned int i=0; i<count; i++)
			*dst++ = GetNextChar();
}

#else

uni_char CSS_Buffer::LookAheadSlow(unsigned int n)
{
	OP_ASSERT(bytes_left <= n);

	int la_cnt = bytes_left;
	for (unsigned int i=buf_idx+1; i<buf_cnt; i++)
	{
		if (la_cnt+len[i] > n)
			return buf[i][n-la_cnt];
		la_cnt += len[i];
	}
	return 0;
}

#endif // LIMITED_FOOTPRINT_DEVICE
