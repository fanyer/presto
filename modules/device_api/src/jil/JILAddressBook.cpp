/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef DOM_JIL_API_SUPPORT

#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT

#include "modules/device_api/WildcardCmp.h"
#include "modules/pi/device_api/OpAddressBook.h"
#include "modules/device_api/jil/JILAddressBook.h"

#include "modules/device_api/WildcardCmp.h"
#include "modules/ecmascript/ecmascript.h"

JIL_AddressBookMappings::JIL_AddressBookMappings()
	: m_address_book(NULL)
{
}

/* static */ OP_STATUS
JIL_AddressBookMappings::Make(JIL_AddressBookMappings*& new_obj, OpAddressBook* address_book)
{
	OP_ASSERT(address_book);
	new_obj = OP_NEW(JIL_AddressBookMappings, ());
	RETURN_OOM_IF_NULL(new_obj);
	OpAutoPtr<JIL_AddressBookMappings> new_obj_deleter(new_obj);
	new_obj->m_address_book = address_book;
	const OpAddressBook::OpAddressBookFieldInfo* field_infos;
	unsigned int field_infos_len;
	address_book->GetFieldInfos(&field_infos, &field_infos_len);

	unsigned int i;
	// those 3 are special because more than 1 meaning can be mapped those JIL properties
	// work_phone_meaning :=  WORK > WORK_MOBILE > WORK_LANDLINE
	OpAddressBook::OpAddressBookFieldInfo::Meaning work_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE;
	// mobile_phone_meaning :=  MOBILE > PRIVATE_MOBILE
	OpAddressBook::OpAddressBookFieldInfo::Meaning mobile_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_MOBILE_PHONE;
	// home_phone_meaning :=  PRIVATE_LANDLINE > LANDLINE
	OpAddressBook::OpAddressBookFieldInfo::Meaning home_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_LANDLINE_PHONE;

	unsigned int first_name_index = field_infos_len;
	unsigned int middle_name_index = field_infos_len;
	unsigned int last_name_index = field_infos_len;
	unsigned int address_country_index = field_infos_len;
	unsigned int address_state_index = field_infos_len;
	unsigned int address_city_index = field_infos_len;
	unsigned int address_street_index = field_infos_len;
	unsigned int address_number_index = field_infos_len;
	unsigned int address_postal_code_index = field_infos_len;

	for (i = 0; i < field_infos_len; ++i)
	{
		switch (field_infos[i].meaning)
		{
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE:
				if (work_phone_meaning == OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE)
					work_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_PHONE:
				work_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_PHONE;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_MOBILE_PHONE:
				mobile_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_MOBILE_PHONE;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_LANDLINE_PHONE:
				home_phone_meaning = OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_LANDLINE_PHONE;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_FIRST_NAME:
				first_name_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_MIDDLE_NAME:
				middle_name_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_LAST_NAME:
				last_name_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_COUNTRY:
				address_country_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STATE:
				address_state_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_CITY:
				address_city_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STREET:
				address_street_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_NUMBER:
				address_number_index = i;
				break;
			case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_POSTAL_CODE:
				address_postal_code_index = i;
				break;
		}
	}
	typedef JIL_AddressBookMappings::Mapping Mapping;
	for (i = 0; i < field_infos_len; ++i)
	{
		const uni_char* property_name = NULL;
		BOOL is_predefined_jil_property = TRUE;
		Mapping* new_mapping = OP_NEW(Mapping, ());
		RETURN_OOM_IF_NULL(new_mapping);
		OpAutoPtr<Mapping> new_mapping_deleter(new_mapping);
		switch (field_infos[i].meaning)
		{
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_NAME:
			property_name = UNI_L("fullName");
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_ADDRESS:
			property_name = UNI_L("address");
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_EMAIL:
			property_name = UNI_L("eMail");
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_COMPANY:
			property_name = UNI_L("company");
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_TITLE:
			property_name = UNI_L("title");
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE:
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE:
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_PHONE:
			if (field_infos[i].meaning == work_phone_meaning)
				property_name = UNI_L("workPhone");
			else
			{
				if (field_infos[i].meaning == OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE)
					property_name = UNI_L("workMobilePhone");
				else if (field_infos[i].meaning == OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE)
					property_name = UNI_L("workLandlinePhone");
				else
					OP_ASSERT(FALSE);
			}
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_MOBILE_PHONE:
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_MOBILE_PHONE:
			if (field_infos[i].meaning == mobile_phone_meaning)
				property_name = UNI_L("mobilePhone");
			else
			{
				OP_ASSERT(field_infos[i].meaning == OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_MOBILE_PHONE);
				property_name = UNI_L("privateMobilePhone");
			}
			break;
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_LANDLINE_PHONE:
		case OpAddressBook::OpAddressBookFieldInfo::MEANING_LANDLINE_PHONE:
			if (field_infos[i].meaning == home_phone_meaning)
				property_name = UNI_L("homePhone");
			else
			{
				OP_ASSERT(field_infos[i].meaning == OpAddressBook::OpAddressBookFieldInfo::MEANING_LANDLINE_PHONE);
				property_name = UNI_L("landlinePhone");
			}
			break;
		}

		if (property_name == NULL)
		{	// Catch even the cases with asserts above
			is_predefined_jil_property = FALSE;
			property_name = field_infos[i].name;
		}

		new_mapping->field_index = i;
		new_mapping->multiplicity = field_infos[i].multiplicity;

		OP_ASSERT(property_name);
		OP_STATUS status = new_obj->m_mappings.Add(property_name, new_mapping);
		if (OpStatus::IsError(status))
		{
			if (status == OpStatus::ERR)
			{
				// funny thing happened some platform field has the same name but is other property is
				// mapped to predefined JIL property using it's meaning.
				// This will probably never happen, but just in case ...
				OP_ASSERT(FALSE); // this case is handled but is undesired and probably symptom of weird PI impl
				if (is_predefined_jil_property)
				{
					Mapping* old_mapping;
					RETURN_IF_ERROR(new_obj->m_mappings.Remove(property_name, &old_mapping));
					OP_DELETE(old_mapping);
					RETURN_IF_ERROR(new_obj->m_mappings.Add(property_name, new_mapping));
					new_mapping_deleter.release();
				}
				// else just ignore it - hard to decide what to do with such a property
			}
			else
				return status;
		}
		new_mapping_deleter.release();
	}
	// Virtual fields
	if (!new_obj->m_mappings.Contains(UNI_L("fullName")))
	{
		Mapping* new_mapping = OP_NEW(Mapping, ());
		OpAutoPtr<Mapping> new_mapping_deleter(new_mapping);
		RETURN_OOM_IF_NULL(new_mapping);
		new_mapping->field_index = FIELD_INDEX_VIRTUAL;
		new_mapping->multiplicity = 1;

		if (first_name_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(first_name_index));
		if (middle_name_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(middle_name_index));
		if (last_name_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(last_name_index));

		new_mapping->virtual_includes_multiple_values = TRUE;

		RETURN_IF_ERROR(new_obj->m_mappings.Add(UNI_L("fullName"), new_mapping));
		new_mapping_deleter.release();
	}
	else if (!new_obj->m_mappings.Contains(UNI_L("address")))
	{
		Mapping* new_mapping = OP_NEW(Mapping, ());
		OpAutoPtr<Mapping> new_mapping_deleter(new_mapping);
		RETURN_OOM_IF_NULL(new_mapping);
		new_mapping->field_index = FIELD_INDEX_VIRTUAL;
		new_mapping->multiplicity = 1;

		if (address_street_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_street_index));
		if (address_number_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_number_index));
		if (address_postal_code_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_postal_code_index));
		if (address_city_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_city_index));
		if (address_state_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_state_index));
		if (address_country_index < field_infos_len)
			RETURN_IF_ERROR(new_mapping->virtual_source_indexes.Add(address_country_index));

		new_mapping->virtual_includes_multiple_values = FALSE;

		RETURN_IF_ERROR(new_obj->m_mappings.Add(UNI_L("address"), new_mapping));
		new_mapping_deleter.release();
	}

	// END Virtual fields
	new_obj_deleter.release();
	return OpStatus::OK;
}

