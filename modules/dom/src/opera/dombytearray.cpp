/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1996-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined WEBSERVER_SUPPORT || defined DOM_GADGET_FILE_API_SUPPORT

#include "modules/dom/src/opera/dombytearray.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domglobaldata.h"

/* static */ OP_STATUS
DOM_ByteArray::Make(DOM_ByteArray *&byte_array, DOM_Runtime *runtime, unsigned int data_size)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(byte_array = OP_NEW(DOM_ByteArray, ()), runtime,  runtime->GetPrototype(DOM_Runtime::BYTEARRAY_PROTOTYPE), "ByteArray"));
	RETURN_IF_ERROR(byte_array->m_binary_data.AppendZeroes(data_size));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_ByteArray::MakeFromFile(DOM_ByteArray *&byte_array, DOM_Runtime *runtime, OpFile *file, unsigned int data_size)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(byte_array = OP_NEW(DOM_ByteArray, ()), runtime,  runtime->GetPrototype(DOM_Runtime::BYTEARRAY_PROTOTYPE), "ByteArray"));

	OpFileLength size = data_size;
	OpFileLength length_read = 0;
	RETURN_IF_ERROR(byte_array->m_binary_data.AppendZeroes(data_size));

	RETURN_IF_ERROR(file->Read(byte_array->m_binary_data.GetStorage(), size, &length_read));
	unsigned long truncated_length_read = static_cast<unsigned long>(length_read);
	OP_ASSERT(length_read == OpFileLength(truncated_length_read));
	byte_array->m_binary_data.Truncate(truncated_length_read);
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ByteArray::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, m_binary_data.Length());
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_ByteArray::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		{
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;

			if (value->value.number < 0 || value->value.number > UINT_MAX)
				return PUT_SUCCESS; // Just ignore the attempt

			unsigned long new_len = static_cast<unsigned long>(value->value.number);
			if (new_len < m_binary_data.Length())
				m_binary_data.Truncate(new_len);
			else if (new_len > m_binary_data.Length())
				PUT_FAILED_IF_ERROR(m_binary_data.AppendZeroes(new_len - m_binary_data.Length()));

			OP_ASSERT(m_binary_data.Length() == new_len);
			return PUT_SUCCESS;
		}
	}

	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_ByteArray::GetIndex(int property_index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && static_cast<unsigned int>(property_index) < m_binary_data.Length())
	{
		DOMSetNumber(return_value, static_cast<unsigned>(m_binary_data.GetStorage()[property_index]));
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_ByteArray::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (value->type != VALUE_NUMBER)
		return PUT_NEEDS_NUMBER;

	if (property_index < 0)
		return PUT_FAILED;

	unsigned long pos = static_cast<unsigned int>(property_index);
	if (m_binary_data.Length() <= pos)
		PUT_FAILED_IF_ERROR(m_binary_data.AppendZeroes(pos - m_binary_data.Length() + 1));

	m_binary_data.GetStorage()[property_index] = static_cast<char>(static_cast<unsigned char>(op_floor(value->value.number)));
	return PUT_SUCCESS;
}

unsigned char *DOM_ByteArray::GetDataPointer()
{
	return reinterpret_cast<unsigned char*>(m_binary_data.GetStorage());
}

int DOM_ByteArray::GetDataSize()
{
	return m_binary_data.Length();
}

/* virtual */ int
DOM_ByteArray_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number < 0)
		return ES_FAILED;

	DOM_ByteArray *byte_array;
	CALL_FAILED_IF_ERROR(DOM_ByteArray::Make(byte_array, GetEnvironment()->GetDOMRuntime(), static_cast<unsigned int>(argv[0].value.number)));

	DOMSetObject(return_value, byte_array);
	return ES_VALUE;
}

#endif // WEBSERVER_SUPPORT || DOM_GADGET_FILE_API_SUPPORT
