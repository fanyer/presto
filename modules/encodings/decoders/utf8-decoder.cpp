/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/decoders/utf8-decoder.h"

UTF8toUTF16Converter::UTF8toUTF16Converter()
{
}

int UTF8toUTF16Converter::Convert(const void *src, int len, void *dest,
				  int maxlen, int *read_ext)
{
	if (dest)
		return this->UTF8Decoder::Convert(src, len, dest, maxlen, read_ext);
	else
		return this->UTF8Decoder::Measure(src, len, maxlen, read_ext);
}

const char *UTF8toUTF16Converter::GetCharacterSet()
{
	return "utf-8";
}

void UTF8toUTF16Converter::Reset()
{
	this->InputConverter::Reset();
	this->UTF8Decoder::Reset();
}
