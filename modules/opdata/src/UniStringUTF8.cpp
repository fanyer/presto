/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/opdata/UniStringUTF8.h"
#include "modules/unicode/utf8.h"

OP_STATUS UniString_UTF8::FromUTF8(UniString& out, const char* utf8_string, int utf8_byte_length /* = -1 */)
{
	if (!utf8_string)
		return OpStatus::ERR;

	if (utf8_byte_length == -1)
		utf8_byte_length = op_strlen(utf8_string);

	if (utf8_byte_length == 0)
		return OpStatus::OK;

	UTF8Decoder decoder;
	const int uni_byte_length = decoder.Measure(utf8_string, utf8_byte_length, INT_MAX, NULL);

	// Allocate a uni_char string and copy content
	uni_char* u_str = OP_NEWA(uni_char, uni_byte_length);
	RETURN_OOM_IF_NULL(u_str);

	int read = 0;
	decoder.Reset();
	if (decoder.Convert(utf8_string, utf8_byte_length, u_str, uni_byte_length, &read) < 0)
	{
		OP_DELETEA(u_str);
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_ASSERT(read > 0 && read <= utf8_byte_length);

	// Let UniString take ownership of the uni_char string
	OP_STATUS status = out.SetRawData(OPDATA_DEALLOC_OP_DELETEA, u_str, UNICODE_DOWNSIZE(uni_byte_length), uni_byte_length);
	if (OpStatus::IsError(status))
		OP_DELETEA(u_str);

	return status;
}

OP_STATUS UniString_UTF8::ToUTF8(char** utf8_string, const UniString& src)
{
	const uni_char* u_str = src.Data(true);
	RETURN_OOM_IF_NULL(u_str);
	const size_t uni_byte_length = UNICODE_SIZE(src.Length());

	// Measure the length of the resulting UTF8-encoded string and allocate it
	UTF8Encoder encoder;
	const int utf8_byte_length = encoder.Measure(u_str, uni_byte_length);
	*utf8_string = OP_NEWA(char, utf8_byte_length + 1);
	RETURN_OOM_IF_NULL(*utf8_string);

	// Create the UTF8-encoded string
	int read = 0;
	encoder.Reset();
	if (encoder.Convert(u_str, uni_byte_length, *utf8_string, utf8_byte_length, &read) < 0)
	{
		OP_DELETEA(*utf8_string);
		*utf8_string = 0;
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_ASSERT(static_cast<size_t>(read) == uni_byte_length);
	(*utf8_string)[utf8_byte_length] = 0;
	return OpStatus::OK;
}
