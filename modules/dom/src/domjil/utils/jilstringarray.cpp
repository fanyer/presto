/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/utils/jilstringarray.h"
#include "modules/pi/device_api/OpMessaging.h"

DOM_JILString_Array::DOM_JILString_Array()
	: m_length(0)
{}

/* static */ OP_STATUS
DOM_JILString_Array::Make(DOM_JILString_Array*& new_obj, DOM_Runtime* origining_runtime, const OpAutoVector<OpString>* from_array)
{
	new_obj = OP_NEW(DOM_JILString_Array, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetArrayPrototype(), "StringArray"));
	if (from_array)
	{
		unsigned int i_len;
		for (i_len = 0; i_len < from_array->GetCount(); i_len++)
		{
			OpAutoPtr<DOM_JIL_Array_Element> ap_single_to_add(OP_NEW(DOM_JIL_Array_Element, ()));
			RETURN_OOM_IF_NULL(ap_single_to_add.get());
			uni_char* new_str = UniSetNewStr(from_array->Get(i_len)->CStr());
			RETURN_OOM_IF_NULL(new_str);

			DOMSetString(&ap_single_to_add->m_val, new_str);
			ap_single_to_add->m_index = i_len;
			RETURN_IF_ERROR(new_obj->m_array.Add(ap_single_to_add.get()));
			ap_single_to_add.release();
		}
		new_obj->m_length = i_len;
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_JILString_Array::Make(DOM_JILString_Array*& new_obj, DOM_Runtime* origining_runtime, ES_Object* from_object, int len)
{
	RETURN_IF_ERROR(DOM_JILString_Array::Make(new_obj, origining_runtime));
	for (int i = 0; i < len; i++)
	{
		ES_Value i_val;
		if (origining_runtime->GetIndex(from_object, i, &i_val) != OpBoolean::IS_TRUE || i_val.type != VALUE_STRING)
			return OpStatus::ERR;
		if (new_obj->PutIndex(i, &i_val, origining_runtime) != PUT_SUCCESS)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* virtual */
DOM_JILString_Array::~DOM_JILString_Array()
{
	for (UINT32 i_len = 0; i_len < m_array.GetCount(); i_len++)
		DOMFreeValue(m_array.Get(i_len)->m_val);
}

void
DOM_JILString_Array::Empty()
{
	m_length = 0;
	for (UINT32 i_len = 0; i_len < m_array.GetCount(); i_len++)
		DOMFreeValue(m_array.Get(i_len)->m_val);
	m_array.DeleteAll();
}

OP_STATUS
DOM_JILString_Array::Remove(const uni_char* to_remove)
{
	int shift_by = 0;
	BOOL was_removed = FALSE;
	for (UINT32 i_len = 0; i_len < m_array.GetCount(); i_len++)
	{
		ES_Value& in_array_val = m_array.Get(i_len)->m_val;
		if (in_array_val.type == VALUE_STRING && uni_str_eq(in_array_val.value.string, to_remove))
		{
			DOM_JIL_Array_Element* removed = m_array.Remove(i_len);
			DOMFreeValue(removed->m_val);
			OP_DELETE(removed);
			m_length--;
			shift_by++;
			i_len--;
			was_removed = TRUE;
		}
		// TODO: change method input parameter, to handle null and undef
		else if (shift_by)
			m_array.Get(i_len)->m_index -= shift_by;
	}
	return was_removed ? OpStatus::OK : OpStatus::ERR_NO_SUCH_RESOURCE;
}

/* virtual */ ES_GetState
DOM_JILString_Array::GetName(OpAtom property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_code == OP_ATOM_length)
	{
		DOMSetNumber(value, m_length);
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILString_Array::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_code == OP_ATOM_length)
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		if (op_isintegral(value->value.number) && value->value.number >= 0 && value->value.number <= INT_MAX - 1)
			m_length = static_cast<int>(value->value.number);
		return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_JILString_Array::GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	int vector_index = FindIndex(property_index);
	if (vector_index == -1)
		return GET_FAILED;
	ES_Value& elem = m_array.Get(vector_index)->m_val;
	if (elem.type == VALUE_STRING)
		DOMSetString(value, elem.value.string);
	else if (elem.type == VALUE_NULL)
		DOMSetNull(value);
	else if (elem.type == VALUE_UNDEFINED)
		DOMSetUndefined(value);
	else
		OP_ASSERT(!"No other type should be in this typed array.");
	return GET_SUCCESS;
}

int DOM_JILString_Array::FindIndex(int to_find)
{
	for (UINT32 i = 0; i < m_array.GetCount();  i++)
		if (m_array.Get(i)->m_index == to_find)
			return i;
	return -1;
}

/* virtual */ ES_PutState
DOM_JILString_Array::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (value->type != VALUE_STRING && value->type != VALUE_UNDEFINED && value->type != VALUE_NULL)
		return PUT_SUCCESS;
	if (property_index < 0 || property_index > INT_MAX - 1)
		return PUT_SUCCESS;
	OpAutoPtr<DOM_JIL_Array_Element> to_insert(OP_NEW(DOM_JIL_Array_Element, ()));
	if (!to_insert.get())
		return PUT_NO_MEMORY;

	if (value->type == VALUE_STRING)
		PUT_FAILED_IF_ERROR(DOMCopyValue(to_insert->m_val, *value));
	else if (value->type == VALUE_NULL)
		DOMSetNull(&to_insert->m_val);
	else
		DOMSetUndefined(&to_insert->m_val);//TODO: write selftest

	to_insert->m_index = property_index;
	DOM_JIL_Array_Element* to_remove = NULL;
	int insertion_place = FindIndex(property_index);
	if (insertion_place == -1)
	{
		insertion_place = 0;
		int end = m_array.GetCount();
		while (end > insertion_place)
		{
			int step = (end - insertion_place) / 2;
			if (m_array.Get(insertion_place + step)->m_index < property_index)
				insertion_place += step + 1;
			else
				end = insertion_place + step;
		}
		if (m_array.GetCount())
			insertion_place++;
	}
	else
		to_remove = m_array.Remove(insertion_place);
	to_insert->m_index = property_index;


	PUT_FAILED_IF_ERROR(m_array.Insert(insertion_place, to_insert.get()));
	to_insert.release();
	if (to_remove)
		DOMFreeValue(to_remove->m_val);
	OP_DELETE(to_remove);

	if (property_index + 1 > m_length)
		m_length = property_index + 1;
	return PUT_SUCCESS;
}

BOOL
DOM_JILString_Array::GetIndex(UINT32 index, ES_Value*& result)
{
	if (index >= m_array.GetCount())
		return FALSE;
	result = &m_array.Get(index)->m_val;
	return TRUE;
}

OP_STATUS
DOM_JILString_Array::GetCopy(DOM_JILString_Array*& to_array, DOM_Runtime* origining_runtime)
{
	RETURN_IF_ERROR(DOM_JILString_Array::Make(to_array, origining_runtime));
	ES_Value* i_val;
	for (UINT32 i = 0; GetIndex(i, i_val); i++)
		if (to_array->PutIndex(i, i_val, origining_runtime) != PUT_SUCCESS)
			return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS
DOM_JILString_Array::GetCopy(OpAutoVector<OpString>* to, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(to);
	ES_Value* string;
	UINT32 i = 0;
	while (GetIndex(i++, string))
		if (string->type == VALUE_STRING)
		{
			OpString* to_add = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(to_add);
			OpAutoPtr<OpString> ap_to_add(to_add);
			RETURN_IF_ERROR(to_add->Set(string->value.string));
			RETURN_IF_ERROR(to->Add(to_add));
			ap_to_add.release();
		}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILString_Array::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	for (unsigned i = 0; i < m_array.GetCount(); i++)
		enumerator->AddPropertiesL(m_array.Get(i)->m_index, 1);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_JILString_Array::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	unsigned max_index = 0;
	for (unsigned i = 0; i < m_array.GetCount(); i++)
	{
		unsigned index = m_array.Get(i)->m_index;
		max_index = MAX(index, max_index);
	}
	count = m_array.GetCount() ? max_index + 1 : 0;
	return GET_SUCCESS;
}

/* virtual */
void DOM_JILString_Array::GCTrace()
{
	DOM_Object::GCTrace();
	for (int i = static_cast<int>(m_array.GetCount()) - 1; i > 0; i--)
		GCMark(m_array.Get(i)->m_val);
}

#endif // DOM_JIL_API_SUPPORT

