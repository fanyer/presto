/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_input_common.h"
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_debug.h"
//
#include "modules/protobuf/src/opvaluevector.h"
//
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"

#if defined(PROTOBUF_JSON_SUPPORT) || defined(PROTOBUF_XML_SUPPORT)
# include "modules/formats/base64_decode.h"
#endif // PROTOBUF_JSON_SUPPORT || PROTOBUF_XML_SUPPORT

#ifdef PROTOBUF_XML_SUPPORT
# include "modules/xmlutils/xmlnames.h"
#endif // PROTOBUF_XML_SUPPORT

/*
*
* Protocol Buffer features which are not supported:
* - Extensions
* - Groups
* - Services (we have our own way of creating them)
*
* TODO (before core 2.3):
* - When fields are missing (only for optional) a default value must be set (if there is a default).
* - Decide on format of Bytes for JSON and XML, now it is encoded using base64.
* - Decode on encoding of strings in XML, in relation to special characters and preserving whitespace.
* - Proper error handling when required fields are missing or input data is incorrect (e.g. invalid JSON)
* - Decide what to support of JSON, now it does not use objects, bools.
* - Support protobuf enums.
*
*/

/* OpProtobufInputStream */

OpProtobufInputStream::OpProtobufInputStream()
{
}

OP_STATUS
OpProtobufInputStream::Construct(const OpProtobufDataRange &r)
{
	range = r;
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::Construct(const unsigned char *_in, int _len)
{
	OP_ASSERT(_in != NULL);
	if (_in == NULL)
		return OpStatus::ERR_NULL_POINTER;

	return Construct(OpProtobufDataRange(reinterpret_cast<const char *>(_in), _len));
}

OP_STATUS
OpProtobufInputStream::Read(OpProtobufInstanceProxy &instance)
{
	RETURN_IF_ERROR(ReadMessage(instance));
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadMessage(OpProtobufInstanceProxy &instance)
{
	// TODO: Add proper error handling, e.g when required fields are missing or input is not according to type.
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	while (!range.IsEmpty())
	{
		size_t last = range.Length();
		INT32 header;
		OP_STATUS status = ReadVarInt32(header);
		if (OpStatus::IsError(status))
		{
			PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Failed parsing protobuf header (varint), error=%d", (int)status));
			return status;
		}
		int type = OpProtobufWireFormat::DecodeFieldType32(header);
		int num = OpProtobufWireFormat::DecodeFieldNumber32(header);
		// TODO: The selection of field according to number should use a more optimistic approach where it assumes fields are in order.
		const OpProtobufField *cur_field = NULL;
		int cur_idx = -1;
		for (int idx = 0; idx < message->GetFieldCount(); ++idx)
		{
			const OpProtobufField &field = fields[idx];
			if (field.GetNumber() == num)
			{
				OpProtobufWireFormat::WireType wiretype = OpProtobufFormat::ToWireType(field.GetType());
				if (wiretype != type) // Make sure we have the correct wire-type
				{
					PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Field (number=%d) has incorrect wireformat=%d, expected=%d", num, type, OpProtobufFormat::ToWireType(field.GetType())));
					return OpStatus::ERR;
				}

				cur_field = &field;
				cur_idx = idx;
				break;
			}
		}
		if (cur_field == NULL) // Field not found, TODO: Skip it
		{
			PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Field (number=%d) not found", num));
			return OpStatus::ERR;
		}
		OP_ASSERT(cur_idx >= 0);
		status = ReadField(instance, cur_idx, *cur_field);
		if (OpStatus::IsError(status))
		{
			PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Field (number=%d) could not be decoded, error=%d", num, (int)status));
			return status;
		}

		if (last == range.Length()) // The position has not moved, internal error
		{
			PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Decoding field (number=%d) did not consume any bytes", num));
			return OpStatus::ERR;
		}
	}
	// Check if all required fields have been parsed
	OP_STATUS status = OpProtobufInput::IsValid(instance, message);
#ifdef PROTOBUF_DEBUG_SUPPORT
	if (OpStatus::IsError(status))
	{
		PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: All required fields were not parsed"));
	}
#endif
	return status;
}

OP_STATUS
OpProtobufInputStream::ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		has_bits.SetBit(field_idx);
	}
	switch (field.GetType())
	{
		case OpProtobufFormat::Double:
		{
			double value;
			RETURN_IF_ERROR(ReadDouble(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarDouble(value, instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Float:
		{
			float value;
			RETURN_IF_ERROR(ReadFloat(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarFloat(value, instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Int32:
		{
			INT32 value;
			RETURN_IF_ERROR(ReadVarInt32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(value, instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Sint32:
		{
			INT32 value;
			RETURN_IF_ERROR(ReadVarInt32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(OpProtobufWireFormat::UnZigZag32(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Uint32:
		{
			INT32 value;
			RETURN_IF_ERROR(ReadVarInt32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT32(static_cast<UINT32>(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Sfixed32:
		{
			UINT32 value;
			RETURN_IF_ERROR(ReadFixed32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(static_cast<INT32>(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Fixed32:
		{
			UINT32 value;
			RETURN_IF_ERROR(ReadFixed32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT32(value, instance, field_idx, field));
			break;
		}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_PROTO_SUPPORT
		case OpProtobufFormat::Int64:
		{
			INT64 value;
			RETURN_IF_ERROR(ReadVarInt64(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT64(value, instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Sint64:
		{
			INT64 value;
			RETURN_IF_ERROR(ReadVarInt64(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT64(OpProtobufWireFormat::UnZigZag64(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Uint64:
		{
			INT64 value;
			RETURN_IF_ERROR(ReadVarInt64(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT64(static_cast<UINT64>(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Sfixed64:
		{
			UINT64 value;
			RETURN_IF_ERROR(ReadFixed64(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarINT64(static_cast<INT64>(value), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Fixed64:
		{
			UINT64 value;
			RETURN_IF_ERROR(ReadFixed64(value));
			RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT64(value, instance, field_idx, field));
			break;
		}
#else // OP_PROTOBUF_64BIT_PROTO_SUPPORT
		case OpProtobufFormat::Int64:
		case OpProtobufFormat::Sint64:
		case OpProtobufFormat::Uint64:
		case OpProtobufFormat::Sfixed64:
		case OpProtobufFormat::Fixed64:
			OP_ASSERT(!"64bit values are not supported");
			return OpStatus::ERR;
			break;
#endif // OP_PROTOBUF_64BIT_PROTO_SUPPORT
#endif // OP_PROTOBUF_64BIT_SUPPORT
		case OpProtobufFormat::Bool:
		{
			INT32 value;
			RETURN_IF_ERROR(ReadVarInt32(value));
			RETURN_IF_ERROR(OpProtobufInput::AddBool(value == 0 ? FALSE : TRUE, instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::String:
		{
			INT32 str_len;
			RETURN_IF_ERROR(ReadVarInt32(str_len));
			if (static_cast<size_t>(str_len) > range.Length())
				return OpStatus::ERR;
			OpProtobufDataRange new_range(range, str_len);
			range.PopFront(str_len);
			RETURN_IF_ERROR(OpProtobufInput::AddString(new_range.ChunkRange(), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Bytes:
		{
			INT32 bytes_len;
			RETURN_IF_ERROR(ReadVarInt32(bytes_len));
			if (static_cast<size_t>(bytes_len) > range.Length())
				return OpStatus::ERR;
			OpProtobufDataRange new_range(range, bytes_len);
			range.PopFront(bytes_len);
			RETURN_IF_ERROR(OpProtobufInput::AddBytes(new_range.ChunkRange(), instance, field_idx, field));
			break;
		}
		case OpProtobufFormat::Message:
		{
			INT32 msg_size;
			RETURN_IF_ERROR(ReadVarInt32(msg_size));
			if (msg_size == 0)
				break;

			// Constrain the current parsing scope to the sub-message by replacing end-pointer and length
			// Original end-pointer and length is restored when sub-message is parsed
			OpProtobufDataRange orig_range = range;
			range.PopBack(range.Length() - msg_size);

			OP_STATUS status = OpStatus::OK;
			if (field.GetQuantifier() == OpProtobufField::Repeated)
			{
				const OpProtobufMessage *sub_message = field.GetMessage();
				OP_ASSERT(sub_message != NULL);
				void *sub_instance_ptr = NULL;
				status = OpProtobufInput::AddMessage(sub_instance_ptr, instance, field_idx, field);
				if (OpStatus::IsSuccess(status))
				{
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					status = ReadMessage(sub_instance);
				}
			}
			else
			{
				const OpProtobufMessage *sub_message = field.GetMessage();
				void *sub_instance_ptr;
				if (field.GetQuantifier() == OpProtobufField::Required)
					sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
				else // Optional
					RETURN_IF_ERROR(OpProtobufInput::CreateMessage(sub_instance_ptr, instance, field_idx, field)); // Message *member;
				OP_ASSERT(sub_message != NULL);
				OP_ASSERT(sub_instance_ptr != NULL);
				OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
				status = ReadMessage(sub_instance);
			}

			// Restore end-pointer and length
			range = orig_range;
			range.PopFront(msg_size);

			if (OpStatus::IsError(status))
			{
				PROTOBUF_DEBUG("OpProtobufInputStream", ("Error: Sub-message '%s' of field (number=%d) could not be decoded, error=%d", field.GetMessage()->GetName(), field.GetNumber(), (int)status));
				return status;
			}
			break;
		}
		default:
			OP_ASSERT(!"Should not happen");
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadVarInt32(INT32 &out)
{
	int len;
	RETURN_IF_ERROR(OpProtobufWire::DecodeVarInt32(range, len, out));
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadVarInt64(INT64 &out)
{
	int len;
	RETURN_IF_ERROR(OpProtobufWire::DecodeVarInt64(range, len, out));
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadFixed32(UINT32 &out)
{
	if (range.Length() < 4)
		return OpStatus::ERR;
	RETURN_IF_ERROR(OpProtobufWire::DecodeFixed32(range, out));
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadFixed64(UINT64 &out)
{
	if (range.Length() < 8)
		return OpStatus::ERR;
	RETURN_IF_ERROR(OpProtobufWire::DecodeFixed64(range, out));
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadDouble(double &out)
{
	UINT64 i;
	RETURN_IF_ERROR(ReadFixed64(i));
	out = op_implode_double(i >> 32, i & 0xffffffffu);
	return OpStatus::OK;
}

OP_STATUS
OpProtobufInputStream::ReadFloat(float &out)
{
	union
	{
		UINT32 i;
		float f;
	} val;
	RETURN_IF_ERROR(ReadFixed32(val.i));
	out = val.f;
	return OpStatus::OK;
}

#ifdef PROTOBUF_JSON_SUPPORT

/* OpJSONInputStream */

OpJSONInputStream::OpJSONInputStream()
	: parser(lexer)
{
}

OP_STATUS
OpJSONInputStream::Construct(const uni_char *in, int len)
{
	OP_ASSERT(in != NULL);
	if (in == NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(input.Construct(in, len));
	return lexer.Construct(&input);
}

OP_STATUS
OpJSONInputStream::Read(OpProtobufInstanceProxy &instance)
{
	OP_STATUS status = lexer.Match(Lexer::TOK_BRACKET_START);
	if (OpStatus::IsSuccess(status))
	{
		status = ReadMessage(instance);
		if (OpStatus::IsSuccess(status))
		{
			status = lexer.Match(Lexer::TOK_BRACKET_END);
			if (OpStatus::IsSuccess(status))
			{
				return OpStatus::OK;
			}
			else
				lexer.SetError(UNI_L("Expected closing square bracket"));
		}
	}
	else
		lexer.SetError(UNI_L("Expected opening square bracket"));
	if (OpStatus::IsError(status))
	{
		error.SetDescription(lexer.GetError().description.CStr());
		error.SetLine(lexer.GetError().line);
		error.SetColumn(lexer.GetError().column);
		error.SetOffset(lexer.GetError().offset);
	}
	return status;
}

OP_STATUS
OpJSONInputStream::ReadMessage(OpProtobufInstanceProxy &instance)
{
	// TODO: Add proper error handling, e.g when required fields are missing or input is not according to type.
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	BOOL first = TRUE;
	for (int idx = 0; idx < message->GetFieldCount(); ++idx)
	{
		Lexer::MatchResult res;
		RETURN_IF_ERROR(lexer.LT(1, res));
		if (res.token == Lexer::TOK_BRACKET_END)
		{
			for (int check_idx = idx; check_idx < message->GetFieldCount(); ++check_idx)
			{
				const OpProtobufField &field = fields[check_idx];
				if (field.GetQuantifier() == OpProtobufField::Required)
				{
					uni_char buf[300]; // ARRAY OK 2009-07-09 jhoff
					OpString field_name;
					OP_STATUS status = field_name.Set(field.GetName());
					if (OpStatus::IsSuccess(status))
					{
						OpString message_name;
						status = message_name.Set(message->GetName());
						if (OpStatus::IsSuccess(status))
						{
							int chars = uni_snprintf(buf, ARRAY_SIZE(buf), UNI_L("Required field %s.%s = %d is missing from input"), message_name.CStr(), field_name.CStr(), field.GetNumber());
							if (0 <= chars && (unsigned int)chars < ARRAY_SIZE(buf))
								lexer.SetError(buf, res);
						}
					}
					return OpStatus::ERR_PARSING_FAILED;
				}
			}
			// All trailing fields can be skipped, parse the bracket and return
			return OpStatus::OK;
		}
		if (first)
			first = FALSE;
		else
		{
			RETURN_IF_ERROR(lexer.Match(Lexer::TOK_COMMA));
		}
		RETURN_IF_ERROR(ReadField(instance, idx, fields[idx]));
	}
	return OpStatus::OK;
}

OP_STATUS
OpJSONInputStream::ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, BOOL embed, BOOL first)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (field.GetQuantifier() != OpProtobufField::Required)
	{
		Lexer::Token t;
		RETURN_IF_ERROR(lexer.LT(1, t));
		if (message->GetOffsetToBitfield() >= 0)
		{
			OpProtobufBitFieldRef has_bits = instance.GetBitfield();
			has_bits.AssignBit(field_idx, t != Lexer::TOK_NULL);
		}
		if (t == Lexer::TOK_NULL)
			return lexer.Match(Lexer::TOK_NULL);
	}
	BOOL is_repeated = FALSE;
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		is_repeated = TRUE;
		RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_START));
	}
	OpProtobufFormat::FieldType type = field.GetType();
	unsigned array_count = 0; // Number of array items that have been read
	do
	{
		if (is_repeated)
		{
			Lexer::Token t;
			RETURN_IF_ERROR(lexer.LT(1, t));
			if (t == Lexer::TOK_BRACKET_END)
			{
				if (array_count == 0 && message->GetOffsetToBitfield() >= 0)
				{
					// Empty arrays should be treated as if the array was not sent
					instance.GetBitfield().ClearBit(field_idx);
				}
				break;
			}
		}

		if (first)
			first = FALSE;
		else
			RETURN_IF_ERROR(lexer.Match(Lexer::TOK_COMMA));

		switch (type)
		{
			case OpProtobufFormat::Double:
			{
				double number;
				RETURN_IF_ERROR(parser.ParseDouble(number));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarDouble(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Float:
			{
				double number;
				RETURN_IF_ERROR(parser.ParseDouble(number));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarFloat(static_cast<float>(number), instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				INT32 number;
				RETURN_IF_ERROR(parser.ParseSignedInteger(number));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				UINT32 number;
				RETURN_IF_ERROR(parser.ParseUnsignedInteger(number));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT32(number, instance, field_idx, field));
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_JSON_SUPPORT
			// Not yet implemented, requires support for parsing unsigned 64 bit values
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
				INT32 number;
				RETURN_IF_ERROR(parser.ParseSignedInteger(number));
				RETURN_IF_ERROR(OpProtobufInput::AddBool(number != 0 ? TRUE : FALSE, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::String:
			{
				if (field.GetQuantifier() == OpProtobufField::Repeated)
				{
					OpAutoPtr<OpString> str(OP_NEW(OpString, ()));
					RETURN_OOM_IF_NULL(str.get());
					RETURN_IF_ERROR(parser.ParseString(*str.get()));
					RETURN_IF_ERROR(instance.AddString(field_idx, str.get(), TRUE));
					str.release();
				}
				else
				{
					if (instance.IsFieldOpString(field_idx)) // Optimization when field is of type OpString
					{
						OpString *str = instance.FieldPtrOpString(field_idx);
						RETURN_IF_ERROR(parser.ParseString(*str));
					}
					else
					{
						OpString str;
						RETURN_IF_ERROR(parser.ParseString(str));
						RETURN_IF_ERROR(instance.SetString(field_idx, OpProtobufStringChunkRange(str)));
					}
				}
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				OpString base64_str;
				RETURN_IF_ERROR(parser.ParseString(base64_str));
				char *utf8 = NULL;
				RETURN_IF_ERROR(base64_str.UTF8Alloc(&utf8));
				if (utf8)
				{
					// Lots of (too much) code to convert base64 to binary data
					unsigned long base64_len = op_strlen(utf8);
					unsigned long tmp_len = ((base64_len+3)/4)*3;
					unsigned long read_pos = 0;
					BOOL warning = FALSE;

					OpHeapArrayAnchor<char> tmp(OP_NEWA(char, tmp_len));
					if (tmp.Get() == NULL)
					{
						delete []utf8;
						return OpStatus::ERR_NO_MEMORY;
					}

					int decode_len = GeneralDecodeBase64(reinterpret_cast<const unsigned char *>(utf8), base64_len, read_pos, reinterpret_cast<unsigned char *>(tmp.Get()), warning, tmp_len);

					delete []utf8;
					if (warning)
						return OpStatus::ERR;
					if (read_pos != base64_len)
						return OpStatus::ERR;
					RETURN_IF_ERROR(OpProtobufInput::AddBytes(OpProtobufDataChunkRange(tmp.Get(), decode_len), instance, field_idx, field));
				}
				else
				{
					RETURN_IF_ERROR(OpProtobufInput::AddBytes(OpProtobufDataChunkRange(), instance, field_idx, field));
				}
				break;
			}
			case OpProtobufFormat::Message:
			{
				if (field.GetQuantifier() == OpProtobufField::Repeated)
				{
					RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_START));

					const OpProtobufMessage *sub_message = field.GetMessage();
					OP_ASSERT(sub_message != NULL);
					void *sub_instance_ptr = NULL;
					RETURN_IF_ERROR(OpProtobufInput::AddMessage(sub_instance_ptr, instance, field_idx, field));
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					RETURN_IF_ERROR(ReadMessage(sub_instance));

					RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_END));
				}
				else
				{
					if (!embed)
						RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_START));

					const OpProtobufMessage *sub_message = field.GetMessage();
					void *sub_instance_ptr;
					if (field.GetQuantifier() == OpProtobufField::Required)
						sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
					else // Optional
						RETURN_IF_ERROR(OpProtobufInput::CreateMessage(sub_instance_ptr, instance, field_idx, field));
					OP_ASSERT(sub_message != NULL);
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					RETURN_IF_ERROR(ReadMessage(sub_instance));

					if (!embed)
						RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_END));
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		if (is_repeated)
			++array_count;
	} while(is_repeated);
	if (is_repeated)
	{
		RETURN_IF_ERROR(lexer.Match(Lexer::TOK_BRACKET_END));
	}
	return OpStatus::OK;
}

#endif // PROTOBUF_JSON_SUPPORT

#ifdef PROTOBUF_XML_SUPPORT

/* OpXMLInputStream */

OpXMLInputStream::OpXMLInputStream()
	: is_valid(FALSE)
	, has_error(FALSE)
{
}

OP_STATUS
OpXMLInputStream::Construct(const char *in, int len)
{
	OP_ASSERT(in != NULL);
	if (in == NULL)
		return OpStatus::ERR_NULL_POINTER;
	has_error = is_valid = FALSE;
	RETURN_IF_ERROR(text.SetFromUTF8(in, len));
	OP_STATUS status = xml.Parse(text.DataPtr(), text.Length());
	if (status == OpStatus::ERR)
	{
#ifdef XML_ERRORS
	    /* Parse error access.  Enabled by FEATURE_XML_ERRORS. */
		// FIXME: Should we always set this feature when scope is in use?

		RETURN_IF_ERROR(error.SetDescription(xml.GetErrorDescription()));
		if (xml.GetErrorLocation().start.line != ~0u)
			error.SetLine(xml.GetErrorLocation().start.line);
		if (xml.GetErrorLocation().start.column != ~0u)
			error.SetColumn(xml.GetErrorLocation().start.column);
		has_error = TRUE;
#endif // XML_ERRORS
		return status;
	}
	else if (OpStatus::IsError(status))
		return status;
	is_valid = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpXMLInputStream::Read(OpProtobufInstanceProxy &instance)
{	
	OP_ASSERT(is_valid);
	if (!is_valid)
		return OpStatus::ERR;
	OpString name;
	RETURN_IF_ERROR(name.Set(instance.GetProtoMessage()->GetName()));
	if (!xml.EnterElement(name.DataPtr()))
		return OpStatus::ERR;
	RETURN_IF_ERROR(ReadMessage(xml, instance));
	xml.LeaveElement();
	return OpStatus::OK;
}

OP_STATUS
OpXMLInputStream::ReadMessage(XMLFragment &xml, OpProtobufInstanceProxy &instance)
{
	// TODO: Add proper error handling, e.g when required fields are missing or input is not according to type.
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	OpString fieldname;
	while (xml.HasMoreElements())
	{
		// TODO: The code for finding correct field according to xml element name should
		//       be placed in a separate class and should provide optimistic matching
		//       by assuming the order of elements are as the field defs.
		if (!xml.EnterAnyElement())
			return OpStatus::ERR;
		const XMLCompleteName xmlname = xml.GetElementName();
		const uni_char *name = xmlname.GetLocalPart();
		const OpProtobufField *field = NULL;
		int field_idx = -1;
		for (int idx = 0; idx < message->GetFieldCount(); ++idx)
		{
			const OpProtobufField *cur = &fields[idx];
			RETURN_IF_ERROR(fieldname.Set(cur->GetName()));
			if (fieldname.Compare(name) == 0)
			{
				field = cur;
				field_idx = idx;
				break;
			}
		}

		if (field)
		{
			OP_ASSERT(field_idx >= 0);
			RETURN_IF_ERROR(ReadField(xml, instance, field_idx, *field, fieldname.DataPtr()));
		}
		xml.LeaveElement();
	}
	// Check if all required fields have been parsed
	return OpProtobufInput::IsValid(instance, message);
}

OP_STATUS
OpXMLInputStream::ReadField(XMLFragment &xml, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, const uni_char *name)
{
	OP_ASSERT(name != NULL);
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		has_bits.SetBit(field_idx);
	}
	// Copy out the field name but without the suffix "List"
	OpString item_name;
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		const uni_char *field_name = field.GetName();
		int field_name_len = uni_strlen(field_name);
		int item_name_len = field_name_len - 4;
		if (field_name_len <= 4 || uni_strcmp(field_name + field_name_len - 4, UNI_L("List")) != 0 || item_name_len > 400)
			return OpStatus::ERR;
		RETURN_IF_ERROR(item_name.Append(field_name, item_name_len));
	}
	while (TRUE)
	{
		if (field.GetQuantifier() == OpProtobufField::Repeated)
		{
			if (!xml.HasMoreElements())
				return OpStatus::OK;
			if (item_name.CStr() == NULL || !xml.EnterElement(item_name.CStr()))
				return OpStatus::ERR_PARSING_FAILED;
		}
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			case OpProtobufFormat::Float:
			{
				const uni_char *text = xml.GetText();
				if (text == NULL)
					return OpStatus::ERR_PARSING_FAILED;
				uni_char *end = NULL;
				double number = uni_strtod(text, &end);
				if (*end != '\0')
					return OpStatus::ERR_PARSING_FAILED;
				if (field.GetType() == OpProtobufFormat::Float)
					RETURN_IF_ERROR(OpProtobufInput::AddScalarFloat(static_cast<float>(number), instance, field_idx, field));
				else
					RETURN_IF_ERROR(OpProtobufInput::AddScalarDouble(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				const long int32_max = 0x7fffffff;
				const long int32_min = -int32_max - 1;

				long number;
				ConvFunction<long> f(uni_strtol);
				RETURN_IF_ERROR(OpProtobufUtils::ParseLong(xml.GetText(), f, int32_min, int32_max, number));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				const unsigned long uint32_max = 0xffffffffU;
				const unsigned long uint32_min = 0;

				unsigned long number;
				ConvFunction<unsigned long> f(uni_strtoul);
				RETURN_IF_ERROR(OpProtobufUtils::ParseLong(xml.GetText(), f, uint32_min, uint32_max, number, '-'));
				RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT32(number, instance, field_idx, field));	
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
#ifdef OP_PROTOBUF_64BIT_XML_SUPPORT
			// Not yet implemented, requires support for parsing unsigned 64 bit values
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
				const uni_char *text = xml.GetText();
				if (text == NULL)
					return OpStatus::ERR_PARSING_FAILED;
				uni_char *end = NULL;
				unsigned long number = uni_strtoul(text, &end, 10);
				if (*end != '\0')
					return OpStatus::ERR_PARSING_FAILED;
				if (number != 0 && number != 1) // We use 0 for FALSE, and 1 for TRUE, other values are not allowed
					return OpStatus::ERR_PARSING_FAILED;
				RETURN_IF_ERROR(OpProtobufInput::AddBool(text ? (number == 0 ? FALSE : TRUE) : FALSE, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::String:
			{
				const uni_char *text = xml.GetText();
				RETURN_IF_ERROR(OpProtobufInput::AddString(text, uni_strlen(text), instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				const uni_char *text = xml.GetText();
				if (text)
				{
					// Lots of (too much) code to convert base64 to binary data
					OpString8 input;
					RETURN_IF_ERROR(input.SetUTF8FromUTF16(text));

					int input_len = input.Length();
					int tmp_len = ((input_len+3)/4)*3;
					unsigned long read_pos = 0;
					BOOL warning = FALSE;

					OpHeapArrayAnchor<char> tmp(OP_NEWA(char, tmp_len));
					RETURN_OOM_IF_NULL(tmp.Get());

					int decode_len = GeneralDecodeBase64(reinterpret_cast<const unsigned char *>(input.DataPtr()), input_len, read_pos, reinterpret_cast<unsigned char *>(tmp.Get()), warning, tmp_len);

					if (warning)
						return OpStatus::ERR;

					if ((int)read_pos != input_len)
						return OpStatus::ERR;
					RETURN_IF_ERROR(OpProtobufInput::AddBytes(OpProtobufDataChunkRange(tmp.Get(), decode_len), instance, field_idx, field));
				}
				else
					RETURN_IF_ERROR(OpProtobufInput::AddBytes(OpProtobufDataChunkRange(), instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Message:
			{
				if (field.GetQuantifier() == OpProtobufField::Repeated)
				{
					const OpProtobufMessage *sub_message = field.GetMessage();
					OP_ASSERT(sub_message != NULL);

					while (xml.HasMoreElements())
					{
						void *sub_instance_ptr = NULL;
						RETURN_IF_ERROR(OpProtobufInput::AddMessage(sub_instance_ptr, instance, field_idx, field));
						OP_ASSERT(sub_instance_ptr != NULL);
						OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
						RETURN_IF_ERROR(ReadMessage(xml, sub_instance));
					}
				}
				else
				{
					const OpProtobufMessage *sub_message = field.GetMessage();
					void *sub_instance_ptr;
					if (field.GetQuantifier() == OpProtobufField::Required)
						sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
					else // Optional
						RETURN_IF_ERROR(OpProtobufInput::CreateMessage(sub_instance_ptr, instance, field_idx, field)); // Message *member;
					OP_ASSERT(sub_message != NULL);
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					RETURN_IF_ERROR(ReadMessage(xml, sub_instance));
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		if (field.GetQuantifier() == OpProtobufField::Repeated)
		{
			xml.LeaveElement(); // item_name
		}
		else
			break;
	}
	return OpStatus::OK;
}

#endif // PROTOBUF_XML_SUPPORT

#endif // PROTOBUF_SUPPORT