OP_STATUS
JIL_AddressBookMappings::GetAvailableAttributes(OpAddressBookItem* item, JIL_AddressBookMappings::CollectionBuilder* collection)
{
	OpAutoPtr<OpHashIterator> iterator(m_mappings.GetIterator());
	RETURN_OOM_IF_NULL(iterator.get());

	for (OP_STATUS has_next = iterator->First(); OpStatus::IsSuccess(has_next); has_next = iterator->Next())
	{
		Mapping* mapping = static_cast<Mapping*>(iterator->GetData());
#ifdef DAPI_JIL_GETAVAILABLEATTRIBUTES_REAL_ONLY
		if (mapping->IsVirtualField())
			continue;	// Skip virtual attributes
#endif // DAPI_JIL_GETAVAILABLEATTRIBUTES_REAL_ONLY
		const uni_char* base_name = static_cast<const uni_char*>(iterator->GetKey());
		RETURN_IF_ERROR(collection->Add(base_name));


		unsigned int value_count = 0;
		if (mapping->multiplicity > 1)
			value_count = mapping->multiplicity;
		else if (mapping->multiplicity == 0 && item)
			RETURN_IF_ERROR(item->GetValueCount(mapping->field_index, &value_count));

		OpString attribute_name;
		for (unsigned int i = 2; i <= value_count; ++i)
		{
			attribute_name.Empty();
			RETURN_IF_ERROR(attribute_name.AppendFormat(UNI_L("%s%d"), base_name, i));
			RETURN_IF_ERROR(collection->Add(attribute_name.CStr()));
		}
	}
	return OpStatus::OK;
}

