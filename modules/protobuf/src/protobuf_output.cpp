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

#include "modules/protobuf/src/protobuf_output.h"

#include "modules/protobuf/src/protobuf_output_common.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/protobuf_data.h"
#include "modules/protobuf/src/protobuf_utils.h"

#ifdef PROTOBUF_JSON_SUPPORT
#include "modules/protobuf/src/json_tools.h"
#endif // PROTOBUF_JSON_SUPPORT

#include "modules/protobuf/src/opvaluevector.h"

#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"
#include "modules/unicode/utf8.h"
#include "modules/util/tempbuf.h"
#include "modules/stdlib/util/opbitvector.h"
#include "modules/stdlib/include/double_format.h"

#ifdef PROTOBUF_XML_SUPPORT
# include "modules/formats/encoder.h"
#endif // PROTOBUF_XML_SUPPORT

/* OpProtobufBasicOutputStream */

OpProtobufBasicOutputStream::OpProtobufBasicOutputStream(OpProtobufDataOutputRange *out_range)
	: out_range(out_range)
{
	OP_ASSERT(out_range);
}

OP_STATUS
OpProtobufBasicOutputStream::WriteFieldHeader(OpProtobufWireFormat::WireType wire_type, int number)
{
	return WriteVarInt32(OpProtobufWireFormat::EncodeField32(wire_type, number));
}

OP_STATUS
OpProtobufBasicOutputStream::WriteVarInt32(INT32 input)
{
	return WriteVarInt64((UINT32)input);
}

OP_STATUS
OpProtobufBasicOutputStream::WriteVarInt64(INT64 input)
{
	char data[10]; // ceil(64 / 7) = 10 // ARRAY OK 2009-05-05 jhoff
	int len;
	RETURN_IF_ERROR(OpProtobufWireFormat::EncodeVarInt64(data, sizeof(data), len, input));
	return out_range->Put(OpProtobufDataChunk(data, len));
}

OP_STATUS
OpProtobufBasicOutputStream::WriteUint32(UINT32 n)
{
	char d[4]; // ARRAY OK 2009-05-05 jhoff
	RETURN_IF_ERROR(OpProtobufWireFormat::EncodeFixed32(d, sizeof(d), n));
	return out_range->Put(OpProtobufDataChunk(d, 4));
}

OP_STATUS
OpProtobufBasicOutputStream::WriteUint64(UINT64 n)
{
	char d[8]; // ARRAY OK 2009-05-05 jhoff
	RETURN_IF_ERROR(OpProtobufWireFormat::EncodeFixed64(d, sizeof(d), n));
	return out_range->Put(OpProtobufDataChunk(d, 8));
}

OP_STATUS
OpProtobufBasicOutputStream::WriteFloat(float n)
{
	// If the number is a NaN we send out a specific bit sequence
	// This ensures better compatibility with various platforms
	if (op_isnan(n))
	{
		// This represents a 32bit quiet NaN
		// s = sign, a = quiet bit, x = payload
		// s111 1111 1axx xxxx xxxx xxxx xxxx xxxx
		//
		// Version sent on the wire (little endian)
		// xxxx xxxx xxxx xxxx 1axx xxxx s111 1111
		//
		// The payload has at least one bit set to ensure that it stays a NaN even if the quiet-bit is unset.
		unsigned char d[4] = {0xff, 0xff, 0xff, 0x7f}; // ARRAY OK 2009-03-31 jborsodi
		return out_range->Put(OpProtobufDataChunk(reinterpret_cast<char *>(d), 4));
	}
	union
	{
		float f;
		INT32 i;
	};
	f = n;
	return WriteUint32(i);
}

OP_STATUS
OpProtobufBasicOutputStream::WriteDouble(double n)
{
	// If the number is a NaN we send out a specific bit sequence
	// This ensures better compatibility with various platforms
	if (op_isnan(n))
	{
		// This represents a 64bit quiet NaN
		// s = sign, a = quiet bit, x = payload
		// s111 1111 1111 axxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
		//
		// Version sent on the wire (little endian)
		// xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx 1111 axxx s111 1111
		//
		// The payload has at least one bit set to ensure that it stays a NaN even if the quiet-bit is unset.
		unsigned char d[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}; // ARRAY OK 2009-03-31 jborsodi
		return out_range->Put(OpProtobufDataChunk(reinterpret_cast<char *>(d), 8));
	}
	UINT32 hiword, loword;
	op_explode_double(n, hiword, loword);
	return WriteUint64( (static_cast<UINT64>(hiword) << 32) | static_cast<UINT64>(loword) );
}

OP_STATUS
OpProtobufBasicOutputStream::WriteBool(BOOL v)
{
	char data[1] = {v ? 1 : 0};
	return out_range->Put(OpProtobufDataChunk(data, 1));
}

