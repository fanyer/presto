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

#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/util/opstring.h"
#include "modules/unicode/utf8.h"

/* OpProtobufUtils */

/*static*/
OP_STATUS
OpProtobufUtils::Copy(const TempBuffer &from, ByteBuffer &to)
{
	uni_char *str = from.GetStorage();
	int char_count = from.Length();
	UTF8Encoder converter;
	char buf[1024]; // ARRAY OK 2009-05-05 jhoff
	int char_remaining = char_count;
	while (char_remaining > 0)
	{
		int bytes_read;
		int bytes_written = converter.Convert(str, char_remaining*sizeof(uni_char), buf, sizeof(buf), &bytes_read);
		int unichars_read = bytes_read/sizeof(uni_char);
		if (unichars_read <= 0 || bytes_written < 0)
			return OpStatus::ERR;
		str += unichars_read;
		RETURN_IF_ERROR(Append(to, buf, (unsigned int)bytes_written));
		char_remaining -= unichars_read;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::Copy(OpProtobufStringChunkRange from, TempBuffer &to)
{
	for (; !from.IsEmpty(); from.PopFront())
	{
		const OpProtobufStringChunk &chunk = from.Front();
		RETURN_IF_ERROR(to.Append(chunk.GetString(), chunk.GetLength()));
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::Convert(ByteBuffer &out, const uni_char *in_str, int char_count)
{
	if (char_count < 0)
		char_count = uni_strlen(in_str);
	if (char_count == 0)
		return OpStatus::OK;

	UTF8Encoder converter;
	char buf[1024]; // ARRAY OK 2009-05-05 jhoff
	int chars_left = char_count;
	for (int i = 0; i < char_count;)
	{
		int bytes_read;
		int bytes_written = converter.Convert(in_str, chars_left*sizeof(uni_char), buf, sizeof(buf), &bytes_read);
		int unichars_read = bytes_read/sizeof(uni_char);
		if (unichars_read == 0 || bytes_written < 0)
			return OpStatus::ERR;
		in_str += unichars_read;
		RETURN_IF_ERROR(out.AppendBytes(buf, (unsigned int)bytes_written));
		i += unichars_read;
		chars_left -= unichars_read;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::Convert(OpString &out, const char *in_str, int char_count)
{
	if (char_count < 0)
		char_count = op_strlen(in_str);
	if (char_count == 0)
		return OpStatus::OK;

	UTF8Decoder converter;
	uni_char buf[1024]; // ARRAY OK 2009-05-05 jhoff
	unsigned int chars_left = (unsigned int)char_count;
	for (unsigned int i = 0; i < (unsigned int)char_count;)
	{
		int bytes_read;
		int bytes_written = converter.Convert(in_str, chars_left*sizeof(char), buf, sizeof(buf), &bytes_read);
		if (bytes_read <= 0 || bytes_written < 0)
			return OpStatus::ERR;
		unsigned int chars_read = (unsigned int)bytes_read/sizeof(char);
		unsigned int unichars_written = (unsigned int)bytes_written/sizeof(uni_char);
		in_str += chars_read;
		RETURN_IF_ERROR(out.Append(buf, unichars_written));
		i += chars_read;
		chars_left -= chars_read;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::ConvertUTF8toUTF16(ByteBuffer &out, const ByteBuffer &in, int char_count)
{
	if (char_count < 0)
		char_count = in.Length();
	if (char_count == 0)
		return OpStatus::OK;

	UTF8Decoder converter;
	char in_buf[1024]; // ARRAY OK 2009-06-09 jborsodi
	uni_char buf[1024]; // ARRAY OK 2009-05-05 jhoff
	unsigned int chars_left = (unsigned int)char_count;
	for (unsigned int i = 0; i < (unsigned int)char_count;)
	{
		int bytes_read;
		unsigned int extract_size = MIN(chars_left, ARRAY_SIZE(in_buf));
		in.Extract(i, extract_size, in_buf);
		int bytes_written = converter.Convert(in_buf, (int)extract_size*sizeof(char), buf, sizeof(buf), &bytes_read);
		if (bytes_read <= 0 || bytes_written < 0)
			return OpStatus::ERR;
		RETURN_IF_ERROR(OpProtobufUtils::Append(out, Bytes(reinterpret_cast<char *>(buf), (unsigned int)bytes_written)));
		unsigned int chars_read = (unsigned int)bytes_read/sizeof(char);
		i += chars_read;
		chars_left -= chars_read;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::ConvertUTF8toUTF16(TempBuffer &out, const ByteBuffer &in, int char_count)
{
	if (char_count < 0)
		char_count = in.Length();
	if (char_count == 0)
	{
		return out.Expand(0);
	}

	UTF8Decoder converter;
	char in_buf[1024]; // ARRAY OK 2009-06-09 jborsodi
	uni_char buf[1024]; // ARRAY OK 2009-05-05 jhoff
	unsigned int chars_left = (unsigned int)char_count;
	for (unsigned int i = 0; i < (unsigned int)char_count;)
	{
		int bytes_read;
		unsigned int extract_size = MIN(chars_left, ARRAY_SIZE(in_buf));
		in.Extract(i, extract_size, in_buf);
		int bytes_written = converter.Convert(in_buf, (int)extract_size*sizeof(char), buf, sizeof(buf), &bytes_read);
		if (bytes_read <= 0 || bytes_written < 0)
			return OpStatus::ERR;
		unsigned int unichars_written = (unsigned int)bytes_written/sizeof(uni_char);
		RETURN_IF_ERROR(OpProtobufUtils::Append(out, buf, unichars_written));
		unsigned int chars_read = (unsigned int)bytes_read/sizeof(char);
		i += chars_read;
		chars_left -= chars_read;
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::ExtractUTF16BE(OpString &out, const ByteBuffer &in, int char_count)
{
	char *data = in.Copy();
	if (data == NULL)
		return OpStatus::ERR_NO_MEMORY;
	if (char_count < 0)
		char_count = in.Length()/2;
	OP_STATUS status = out.Set((uni_char *)data, char_count);
	OP_DELETEA(data);
	return status;
}

#ifndef OPERA_BIG_ENDIAN
//
// If we are not on a big-endian machine, we need this to swap our data
// into or from network byte order.
//
/*static*/
void
OpProtobufUtils::ByteSwap(uni_char* buf, size_t buf_len)
{
	for ( unsigned int i=0 ; i < buf_len ; i++ )
		buf[i] = op_htons(buf[i]);  // This is a byteswap on LE machines
}
#endif // OPERA_BIG_ENDIAN

/*static*/
int
OpProtobufUtils::ParseDelimitedInteger(const uni_char *buffer, unsigned int buffer_len, uni_char delimiter, int &chars_read)
{
	uni_char *end = NULL;
	int value = uni_strtol(buffer, &end, 10, NULL );
	if (end == (buffer + buffer_len))
	{
		chars_read = 0;
		return 0;
	}
	if (*end != delimiter)
	{
		chars_read = -1;
		return 0;
	}
	chars_read = end - buffer + 1; // delimiter is included
	return value;
}

/*static*/
char *
OpProtobufUtils::uitoa( unsigned int value, char *str, int radix )
{
	char *ret = str, *t = str;

	if (value == 0)
	{
		*str++ = '0';
		*str = 0;
		return ret;
	}

	if (radix < 2 || radix > 36)
		radix = 10;

	while (value != 0)
	{
		*str++ = "0123456789abcdefghijklmnopqrstuvwxyz"[value % radix];
		value = value / radix;
	}
	*str = 0;

	--str;
	while (str > t)
	{
		char c = *str;
		*str = *t;
		*t = c;
		--str;
		++t;
	}

	return ret;
}

/*static*/
OP_STATUS
OpProtobufUtils::AppendUniString(OpProtobufValueVector<UniString> &container, const UniString &string)
{
	return container.Add(string);
}

/*static*/
OP_STATUS
OpProtobufUtils::AppendUniString(OpProtobufValueVector<UniString> &container, const uni_char *string, int len)
{
	UniString tmp;
	RETURN_IF_ERROR(tmp.SetCopyData(string, len));
	RETURN_IF_ERROR(container.Add(tmp));
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufUtils::AppendUniString(OpProtobufValueVector<UniString> &container, const char *string, int len)
{
	UniString tmp;
	TempBuffer buf;
	RETURN_IF_ERROR(buf.Append(string, len));
	RETURN_IF_ERROR(tmp.SetCopyData(buf.GetStorage(), buf.Length()));
	RETURN_IF_ERROR(container.Add(tmp));
	return OpStatus::OK;
}


#endif // PROTOBUF_SUPPORT
