/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_VCARD_SUPPORT

#include "modules/device_api/OpVCardEntry.h"
#include "modules/pi/device_api/OpAddressBook.h"
#include "modules/device_api/src/VCardGlobals.h"

static OpString* MakeOpString(const uni_char* val)
{
	OpString* retval = OP_NEW(OpString, ());
	RETURN_VALUE_IF_NULL(retval, NULL);
	OP_STATUS status = retval->Set(val);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(retval);
		return NULL;
	}
	return retval;
}

/*static*/ BOOL
OpVCardEntry::FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::Meaning meaning, const OpAddressBook::OpAddressBookFieldInfo* infos, UINT32 infos_count, UINT32& index)
{
	for (index = 0; index < infos_count; ++index)
	{
		if (infos[index].meaning == meaning)
			return TRUE;
	}
	return FALSE;
}

OP_STATUS OpVCardEntry::ImportFromOpAddressBookItem(OpAddressBookItem* address_book_item)
{
	const OpAddressBook::OpAddressBookFieldInfo* infos;
	UINT32 count;
	address_book_item->GetAddressBook()->GetFieldInfos(&infos, &count);
	OpString current_val;
	UINT32 index = 0;
	// formatted name is made by concatenating title and full name
	// if there is no full name than we try to make it using first/second/last name
	// TODO - in some locales family name comes first etc. - make it work too
	OpString formatted_name;
	OpString value_buffer;
	BOOL has_full_name = FALSE;

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_TITLE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
			{
				RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
				RETURN_IF_ERROR(AddHonorificPrefix(value_buffer.CStr()));
			}
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_NAME, infos, count, index))
	{
		has_full_name = TRUE;
		RETURN_IF_ERROR(address_book_item->GetValue(index, 0, &value_buffer));
		if (value_buffer.CStr())
			RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_FIRST_NAME, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		if (val_count > 0)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, 0, &value_buffer));
			if (value_buffer.CStr())
			{
				RETURN_IF_ERROR(AddGivenName(value_buffer.CStr()));
				if (!has_full_name)
					RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
			}
		}

		for (UINT32 i = 1; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
			{
				RETURN_IF_ERROR(AddAdditionalName(value_buffer.CStr()));
				if (!has_full_name)
					RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
			}
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_MIDDLE_NAME, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);

		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
			{
				RETURN_IF_ERROR(AddAdditionalName(value_buffer.CStr()));
				if (!has_full_name)
					RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
			}
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_LAST_NAME, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
			{
				RETURN_IF_ERROR(AddFamilyName(value_buffer.CStr()));
				if (!has_full_name)
					RETURN_IF_ERROR(formatted_name.AppendFormat(UNI_L("%s "), value_buffer.CStr()));
			}
		}
	}

	if (!formatted_name.IsEmpty())
	{
		formatted_name.Strip();
		RETURN_IF_ERROR(SetFormattedName(formatted_name.CStr()));

	}
	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_EMAIL, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddEMail(OpVCardEntry::EMAIL_DEFAULT, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_LANDLINE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_HOME, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_WORK, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_LANDLINE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_MOBILE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_CELL, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_CELL|OpVCardEntry::TELEPHONE_WORK, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_MOBILE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_CELL, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE|OpVCardEntry::TELEPHONE_WORK, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_PHONE, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_VOICE, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_FAX, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_FAX|OpVCardEntry::TELEPHONE_HOME, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_FAX, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_FAX|OpVCardEntry::TELEPHONE_WORK, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_FAX, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			if (value_buffer.CStr())
				RETURN_IF_ERROR(AddTelephoneNumber(OpVCardEntry::TELEPHONE_FAX, value_buffer.CStr()));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_COMPANY, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		for (UINT32 i = 0; i < val_count; ++i)
		{
			RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
			const uni_char* organization_chain[1] = {value_buffer.CStr()};
			if (value_buffer.CStr())
				RETURN_IF_ERROR(SetOrganizationalChain(organization_chain, ARRAY_SIZE(organization_chain)));
		}
	}

	if (FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_ADDRESS, infos, count, index))
	{
		UINT32 val_count;
		address_book_item->GetValueCount(index, &val_count);
		OpString label_buffer;
		for (UINT32 i = 0; i < val_count; ++i)
		{
			UINT32 val_count;
			address_book_item->GetValueCount(index, &val_count);
			for (UINT32 i = 0; i < val_count; ++i)
			{
				RETURN_IF_ERROR(address_book_item->GetValue(index, i, &value_buffer));
				if (value_buffer.CStr())
				{
					label_buffer.Empty();
					RETURN_IF_ERROR(label_buffer.AppendFormat(UNI_L("%s\n%s"),formatted_name.CStr(), value_buffer.CStr()));
					RETURN_IF_ERROR(AddPostalLabelAddress(OpVCardEntry::ADDRESS_DEFAULT, label_buffer.CStr()));
				}
			}
		}
	}

	UINT32 country_index = 0, state_index = 0, city_index = 0, street_index = 0, number_index = 0, postal_code_index = 0;
	BOOL has_country = FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_COUNTRY, infos, count, country_index);
	BOOL has_state = FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STATE, infos, count, state_index);
	BOOL has_city = FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_CITY, infos, count, city_index);
	BOOL has_street = FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STREET, infos, count, street_index);
	BOOL has_number	= FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_NUMBER, infos, count, number_index);
	BOOL has_postal_code = FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_POSTAL_CODE, infos, count, postal_code_index);

	UINT32 max_count = 0, tmp_count = 0;
	address_book_item->GetValueCount(country_index, &max_count);
	address_book_item->GetValueCount(state_index, &tmp_count);
	max_count = max_count > tmp_count ? max_count : tmp_count;
	address_book_item->GetValueCount(city_index, &tmp_count);
	max_count = max_count > tmp_count ? max_count : tmp_count;
	address_book_item->GetValueCount(street_index, &tmp_count);
	max_count = max_count > tmp_count ? max_count : tmp_count;
	address_book_item->GetValueCount(number_index, &tmp_count);
	max_count = max_count > tmp_count ? max_count : tmp_count;
	address_book_item->GetValueCount(postal_code_index, &tmp_count);
	max_count = max_count > tmp_count ? max_count : tmp_count;

	OpString country_buffer, state_buffer, city_buffer, street_buffer, number_buffer, postal_code_buffer;
	OpString street_address;
	for (UINT32 i = 0; i < max_count; ++i)
	{
		if (has_country)
			RETURN_IF_ERROR(address_book_item->GetValue(country_index, i, &country_buffer));
		if (has_state)
			RETURN_IF_ERROR(address_book_item->GetValue(state_index, i, &state_buffer));
		if (has_city)
			RETURN_IF_ERROR(address_book_item->GetValue(city_index, i, &city_buffer));
		if (has_street)
			RETURN_IF_ERROR(address_book_item->GetValue(street_index, i, &street_buffer));
		if (has_number)
			RETURN_IF_ERROR(address_book_item->GetValue(number_index, i, &number_buffer));
		if (has_postal_code)
			RETURN_IF_ERROR(address_book_item->GetValue(postal_code_index, i, &postal_code_buffer));

		if (country_buffer.CStr() || state_buffer.CStr() || city_buffer.CStr() || street_buffer.CStr()
			|| number_buffer.CStr() || postal_code_buffer.CStr()) // at least one must be non-NULL
		{
			street_address.Empty();
			RETURN_IF_ERROR(street_address.AppendFormat(UNI_L("%s %s"), street_buffer.CStr(), number_buffer.CStr()));
			RETURN_IF_ERROR(AddStructuredAddress(OpVCardEntry::ADDRESS_DEFAULT ,NULL, NULL, street_address.CStr(), city_buffer.CStr()
													   , state_buffer.CStr(), postal_code_buffer.CStr(), country_buffer.CStr()));
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpVCardEntry::ApplyTypeFlags(OpDirectoryEntryContentLine* value, int flags, const uni_char* const* flags_texts, int last_flag)
{
	// delete all type flags
	while (value->GetParams()->Delete(UNI_L("TYPE"), 0) == OpStatus::OK) { }

	for (int mask = 1, i = 0; mask <= last_flag; mask <<= 1, ++i)
	{
		if (flags & mask)
		{
			OpString* type_string = MakeOpString(flags_texts[i]);
			RETURN_OOM_IF_NULL(type_string);
			RETURN_IF_ERROR(value->GetParams()->Add(UNI_L("TYPE"), type_string));
		}
	}
	return OpStatus::OK;
}

// see RFC2426 (vCard) - FN
OP_STATUS OpVCardEntry::SetFormattedName(const uni_char* fn)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("FN")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(fn);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - N - 1st field
OP_STATUS OpVCardEntry::AddFamilyName(const uni_char* family_name)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("N"), 5));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(family_name);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(0, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - N - 2nd field
OP_STATUS OpVCardEntry::AddGivenName(const uni_char* given_name)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("N"), 5));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(given_name);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(1, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - N - 3rd field
OP_STATUS OpVCardEntry::AddAdditionalName(const uni_char* additional_name)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("N"), 5));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(additional_name);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(2, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - N - 4th field
OP_STATUS OpVCardEntry::AddHonorificPrefix(const uni_char* honorific_prefix)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("N"), 5));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(honorific_prefix);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(3, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - N - 5th field
OP_STATUS OpVCardEntry::AddHonorificSuffix(const uni_char* honorific_suffix)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("N"), 5));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(honorific_suffix);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(4, value_struct));
	return OpStatus::OK;
}
// see RFC2426 (vCard) - NICKNAME
OP_STATUS OpVCardEntry::AddNickname(const uni_char* nickname)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("NICKNAME")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(nickname);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->AddValue(0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - BDAY
OP_STATUS OpVCardEntry::SetBirthday(int year, int month, int day, double time_of_day = 0.0, double time_offset = 0.0)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}


