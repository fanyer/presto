/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_DIRECTORY_ENTRY_SUPPORT

#include "modules/device_api/OpDirectoryEntry.h"

OpDirectoryEntryContentLine::OpDirectoryEntryContentLine()
	: m_values(1)
	, m_params(FALSE)
{
}

OP_STATUS OpDirectoryEntryContentLine::EnsureFieldExist(UINT32 field_index)
{
	while (m_values.GetCount() <= field_index)
		RETURN_IF_ERROR(m_values.Add(NULL));

	OpAutoVector<ValueStruct>* field_content = m_values.Get(field_index);
	if (!field_content)
	{
		RETURN_OOM_IF_NULL(field_content = OP_NEW(OpAutoVector<ValueStruct>, (1)));
		OP_STATUS error = m_values.Replace(field_index, field_content);
		OP_ASSERT(OpStatus::IsSuccess(error)); // replace should always succeed here -- if it fails we'd have a memleak
		OpStatus::Ignore(error); // avoid warning in release
	}
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::AddValue(UINT32 field_index, ValueStruct* value)
{
	OpAutoPtr<ValueStruct> value_deleter(value);
	RETURN_IF_ERROR(EnsureFieldExist(field_index));
	OpAutoVector<ValueStruct>* field_content = m_values.Get(field_index);
	OP_ASSERT(field_content);
	RETURN_IF_ERROR(field_content->Add(value));
	value_deleter.release();
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::SetValue(UINT32 field_index, UINT32 value_index, ValueStruct* value)
{
	OpAutoPtr<ValueStruct> value_deleter(value);
	RETURN_IF_ERROR(EnsureFieldExist(field_index));
	OpAutoVector<ValueStruct>* field_content = m_values.Get(field_index);
	OP_ASSERT(field_content);
	while (field_content->GetCount() <= value_index)
		RETURN_IF_ERROR(field_content->Add(NULL));
	ValueStruct* old_value = field_content->Get(value_index);
	RETURN_IF_ERROR(field_content->Replace(value_index, value));
	OP_DELETE(old_value);
	value_deleter.release();
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::DeleteValue(UINT32 field_index)
{
	OpAutoVector<ValueStruct>* field_content =  m_values.Get(field_index);
	if (!field_content)
		return OpStatus::ERR;
	field_content->DeleteAll();
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::DeleteValue(UINT32 field_index, UINT32 value_index)
{
	OpAutoVector<ValueStruct>* field_content =  m_values.Get(field_index);
	if (!field_content || field_content->GetCount() <= value_index)
		return OpStatus::ERR;
	field_content->Delete(value_index);
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::GetFieldCount(UINT32& count)
{
	count = m_values.GetCount();
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::GetValueCount(UINT32 field_index, UINT32& count)
{
	OpAutoVector<ValueStruct>* field_content =  m_values.Get(field_index);
	if (!field_content)
		return OpStatus::ERR;
	count = field_content->GetCount();
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::GetValue(UINT32 field_index, UINT32 value_index, ValueStruct*& value)
{
	OpAutoVector<ValueStruct>* field_content =  m_values.Get(field_index);
	if (!field_content)
		return OpStatus::ERR;
	value = field_content->Get(value_index);
	return value ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS OpDirectoryEntryContentLine::Print(OpFileDescriptor* opened_file)
{
	RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, m_name.CStr(), m_name.Length()));

	OpAutoPtr<OpStringHashMultiMap<OpString>::MultiMapIterator> iterator(m_params.GetIterator());
	RETURN_OOM_IF_NULL(iterator.get());

	OpString case_swap_buffer;
	for (OP_STATUS status = iterator->First(); OpStatus::IsSuccess(status); status = iterator->Next())
	{
		OpVector<OpString>* values = iterator->GetValues();
		if (!values || values->GetCount() == 0)
			continue;
		RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L(";"), 1));
		RETURN_IF_ERROR(case_swap_buffer.Set(iterator->GetKey()));
		case_swap_buffer.MakeUpper();
		RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, case_swap_buffer.CStr(), case_swap_buffer.Length()));
		RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L("="), 1));
		for (UINT32 i = 0; i < values->GetCount(); ++i)
		{
			if (i > 0)
				RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L(","), 1));

			OpString* value = values->Get(i);
			if (!value)
				continue;
			RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, value->CStr(), value->Length()));
		}
	}

	RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L(":"), 1));
	for (UINT32 i = 0; i < m_values.GetCount(); ++i)
	{
		if (i > 0)
			RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L(";"), 1));
		OpAutoVector<ValueStruct>* field = m_values.Get(i);
		if (field)
		{
			for (UINT32 j = 0; j < field->GetCount(); ++j)
			{
				if (j > 0)
					RETURN_IF_ERROR(m_encoder.PrintNonEscaped(opened_file, UNI_L(","), 1));
				ValueStruct* value = field->Get(j);
				RETURN_IF_ERROR(PrintValueStruct(opened_file, value));
			}
		}
	}
	RETURN_IF_ERROR(m_encoder.FlushLine(opened_file, TRUE));
	return OpStatus::OK;
}

