/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/protobuf/src/protobuf_data.h"

/* OpProtobufStringOutputAdapter */

OP_STATUS
OpProtobufStringOutputAdapter::Put(const OpProtobufDataChunk &chunk)
{
	const char *in_str = chunk.GetData();
	unsigned char_count = chunk.GetLength();
	unsigned chars_left = char_count;
	for (unsigned i = 0; i < char_count;)
	{
		int bytes_read;
		int bytes_written = converter.Convert(in_str, chars_left*sizeof(char), buf, sizeof(buf), &bytes_read);
		if (bytes_read <= 0 || bytes_written < 0)
			return OpStatus::ERR;
		unsigned unichars_written = static_cast<unsigned>(bytes_written)/sizeof(uni_char);
		in_str += bytes_read;
		RETURN_IF_ERROR(output->Put(OpProtobufStringChunk(buf, unichars_written)));
		i += bytes_read;
		chars_left -= bytes_read;
	}
	return OpStatus::OK;
}