OP_STATUS
OpProtobufBasicOutputStream::WriteString(OpProtobufStringChunkRange range)
{
	UTF8Encoder converter;
	int utf8_len = OpProtobufFormat::StringDataSize(range);
	RETURN_IF_ERROR(WriteVarInt32(utf8_len));
	char buf[1024]; // ARRAY OK 2009-05-05 jhoff

	for (; !range.IsEmpty(); range.PopFront())
	{
		const OpProtobufStringChunk &chunk = range.Front();
		const uni_char *str = chunk.GetString();
		int char_remaining = chunk.GetLength();
		while (char_remaining > 0)
		{
			int read;
			int written = converter.Convert(str, char_remaining*sizeof(uni_char), buf, sizeof(buf)/sizeof(char), &read);
			int char_read = read/sizeof(uni_char);
			if (char_read == 0)
				return OpStatus::ERR;
			str += char_read;
			RETURN_IF_ERROR(out_range->Put(OpProtobufDataChunk(buf, written)));
			char_remaining -= char_read;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufBasicOutputStream::WriteBytes(const OpProtobufDataChunkRange &range)
{
	RETURN_IF_ERROR(WriteVarInt32(range.Length()));
	OpProtobufDataOutputAdapter adapter(out_range);
	return adapter.Put(range);
}

/* OpProtobufOutputStream */

OpProtobufOutputStream::OpProtobufOutputStream(OpProtobufDataOutputRange *out_range)
	: OpProtobufBasicOutputStream(out_range)
{
}

int
OpProtobufOutputStream::CalculateMessageSize(const OpProtobufInstanceProxy &instance) const
{
	int size = instance.GetInstanceEncodedSize();
	if (size < 0)
	{
		size = 0;
		const OpProtobufMessage *message = instance.GetProtoMessage();
		for (int idx = 0; idx < message->GetFieldCount(); ++idx)
		{
			const OpProtobufField &field = message->GetField(idx);
			int field_size = CalculateFieldSize(instance, idx, field);
			size += field_size;
		}
		instance.SetInstanceEncodedSize(size);
		return size;
	}
	return size;
}

int
OpProtobufOutputStream::CalculateFieldSize(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field) const
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0 && field.GetQuantifier() != OpProtobufField::Required)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		if (!has_bits.IsSet(field_idx))
			return 0; // Field is not set, skip it
	}
	int field_size = OpProtobufWireFormat::FieldHeaderSize(OpProtobufFormat::ToWireType(field.GetType()), field.GetNumber());
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		switch (field.GetType())
		{
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			{
				const OpValueVector<INT64> *vector = instance.FieldPtrOpValueVectorINT64(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT64 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Int64Size(value);
				}
				return size;
			}
			case OpProtobufFormat::Sint64:
			{
				const OpValueVector<INT64> *vector = instance.FieldPtrOpValueVectorINT64(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT64 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Sint64Size(value);
				}
				return size;
			}
			case OpProtobufFormat::Uint64:
			{
				const OpValueVector<UINT64> *vector = instance.FieldPtrOpValueVectorUINT64(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					UINT64 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Uint64Size(value);
				}
				return size;
			}
			case OpProtobufFormat::Sfixed64:
			{
				const OpValueVector<INT64> *vector = instance.FieldPtrOpValueVectorINT64(field_idx);
				return (field_size + OpProtobufFormat::Fixed64Size)*vector->GetCount();
			}
			case OpProtobufFormat::Fixed64:
			{
				const OpValueVector<UINT64> *vector = instance.FieldPtrOpValueVectorUINT64(field_idx);
				return (field_size + OpProtobufFormat::Fixed64Size)*vector->GetCount();
			}
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return 0;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Float:
			{
				const OpValueVector<float> *vector = instance.FieldPtrOpValueVectorFloat(field_idx);
				return (field_size + OpProtobufFormat::FloatSize)*vector->GetCount();
			}
			case OpProtobufFormat::Double:
			{
				const OpValueVector<double> *vector = instance.FieldPtrOpValueVectorDouble(field_idx);
				return (field_size + OpProtobufFormat::DoubleSize)*vector->GetCount();
			}
			case OpProtobufFormat::Int32:
			{
				const OpValueVector<INT32> *vector = instance.FieldPtrOpValueVectorINT32(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT32 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Int32Size(value);
				}
				return size;
			}
			case OpProtobufFormat::Uint32:
			{
				const OpValueVector<UINT32> *vector = instance.FieldPtrOpValueVectorUINT32(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					UINT32 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Uint32Size(value);
				}
				return size;
			}
			break;
			case OpProtobufFormat::Sint32:
			{
				const OpValueVector<INT32> *vector = instance.FieldPtrOpValueVectorINT32(field_idx);
				int size = 0;
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT32 value = vector->Get(i);
					size += field_size + OpProtobufFormat::Sint32Size(value);
				}
				return size;
			}
			case OpProtobufFormat::Fixed32:
			{
				const OpValueVector<UINT32> *vector = instance.FieldPtrOpValueVectorUINT32(field_idx);
				return (field_size + OpProtobufFormat::Fixed32Size)*vector->GetCount();
			}
			case OpProtobufFormat::Sfixed32:
			{
				const OpValueVector<INT32> *vector = instance.FieldPtrOpValueVectorINT32(field_idx);
				return (field_size + OpProtobufFormat::Fixed32Size)*vector->GetCount();
			}
			case OpProtobufFormat::Bool:
			{
				const OpINT32Vector *vector = instance.FieldPtrOpINT32Vector(field_idx);
				return (field_size + OpProtobufFormat::BoolSize)*vector->GetCount();
			}
			case OpProtobufFormat::String:
			{
				OpProtobufStringArrayRange range(instance.GetStringArray(field_idx));
				int size = 0;
				for (; !range.IsEmpty(); range.PopFront())
				{
					size += field_size + OpProtobufFormat::StringSize(range.Front());
				}
				return size;
			}
			case OpProtobufFormat::Bytes:
			{
				OpProtobufDataArrayRange range(instance.GetBytesArray(field_idx));
				int size = 0;
				for (; !range.IsEmpty(); range.PopFront())
				{
					size += field_size + OpProtobufFormat::BytesSize(range.Front());
				}
				return size;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufRepeatedItems *messages = instance.FieldPtrOpProtobufRepeatedItems(field_idx);
				const OpProtobufMessage *sub_message_type = field.GetMessage();
				OP_ASSERT(sub_message_type != NULL);
				if (sub_message_type == NULL)
					return 0;
				int total_size = 0;
				for (unsigned int i = 0; i < messages->GetCount(); ++i)
				{
					void *sub_message = messages->Get(i);
					OP_ASSERT(sub_message != NULL);
					if (sub_message == NULL)
						return 0;
					OpProtobufInstanceProxy sub_instance(sub_message_type, sub_message);
					int message_size = CalculateMessageSize(sub_instance);
					OP_ASSERT(message_size > 0);
					total_size += field_size + OpProtobufWireFormat::VarIntSize64(message_size) + message_size;
				}
				return total_size;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return 0;
		}
	}
	else // Required | Optional
	{
		switch (field.GetType())
		{
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			{
				INT64 *value = instance.FieldPtrINT64(field_idx);
				return field_size + OpProtobufFormat::Int64Size(*value);
			}
			case OpProtobufFormat::Sint64:
			{
				INT64 *value = instance.FieldPtrINT64(field_idx);
				return field_size + OpProtobufFormat::Sint64Size(*value);
			}
			case OpProtobufFormat::Uint64:
			{
				UINT64 *value = instance.FieldPtrUINT64(field_idx);
				return field_size + OpProtobufFormat::Uint64Size(*value);
			}
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Fixed64:
				return field_size + OpProtobufFormat::Fixed64Size;
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return 0;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Float:
				return field_size + OpProtobufFormat::FloatSize;
			case OpProtobufFormat::Double:
				return field_size + OpProtobufFormat::DoubleSize;
			case OpProtobufFormat::Fixed32:
			case OpProtobufFormat::Sfixed32:
				return field_size + OpProtobufFormat::Fixed32Size;
			case OpProtobufFormat::Int32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				return field_size + OpProtobufFormat::Int32Size(*value);
			}
			case OpProtobufFormat::Uint32:
			{
				UINT32 *value = instance.FieldPtrUINT32(field_idx);
				return field_size + OpProtobufFormat::Uint32Size(*value);
			}
			case OpProtobufFormat::Sint32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				return field_size + OpProtobufFormat::Sint32Size(*value);
			}
			case OpProtobufFormat::Bool:
				return field_size + OpProtobufFormat::BoolSize;
			case OpProtobufFormat::String:
			{
				const OpProtobufStringChunkRange &range = instance.GetStringRange(field_idx);
				return field_size + OpProtobufFormat::StringSize(range);
			}
			case OpProtobufFormat::Bytes:
			{
				const OpProtobufDataChunkRange &range = instance.GetBytesRange(field_idx);
				return field_size + OpProtobufFormat::BytesSize(range);
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufMessage *sub_message_type = field.GetMessage();
				void *sub_instance_ptr;
				if (field.GetQuantifier() == OpProtobufField::Required)
					 sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
				else // Optional
					sub_instance_ptr = *instance.FieldPtrVoidPtr(field_idx); // Message *member;
				OP_ASSERT(sub_message_type != NULL);
				OpProtobufInstanceProxy sub_instance(sub_message_type, sub_instance_ptr);
				int message_size = CalculateMessageSize(sub_instance);
				OP_ASSERT(message_size > 0);
				return field_size + OpProtobufWireFormat::VarIntSize64(message_size) + message_size;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return 0; // Field with illegal types should simply be skipped
		}
	}
}

