/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/unicode/utf8.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"

#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/protobuf_data.h"

/* OpProtobufFormat */

int
OpProtobufFormat::StringDataSize(OpProtobufStringChunkRange range)
{
	UTF8Encoder converter;
	int utf8_len = 0;
	for (; !range.IsEmpty(); range.PopFront())
	{
		const OpProtobufStringChunk &chunk = range.Front();
		utf8_len += converter.Measure(reinterpret_cast<const void *>(chunk.GetString()), chunk.GetLength()*sizeof(uni_char));
	}
	return utf8_len;
}

int
OpProtobufFormat::StringSize(OpProtobufStringChunkRange range)
{
	int len = StringDataSize(range);
	return OpProtobufWireFormat::VarIntSize32(len) + len;
}

/*static*/
int
OpProtobufFormat::StringSize(const uni_char *str, int char_count)
{
	OP_ASSERT(str != NULL);
	if (char_count < 0)
		char_count = uni_strlen(str);
	if (char_count == 0)
		return 1; // 1 is for the "size" field which says the strings is 0 long
	OpProtobufStringChunkRange range(str, char_count);
	int len = StringDataSize(range);
	return OpProtobufWireFormat::VarIntSize32(len) + len;
}

/*static*/
int
OpProtobufFormat::BytesSize(const OpProtobufDataChunkRange &range)
{
	int len = range.Length();
	if (len == 0)
		return 1; // 1 is for the "size" field which says the number of bytes is 0
	return OpProtobufWireFormat::VarIntSize32(len) + len;
}

/*static*/
int
OpProtobufFormat::StringSize(const OpString &str, int char_count)
{
	if (char_count < 0)
		char_count = str.Length();
	if (char_count == 0)
		return 1; // 1 is for the "size" field which says the strings is 0 long
	return StringSize(str.DataPtr(), char_count);
}

#endif // PROTOBUF_SUPPORT