BOOL
JIL_AddressBookMappings::GetMapping(const uni_char* property_name, int* field_index, int* value_index)
{
	OP_ASSERT(field_index);
	OP_ASSERT(value_index);
	Mapping* mapping;
	BOOL result = GetMapping(property_name, &mapping, value_index);
	if (result)
		*field_index = mapping->field_index;
	return result;
}

BOOL
JIL_AddressBookMappings::GetMapping(const uni_char* property_name, JIL_AddressBookMappings::Mapping** mapping, int* value_index)
{
	OP_ASSERT(mapping);
	OP_ASSERT(property_name);

	int full_name_len = static_cast<int>(uni_strlen(property_name));
	int base_name_len = full_name_len;

	// Search for numeric suffix to get the basename.
	for (; base_name_len > 0 && uni_isdigit(property_name[base_name_len-1]); --base_name_len)
		{ }
	OpString property_str; property_str.Set(property_name, base_name_len);
	RETURN_VALUE_IF_ERROR(m_mappings.GetData(property_str.CStr(), mapping), FALSE);
	OP_ASSERT(*mapping);
	if (base_name_len == full_name_len)
		*value_index = 0;
	else
	{
		int index_val = uni_atoi(property_name + base_name_len) - 1;
		if ((*mapping)->multiplicity != 0 && (*mapping)->multiplicity <= index_val)
			return FALSE;
		*value_index = index_val;
	}
	return TRUE;
}