OP_STATUS
OpProtobufOutputStream::Write(const OpProtobufInstanceProxy &instance)
{
	level = 0;
	OP_STATUS status = WriteMessage(instance);
	OpStatus::Ignore(status);
	OP_ASSERT(level == 0);
	return status;
}

OP_STATUS
OpProtobufOutputStream::WriteMessage(const OpProtobufInstanceProxy &instance)
{
	++level;
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	for (int idx = 0; idx < message->GetFieldCount(); ++idx)
	{
		RETURN_IF_ERROR(WriteField(instance, idx, fields[idx]));
	}
	OP_ASSERT(level > 0);
	--level;
	return OpStatus::OK;
}

OP_STATUS
OpProtobufOutputStream::WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0 && field.GetQuantifier() != OpProtobufField::Required)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		if (has_bits.IsUnset(field_idx))
			return OpStatus::OK; // Field is not set, skip it
	}
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		switch (field.GetType())
		{
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Uint64:
			{
				const OpValueVector<INT64> *vector = instance.FieldPtrOpValueVectorINT64(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT64 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
					RETURN_IF_ERROR(WriteVarInt64(value));
				}
				break;
			}
			case OpProtobufFormat::Sint64:
			{
				const OpValueVector<INT64> *vector = instance.FieldPtrOpValueVectorINT64(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT64 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
					RETURN_IF_ERROR(WriteVarInt64(OpProtobufWireFormat::ZigZag64(value)));
				}
				break;
			}
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Fixed64:
			{
				const OpValueVector<UINT64> *vector = instance.FieldPtrOpValueVectorUINT64(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					UINT64 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed64, field.GetNumber()));
					RETURN_IF_ERROR(WriteUint64(value));
				}
				break;
			}
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Float:
			{
				const OpValueVector<float> *vector = instance.FieldPtrOpValueVectorFloat(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					float value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed32, field.GetNumber()));
					RETURN_IF_ERROR(WriteFloat(value));
				}
				break;
			}
			case OpProtobufFormat::Double:
			{
				const OpValueVector<double> *vector = instance.FieldPtrOpValueVectorDouble(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					double value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed64, field.GetNumber()));
					RETURN_IF_ERROR(WriteDouble(value));
				}
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Uint32:
			{
				const OpValueVector<INT32> *vector = instance.FieldPtrOpValueVectorINT32(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT32 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
					RETURN_IF_ERROR(WriteVarInt32(value));
				}
				break;
			}
			case OpProtobufFormat::Sint32:
			{
				const OpValueVector<INT32> *vector = instance.FieldPtrOpValueVectorINT32(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					INT32 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
					RETURN_IF_ERROR(WriteVarInt32(OpProtobufWireFormat::ZigZag32(value)));
				}
				break;
			}
			case OpProtobufFormat::Fixed32:
			case OpProtobufFormat::Sfixed32:
			{
				const OpValueVector<UINT32> *vector = instance.FieldPtrOpValueVectorUINT32(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					UINT32 value = vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed32, field.GetNumber()));
					RETURN_IF_ERROR(WriteUint32(value));
				}
				break;
			}
			case OpProtobufFormat::Bool:
			{
				const OpINT32Vector *vector = instance.FieldPtrOpINT32Vector(field_idx);
				for (unsigned int i = 0; i < vector->GetCount(); ++i)
				{
					BOOL value = !!vector->Get(i);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
					RETURN_IF_ERROR(WriteBool(value));
				}
				break;
			}
			case OpProtobufFormat::String:
			{
				OpProtobufStringArrayRange range(instance.GetStringArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
					RETURN_IF_ERROR(WriteString(range.Front()));
				}
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				OpProtobufDataArrayRange range(instance.GetBytesArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
					RETURN_IF_ERROR(WriteBytes(range.Front()));
				}
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufRepeatedItems *messages = instance.FieldPtrOpProtobufRepeatedItems(field_idx);
				const OpProtobufMessage *sub_message = field.GetMessage();
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				for (unsigned int i = 0; i < messages->GetCount(); ++i)
				{
					void *sub_instance_ptr = messages->Get(i);
					OP_ASSERT(sub_instance_ptr != NULL);
					if (sub_instance_ptr == NULL)
						return OpStatus::ERR;
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					int size = CalculateMessageSize(sub_instance);
					OP_ASSERT(size > 0);
					RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
					RETURN_IF_ERROR(WriteVarInt32(size));
#ifdef DEBUG_ENABLE_OPASSERT
					unsigned int pre_len = OutputRange()->Length();
#endif
					RETURN_IF_ERROR(WriteMessage(sub_instance));
#ifdef DEBUG_ENABLE_OPASSERT
					OP_ASSERT(OutputRange()->Length() == pre_len + size);
#endif
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
	}
	else // Required | Optional
	{
		switch (field.GetType())
		{
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
#ifdef OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Uint64:
			{
				INT64 *value = instance.FieldPtrINT64(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
				RETURN_IF_ERROR(WriteVarInt64(*value));
				break;
			}
			case OpProtobufFormat::Sint64:
			{
				INT64 *value = instance.FieldPtrINT64(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
				RETURN_IF_ERROR(WriteVarInt64(OpProtobufWireFormat::ZigZag64(*value)));
				break;
			}
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Fixed64:
			{
				INT64 *value = instance.FieldPtrINT64(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed64, field.GetNumber()));
				RETURN_IF_ERROR(WriteUint64(*value));
				break;
			}
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Float:
			{
				const float *value = instance.FieldPtrFloat(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed32, field.GetNumber()));
				RETURN_IF_ERROR(WriteFloat(*value));
				break;
			}
			case OpProtobufFormat::Double:
			{
				const double *value = instance.FieldPtrDouble(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed64, field.GetNumber()));
				RETURN_IF_ERROR(WriteDouble(*value));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Uint32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
				RETURN_IF_ERROR(WriteVarInt32(*value));
				break;
			}
			case OpProtobufFormat::Sint32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
				RETURN_IF_ERROR(WriteVarInt32(OpProtobufWireFormat::ZigZag32(*value)));
				break;
			}
			case OpProtobufFormat::Fixed32:
			case OpProtobufFormat::Sfixed32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::Fixed32, field.GetNumber()));
				RETURN_IF_ERROR(WriteUint32(*value));
				break;
			}
			case OpProtobufFormat::Bool:
			{
				BOOL *value = instance.FieldPtrBOOL(field_idx);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::VarInt, field.GetNumber()));
				RETURN_IF_ERROR(WriteBool(*value));
				break;
			}
			case OpProtobufFormat::String:
			{
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
				RETURN_IF_ERROR(WriteString(instance.GetStringRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
				RETURN_IF_ERROR(WriteBytes(instance.GetBytesRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufMessage *sub_message = field.GetMessage();
				void *sub_instance_ptr;
				if (field.GetQuantifier() == OpProtobufField::Required)
					 sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
				else // Optional
					sub_instance_ptr = *instance.FieldPtrVoidPtr(field_idx); // Message *member;
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
				int size = CalculateMessageSize(sub_instance);
				RETURN_IF_ERROR(WriteFieldHeader(OpProtobufWireFormat::LengthDelimited, field.GetNumber()));
				RETURN_IF_ERROR(WriteVarInt32(size));
#ifdef DEBUG_ENABLE_OPASSERT
				unsigned int pre_len = OutputRange()->Length();
#endif
				RETURN_IF_ERROR(WriteMessage(sub_instance));
#ifdef DEBUG_ENABLE_OPASSERT
				OP_ASSERT(OutputRange()->Length() == pre_len + size);
#endif
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

#ifdef PROTOBUF_JSON_SUPPORT

/* OpJSONOutputStream */

OpJSONOutputStream::OpJSONOutputStream(ByteBuffer &out)
	: out(out)
{
}

OP_STATUS
OpJSONOutputStream::Write(const OpProtobufInstanceProxy &instance)
{
	RETURN_IF_ERROR(out.Append1('['));
	RETURN_IF_ERROR(WriteMessage(instance));
	return out.Append1(']');
}

OP_STATUS
OpJSONOutputStream::WriteMessage(const OpProtobufInstanceProxy &instance, BOOL first)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	int count = message->GetFieldCount();
	if (message->GetOffsetToBitfield() >= 0)
	{
		count = 0;
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		for (int idx = 0; idx < message->GetFieldCount(); ++idx)
		{
			const OpProtobufField &field = fields[idx];
			if (field.GetQuantifier() == OpProtobufField::Required || has_bits.IsSet(idx))
			{
				if (field.GetQuantifier() == OpProtobufField::Repeated)
				{
					if (OpProtobufOutput::GetFieldCount(instance, idx, field) > 0)
						count = idx + 1;
				}
				else
					count = idx + 1;
			}
		}
	}
	for (int idx = 0; idx < count; ++idx)
	{
		const OpProtobufField &field = fields[idx];
		if (first)
			first = FALSE;
		else
			RETURN_IF_ERROR(out.Append1(','));
		RETURN_IF_ERROR(WriteField(instance, idx, field));
	}
	return OpStatus::OK;
}

OP_STATUS
OpJSONOutputStream::WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, BOOL embed, BOOL first)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0 && field.GetQuantifier() == OpProtobufField::Optional)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		if (!has_bits.IsSet(field_idx))
		{
			RETURN_IF_ERROR(out.AppendString("null"));
			return OpStatus::OK;
		}
	}
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		if (!embed)
			RETURN_IF_ERROR(out.Append1('['));
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				const OpValueVector<double> *value = instance.FieldPtrOpValueVectorDouble(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpDouble(out, value->Get(i)));
				}
				break;
			}
			case OpProtobufFormat::Float:
			{
				const OpValueVector<float> *value = instance.FieldPtrOpValueVectorFloat(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpDouble(out, value->Get(i)));
				}
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				const OpValueVector<INT32> *value = instance.FieldPtrOpValueVectorINT32(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpSignedLong(out, value->Get(i)));
				}
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				const OpValueVector<UINT32> *value = instance.FieldPtrOpValueVectorUINT32(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpUnsignedLong(out, value->Get(i)));
				}
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_JSON_SUPPORT
			// Not yet implemented, requires export of unsigned 64 bit values
