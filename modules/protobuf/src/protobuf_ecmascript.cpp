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

#ifdef PROTOBUF_ECMASCRIPT_SUPPORT

#include "modules/protobuf/src/protobuf_ecmascript.h"

#include "modules/protobuf/src/protobuf_input_common.h"
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/protobuf/src/protobuf_output_common.h"
#include "modules/protobuf/src/json_tools.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_data.h"
//
#include "modules/protobuf/src/opvaluevector.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/opstring.h"
#include "modules/formats/base64_decode.h"

// For Ecmascript functions
#include "modules/ecmascript/ecmascript.h"

/* OpESInputStream */

class OpESInputStreamPrivate
{
public:
	OpESInputStreamPrivate();

	OP_STATUS ReadMessage(OpProtobufInstanceProxy &instance, ES_Object *es_object, ES_Runtime *es_runtime, unsigned &array_index);
	OP_STATUS ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, ES_Runtime *es_runtime, ES_Object *array_object, unsigned &array_index);
};

OpESInputStreamPrivate::OpESInputStreamPrivate()
{
}

static OP_STATUS
GetArrayLength(ES_Object *es_object, ES_Runtime *es_runtime, unsigned int *length)
{
	ES_Value value;
	OP_STATUS result = es_runtime->GetName(es_object, UNI_L("length"), &value);
	RETURN_IF_ERROR(result);
	if (result == OpBoolean::IS_FALSE || value.type != VALUE_NUMBER)
		return OpStatus::ERR_PARSING_FAILED;
	*length = static_cast<unsigned int>(value.value.number);
	return OpStatus::OK;
}

static BOOL
IsArray(ES_Value &value, ES_Runtime *es_runtime)
{
	// TODO: This could be expanded to check if the prototype is array
	return value.type == VALUE_OBJECT;
}

#define RETURN_IF_ERROR_OR_FALSE(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
		else if (RETURN_IF_ERROR_TMP == OpBoolean::IS_FALSE) \
			return OpStatus::ERR_PARSING_FAILED; \
	} while (0)

OP_STATUS
OpESInputStreamPrivate::ReadMessage(OpProtobufInstanceProxy &instance, ES_Object *es_object, ES_Runtime *es_runtime, unsigned &array_index)
{
	OP_ASSERT(es_object != NULL);

	// TODO: Add proper error handling, e.g when required fields are missing or input is not according to type.
	const OpProtobufMessage *message = instance.GetProtoMessage();
	const OpProtobufField *fields = message->GetFields();
	unsigned int array_length;
	RETURN_IF_ERROR(GetArrayLength(es_object, es_runtime, &array_length));
	for (unsigned int idx = 0; idx < static_cast<unsigned int>(message->GetFieldCount()); ++idx)
	{
		const OpProtobufField &field = fields[idx];
		if (array_index >= array_length)
		{
			if (field.GetQuantifier() == OpProtobufField::Required)
				return OpStatus::ERR_PARSING_FAILED;
			// Check if all fields following this can be left out.
			for (int miss_idx = idx + 1; miss_idx < message->GetFieldCount(); ++miss_idx)
			{
				const OpProtobufField &miss_field = fields[miss_idx];
				if (miss_field.GetQuantifier() == OpProtobufField::Required)
					return OpStatus::ERR_PARSING_FAILED;
			}
			// All trailing fields can be skipped, parse the bracket and return
			return OpStatus::OK;
		}
		RETURN_IF_ERROR(ReadField(instance, idx, field, es_runtime, es_object, array_index));
	}
	return OpStatus::OK;
}