// see RFC2426 (vCard) - ADR
OP_STATUS OpVCardEntry::AddStructuredAddress(int type_flags
				   , const uni_char* post_office_box
				   , const uni_char* extended_address
				   , const uni_char* street_address
				   , const uni_char* locality
				   , const uni_char* region
				   , const uni_char* postal_code
				   , const uni_char* country_name)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = ConstructNewNamedContentLine(UNI_L("ADR"), 7));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(post_office_box);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(extended_address);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(1, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(street_address);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(2, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(locality);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(3, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(region);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(4, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(postal_code);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(5, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewStringValue(country_name);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(6, 0, value_struct));

	return ApplyAddressTypeFlags(value, type_flags);
}

OP_STATUS OpVCardEntry::ApplyAddressTypeFlags(OpDirectoryEntryContentLine* value, int flags)
{
	OP_ASSERT(1 << (ARRAY_SIZE(g_device_api.GetVCardGlobals()->address_type_names)-1) == ADDRESS_LAST);
	return ApplyTypeFlags(value, flags, g_device_api.GetVCardGlobals()->address_type_names, ADDRESS_LAST);
}


// see RFC2426 (vCard) - LABEL
OP_STATUS OpVCardEntry::AddPostalLabelAddress(int type_flags
	               , const uni_char* address)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = ConstructNewNamedContentLine(UNI_L("LABEL")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(address);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));

	return ApplyAddressTypeFlags(value, type_flags);
}

// see RFC2426 (vCard) - TEL
OP_STATUS OpVCardEntry::AddTelephoneNumber(int type_flags
	               , const uni_char* number)
{
	//TODO validate telephone
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = ConstructNewNamedContentLine(UNI_L("TEL")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(number);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));

	return ApplyTelephoneTypeFlags(value, type_flags);
}
OP_STATUS OpVCardEntry::ApplyTelephoneTypeFlags(OpDirectoryEntryContentLine* value, int flags)
{
	OP_ASSERT(1 << (ARRAY_SIZE(g_device_api.GetVCardGlobals()->phone_type_names)-1) == TELEPHONE_LAST);
	return ApplyTypeFlags(value, flags, g_device_api.GetVCardGlobals()->phone_type_names, TELEPHONE_LAST);
}

