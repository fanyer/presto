/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/encoders/encoder-utility.h"

int UTF16toUTF8Converter::Convert(const void *src, int len, void *dest,
                                  int maxlen, int *read_ext)
{
	return this->UTF8Encoder::Convert(src, len, dest, maxlen, read_ext);
}

const char *UTF16toUTF8Converter::GetDestinationCharacterSet()
{
	return "utf-8";
}

void UTF16toUTF8Converter::Reset()
{
	this->OutputConverter::Reset();
	this->UTF8Encoder::Reset();
}
