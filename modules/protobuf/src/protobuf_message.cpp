/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"

#include "modules/protobuf/src/protobuf_data.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/json_tools.h"
#include "modules/protobuf/src/protobuf_message.h"

// Helper methods for generated code

OP_STATUS
OpProtobufAppend(ByteBuffer &to, const ByteBuffer &from, unsigned int max_len)
{
	unsigned int remaining = MIN(max_len, from.Length());
	unsigned int count = from.GetChunkCount();
	for (unsigned int i = 0; i < count; ++i)
	{
		unsigned int nbytes;
		char *chunk = from.GetChunk(i, &nbytes);
		OP_ASSERT(chunk != NULL);
		if (nbytes == 0)
			continue;
		unsigned int copy_size = MIN(nbytes, remaining);
		RETURN_IF_ERROR(to.AppendBytes(chunk, copy_size));
		remaining -= copy_size;
		if (remaining == 0)
			break;
	}
	return OpStatus::OK;
}

/* OpProtobufWire */

/*static*/
OP_STATUS
OpProtobufWire::DecodeVarInt32(OpProtobufDataRange &it, int &len, INT32 &num)
{
	OpProtobufDataRange start = it;
	if (it.IsEmpty())
	{
		len = 0;
		return OpStatus::ERR;
	}
	unsigned char c = it.Front();
	it.PopFront();
	if ((c & 0x80) != 0x80)
	{
		num = static_cast<int>(c);
		len = 1;
		return OpStatus::OK;
	}
	int v = static_cast<int>(c & 0x7f), shift = 7;
	for (; !it.IsEmpty(); shift += 7)
	{
		if (shift > 31)
			break;
		c = it.Front();
		it.PopFront();
		if (c & 0x80)
			v |= static_cast<int>(c & 0x7f) << shift;
		else
		{
			v |= static_cast<int>(c) << shift;
			num = v;
			len = start.DistanceTo(it);
			return OpStatus::OK;
		}
	}
	// We have received a varint larger than 5 bytes, read and discard remaining bytes (up to 5 more).
	for (; !it.IsEmpty(); shift += 7)
	{
		if (shift > 63) // Must not exceed 64 bit boundary
			break;
		c = it.Front();
		it.PopFront();
		if (!(c & 0x80))
		{
			num = v;
			len = start.DistanceTo(it);
			return OpStatus::OK;
		}
	}
	len = start.DistanceTo(it);
	return OpStatus::ERR;
}

/*static*/
OP_STATUS
OpProtobufWire::DecodeVarInt64(OpProtobufDataRange &it, int &len, INT64 &num)
{
	OpProtobufDataRange start = it;
	if (it.IsEmpty())
	{
		len = 0;
		return OpStatus::ERR;
	}
	unsigned char c = it.Front();
	it.PopFront();
	if ((c & 0x80) != 0x80)
	{
		num = static_cast<INT64>(c);
		len = 1;
		return OpStatus::OK;
	}
	INT64 v = static_cast<INT64>(c & 0x7f), shift = 7;
	for (; !it.IsEmpty(); shift += 7)
	{
		if (shift > 63)
			break;
		c = it.Front();
		it.PopFront();
		if (c & 0x80)
			v |= static_cast<INT64>(c & 0x7f) << shift;
		else
		{
			v |= static_cast<INT64>(c) << shift;
			num = v;
			len = start.DistanceTo(it);
			return OpStatus::OK;
		}
	}
	len = start.DistanceTo(it);
	return OpStatus::ERR;
}

