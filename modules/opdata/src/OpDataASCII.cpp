/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/opdata/OpDataASCII.h"
#include "modules/unicode/unicode_stringiterator.h"

/* static */
OP_STATUS ASCIIOpData::ToUniString(const OpData& source, UniString& dest)
{
	OP_STATUS status = OpStatus::OK;
	size_t written = dest.Length();
	uni_char* append = dest.GetAppendPtr(source.Length());
	RETURN_OOM_IF_NULL(append);
	for (OpDataCharIterator itr(source); itr; ++itr)
	{
		char c = *itr;
		if ((c & 0x7F) != c)
			status = OpStatus::ERR_OUT_OF_RANGE;
		else
		{
			*append = static_cast<uni_char>(c);
			++append;
			++written;
		}
	}
	dest.Trunc(written);
	return status;
}

/* static */
OP_STATUS ASCIIOpData::FromUniString(const UniString& source, OpData& dest)
{
	OP_STATUS status = OpStatus::OK;
	size_t written = dest.Length();
	char* append = dest.GetAppendPtr(source.Length());
	RETURN_OOM_IF_NULL(append);
	for (UnicodeStringIterator itr(source); !itr.IsAtEnd(); ++itr)
	{
		uni_char c = *itr;
		if ((c & 0x007F) != c)
			status = OpStatus::ERR_OUT_OF_RANGE;
		else
		{
			*append = static_cast<char>(c);
			++append;
			++written;
		}
	}
	dest.Trunc(written);
	return status;
}

/* static */
OP_BOOLEAN ASCIIOpData::CompareWith(const OpData& ascii, const UniString& other)
{
	if (ascii.Length() != other.Length())
		return OpBoolean::IS_FALSE;

	OpDataCharIterator a(ascii);
	UnicodeStringIterator o(other);
	while (a)
	{
		OP_ASSERT(!o.IsAtEnd());	// since we know their lengths are equal.
		char c = *a;
		if ((c & 0x7F) != c)
			return OpStatus::ERR_OUT_OF_RANGE;
		else if (static_cast<uni_char>(c) != *o)
			return OpBoolean::IS_FALSE;
		++a;
		++o;
	}
	OP_ASSERT(o.IsAtEnd());
	return OpBoolean::IS_TRUE;
}
