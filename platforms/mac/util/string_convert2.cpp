/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Anders Markussen
 */

#include "core/pch.h"

/////////////////////////////////////////////////////////////////////////
//
// NOTE: You CANNOT use any core functionallity in the StringConvert as 
//		 it is used outside of core.
//
/////////////////////////////////////////////////////////////////////////

#include "adjunct/desktop_util/string/string_convert.h"

/////////////////////////////////////////////////////////////////////////

uni_char* StringConvert::UTF16FromUTF8(const char* input)
{
	int len = 0;
	const char* scan = input;
	uni_char *out = NULL;
	uni_char *out_scan;
	while (char c = *scan++) {
		if (!(c&0x80)) {					// 7 bits
			len++;
		} else if (!(c&0x40)) {
			return NULL; // invalid, 10xx xxxx can only happen as second, third or fourth letter in a multibyte sequence.
		} else if (!(c&0x20)) {				// 11 bits
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, second byte in a 2-byte sequence needs to be 10xx xxxx
			len++;
		} else if (!(c&0x10)) {				// 16 bits
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, second byte in a 3-byte sequence needs to be 10xx xxxx
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, third  byte in a 3-byte sequence needs to be 10xx xxxx
			len++;
		} else if (!(c&0x08)) {				// 20 bits
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, second byte in a 4-byte sequence needs to be 10xx xxxx
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, third  byte in a 4-byte sequence needs to be 10xx xxxx
			if ((*scan++ & 0xC0) != 0x80)
				return NULL; // invalid, fourth byte in a 4-byte sequence needs to be 10xx xxxx
			len += 2;
		} else {
			return NULL; // invalid, this is a 5 or 6-byte sequnce, not allowed.
		}
	}
	out = new uni_char[len+1];
	out_scan = out;
	scan = input;
	while (char c = *scan++) {
		if (!(c&0x80)) {
			*out_scan++ = c;
		} else if (!(c&0x20)) {
			*out_scan++ = ((c&0x1f)<<6)|((*scan++)&0x3f);
		} else if (!(c&0x10)) {
			uni_char t = ((c&0x0f)<<12)|(((*scan++)&0x3f)<<6);
			t |= ((*scan++)&0x3f);
			*out_scan++ = t;
		} else if (!(c&0x08)) {
			unsigned long t = ((c&0x03)<<18)|(((*scan++)&0x3f)<<12);
			t |= (((*scan++)&0x3f)<<6);
			t |= ((*scan++)&0x3f);
			t -= 0x10000;
			*out_scan++ = 0xD800 + (t >> 10);
			*out_scan++ = 0xDC00 + (t & 0x03FF);
		}
	}
	*out_scan = 0;
	return out;
}

/////////////////////////////////////////////////////////////////////////

char* StringConvert::UTF8FromUTF16(const uni_char* input)
{
	int len = 0;
	const uni_char* scan = input;
	char *out = NULL;
	char *out_scan;
	while (uni_char c = *scan++) {
		if (!(c&0xFF80)) {						// 7 bits
			len++;
		} else if (!(c&0xF800)) {				// 11 bits
			len+=2;
		} else if ((c&0xD800) != 0xD800) {		// 16 bits
			len+=3;
		} else {								// 20 bits
			if ((c&0xDC00) != 0xD800)
				return NULL; // invalid, first uni_char in a surrogate pair needs to be 1101 10xx xxxx xxxx
			if (((*scan++) & 0xDC00) != 0xDC00)
				return NULL; // invalid, second uni_char in a surrogate pair needs to be 1101 11xx xxxx xxxx
			len += 4;
		}
	}
	out = new char[len+1];
	out_scan = out;
	scan = input;
	while (uni_char c = *scan++) {
		if (!(c&0xFF80)) {						// 7 bits
			*out_scan++ = c;
		} else if (!(c&0xF800)) {				// 11 bits
			*out_scan++ = 0xC0 | (c>>6);
			*out_scan++ = 0x80 | (c&0x3f);
		} else if ((c&0xD800) != 0xD800) {		// 16 bits
			*out_scan++ = 0xE0 | (c>>12);
			*out_scan++ = 0x80 | ((c>>6)&0x3f);
			*out_scan++ = 0x80 | ((c)&0x3f);
		} else {								// 20 bits
			unsigned long t = ((c&0x03FF)<<10)|((*scan++)&0x03FF) + 0x10000;
			*out_scan++ = 0xF0 | (t>>18);
			*out_scan++ = 0x80 | ((t>>12)&0x3f);
			*out_scan++ = 0x80 | ((t>>6)&0x3f);
			*out_scan++ = 0x80 | ((t)&0x3f);
		}
	}
	*out_scan = 0;
	return out;
}

/////////////////////////////////////////////////////////////////////////

UINT32 StringConvert::ConvertLineFeedsToLineBreaks(const uni_char* src, UINT32 src_size, uni_char* dst, UINT32 dst_size)
{
	if (dst == NULL)
		dst_size = 0;

	UINT32 req_size = 0;
	uni_char* s = dst;

	//Copy what can fit in the destination buffer
	UINT32 i;
	for(i = 0; i < src_size && (UINT32)(s - dst) < dst_size; i++)
	{
		switch (src[i])
		{
		case '\r':
			break;
		case '\n':
			*(s++) = '\r';
			//fall-through, \n will be added after the \r
		default:
			*(s++) = src[i];
			break;
		}
	}

	req_size = s - dst;

	//Count how much bigger the buffer should be to fit the whole result
	for (;i < src_size;i++)
	{
		switch (src[i])
		{
		case '\r':
			break;
		case '\n':
			req_size++;
		default:
			req_size++;
			break;
		}
	}

	return req_size;
}

/////////////////////////////////////////////////////////////////////////
