/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef PI_ADDRESSBOOK

#include "modules/pi/device_api/OpAddressBook.h"

OpAddressBookItem::~OpAddressBookItem()
{
	OP_DELETEA(m_fields);
}

/* static */ OP_STATUS
OpAddressBookItem::Make(OpAddressBookItem** new_addressbook_item, OpAddressBook* address_book)
{
	OP_ASSERT(address_book);
	OP_ASSERT(new_addressbook_item);
	*new_addressbook_item = OP_NEW(OpAddressBookItem, (address_book));
	RETURN_OOM_IF_NULL(new_addressbook_item);
	OpAutoPtr<OpAddressBookItem> new_addressbook_deleter(*new_addressbook_item);
	
	const OpAddressBook::OpAddressBookFieldInfo* infos;
	unsigned int infos_count;
	address_book->GetFieldInfos(&infos, &infos_count);
	(*new_addressbook_item)->m_fields = OP_NEWA(OpAutoVector<OpString>, infos_count);
	RETURN_OOM_IF_NULL((*new_addressbook_item)->m_fields);
	new_addressbook_deleter.release();
	return OpStatus::OK;
}
	
/* static */ OP_STATUS
OpAddressBookItem::Make(OpAddressBookItem** new_addressbook_item, OpAddressBookItem* item)
{
	OP_ASSERT(item);
	OP_ASSERT(new_addressbook_item);
	RETURN_IF_ERROR(Make(new_addressbook_item, item->GetAddressBook()));
	OpAutoPtr<OpAddressBookItem> new_addressbook_deleter(*new_addressbook_item);
	OpString id;
	RETURN_IF_ERROR(item->GetId(&id));
	RETURN_IF_ERROR((*new_addressbook_item)->SetId(id.CStr()));
	RETURN_IF_ERROR((*new_addressbook_item)->ImportFields(item));
	new_addressbook_deleter.release();
	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS 
OpAddressBookItem::AppendValue(unsigned int field_info_index, const uni_char* value)
{
	unsigned int count;
	RETURN_IF_ERROR(GetValueCount(field_info_index, &count));
	RETURN_IF_ERROR(EnsureValue(field_info_index, count));
		
	OpString* str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	RETURN_IF_ERROR(str->Set(value));
	OP_STATUS error = m_fields[field_info_index].Add(str);
	if (OpStatus::IsError(error))
		OP_DELETE(str);
	return error;
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::RemoveValue(unsigned int field_info_index, unsigned int value_index)
{
	RETURN_IF_ERROR(EnsureValue(field_info_index, value_index));
	m_fields[field_info_index].Delete(value_index);
	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::GetValue(unsigned int field_info_index, unsigned int value_index, OpString* value) const
{
	const uni_char* value_ptr;
	RETURN_IF_ERROR(PeekValue(field_info_index, value_index, &value_ptr));
	return value->Set(value_ptr);
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::PeekValue(unsigned int field_info_index, unsigned int value_index, const uni_char** value) const
{
	OP_ASSERT(value);
	unsigned int count;
	RETURN_IF_ERROR(GetValueCount(field_info_index, &count));
	if (value_index < count)
	{
		OpString* value_string = m_fields[field_info_index].Get(value_index);
		if (value_string)
		{
			*value = m_fields[field_info_index].Get(value_index)->CStr();
			return OpStatus::OK;
		}
	}

	*value = NULL;
	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::SetValue(unsigned int field_info_index, unsigned int value_index, const uni_char* value)
{
	RETURN_IF_ERROR(EnsureValue(field_info_index, value_index));
	while (m_fields[field_info_index].GetCount() <= value_index)
		RETURN_IF_ERROR(m_fields[field_info_index].Add(NULL));
	
	OpString* str = OP_NEW(OpString, ());
	RETURN_OOM_IF_NULL(str);
	OpAutoPtr<OpString> str_deleter(str);
	RETURN_IF_ERROR(str->Set(value));
	OpString* old_val = m_fields[field_info_index].Get(value_index); 
	RETURN_IF_ERROR(m_fields[field_info_index].Replace(value_index, str));
	OP_DELETE(old_val);
	str_deleter.release();
	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::GetValueCount(unsigned int field_info_index, unsigned int* count) const
{
	OP_ASSERT(count);
	RETURN_IF_ERROR(EnsureValue(field_info_index, 0));
	*count = m_fields[field_info_index].GetCount();
	return OpStatus::OK;
}

OP_STATUS
OpAddressBookItem::ImportFields(OpAddressBookItem* item)
{
	OP_ASSERT(item);
	OP_ASSERT(item->m_addressbook == this->m_addressbook);
	const OpAddressBook::OpAddressBookFieldInfo* infos;
	unsigned int infos_count;
	m_addressbook->GetFieldInfos(&infos, &infos_count);
	OpAutoArray<OpAutoVector<OpString> > fields_copy(OP_NEWA(OpAutoVector<OpString>, infos_count));
	RETURN_OOM_IF_NULL(fields_copy.get());
	for(unsigned int i = 0; i < infos_count; ++i)
	{

		for(unsigned int j = 0; j < item->m_fields[i].GetCount(); ++j)
		{
			OpString* input = item->m_fields[i].Get(j);
			if (input)
			{
				OpString* new_str = OP_NEW(OpString, ());
				RETURN_OOM_IF_NULL(new_str);
				OP_STATUS err = new_str->Set(input->CStr());
				if (OpStatus::IsError(err))
				{
					OP_DELETE(new_str);
					return err;
				}
				input = new_str;
			}
			RETURN_IF_ERROR(fields_copy[i].Add(input));
		}
	}
	OP_DELETEA(m_fields);
	m_fields = fields_copy.release();
	return OpStatus::OK;
}

OP_ADDRESSBOOKSTATUS
OpAddressBookItem::EnsureValue(unsigned int field_info_index, unsigned int value_index) const
{
	const OpAddressBook::OpAddressBookFieldInfo* infos;
	unsigned int infos_count;
	m_addressbook->GetFieldInfos(&infos, &infos_count);
	if (field_info_index >= infos_count)
		return OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE;
	else if (infos[field_info_index].multiplicity != 0 && value_index >= infos[field_info_index].multiplicity)
		return OpAddressBookStatus::ERR_VALUE_INDEX_OUT_OF_RANGE;
	else
		return OpStatus::OK;
}

#endif  // PI_ADDRESSBOOK