/*static*/
OP_STATUS
OpProtobufWire::DecodeFixed32(OpProtobufDataRange &range, UINT32 &num)
{
	OP_ASSERT(range.Length() >= 4);
	// TODO: Optimize with proper le/be code.
	unsigned char udata[4]; // ARRAY OK 2011-07-04 jborsodi
	if (range.CopyInto(reinterpret_cast<char *>(&udata[0]), sizeof(udata)) != sizeof(udata))
		return OpStatus::ERR;
	num =      udata[0]
			| (udata[1] << 8)
			| (udata[2] << 16)
			| (udata[3] << 24);
	range.PopFront(4);
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpProtobufWire::DecodeFixed64(OpProtobufDataRange &range, UINT64 &num)
{
	OP_ASSERT(range.Length() >= 8);
	// TODO: Optimize with proper le/be code.
	unsigned char udata[8]; // ARRAY OK 2011-07-04 jborsodi
	if (range.CopyInto(reinterpret_cast<char *>(&udata[0]), sizeof(udata)) != sizeof(udata))
		return OpStatus::ERR;
	UINT32 a =     udata[0]
				| (udata[1] << 8)
				| (udata[2] << 16)
				| (udata[3] << 24);
	UINT32 b =     udata[4]
				| (udata[5] << 8)
				| (udata[6] << 16)
				| (udata[7] << 24);
	num = (static_cast<UINT64>(b) << 32) | a;
	range.PopFront(8);
	return OpStatus::OK;
}

/* OpProtobufMessage */

OpProtobufMessage::~OpProtobufMessage()
{
	OP_DELETEA(fields);
	OP_DELETEA(offsets);
}

/* OpProtobufInstanceProxy */

OpProtobufBitFieldRef
OpProtobufInstanceProxy::GetBitfield() const
{
	OpProtobufBitFieldRef::Type *fields = reinterpret_cast<OpProtobufBitFieldRef::Type *>(reinterpret_cast<char *>(instance_ptr) + message->GetOffsetToBitfield());
	return OpProtobufBitFieldRef(fields, message->GetFieldCount());
}

INT32 *
OpProtobufInstanceProxy::GetCurrentIDPtr() const
{
	return reinterpret_cast<INT32 *>(reinterpret_cast<char *>(instance_ptr) + message->GetOffsetToBitfield());
}

int
OpProtobufInstanceProxy::GetInstanceEncodedSize() const
{
	int *encoded_size = reinterpret_cast<int *>(reinterpret_cast<char *>(instance_ptr) + message->GetOffsetToEncodedSize());
	return *encoded_size;
}

void
OpProtobufInstanceProxy::SetInstanceEncodedSize(int size) const
{
	int *encoded_size = reinterpret_cast<int *>(reinterpret_cast<char *>(instance_ptr) + message->GetOffsetToEncodedSize());
	*encoded_size = size;
}

// String

BOOL
OpProtobufInstanceProxy::IsFieldOpString(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	return field.GetType() == OpProtobufFormat::String && field.GetStringDataType() == OpProtobufField::String_OpString;
}

OpProtobufStringChunkRange
OpProtobufInstanceProxy::GetStringRange(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		return OpProtobufStringChunkRange(*FieldPtrOpString(field_idx));
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		return OpProtobufStringChunkRange(*FieldPtrUniString(field_idx));
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		return OpProtobufStringChunkRange(*FieldPtrTempBuffer(field_idx));
	}
}

int
OpProtobufInstanceProxy::GetStringArrayCount(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		return FieldPtrOpAutoVectorOpString(field_idx)->GetCount();
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		return FieldPtrOpValueVectorUniString(field_idx)->GetCount();
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		return FieldPtrOpAutoVectorTempBuffer(field_idx)->GetCount();
	}
}

OpProtobufStringArrayRange
OpProtobufInstanceProxy::GetStringArray(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		return OpProtobufStringArrayRange(*FieldPtrOpAutoVectorOpString(field_idx));
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		return OpProtobufStringArrayRange(*FieldPtrOpValueVectorUniString(field_idx));
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		return OpProtobufStringArrayRange(*FieldPtrOpAutoVectorTempBuffer(field_idx));
	}
}

OP_STATUS
OpProtobufInstanceProxy::SetString(int field_idx, const OpProtobufStringChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		OpString &str = *FieldPtrOpString(field_idx);
		str.Empty();
		return Put(str, range);
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		UniString &str = *FieldPtrUniString(field_idx);
		str.Clear();
		return Put(str, range);
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		TempBuffer &str = *FieldPtrTempBuffer(field_idx);
		str.Clear();
		return Put(str, range);
	}
}

OP_STATUS
OpProtobufInstanceProxy::SetString(int field_idx, const OpProtobufDataChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		OpString &str = *FieldPtrOpString(field_idx);
		str.Empty();
		return Put(str, range);
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		UniString &str = *FieldPtrUniString(field_idx);
		str.Clear();
		return Put(str, range);
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		TempBuffer &str = *FieldPtrTempBuffer(field_idx);
		str.Clear();
		return Put(str, range);
	}
}