#else // OP_PROTOBUF_64BIT_JSON_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_JSON_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				// TODO: Repeated BOOLs should really have a more optimized class, OpBitvector is useless since it does not know how many bits it has.
				const OpINT32Vector *value = instance.FieldPtrOpINT32Vector(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpUnsignedLong(out, value->Get(i) ? 1 : 0));
				}
				break;
			}
			case OpProtobufFormat::String:
			{
				OpProtobufStringArrayRange range(instance.GetStringArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpString(out, range.Front()));
				}
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				OpProtobufDataArrayRange range(instance.GetBytesArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					if (first)
						first = FALSE;
					else
						RETURN_IF_ERROR(out.Append1(','));
					RETURN_IF_ERROR(OpScopeJSON::DumpBytes(out, range.Front()));
				}
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufRepeatedItems *messages = instance.FieldPtrOpProtobufRepeatedItems(field_idx);
				const OpProtobufMessage *sub_message = field.GetMessage();
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				for (unsigned int i = 0; i < messages->GetCount(); ++i)
				{
					void *sub_instance_ptr = messages->Get(i);
					OP_ASSERT(sub_instance_ptr != NULL);
					if (sub_instance_ptr == NULL)
						return OpStatus::ERR;
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					if (first)
					{
						RETURN_IF_ERROR(out.Append1('['));
						first = FALSE;
					}
					else
						RETURN_IF_ERROR(out.AppendString(",["));
					RETURN_IF_ERROR(WriteMessage(sub_instance));
					RETURN_IF_ERROR(out.Append1(']'));
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		if (!embed)
			RETURN_IF_ERROR(out.Append1(']'));
	}
	else // Required | Optional
	{
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				double *value = instance.FieldPtrDouble(field_idx);
				RETURN_IF_ERROR(OpScopeJSON::DumpDouble(out, *value));
				break;
			}
			case OpProtobufFormat::Float:
			{
				float *value = instance.FieldPtrFloat(field_idx);
				RETURN_IF_ERROR(OpScopeJSON::DumpDouble(out, *value));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				RETURN_IF_ERROR(OpScopeJSON::DumpSignedLong(out, *value));
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				UINT32 *value = instance.FieldPtrUINT32(field_idx);
				RETURN_IF_ERROR(OpScopeJSON::DumpUnsignedLong(out, *value));
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_JSON_SUPPORT
			// Not yet implemented, requires export of unsigned 64 bit values
#else // OP_PROTOBUF_64BIT_JSON_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_JSON_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				BOOL *value = instance.FieldPtrBOOL(field_idx);
				RETURN_IF_ERROR(OpScopeJSON::DumpSignedLong(out, *value ? 1 : 0));
				break;
			}
			case OpProtobufFormat::String:
			{
				RETURN_IF_ERROR(OpScopeJSON::DumpString(out, instance.GetStringRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				RETURN_IF_ERROR(OpScopeJSON::DumpBytes(out, instance.GetBytesRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufMessage *sub_message = field.GetMessage();
				void *sub_instance_ptr;
				if (field.GetQuantifier() == OpProtobufField::Required)
					sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
				else // Optional
					sub_instance_ptr = *instance.FieldPtrVoidPtr(field_idx); // Message *member;
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
				if (!embed)
					RETURN_IF_ERROR(out.Append1('['));
				RETURN_IF_ERROR(WriteMessage(sub_instance, first));
				if (!embed)
					RETURN_IF_ERROR(out.Append1(']'));
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

#endif // PROTOBUF_JSON_SUPPORT

#ifdef PROTOBUF_XML_SUPPORT

/* OpXMLOutputStream */

OpXMLOutputStream::OpXMLOutputStream(ByteBuffer &out)
	: out(out)
{
}

OP_STATUS
OpXMLOutputStream::Write(const OpProtobufInstanceProxy &instance)
{
	const OpProtobufMessage *msg = instance.GetProtoMessage();
	RETURN_IF_ERROR(out.Append1('<'));
	RETURN_IF_ERROR(out.AppendString(msg->GetName()));
	RETURN_IF_ERROR(out.Append1('>'));
	RETURN_IF_ERROR(WriteMessage(instance));
	RETURN_IF_ERROR(out.AppendBytes("</", 2));
	RETURN_IF_ERROR(out.AppendString(msg->GetName()));
	return out.Append1('>');
}

OP_STATUS
OpXMLOutputStream::WriteMessage(const OpProtobufInstanceProxy &instance)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	for (int idx = 0; idx < message->GetFieldCount(); ++idx)
	{
		RETURN_IF_ERROR(WriteField(instance, idx, fields[idx]));
	}
	return OpStatus::OK;
}

OP_STATUS
OpXMLOutputStream::WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0 && field.GetQuantifier() != OpProtobufField::Required)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		if (!has_bits.IsSet(field_idx))
		{
			return OpStatus::OK;
		}
	}
	char buf[2 + 400 + 1 + 1] = "<<"; // ARRAY OK 2008-10-27 jborsodi
	const uni_char *field_name = field.GetName();
	int field_name_len = uni_strlen(field_name);
	UTF8Encoder converter;
	int read;
	int written = converter.Convert(field_name, field_name_len*sizeof(uni_char), buf + 2, sizeof(buf) - 2 - 1 - 1, &read);
	if (read == 0)
		return OpStatus::ERR;
	// We use the same string for the start and ending tag, but with different offsets
	buf[2 + written] = '>';
	buf[2 + written + 1] = '\0';
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		// Copy out the field name but without the suffix "List"
		char item_buf[2 + 400 + 1 + 1] = "<<"; // ARRAY OK 2008-10-27 jborsodi
		int item_name_len = field_name_len - 4;
		if (field_name_len <= 4 || uni_strcmp(field_name + field_name_len - 4, UNI_L("List")) != 0 || item_name_len > 400)
			return OpStatus::ERR;
		written = converter.Convert(field_name, item_name_len*sizeof(uni_char), item_buf + 2, sizeof(item_buf) - 2 - 1 - 1, &read);
		if (read == 0)
			return OpStatus::ERR;
		// We use the same string for the start and ending tag, but with different offsets
		item_buf[2 + written] = '>';
		item_buf[2 + written + 1] = '\0';

		buf[1] = '<';
		RETURN_IF_ERROR(out.AppendString(buf + 1));
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				const OpValueVector<double> *value = instance.FieldPtrOpValueVectorDouble(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteDouble(value->Get(i)));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::Float:
			{
				const OpValueVector<float> *value = instance.FieldPtrOpValueVectorFloat(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteDouble(value->Get(i)));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				const OpValueVector<INT32> *value = instance.FieldPtrOpValueVectorINT32(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteSignedLong(value->Get(i)));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				const OpValueVector<UINT32> *value = instance.FieldPtrOpValueVectorUINT32(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteUnsignedLong(value->Get(i)));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_XML_SUPPORT
			// Not yet implemented, requires export of unsigned 64 bit values
#else // OP_PROTOBUF_64BIT_XML_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_XML_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				// TODO: Repeated BOOLs should really have a more optimized class, OpBitvector is useless since it does not know how many bits it has.
				const OpINT32Vector *value = instance.FieldPtrOpINT32Vector(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteUnsignedLong(value->Get(i) ? 1 : 0));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::String:
			{
				OpProtobufStringArrayRange range(instance.GetStringArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteString(range.Front()));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				OpProtobufDataArrayRange range(instance.GetBytesArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
					RETURN_IF_ERROR(WriteBytes(range.Front()));
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufRepeatedItems *messages = instance.FieldPtrOpProtobufRepeatedItems(field_idx);
				const OpProtobufMessage *sub_message = field.GetMessage();
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				for (unsigned int i = 0; i < messages->GetCount(); ++i)
				{
					void *sub_instance_ptr = messages->Get(i);
					OP_ASSERT(sub_instance_ptr != NULL);
					if (sub_instance_ptr == NULL)
						return OpStatus::ERR;
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);

					item_buf[1] = '<';
					RETURN_IF_ERROR(out.AppendString(item_buf + 1));
/*					RETURN_IF_ERROR(out.Append1('<'));
					RETURN_IF_ERROR(out.AppendString(sub_message->GetName()));
					RETURN_IF_ERROR(out.Append1('>'));*/

					RETURN_IF_ERROR(WriteMessage(sub_instance));

/*					RETURN_IF_ERROR(out.AppendBytes("</", 2));
					RETURN_IF_ERROR(out.AppendString(sub_message->GetName()));
					RETURN_IF_ERROR(out.Append1('>'));*/
					item_buf[1] = '/';
					RETURN_IF_ERROR(out.AppendString(item_buf));
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		buf[1] = '/';
		RETURN_IF_ERROR(out.AppendString(buf));
	}
	else // Required | Optional
	{
		RETURN_IF_ERROR(out.AppendString(buf + 1));
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				double *value = instance.FieldPtrDouble(field_idx);
				RETURN_IF_ERROR(WriteDouble(*value));
				break;
			}
			case OpProtobufFormat::Float:
			{
				float *value = instance.FieldPtrFloat(field_idx);
				RETURN_IF_ERROR(WriteDouble(*value));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				INT32 *value = instance.FieldPtrINT32(field_idx);
				RETURN_IF_ERROR(WriteSignedLong(*value));
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				UINT32 *value = instance.FieldPtrUINT32(field_idx);
				RETURN_IF_ERROR(WriteUnsignedLong(*value));
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_XML_SUPPORT
			// Not yet implemented, requires export of unsigned 64 bit values
#else // OP_PROTOBUF_64BIT_XML_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_XML_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				BOOL *value = instance.FieldPtrBOOL(field_idx);
				RETURN_IF_ERROR(WriteUnsignedLong(*value ? 1 : 0));
				break;
			}
			case OpProtobufFormat::String:
			{
				RETURN_IF_ERROR(WriteString(instance.GetStringRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				RETURN_IF_ERROR(WriteBytes(instance.GetBytesRange(field_idx)));
				break;
			}
			case OpProtobufFormat::Message:
			{
				const OpProtobufMessage *sub_message = field.GetMessage();
				void *sub_instance_ptr;
				if (field.GetQuantifier() == OpProtobufField::Required)
					sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
				else // Optional
					sub_instance_ptr = *instance.FieldPtrVoidPtr(field_idx); // Message *member;
				OP_ASSERT(sub_message != NULL);
				if (sub_message == NULL)
					return OpStatus::ERR;
				OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
				RETURN_IF_ERROR(WriteMessage(sub_instance));
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		buf[1] = '/';
		RETURN_IF_ERROR(out.AppendString(buf));
	}
	return OpStatus::OK;
}

/*static*/
OP_STATUS
OpXMLOutputStream::WriteDouble(double value)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-17 jborsodi
	if(OpDoubleFormat::ToString(buf, value) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return out.AppendString(buf);
}

OP_STATUS
OpXMLOutputStream::WriteSignedLong(long value)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-24 jborsodi
	op_itoa(value, buf, 10);
	return out.AppendString(buf);
}

OP_STATUS
OpXMLOutputStream::WriteUnsignedLong(long value)
{
	char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2008-10-24 jborsodi
	OpProtobufUtils::uitoa(value, buf, 10);
	return out.AppendString(buf);
}

OP_STATUS
OpXMLOutputStream::WriteString(OpProtobufStringChunkRange range)
{
	if (range.IsEmpty())
	{
		return OpStatus::OK;
	}

	// TODO: Whitespace needs more attention, if they are present alternate presentation should be applied.

	for (; !range.IsEmpty(); range.PopFront())
	{
		OpProtobufStringChunk chunk = range.Front();
		const uni_char *string = chunk.GetString();
		int len = chunk.GetLength();

		OP_ASSERT(string != 0);
		const uni_char *iter = string;
		size_t new_size = 0;
		for (int i = 0; i < len; ++i, ++iter)
		{
			switch (*iter)
			{
			case '<':
			case '&':
				if (new_size)
					RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
				new_size = 0;
				RETURN_IF_ERROR(out.AppendString(*iter == '<' ? "&lt;" : "&amp;"));
				continue;
			default:
				++new_size;
			}
		}
		if (new_size)
			RETURN_IF_ERROR(OpProtobufUtils::Convert(out, iter - new_size, new_size));
	}
	return OpStatus::OK;
}

OP_STATUS
OpXMLOutputStream::WriteCString(const char *string, int len)
{
	if (!string || string[0] == '\0')
	{
		return OpStatus::OK;
	}
	if (len < 0)
		len = op_strlen(string);

	// TODO: Whitespace needs more attention, if they are present alternate presentation should be applied.

	OP_ASSERT(string != 0);
	const char *iter = string;
	size_t new_size = 0;
	for (int i = 0; i < len; ++i, ++iter)
	{
		switch (*iter)
		{
			case '<':
			case '&':
				if (new_size)
					RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
				new_size = 0;
				RETURN_IF_ERROR(out.AppendString(*iter == '<' ? "&lt;" : "&amp;"));
				continue;
			default:
				++new_size;
		}
	}
	if (new_size)
		RETURN_IF_ERROR(out.AppendBytes(iter - new_size, new_size));
	return OpStatus::OK;
}

OP_STATUS
OpXMLOutputStream::WriteBytes(OpProtobufDataChunkRange range)
{
	int sourcelen = range.Length();
	OpHeapArrayAnchor<char> source(OP_NEWA(char, sourcelen));
	RETURN_OOM_IF_NULL(source.Get());

	OpProtobufContigousDataOutputRange out_range(source.Get(), sourcelen);
	OpProtobufDataOutputAdapter adapter(&out_range);
	RETURN_IF_ERROR(adapter.Put(range));

	char *target = NULL;
	int targetlen = 0;
	MIME_Encode_Error err = MIME_Encode_SetStr(target, targetlen, source.Get(), sourcelen, "UTF-8", GEN_BASE64_ONELINE);
	if (err != MIME_NO_ERROR)
	{
		OP_DELETEA(target);
		return OpStatus::ERR;
	}
	OP_STATUS status = WriteCString(target, targetlen);
	OP_DELETEA(target);
	return status;
}

#endif // PROTOBUF_XML_SUPPORT

#endif // PROTOBUF_SUPPORT