OP_ADDRESSBOOKSTATUS
JIL_AddressBookMappings::GetAttributeValue(const OpAddressBookItem* address_book_item, const uni_char* attribute_name, OpString* value)
{
	int value_index;
	Mapping* mapping;
	BOOL has_mapping = GetMapping(attribute_name, &mapping, &value_index);
	if (!has_mapping)
		return OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE;

	if (!mapping->IsVirtualField())
		RETURN_IF_ERROR(address_book_item->GetValue(mapping->field_index, value_index, value));
	else
	{
		value->Empty();
		UINT32 count = mapping->virtual_source_indexes.GetCount();
		for (UINT32 i = 0; i < count; ++i)
		{
			unsigned int value_count = 1;
			if (mapping->virtual_includes_multiple_values)
				RETURN_IF_ERROR(address_book_item->GetValueCount(mapping->virtual_source_indexes.Get(i), &value_count));

			for (unsigned int value_index = 0; value_index < value_count; ++value_index)
			{
				const uni_char* real_field_value;
				RETURN_IF_ERROR(address_book_item->PeekValue(mapping->virtual_source_indexes.Get(i), value_index, &real_field_value));
				if (real_field_value != NULL)
					RETURN_IF_ERROR(value->AppendFormat(UNI_L("%s "), real_field_value));
			}
		}
		// Strip added spaces. This also removes and leading and trailing
		// spaces that might be part of the actual values. But it doesn't
		// matter much and simplifies the code a lot.
		value->Strip();
	}

	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
JIL_AddressBookMappings::SetAttributeValue(OpAddressBookItem* address_book_item, const uni_char* attribute_name, const uni_char* value)
{
	int field_index;
	int value_index;

	BOOL has_mapping = GetMapping(attribute_name, &field_index, &value_index);
	if (!has_mapping)
		return OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE;

	if (field_index != FIELD_INDEX_VIRTUAL)
		return address_book_item->SetValue(field_index, value_index, value);
	else
		return OpAddressBookStatus::ERR_NO_ACCESS;
}

BOOL
JIL_AddressBookMappings::HasAttribute(const uni_char* attribute_name)
{
	int dummy_field_index;
	int dummy_value_index;
	return GetMapping(attribute_name, &dummy_field_index, &dummy_value_index);
}

OP_STATUS
JIL_AddressBookItemData::VirtualFieldDataItem::Construct(const uni_char* new_key, const uni_char* new_value)
{
	OP_ASSERT(key == NULL && value == NULL);
	OP_ASSERT(new_key != NULL);
	key = uni_strdup(new_key);
	RETURN_OOM_IF_NULL(key);
	if (new_value != NULL)
	{
		value = uni_strdup(new_value);
		if (value == NULL)
		{
			op_free(key);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
JIL_AddressBookItemData::VirtualFieldDataItem::SetValue(const uni_char* new_value)
{
	uni_char* new_value_copy = NULL;
	if (new_value)
	{
		new_value_copy = uni_strdup(new_value);
		if (new_value_copy == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	op_free(value);
	value = new_value_copy;

	return OpStatus::OK;
}

OP_STATUS
JIL_AddressBookItemData::CloneFrom(JIL_AddressBookItemData& item_to_copy)
{
	OP_ASSERT(GetAddressBookItem() == NULL);

	OpAutoPtr<OpHashIterator> ap_iterator(item_to_copy.m_virtual_fields.GetIterator());
	RETURN_OOM_IF_NULL(ap_iterator.get());

	OP_STATUS iterator_status;
	for (iterator_status = ap_iterator->First(); OpStatus::IsSuccess(iterator_status); iterator_status = ap_iterator->Next())
	{
		VirtualFieldDataItem* src_data_item = static_cast<VirtualFieldDataItem*>(ap_iterator->GetData());
		OP_ASSERT(src_data_item);
		OpAutoPtr<VirtualFieldDataItem> ap_new_data_item(OP_NEW(VirtualFieldDataItem, ()));
		RETURN_OOM_IF_NULL(ap_new_data_item.get());
		RETURN_IF_ERROR(ap_new_data_item->Construct(src_data_item->key, src_data_item->value));
		RETURN_IF_ERROR(m_virtual_fields.Add(ap_new_data_item->key, ap_new_data_item.get()));
		ap_new_data_item.release();
	}

	OpAddressBookItem* source_item = item_to_copy.GetAddressBookItem();
	if (source_item != NULL)
	{
		OpAddressBookItem* new_op_address_book_item;
		RETURN_IF_ERROR(OpAddressBookItem::Make(&new_op_address_book_item, source_item));
		SetAddressBookItem(new_op_address_book_item);
	}

	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
JIL_AddressBookItemData::WriteAttributeValue(const uni_char* attribute_name, const uni_char* new_value)
{
	JIL_AddressBookMappings* mappings;
	RETURN_IF_ERROR(g_device_api.GetAddressBookMappings(mappings));

	OP_ADDRESSBOOKSTATUS setting_status = mappings->SetAttributeValue(m_address_book_item, attribute_name, new_value);
	if (setting_status == OpAddressBookStatus::ERR_NO_ACCESS)
	{
		// Virtual field, set the property
		VirtualFieldDataItem* data_item;
		if (m_virtual_fields.GetData(attribute_name, &data_item) == OpStatus::OK)
			setting_status = data_item->SetValue(new_value);
		else
		{
			VirtualFieldDataItem* new_data_item = OP_NEW(VirtualFieldDataItem, ());
			RETURN_OOM_IF_NULL(new_data_item);
			OP_STATUS status = new_data_item->Construct(attribute_name, new_value);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(new_data_item);
				return status;
			}
			setting_status = m_virtual_fields.Add(new_data_item->key, new_data_item);
		}
	}

	return setting_status;
}

OP_ADDRESSBOOKSTATUS
JIL_AddressBookItemData::ReadAttributeValue(const uni_char* attribute_name, OpString* value)
{
	JIL_AddressBookMappings* mappings;
	RETURN_IF_ERROR(g_device_api.GetAddressBookMappings(mappings));
	if (!value)
	{
		if (m_virtual_fields.Contains(attribute_name) || (mappings->HasAttribute(attribute_name)))
			return OpStatus::OK;
	}
	else
	{
		VirtualFieldDataItem* field_data;
		if (OpStatus::IsSuccess(m_virtual_fields.GetData(attribute_name, &field_data)))
			return value->Set(field_data->value);
		else
		{
			RETURN_IF_ERROR(mappings->GetAttributeValue(m_address_book_item, attribute_name, value));
			return OpStatus::OK;
		}
	}
	return OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE;
}

OP_BOOLEAN
JIL_AddressBookItemData::IsMatch(JIL_AddressBookItemData* match_item)
{
	OP_ASSERT(match_item);
	OP_ASSERT(match_item->HasItem());
	OP_ASSERT(HasItem());
	OpString tmp;
	const OpAddressBook::OpAddressBookFieldInfo* fields;
	unsigned int field_count;
	g_op_address_book->GetFieldInfos(&fields, &field_count);
	unsigned int i;
	OpAddressBookItem* op_reference_item = GetAddressBookItem();
	OpAddressBookItem* op_match_item = match_item->GetAddressBookItem();
	for (i = 0; i < field_count; ++i)
	{
		unsigned int ref_item_field_value_count;
		unsigned int result_field_value_count;
		RETURN_IF_ERROR(op_reference_item->GetValueCount(i, &ref_item_field_value_count));
		RETURN_IF_ERROR(op_match_item->GetValueCount(i, &result_field_value_count));
		for (unsigned int j = 0; j < ref_item_field_value_count; ++j)
		{
			OpString reference_str;
			RETURN_IF_ERROR(op_reference_item->GetValue(i, j, &reference_str));
			if (!reference_str.CStr())
				continue;
			BOOL found = FALSE;
			for (unsigned int k = 0; k < ref_item_field_value_count; ++k)
			{
				OpString value_str;
				RETURN_IF_ERROR(op_match_item->GetValue(i, k, &value_str));
				OP_BOOLEAN match_result = JILWildcardCmp(reference_str.CStr(), value_str.CStr());
				RETURN_IF_ERROR(match_result);
				if (OpBoolean::IS_TRUE == match_result)
				{
					found = TRUE;
					break;
				}
			}
			if (!found)
				return OpBoolean::IS_FALSE;
		}
	}
	// Check virtual fields
	OpAutoPtr<OpHashIterator> ap_iterator(m_virtual_fields.GetIterator());
	RETURN_OOM_IF_NULL(ap_iterator.get());
	OP_STATUS iterator_status;
	for (iterator_status = ap_iterator->First(); OpStatus::IsSuccess(iterator_status); iterator_status = ap_iterator->Next())
	{
		VirtualFieldDataItem* reference_field_data = reinterpret_cast<VirtualFieldDataItem*>(ap_iterator->GetData());

		OpString value;
		RETURN_IF_ERROR(match_item->ReadAttributeValue(reference_field_data->key, &value));
		if (reference_field_data->value)
		{
			OP_BOOLEAN match_result = JILWildcardCmp(reference_field_data->value, value.CStr());
			if (match_result != OpBoolean::IS_TRUE)
				return OpBoolean::IS_FALSE;
		}
	}
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
JIL_AddressBookItemData::IsVirtualFieldModified(const uni_char** modified_attribute_name)
{
	if (m_virtual_fields.GetCount() == 0)
		return OpBoolean::IS_FALSE;

	if (modified_attribute_name)
	{
		OpAutoPtr<OpHashIterator> ap_iterator(m_virtual_fields.GetIterator());
		RETURN_OOM_IF_NULL(ap_iterator.get());
		RETURN_IF_ERROR(ap_iterator->First());
		*modified_attribute_name = reinterpret_cast<const uni_char*>(ap_iterator->GetKey());
	}

	return OpBoolean::IS_TRUE;
}

#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT

#endif // DOM_JIL_API_SUPPORT