OP_STATUS OpDirectoryEntryContentLine::PrintValueStruct(OpFileDescriptor* opened_file, ValueStruct* value_struct)
{
	OP_ASSERT(value_struct);
	switch (value_struct->type)
	{
	case ValueStruct::UNDEF_VAL:
		return OpStatus::OK;
	case ValueStruct::STRING_VAL:
		return m_encoder.PrintEscaped(opened_file, value_struct->value.string_val, static_cast<int>(uni_strlen(value_struct->value.string_val)));
	case ValueStruct::BINARY_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	case ValueStruct::INTEGER_VAL:
		{
			OpString number_str;
			RETURN_IF_ERROR(number_str.AppendFormat(UNI_L("%d"), value_struct->value.integer_val));
			return m_encoder.PrintNonEscaped(opened_file, number_str.CStr(), number_str.Length());
		}
	case ValueStruct::FLOAT_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	case ValueStruct::BOOL_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	case ValueStruct::DATE_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	case ValueStruct::TIME_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	case ValueStruct::DATETIME_VAL:
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	OP_ASSERT(FALSE);
	return OpStatus::ERR;
}

OP_STATUS
OpDirectoryEntryContentLine::TextMessageEncoder::PrintNonEscaped(OpFileDescriptor* opened_file, const uni_char* data, int data_len)
{
	OP_ASSERT(data_len >= 0);
	const uni_char* data_end = data + data_len;
	for (; data < data_end; ++data)
	{
		RETURN_IF_ERROR(PutChar(opened_file, *data));
	}
	return OpStatus::OK;
}

OP_STATUS
OpDirectoryEntryContentLine::TextMessageEncoder::PrintEscaped(OpFileDescriptor* opened_file, const uni_char* data, int data_len)
{
	OP_ASSERT(data_len >= 0);
	const uni_char* data_end = data + data_len;
	for (; data < data_end; ++data)
	{
		if (*data == UNI_L("\n")[0] )
		{
			RETURN_IF_ERROR(PutChar(opened_file, UNI_L("\\")[0]));
			RETURN_IF_ERROR(PutChar(opened_file, UNI_L("n")[0]));
		}
		else if (*data == UNI_L(",")[0] ||
			*data == UNI_L(";")[0] ||
			*data == UNI_L(":")[0] ||
			*data == UNI_L("\\")[0])
		{
			RETURN_IF_ERROR(PutChar(opened_file, UNI_L("\\")[0]));
			RETURN_IF_ERROR(PutChar(opened_file, *data));
			continue;
		}
		else if (uni_iscntrl(*data))
			continue;
		else
			RETURN_IF_ERROR(PutChar(opened_file, *data));
	}
	return OpStatus::OK;
}

OP_STATUS
OpDirectoryEntryContentLine::TextMessageEncoder::PutChar(OpFileDescriptor* opened_file, uni_char data)
{
	if (m_chars_in_buffer == MAX_LINE_LENGTH)
		RETURN_IF_ERROR(FlushLine(opened_file, FALSE));
	OP_ASSERT(m_chars_in_buffer < MAX_LINE_LENGTH);
	m_line_buffer[m_chars_in_buffer] = data;
	++m_chars_in_buffer;
	return OpStatus::OK;
}

OP_STATUS
OpDirectoryEntryContentLine::TextMessageEncoder::FlushLine(OpFileDescriptor* opened_file, BOOL finish_print)
{
	OpString8 conversion_buffer;
	RETURN_IF_ERROR(conversion_buffer.SetUTF8FromUTF16(m_line_buffer, m_chars_in_buffer));
	RETURN_IF_ERROR(conversion_buffer.Append("\r\n"));
	RETURN_IF_ERROR(opened_file->Write(conversion_buffer.CStr(), conversion_buffer.Length()));

	if (finish_print)
		m_chars_in_buffer = 0;
	else
	{
		m_line_buffer[0] = UNI_L(" ")[0];
		m_chars_in_buffer = 1;
	}
	return OpStatus::OK;
}

OpDirectoryEntryContentLine*
OpDirectoryEntry::FindNamedContentLine(const uni_char* name)
{
	for (UINT32 i = 0; i < m_values.GetCount(); ++i)
	{
		OpDirectoryEntryContentLine* value = m_values.Get(i);
		OP_ASSERT(value);
		OP_ASSERT(value->GetName());
		if (uni_stri_eq(value->GetName(), name))
			return value;
	}
	return NULL;
}

OpDirectoryEntryContentLine*
OpDirectoryEntry::ConstructNewNamedContentLine(const uni_char* name, UINT32 fields_num)
{
	OpAutoPtr<OpDirectoryEntryContentLine> new_value_ap(OP_NEW(OpDirectoryEntryContentLine, ()));
	RETURN_VALUE_IF_NULL(new_value_ap.get(), NULL);
	RETURN_VALUE_IF_ERROR(new_value_ap->SetName(name), NULL);
	if (fields_num >0)
		RETURN_VALUE_IF_ERROR(new_value_ap->EnsureFieldExist(fields_num-1), NULL);
	RETURN_VALUE_IF_ERROR(AddContentLine(new_value_ap.get()), NULL);
	return new_value_ap.release();
}

OpDirectoryEntryContentLine*
OpDirectoryEntry::FindOrConstructNamedContentLine(const uni_char* name, UINT32 fields_num)
{
	OP_ASSERT(name);
	// find
	OpDirectoryEntryContentLine* retval = FindNamedContentLine(name);
	if (retval)
		return retval;
	// didn't find so construct
	return ConstructNewNamedContentLine(name, fields_num);
}

/* static */ OpDirectoryEntryContentLine::ValueStruct*
OpDirectoryEntryContentLine::NewStringValue(const uni_char* value)
{
	ValueStruct* retval = OP_NEW(ValueStruct, ());
	RETURN_VALUE_IF_NULL(retval, NULL);
	retval->value.string_val = UniSetNewStr(value);
	if (!retval->value.string_val && value)
	{
		OP_DELETE(retval);
		return NULL;
	}
	if (value)
		retval->type = ValueStruct::STRING_VAL;
	return retval;
}
/* static */ OpDirectoryEntryContentLine::ValueStruct*
OpDirectoryEntryContentLine::NewBinaryValue(const char* value)
{
	OP_ASSERT(FALSE);
	return NULL; // not supported yet
}
/* static */ OpDirectoryEntryContentLine::ValueStruct*
OpDirectoryEntryContentLine::NewIntegerValue(int value)
{
	ValueStruct* retval = OP_NEW(ValueStruct, ());
	RETURN_VALUE_IF_NULL(retval, NULL);
	retval->value.integer_val = value;
	retval->type = ValueStruct::INTEGER_VAL;
	return retval;
}

/* static */ OpDirectoryEntryContentLine::ValueStruct*
OpDirectoryEntryContentLine::NewFloatValue(double value)
{
	ValueStruct* retval = OP_NEW(ValueStruct, ());
	RETURN_VALUE_IF_NULL(retval, NULL);
	retval->value.float_val = value;
	retval->type = ValueStruct::FLOAT_VAL;
	return retval;
}

/* static */ OpDirectoryEntryContentLine::ValueStruct*
OpDirectoryEntryContentLine::NewBooleanValue(bool value)
{
	ValueStruct* retval = OP_NEW(ValueStruct, ());
	RETURN_VALUE_IF_NULL(retval, NULL);
	retval->value.bool_val = value;
	retval->type = ValueStruct::BOOL_VAL;
	return retval;
}

OP_STATUS OpDirectoryEntry::Print(OpFileDescriptor* opened_file)
{
	OP_ASSERT(opened_file->IsWritable());
	for (UINT32 i = 0; i < GetContentLineCount(); i++)
	{
		RETURN_IF_ERROR(GetContentLine(i)->Print(opened_file));
	}
	return OpStatus::OK;
}

#endif // DAPI_DIRECTORY_ENTRY_SUPPORT

