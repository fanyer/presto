/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/formats/hex_converter.h"

const char HexConverter::hexDigitsUppercase[16] =
	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
const char HexConverter::hexDigitsLowercase[16] =
	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

int HexConverter::ToHex(char* dst, int aByte, int flags)
{
	const char *hexDigits = (flags & UseLowercase) ? hexDigitsLowercase : hexDigitsUppercase;
	*dst++ = hexDigits[(aByte>>4) & 0xf];
	*dst++ = hexDigits[aByte & 0xf];
	if (!(flags & DontNullTerminate))
		*dst++ = '\0';
	return 2;
}

int HexConverter::ToHex(uni_char* dst, int aByte, int flags)
{
	const char *hexDigits = (flags & UseLowercase) ? hexDigitsLowercase : hexDigitsUppercase;
	*dst++ = (uni_char)(unsigned char)hexDigits[(aByte>>4) & 0xf];
	*dst++ = (uni_char)(unsigned char)hexDigits[aByte & 0xf];
	if (!(flags & DontNullTerminate))
		*dst++ = (uni_char)'\0';
	return 2;
}

int HexConverter::ToHex(char* dst, const void* src, int src_len, int flags)
{
	const char *hexDigits = (flags & UseLowercase) ? hexDigitsLowercase : hexDigitsUppercase;
	const unsigned char *cSrc = reinterpret_cast<const unsigned char*>(src);
	for (int i=0; i<src_len; i++)
	{
		*dst++ = hexDigits[cSrc[i] >> 4];
		*dst++ = hexDigits[cSrc[i] & 0xf];
	}
	if (!(flags & DontNullTerminate))
		*dst++ = '\0';
	return src_len*2;
}

int HexConverter::ToHex(uni_char* dst, const void* src, int src_len, int flags)
{
	const char *hexDigits = (flags & UseLowercase) ? hexDigitsLowercase : hexDigitsUppercase;
	const unsigned char *cSrc = reinterpret_cast<const unsigned char*>(src);
	for (int i=0; i<src_len; i++)
	{
		*dst++ = hexDigits[cSrc[i] >> 4];
		*dst++ = hexDigits[cSrc[i] & 0xf];
	}
	if (!(flags & DontNullTerminate))
		*dst++ = '\0';
	return src_len*2;
}

OP_STATUS HexConverter::AppendAsHex(OpString8& dst, const void* src, int src_len, int flags)
{
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+src_len*2+1)) return OpStatus::ERR_NO_MEMORY;
	ToHex(dst.CStr()+dst_len, src, src_len, flags & ~DontNullTerminate);
	return OpStatus::OK;
}

OP_STATUS HexConverter::AppendAsHex(OpString& dst, const void* src, int src_len, int flags)
{
	int dst_len = dst.Length();
	if (!dst.Reserve(dst_len+src_len*2+1)) return OpStatus::ERR_NO_MEMORY;
	ToHex(dst.CStr()+dst_len, src, src_len, flags & ~DontNullTerminate);
	return OpStatus::OK;
}

unsigned char HexConverter::FromHex(int c1, int c2)
{
	// Make sure your input is valid before calling this function.
	OP_ASSERT(op_isxdigit(c1));
	OP_ASSERT(op_isxdigit(c2));
	c1 = (c1 | 0x20) - '0' - (c1 >> 6) * 39;
	c2 = (c2 | 0x20) - '0' - (c2 >> 6) * 39;
	return (unsigned char)((c1 << 4) | c2);
}

int HexConverter::FromHex(void* dst, const char* src, int src_len)
{
	unsigned char *cDst = reinterpret_cast<unsigned char*>(dst);
	if (src_len == KAll)
		src_len = INT_MAX;
	for (; src_len >= 2 && op_isxdigit(src[0]) && op_isxdigit(src[1]); src += 2, src_len -= 2)
		*cDst++ = FromHex(src[0], src[1]);
	return (int)(cDst - reinterpret_cast<unsigned char*>(dst));
}

int HexConverter::FromHex(void* dst, const uni_char* src, int src_len)
{
	unsigned char *cDst = reinterpret_cast<unsigned char*>(dst);
	if (src_len == KAll)
		src_len = INT_MAX;
	for (; src_len >= 2 && op_isxdigit(src[0]) && op_isxdigit(src[1]); src += 2, src_len -= 2)
		*cDst++ = FromHex(src[0], src[1]);
	return (int)(cDst - reinterpret_cast<unsigned char*>(dst));
}