OP_STATUS OpVCardEntry::ApplyEmailTypeFlags(OpDirectoryEntryContentLine* value, int flags)
{
	OP_ASSERT(1 << (ARRAY_SIZE(g_device_api.GetVCardGlobals()->email_type_names)-1) == EMAIL_LAST);
	return ApplyTypeFlags(value, flags, g_device_api.GetVCardGlobals()->email_type_names, EMAIL_LAST);
}

// see RFC2426 (vCard) - EMAIL
OP_STATUS OpVCardEntry::AddEMail(int type_flags
	               , const uni_char* email)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = ConstructNewNamedContentLine(UNI_L("EMAIL")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(email);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));

	return ApplyEmailTypeFlags(value, type_flags);
}

// see RFC2426 (vCard) - MAILER
OP_STATUS OpVCardEntry::SetMailer(const uni_char* mailer)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("MAILER")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(mailer);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - TZ
OP_STATUS OpVCardEntry::SetTimezone(double timezone_offset)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("TZ")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewFloatValue(timezone_offset);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - GEO
OP_STATUS OpVCardEntry::SetLocation(double latitude, double longitude)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("GEO")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewFloatValue(latitude);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	value_struct = OpDirectoryEntryContentLine::NewFloatValue(longitude);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(1, 0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - TITLE
OP_STATUS OpVCardEntry::SetOrganizationTitle(const uni_char* title)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("TITLE")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(title);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - ROLE
OP_STATUS OpVCardEntry::SetOrganizationRole(const uni_char* role)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("ROLE")));
	OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(role);
	RETURN_OOM_IF_NULL(value_struct);
	RETURN_IF_ERROR(value->SetValue(0, 0, value_struct));
	return OpStatus::OK;
}