OP_STATUS
OpProtobufInstanceProxy::AddString(int field_idx, const OpProtobufStringChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
		if (str.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(Put(*str.get(), range));
		RETURN_IF_ERROR(FieldPtrOpAutoVectorOpString(field_idx)->Add(str.get()));
		str.release();
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		UniString str;
		RETURN_IF_ERROR(Put(str, range));
		RETURN_IF_ERROR(FieldPtrOpValueVectorUniString(field_idx)->Add(str));
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		OpAutoPtr<TempBuffer> str(OP_NEW(TempBuffer, ()));
		if (str.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(Put(*str.get(), range));
		RETURN_IF_ERROR(FieldPtrOpAutoVectorTempBuffer(field_idx)->Add(str.get()));
		str.release();
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInstanceProxy::AddString(int field_idx, const OpProtobufDataChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
		if (str.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(Put(*str.get(), range));
		RETURN_IF_ERROR(FieldPtrOpAutoVectorOpString(field_idx)->Add(str.get()));
		str.release();
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString)
	{
		UniString str;
		RETURN_IF_ERROR(Put(str, range));
		RETURN_IF_ERROR(FieldPtrOpValueVectorUniString(field_idx)->Add(str));
	}
	else // TempBuffer
	{
		OP_ASSERT(field.GetStringDataType() == OpProtobufField::String_TempBuffer);
		OpAutoPtr<TempBuffer> str(OP_NEW(TempBuffer, ()));
		if (str.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(Put(*str.get(), range));
		RETURN_IF_ERROR(FieldPtrOpAutoVectorTempBuffer(field_idx)->Add(str.get()));
		str.release();
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInstanceProxy::AddString(int field_idx, OpString *str, BOOL takeover)
{
	OP_ASSERT(str);
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetStringDataType() == OpProtobufField::String_OpString)
	{
		if (takeover)
			return FieldPtrOpAutoVectorOpString(field_idx)->Add(str);
		else
			return AddString(field_idx, OpProtobufStringChunkRange(*str));
	}
	else if (field.GetStringDataType() == OpProtobufField::String_UniString ||
				field.GetStringDataType() == OpProtobufField::String_TempBuffer)
	{
		OP_STATUS status = AddString(field_idx, OpProtobufStringChunkRange(*str));
		if (takeover)
			OP_DELETE(str);
		return status;
	}
	else
	{
		OP_ASSERT(!"Invalid string type, add new 'else if' entry if new string type");
		return OpStatus::ERR;
	}
}

// Bytes

BOOL
OpProtobufInstanceProxy::IsFieldByteBuffer(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetType() == OpProtobufFormat::Bytes && field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
		return TRUE;
	else
		return FALSE;
}

OpProtobufDataChunkRange
OpProtobufInstanceProxy::GetBytesRange(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
	{
		return OpProtobufDataChunkRange(*FieldPtrByteBuffer(field_idx));
	}
	else // OpData
	{
		OP_ASSERT(field.GetBytesDataType() == OpProtobufField::Bytes_OpData);
		return OpProtobufDataChunkRange(*FieldPtrOpData(field_idx));
	}
}

int
OpProtobufInstanceProxy::GetBytesArrayCount(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
	{
		return FieldPtrOpAutoVectorByteBuffer(field_idx)->GetCount();
	}
	else // OpData
	{
		OP_ASSERT(field.GetBytesDataType() == OpProtobufField::Bytes_OpData);
		return FieldPtrOpValueVectorOpData(field_idx)->GetCount();
	}
}

OpProtobufDataArrayRange
OpProtobufInstanceProxy::GetBytesArray(int field_idx) const
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
	{
		return OpProtobufDataArrayRange(*FieldPtrOpAutoVectorByteBuffer(field_idx));
	}
	else // OpData
	{
		OP_ASSERT(field.GetBytesDataType() == OpProtobufField::Bytes_OpData);
		return OpProtobufDataArrayRange(*FieldPtrOpValueVectorOpData(field_idx));
	}
}

OP_STATUS
OpProtobufInstanceProxy::SetBytes(int field_idx, const OpProtobufDataChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
	{
		ByteBuffer &buffer = *FieldPtrByteBuffer(field_idx);
		buffer.Clear();
		return Put(buffer, range);
	}
	else // OpData
	{
		OP_ASSERT(field.GetBytesDataType() == OpProtobufField::Bytes_OpData);
		OpData &buffer = *FieldPtrOpData(field_idx);
		buffer.Clear();
		return Put(buffer, range);
	}
}

OP_STATUS
OpProtobufInstanceProxy::AddBytes(int field_idx, const OpProtobufDataChunkRange &range)
{
	const OpProtobufField &field = GetField(field_idx);
	if (field.GetBytesDataType() == OpProtobufField::Bytes_ByteBuffer)
	{
		OpAutoPtr<ByteBuffer> buffer(OP_NEW(ByteBuffer, ()));
		if (buffer.get() == NULL)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(Put(*buffer.get(), range));
		RETURN_IF_ERROR(FieldPtrOpAutoVectorByteBuffer(field_idx)->Add(buffer.get()));
		buffer.release();
	}
	else // OpData
	{
		OP_ASSERT(field.GetBytesDataType() == OpProtobufField::Bytes_OpData);
		OpData buffer;
		RETURN_IF_ERROR(Put(buffer, range));
		RETURN_IF_ERROR(FieldPtrOpValueVectorOpData(field_idx)->Add(buffer));
	}
	return OpStatus::OK;
}

// Message

const OpProtobufMessage *
OpProtobufInstanceProxy::GetFieldProtoMessage(int field_idx) const
{
	OP_ASSERT(message != NULL);
	if (message == NULL || field_idx < 0 || field_idx >= message->GetFieldCount())
		return NULL;
	const OpProtobufField &field = message->GetField(field_idx);
	return field.GetMessage();
}

// Put methods

// String

OP_STATUS
OpProtobufInstanceProxy::Put(OpString &str, OpProtobufStringChunkRange range)
{
	// If the range is empty we should set an empty string in @a str.
	// This ensures that it always contains a non-NULL value.
	if (range.IsEmpty())
		return str.Set("", 0);

	for (; !range.IsEmpty(); range.PopFront())
	{
		const OpProtobufStringChunk &chunk = range.Front();
		RETURN_IF_ERROR(str.Append(chunk.GetString(), chunk.GetLength()));
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInstanceProxy::Put(OpString &str, const OpProtobufDataChunkRange &range)
{
	// If the range is empty we should set an empty string in @a str.
	// This ensures that it always contains a non-NULL value.
	if (range.IsEmpty())
		return str.Set("", 0);

	OpProtobufOpStringOutputRange out_range(str);
	OpProtobufStringOutputAdapter out(&out_range);
	return out.Put(range);
}

OP_STATUS
OpProtobufInstanceProxy::Put(UniString &str, OpProtobufStringChunkRange range)
{
	for (; !range.IsEmpty(); range.PopFront())
	{
		const OpProtobufStringChunk &chunk = range.Front();
		RETURN_IF_ERROR(str.AppendCopyData(chunk.GetString(), chunk.GetLength()));
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInstanceProxy::Put(UniString &str, const OpProtobufDataChunkRange &range)
{
	OpProtobufUniStringOutputRange out_range(str);
	OpProtobufStringOutputAdapter out(&out_range);
	return out.Put(range);
}

OP_STATUS
OpProtobufInstanceProxy::Put(TempBuffer &str, OpProtobufStringChunkRange range)
{
	for (; !range.IsEmpty(); range.PopFront())
	{
		const OpProtobufStringChunk &chunk = range.Front();
		RETURN_IF_ERROR(str.Append(chunk.GetString(), chunk.GetLength()));
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInstanceProxy::Put(TempBuffer &str, const OpProtobufDataChunkRange &range)
{
	OpProtobufTempBufferOutputRange out_range(str);
	OpProtobufStringOutputAdapter out(&out_range);
	return out.Put(range);
}

// Bytes

OP_STATUS
OpProtobufInstanceProxy::Put(ByteBuffer &buffer, const OpProtobufDataChunkRange &range)
{
	OpProtobufByteBufferOutputRange out_range(buffer);
	OpProtobufDataOutputAdapter out(&out_range);
	return out.Put(range);
}

OP_STATUS
OpProtobufInstanceProxy::Put(OpData &buffer, const OpProtobufDataChunkRange &range)
{
	OpProtobufOpDataOutputRange out_range(buffer);
	OpProtobufDataOutputAdapter out(&out_range);
	return out.Put(range);
}

#endif // PROTOBUF_SUPPORT