OP_STATUS
OpESInputStreamPrivate::ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, ES_Runtime *es_runtime, ES_Object *array_object, unsigned &array_index)
{
	const OpProtobufMessage *message = instance.GetProtoMessage();
	ES_Value field_item;
	RETURN_IF_ERROR_OR_FALSE(es_runtime->GetIndex(array_object, array_index++, &field_item));
	if (field.GetQuantifier() != OpProtobufField::Required)
	{
		if (message->GetOffsetToBitfield() >= 0)
		{
			OpProtobufBitFieldRef has_bits = instance.GetBitfield();
			if (field_item.type == VALUE_NULL || field_item.type == VALUE_UNDEFINED)
			{
				has_bits.ClearBit(field_idx);
				return OpStatus::OK;
			}
			else
				has_bits.SetBit(field_idx);
		}
	}
	BOOL is_repeated = FALSE;

	// Used when field_item is an array
	unsigned array_idx = 0;
	unsigned array_length = 1;
	unsigned array_count = 0; // Number of array items that have been read
	ES_Value array_item;
	array_object = NULL;

	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		if (field_item.type != VALUE_OBJECT)
			return OpStatus::ERR_PARSING_FAILED;

		array_object = field_item.value.object;
		RETURN_IF_ERROR(GetArrayLength(array_object, es_runtime, &array_length));
		is_repeated = TRUE;
		array_idx = 0;
	}
	OpProtobufFormat::FieldType type = field.GetType();
	for (; array_idx < array_length; ++array_idx)
	{
		if (is_repeated)
		{
			RETURN_IF_ERROR_OR_FALSE(es_runtime->GetIndex(array_object, array_idx, &array_item));
			++array_count;
		}
		ES_Value &value = is_repeated ? array_item : field_item;

		switch (type)
		{
			case OpProtobufFormat::Double:
			{
				if (value.type != VALUE_NUMBER)
					return OpStatus::ERR_PARSING_FAILED;
				double number = value.value.number;
				RETURN_IF_ERROR(OpProtobufInput::AddScalarDouble(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Float:
			{
				if (value.type != VALUE_NUMBER)
					return OpStatus::ERR_PARSING_FAILED;
				float number = static_cast<float>(value.value.number);
				RETURN_IF_ERROR(OpProtobufInput::AddScalarFloat(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				if (value.type != VALUE_NUMBER || value.value.number < INT_MIN ||  value.value.number > INT_MAX)
					return OpStatus::ERR_PARSING_FAILED;
				INT32 number = static_cast<INT32>(value.value.number);
				RETURN_IF_ERROR(OpProtobufInput::AddScalarINT32(number, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				if (value.type != VALUE_NUMBER || value.value.number < 0 ||  value.value.number > UINT_MAX)
					return OpStatus::ERR_PARSING_FAILED;
				UINT32 number = static_cast<UINT32>(value.value.number);
				RETURN_IF_ERROR(OpProtobufInput::AddScalarUINT32(number, instance, field_idx, field));
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				if (value.type != VALUE_NUMBER)
					return OpStatus::ERR_PARSING_FAILED;
				INT32 number = static_cast<INT32>(value.value.number);
				RETURN_IF_ERROR(OpProtobufInput::AddBool(number != 0 ? TRUE : FALSE, instance, field_idx, field));
				break;
			}
			case OpProtobufFormat::String:
			case OpProtobufFormat::Bytes:
			{
				const uni_char *input_storage;
				int input_len;

				if (value.type == VALUE_STRING)
				{
					input_storage = value.value.string;
					input_len = uni_strlen(input_storage);
				}
				else if (value.type == VALUE_STRING_WITH_LENGTH)
				{
					input_storage = value.value.string_with_length->string;
					input_len = value.value.string_with_length->length;
				}
				else
					return OpStatus::ERR_PARSING_FAILED;

				if (type == OpProtobufFormat::String)
				{
					RETURN_IF_ERROR(OpProtobufInput::AddString(input_storage, input_len, instance, field_idx, field));
				}
				else
				{
					OpString8 utf8;
					RETURN_IF_ERROR(utf8.SetUTF8FromUTF16(input_storage, input_len));
					int base64_len = utf8.Length();
					int tmp_len = ((base64_len+3)/4)*3;
					unsigned long read_pos = 0;
					BOOL warning = FALSE;

					OpHeapArrayAnchor<char> tmp(OP_NEWA(char, tmp_len));
					RETURN_OOM_IF_NULL(tmp.Get());

					int decode_len = GeneralDecodeBase64(reinterpret_cast<const unsigned char *>(utf8.CStr()), base64_len, read_pos, reinterpret_cast<unsigned char *>(tmp.Get()), warning, tmp_len);

					if (warning)
						return OpStatus::ERR;
					if ((int)read_pos != base64_len)
						return OpStatus::ERR;
					RETURN_IF_ERROR(OpProtobufInput::AddBytes(OpProtobufDataChunkRange(tmp.Get(), decode_len), instance, field_idx, field));
				}
				break;
			}
			case OpProtobufFormat::Message:
			{
				if (!IsArray(value, es_runtime))
					return OpStatus::ERR_PARSING_FAILED;
				ES_Value sub_array_value = value;
				OP_ASSERT(sub_array_value.type == VALUE_OBJECT);

				if (field.GetQuantifier() == OpProtobufField::Repeated)
				{
					const OpProtobufMessage *sub_message = field.GetMessage();
					OP_ASSERT(sub_message != NULL);
					void *sub_instance_ptr = NULL;
					RETURN_IF_ERROR(OpProtobufInput::AddMessage(sub_instance_ptr, instance, field_idx, field));
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					unsigned sub_array_index = 0;
					RETURN_IF_ERROR(ReadMessage(sub_instance, sub_array_value.value.object, es_runtime, sub_array_index));
				}
				else
				{

					const OpProtobufMessage *sub_message = field.GetMessage();
					void *sub_instance_ptr;
					if (field.GetQuantifier() == OpProtobufField::Required)
						sub_instance_ptr = instance.FieldPtrVoid(field_idx); // Message member;
					else // Optional
						RETURN_IF_ERROR(OpProtobufInput::CreateMessage(sub_instance_ptr, instance, field_idx, field));
					OP_ASSERT(sub_message != NULL);
					OP_ASSERT(sub_instance_ptr != NULL);
					OpProtobufInstanceProxy sub_instance(sub_message, sub_instance_ptr);
					unsigned sub_array_index = 0;
					RETURN_IF_ERROR(ReadMessage(sub_instance, sub_array_value.value.object, es_runtime, sub_array_index));
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
		if (!is_repeated)
			break;
	}
	if (is_repeated && array_count == 0)
	{
		OP_ASSERT(message->GetOffsetToBitfield() >= 0);
		// Empty arrays should be treated as if the array was not sent
		instance.GetBitfield().ClearBit(field_idx);
	}
	return OpStatus::OK;
}

OpESInputStream::OpESInputStream()
{
}

OP_STATUS
OpESInputStream::Construct(ES_Object *obj, ES_Runtime *runtime)
{
	if (obj == NULL)
		return OpStatus::ERR_PARSING_FAILED;
	root_object = obj;
	es_runtime = runtime;
	return OpStatus::OK;
}

OP_STATUS
OpESInputStream::Read(OpProtobufInstanceProxy &instance)
{
	OpESInputStreamPrivate stream;
	unsigned array_index = 0;
	return stream.ReadMessage(instance, root_object, es_runtime, array_index);
}

/* OpESOutputStream */

class OpESOutputStreamPrivate
{
public:
	OpESOutputStreamPrivate(ES_Runtime *runtime);

	OP_STATUS WriteMessage(const OpProtobufInstanceProxy &instance, ES_Object *array_object, unsigned &array_len);
	OP_STATUS WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, ES_Object *arr, unsigned &arr_len, BOOL embed = FALSE);

	OP_STATUS WriteBytes(OpString *str, OpProtobufDataChunkRange range);

	static void AppendArrayProperty(ES_Object *arr, unsigned int &arr_len, const ES_Value &value, ES_Runtime *es_runtime);

	ES_Runtime *runtime;
};

OpESOutputStreamPrivate::OpESOutputStreamPrivate(ES_Runtime *runtime)
	: runtime(runtime)
{
}

OP_STATUS
OpESOutputStreamPrivate::WriteMessage(const OpProtobufInstanceProxy &instance, ES_Object *array_object, unsigned &array_len)
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
		RETURN_IF_ERROR(WriteField(instance, idx, field, array_object, array_len));
	}
	return OpStatus::OK;
}

/*static*/
void
OpESOutputStreamPrivate::AppendArrayProperty(ES_Object *arr, unsigned int &arr_len, const ES_Value &value, ES_Runtime *es_runtime)
{
	es_runtime->PutIndex(arr, arr_len++, value);
}

OP_STATUS
OpESOutputStreamPrivate::WriteField(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, ES_Object *arr, unsigned &arr_len, BOOL embed)
{
	uni_char empty_string(0);
	const OpProtobufMessage *message = instance.GetProtoMessage();
	if (message->GetOffsetToBitfield() >= 0 && field.GetQuantifier() != OpProtobufField::Required)
	{
		OpProtobufBitFieldRef has_bits = instance.GetBitfield();
		if (!has_bits.IsSet(field_idx))
		{
			ES_Value esvalue;
			if (field.GetQuantifier() == OpProtobufField::Repeated) // Repeated fields are always shown as a list
			{
				ES_Object* empty_obj = NULL;
				RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&empty_obj, 0));
				if (empty_obj == NULL)
					return OpStatus::ERR_NO_MEMORY;

				esvalue.type = VALUE_OBJECT;
				esvalue.value.object = empty_obj;
			}
			else
				esvalue.type = VALUE_NULL;
			AppendArrayProperty(arr, arr_len, esvalue, runtime);
			return OpStatus::OK;
		}
	}
	if (field.GetQuantifier() == OpProtobufField::Repeated)
	{
		ES_Object* repeated_arr = NULL;
		ES_Object* sub_obj = NULL;
		RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&sub_obj, 0));
		if (sub_obj == NULL)
			return OpStatus::ERR_NO_MEMORY;
		repeated_arr = sub_obj;

		ES_Value repeated_esvalue;
		repeated_esvalue.type = VALUE_OBJECT;
		repeated_esvalue.value.object = sub_obj;
		AppendArrayProperty(arr, arr_len, repeated_esvalue, runtime);

		unsigned int repeated_arr_len = 0;
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				const OpValueVector<double> *value = instance.FieldPtrOpValueVectorDouble(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					ES_Value esvalue;
					esvalue.type = VALUE_NUMBER;
					esvalue.value.number = value->Get(i);
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
				}
				break;
			}
			case OpProtobufFormat::Float:
			{
				const OpValueVector<float> *value = instance.FieldPtrOpValueVectorFloat(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					ES_Value esvalue;
					esvalue.type = VALUE_NUMBER;
					esvalue.value.number = value->Get(i);
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
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
					ES_Value esvalue;
					esvalue.type = VALUE_NUMBER;
					esvalue.value.number = value->Get(i);
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
				}
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				const OpValueVector<UINT32> *value = instance.FieldPtrOpValueVectorUINT32(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					ES_Value esvalue;
					esvalue.type = VALUE_NUMBER;
					esvalue.value.number = value->Get(i);
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
				}
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				const OpINT32Vector *value = instance.FieldPtrOpINT32Vector(field_idx);
				for (unsigned int i = 0; i < value->GetCount(); ++i)
				{
					ES_Value esvalue;
					esvalue.type = VALUE_NUMBER;
					esvalue.value.number = value->Get(i) ? 1 : 0;
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
				}
				break;
			}
			case OpProtobufFormat::String:
			{
				OpProtobufStringArrayRange range(instance.GetStringArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					ES_Value esvalue;
					esvalue.type = VALUE_STRING_WITH_LENGTH;
					ES_ValueString value_string;
					esvalue.value.string_with_length = &value_string;
					TempBuffer buf;
					buf.SetCachedLengthPolicy(TempBuffer::TRUSTED);
					RETURN_IF_ERROR(OpProtobufUtils::Copy(range.Front(), buf));

					value_string.string = buf.GetStorage();
					value_string.length = buf.Length();
					if (value_string.string == NULL)
						value_string.string = &empty_string;
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
				}
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				OpProtobufDataArrayRange range(instance.GetBytesArray(field_idx));
				for (; !range.IsEmpty(); range.PopFront())
				{
					ES_Value esvalue;
					ES_ValueString esstring;
					OpString str;
					RETURN_IF_ERROR(WriteBytes(&str, range.Front()));
					esstring.length = str.Length();
					esstring.string = str.CStr();
					if (esstring.string == NULL)
						esstring.string = &empty_string;
					esvalue.type = VALUE_STRING_WITH_LENGTH;
					esvalue.value.string_with_length = &esstring;
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
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

					ES_Object* array_object = NULL;
					RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&array_object, 0));
					if (array_object == NULL)
						return OpStatus::ERR_NO_MEMORY;
					unsigned array_len = 0;
					RETURN_IF_ERROR(WriteMessage(sub_instance, array_object, array_len));
					ES_Value esvalue;
					esvalue.type = VALUE_OBJECT;
					esvalue.value.object = array_object;
					AppendArrayProperty(repeated_arr, repeated_arr_len, esvalue, runtime);
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
		ES_Value esvalue;
		switch (field.GetType())
		{
			case OpProtobufFormat::Double:
			{
				esvalue.type = VALUE_NUMBER;
				esvalue.value.number = *instance.FieldPtrDouble(field_idx);
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
			case OpProtobufFormat::Float:
			{
				esvalue.type = VALUE_NUMBER;
				esvalue.value.number = *instance.FieldPtrFloat(field_idx);
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
			case OpProtobufFormat::Int32:
			case OpProtobufFormat::Sint32:
			case OpProtobufFormat::Sfixed32:
			{
				esvalue.type = VALUE_NUMBER;
				esvalue.value.number = *instance.FieldPtrINT32(field_idx);
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
			case OpProtobufFormat::Uint32:
			case OpProtobufFormat::Fixed32:
			{
				esvalue.type = VALUE_NUMBER;
				esvalue.value.number = *instance.FieldPtrUINT32(field_idx);
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
#ifdef OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Int64:
			case OpProtobufFormat::Sint64:
			case OpProtobufFormat::Sfixed64:
			case OpProtobufFormat::Uint64:
			case OpProtobufFormat::Fixed64:
				OP_ASSERT(!"64bit values are not supported");
				return OpStatus::ERR;
#endif // OP_PROTOBUF_64BIT_SUPPORT
			case OpProtobufFormat::Bool:
			{
				esvalue.type = VALUE_NUMBER;
				esvalue.value.number = *instance.FieldPtrBOOL(field_idx) ? 1 : 0;
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
			case OpProtobufFormat::String:
			{
				TempBuffer buf;
				RETURN_IF_ERROR(OpProtobufUtils::Copy(instance.GetStringRange(field_idx), buf));

				ES_Value esvalue;
				ES_ValueString str;
				esvalue.type = VALUE_STRING_WITH_LENGTH;
				esvalue.value.string_with_length = &str;
				str.length = buf.Length();
				str.string = buf.GetStorage();
				if (str.string == NULL)
					str.string = &empty_string;
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
				break;
			}
			case OpProtobufFormat::Bytes:
			{
				ES_Value esvalue;
				ES_ValueString esstring;
				OpString str;
				RETURN_IF_ERROR(WriteBytes(&str, instance.GetBytesRange(field_idx)));
				esstring.length = str.Length();
				esstring.string = str.CStr();
				esvalue.type = VALUE_STRING_WITH_LENGTH;
				esvalue.value.string_with_length = &esstring;
				if (esstring.string == NULL)
					esstring.string = &empty_string;
				AppendArrayProperty(arr, arr_len, esvalue, runtime);
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

				if (embed)
				{
					RETURN_IF_ERROR(WriteMessage(sub_instance, arr, arr_len));
				}
				else
				{
					ES_Object* array_object = NULL;
					RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&array_object, 0));
					if (array_object == NULL)
						return OpStatus::ERR_NO_MEMORY;
					unsigned array_len = 0;
					RETURN_IF_ERROR(WriteMessage(sub_instance, array_object, array_len));
					esvalue.type = VALUE_OBJECT;
					esvalue.value.object = array_object;
					AppendArrayProperty(arr, arr_len, esvalue, runtime);
				}
				break;
			}
			default:
				OP_ASSERT(!"Should not happen");
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
OpESOutputStreamPrivate::WriteBytes(OpString *str, OpProtobufDataChunkRange range)
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
	str->Set(target, targetlen);
	OP_DELETEA(target);
	return OpStatus::OK;
}

OpESOutputStream::OpESOutputStream(ES_Object *&root, ES_Runtime *runtime)
	: root_object(root), runtime(runtime)
{
	OP_ASSERT(root == NULL && runtime != NULL);
}

OP_STATUS
OpESOutputStream::Write(const OpProtobufInstanceProxy &instance)
{
	RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&root_object, 0));
	OpESOutputStreamPrivate stream(runtime);
	unsigned array_len = 0;
	OP_STATUS status = stream.WriteMessage(instance, root_object, array_len);
	if (OpStatus::IsError(status))
	{
		root_object = NULL;
		return status;
	}
	return OpStatus::OK;
}

/* OpESToJSON */

class OpESToJSONPrivate
{
public:
	OpESToJSONPrivate(ES_Runtime *runtime);

	OP_STATUS WriteArray(ByteBuffer &out, ES_Object *es_object);
	OP_STATUS WriteValue(ByteBuffer &out, ES_Object *es_object);
	OP_STATUS WriteValue(ByteBuffer &out, ES_Value &obj);

	ES_Runtime *runtime;
};

OpESToJSONPrivate::OpESToJSONPrivate(ES_Runtime *runtime)
	: runtime(runtime)
{
}

OP_STATUS
OpESToJSONPrivate::WriteArray(ByteBuffer &out, ES_Object *es_object)
{
	OP_ASSERT(es_object != NULL);
	if (es_object == NULL)
		return OpStatus::ERR;

	unsigned int array_length;
	RETURN_IF_ERROR(GetArrayLength(es_object, runtime, &array_length));
	if (array_length == 0)
		return out.AppendString("[]");

	RETURN_IF_ERROR(out.AppendString("["));
	unsigned int queued_nulls = 0;
	unsigned int idx = 0;
	BOOL first = TRUE;
	for (; idx < array_length; ++idx)
	{
		ES_Value array_entry;
		RETURN_IF_ERROR_OR_FALSE(runtime->GetIndex(es_object, idx, &array_entry));
		if (array_entry.type == VALUE_NULL)
			queued_nulls++;
		else
		{
			if (first)
				first = FALSE;
			else
				RETURN_IF_ERROR(out.AppendString(","));
			for (; queued_nulls > 0; queued_nulls--)
				RETURN_IF_ERROR(out.AppendString("null,"));
			RETURN_IF_ERROR(WriteValue(out, array_entry));
		}
	}
	RETURN_IF_ERROR(out.AppendString("]"));
	return OpStatus::OK;
}

OP_STATUS
OpESToJSONPrivate::WriteValue(ByteBuffer &out, ES_Object *es_object)
{
	ES_Value tmp;
	tmp.type = VALUE_OBJECT;
	tmp.value.object = es_object;
	return WriteValue(out, tmp);
}

OP_STATUS
OpESToJSONPrivate::WriteValue(ByteBuffer &out, ES_Value &field_item)
{
	if (field_item.type == VALUE_NUMBER)
	{
		RETURN_IF_ERROR(OpScopeJSON::DumpDouble(out, field_item.value.number));
	}
	else if (field_item.type == VALUE_BOOLEAN)
	{
		RETURN_IF_ERROR(out.AppendString(field_item.value.boolean ? "true" : "false"));
	}
	else if (field_item.type == VALUE_STRING)
	{
		RETURN_IF_ERROR(OpScopeJSON::DumpUniString(out, field_item.value.string, uni_strlen(field_item.value.string)));
	}
	else if (field_item.type == VALUE_STRING_WITH_LENGTH)
	{
		RETURN_IF_ERROR(OpScopeJSON::DumpUniString(out, field_item.value.string_with_length->string, field_item.value.string_with_length->length));
	}
	else if (field_item.type == VALUE_OBJECT)
	{
		RETURN_IF_ERROR(WriteArray(out, field_item.value.object));
	}
	else if(field_item.type == VALUE_NULL)
	{
		RETURN_IF_ERROR(out.AppendString("null"));
	}
	return OpStatus::OK;
}

OpESToJSON::OpESToJSON()
{
}

OP_STATUS
OpESToJSON::Construct(ES_Object *obj, ES_Runtime *runtime)
{
	OP_ASSERT(obj != NULL);
	if (obj == NULL)
		return OpStatus::ERR_PARSING_FAILED;
	root_object = obj;
	es_runtime = runtime;
	return OpStatus::OK;
}

OP_STATUS
OpESToJSON::Write(ByteBuffer &out)
{
	OpESToJSONPrivate stream(es_runtime);
	return stream.WriteValue(out, root_object);
}

/* OpJSONToES */

class OpJSONToESPrivate
{
public:
	OpJSONToESPrivate(ES_Runtime *runtime, OpScopeJSON::Lexer &lexer, OpScopeJSON::Parser &parser);

	OP_STATUS ReadArray(ES_Object *array_object);

	//static void AppendArrayProperty(ES_Array *arr, unsigned int &arr_len, const JS_Value &value);

	ES_Runtime          *runtime;
	OpScopeJSON::Parser &parser;
	OpScopeJSON::Lexer  &lexer;
};

OpJSONToESPrivate::OpJSONToESPrivate(ES_Runtime *runtime, OpScopeJSON::Lexer &lexer, OpScopeJSON::Parser &parser)
	: runtime(runtime), parser(parser), lexer(lexer)
{
}

OP_STATUS
OpJSONToESPrivate::ReadArray(ES_Object *array_object)
{
	// TODO: This code is untested by selftests
	uni_char empty_string(0);
	unsigned int array_len = 0;
	OpScopeJSON::Lexer::MatchResult res;
	RETURN_IF_ERROR(lexer.Match(OpScopeJSON::Lexer::TOK_BRACKET_START));
	while (TRUE)
	{
		RETURN_IF_ERROR(lexer.LT(1, res));
		if (res.token == OpScopeJSON::Lexer::TOK_BRACKET_END)
		{
			RETURN_IF_ERROR(lexer.Match(res.token));
			break;
		}

		if (array_len > 0)
		{
			RETURN_IF_ERROR(lexer.Match(OpScopeJSON::Lexer::TOK_COMMA));
			RETURN_IF_ERROR(lexer.LT(1, res));
		}

		if (res.token == OpScopeJSON::Lexer::TOK_INTEGER)
		{
			INT32 value;
			RETURN_IF_ERROR(parser.ParseSignedInteger(value));
			ES_Value esvalue;
			esvalue.type = VALUE_NUMBER;
			esvalue.value.number = value;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_REAL)
		{
			double value;
			RETURN_IF_ERROR(parser.ParseDouble(value));
			ES_Value esvalue;
			esvalue.type = VALUE_NUMBER;
			esvalue.value.number = value;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_TRUE || res.token == OpScopeJSON::Lexer::TOK_FALSE)
		{
			RETURN_IF_ERROR(lexer.Match(res.token));
			ES_Value esvalue;
			esvalue.type = VALUE_BOOLEAN;
			esvalue.value.boolean = res.token == OpScopeJSON::Lexer::TOK_TRUE;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_STRING)
		{
			OpString string;
			RETURN_IF_ERROR(parser.ParseString(string));
			ES_Value esvalue;
			ES_ValueString str;
			esvalue.type = VALUE_STRING_WITH_LENGTH;
			esvalue.value.string_with_length = &str;
			str.length = string.Length();
			str.string = string.CStr();
			if (str.string == NULL)
				str.string = &empty_string;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_BRACKET_START)
		{
			ES_Object* sub_array_object = NULL;
			RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&sub_array_object, 0));
			if (sub_array_object == NULL)
				return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(ReadArray(sub_array_object));
			ES_Value esvalue;
			esvalue.type = VALUE_OBJECT;
			esvalue.value.object = sub_array_object;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_CURLY_BRACKET_START)
		{
			OP_ASSERT(!"Not yet supported");
			return OpStatus::ERR_PARSING_FAILED;
		}
		else if (res.token == OpScopeJSON::Lexer::TOK_NULL)
		{
			RETURN_IF_ERROR(lexer.Match(res.token));
			ES_Value esvalue;
			esvalue.type = VALUE_NULL;
			OpESOutputStreamPrivate::AppendArrayProperty(array_object, array_len, esvalue, runtime);
		}
		else
		{
			OP_ASSERT(!"Should not happen"); // Unknown token, might be missing 'else if' entry
			return OpStatus::ERR_PARSING_FAILED;
		}
	}
	return OpStatus::OK;
}

OpJSONToES::OpJSONToES(ES_Object *&root, ES_Runtime *runtime)
	: root_object(root), runtime(runtime)
{
	OP_ASSERT(root == NULL && runtime != NULL);
}

OpJSONToES::~OpJSONToES()
{
}

OP_STATUS
OpJSONToES::Construct(const ByteBuffer &in)
{
	buffer.SetCachedLengthPolicy(TempBuffer::TRUSTED);
	buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);
	// FIXME: HERE: Copy UTF-16 data from bytebuffer to tempbuffer
	RETURN_IF_ERROR(OpProtobufUtils::ConvertUTF8toUTF16(buffer, in));
	return OpStatus::OK;
}

OP_STATUS
OpJSONToES::Read()
{
	OpScopeJSON::Input input;
	RETURN_IF_ERROR(input.Construct(buffer.GetStorage(), buffer.Length()));
	OpScopeJSON::Lexer lexer;
	RETURN_IF_ERROR(lexer.Construct(&input));
	OpScopeJSON::Parser parser(lexer);
	OpScopeJSON::Lexer::MatchResult res;
	RETURN_IF_ERROR(lexer.LT(1, res));
	// FIXME: For now we only supporting arrays as the top-most type, should we change this?
	if (res.token != OpScopeJSON::Lexer::TOK_BRACKET_START)
		return OpStatus::ERR_PARSING_FAILED;

	RETURN_IF_ERROR(runtime->CreateNativeArrayObject(&root_object, 0));
	if (root_object == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OpJSONToESPrivate stream(runtime, lexer, parser);
	OP_STATUS status = stream.ReadArray(root_object);
	if (OpStatus::IsError(status))
	{
		root_object = NULL;
		return status;
	}
	return OpStatus::OK;
}

#endif // PROTOBUF_ECMASCRIPT_SUPPORT

#endif // PROTOBUF_SUPPORT