// see RFC2426 (vCard) - AGENT
OP_STATUS OpVCardEntry::AddAgentURI(const uni_char* uri){ return OpStatus::ERR_NOT_SUPPORTED; }
OP_STATUS OpVCardEntry::AddAgentVCard(const OpVCardEntry* v_card){ return OpStatus::ERR_NOT_SUPPORTED; }
OP_STATUS OpVCardEntry::AddAgentText(const uni_char* agent_text){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - ORG
OP_STATUS OpVCardEntry::SetOrganizationalChain(const uni_char** organizational_chain, UINT32 chain_length)
{
	OpDirectoryEntryContentLine* value;
	RETURN_OOM_IF_NULL(value = FindOrConstructNamedContentLine(UNI_L("ORG")));
	for (UINT32 i = 0; i < chain_length; ++i)
	{
		OpDirectoryEntryContentLine::ValueStruct* value_struct = OpDirectoryEntryContentLine::NewStringValue(organizational_chain[i]);
		RETURN_OOM_IF_NULL(value_struct);
		RETURN_IF_ERROR(value->SetValue(i, 0, value_struct));
	}
	return OpStatus::OK;
}

// see RFC2426 (vCard) - CATEGORIES
OP_STATUS OpVCardEntry::AddCategory(const uni_char* category){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - NOTE
OP_STATUS OpVCardEntry::AddNote(const uni_char* note){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - SORT-STRING
OP_STATUS OpVCardEntry::AddSortString(const uni_char* sort_string){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - URL
OP_STATUS OpVCardEntry::AddURL(const uni_char* url){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - PHOTO
OP_STATUS OpVCardEntry::AddPhoto(const uni_char* uri){ return OpStatus::ERR_NOT_SUPPORTED; }
OP_STATUS OpVCardEntry::AddPhoto(OpFileDescriptor* opened_input_file, const uni_char* type){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - LOGO
OP_STATUS OpVCardEntry::AddLogo(const uni_char* uri){ return OpStatus::ERR_NOT_SUPPORTED; }
OP_STATUS OpVCardEntry::AddLogo(OpFileDescriptor* opened_input_file, const uni_char* type){ return OpStatus::ERR_NOT_SUPPORTED; }

// see RFC2426 (vCard) - SOUND
OP_STATUS OpVCardEntry::AddSound(const uni_char* uri){ return OpStatus::ERR_NOT_SUPPORTED; }
OP_STATUS OpVCardEntry::AddSound(OpFileDescriptor* opened_input_file, const uni_char* type){ return OpStatus::ERR_NOT_SUPPORTED; }


OP_STATUS OpVCardEntry::Print(OpFileDescriptor* opened_file)
{
	OP_ASSERT(opened_file);
	const char* preamble = "BEGIN:VCARD\r\nPROFILE:VCARD\r\nVERSION:3.0\r\n";
	RETURN_IF_ERROR(opened_file->Write(preamble, op_strlen(preamble)));
	RETURN_IF_ERROR(OpDirectoryEntry::Print(opened_file));
	const char* ending = "END:VCARD\r\n";
	return opened_file->Write(ending, op_strlen(ending));
}

#endif // DAPI_VCARD_SUPPORT

