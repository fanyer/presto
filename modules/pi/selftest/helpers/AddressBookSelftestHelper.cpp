/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(SELFTEST) && defined(PI_ADDRESSBOOK)

#include "modules/pi/selftest/helpers/AddressBookSelftestHelper.h"
#include "modules/selftest/testutils.h"

/* static */ OP_STATUS
AddressBookSelftestHelper::GenerateRandomString(OpString& output, const uni_char* allowed_chars, unsigned int max_length)
{
	RETURN_OOM_IF_NULL(output.Reserve(max_length));
	size_t allowed_chars_len = allowed_chars ? uni_strlen(allowed_chars) : 0;
	for (unsigned int i = 0; i < max_length; ++i)
	{
		OP_STATUS status;
		if (allowed_chars_len)
			status = output.Append(allowed_chars + op_rand() % allowed_chars_len, 1);
		else
		{
			uni_char random_char = static_cast<uni_char>(op_rand());
			status = output.Append(&random_char, 1);
		}
		OP_ASSERT(OpStatus::IsSuccess(status)); // as we reserved the space this should never fail
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
AddressBookSelftestHelper::FillAddressBookItemWithData(OpAddressBookItem& item, BOOL add_randomness)
{
	OP_ASSERT(item.GetAddressBook());

	OpString value, random;

	//not neccesarily full list
	const uni_char* printable_chars = UNI_L("~!@#$%^&*()_+{}:\">?|<`1234567890-=[]\\;',./ qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM");
	const uni_char* email_login_chars = UNI_L("_1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM");
	const uni_char* digit_chars = UNI_L("1234567890");

	RETURN_IF_ERROR(GenerateRandomString(value, digit_chars, 5));
	RETURN_IF_ERROR(item.SetId(value.CStr()));

	const OpAddressBook::OpAddressBookFieldInfo* field_infos;
	unsigned int infos_count;
	item.GetAddressBook()->GetFieldInfos(&field_infos, &infos_count);
	const unsigned int VALUE_COUNT_FOR_INFINITE_MULTIPLICITY = 10;
	for (unsigned int i = 0; i < infos_count; ++i)
	{
		unsigned int added_values_count = field_infos[i].multiplicity ? field_infos[i].multiplicity : VALUE_COUNT_FOR_INFINITE_MULTIPLICITY;
		for (unsigned int j = 0; j < added_values_count; ++j)
		{
			switch (field_infos[i].meaning)
			{
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_DISPLAY_NAME:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_NAME:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_FIRST_NAME:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_MIDDLE_NAME:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_LAST_NAME:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_TITLE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_FULL_ADDRESS:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_COUNTRY:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STATE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_CITY:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_STREET:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_COMPANY:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_CUSTOM:
				{
					if(add_randomness)
						RETURN_IF_ERROR(GenerateRandomString(random, printable_chars, 5));
					RETURN_IF_ERROR(value.AppendFormat(UNI_L("%s_%s"), field_infos[i].name, random.CStr()));
					break;
				}
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_NUMBER:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_ADDRESS_POSTAL_CODE:
				{
					if(add_randomness)
						RETURN_IF_ERROR(GenerateRandomString(random, digit_chars, 5));
					RETURN_IF_ERROR(value.AppendFormat(UNI_L("1%s"), random.CStr()));
					break;
				}
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_EMAIL:
				{
					if(add_randomness)
						RETURN_IF_ERROR(GenerateRandomString(random, email_login_chars, 15));
					RETURN_IF_ERROR(value.AppendFormat(UNI_L("login_%s@some.email.org"), random.CStr()));
					break;
				}
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_LANDLINE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_LANDLINE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_LANDLINE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_MOBILE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_MOBILE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_MOBILE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_PHONE:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_PRIVATE_FAX:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_WORK_FAX:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_FAX:
				case OpAddressBook::OpAddressBookFieldInfo::MEANING_CUSTOM_PHONE:
				{
					if(add_randomness)
						RETURN_IF_ERROR(GenerateRandomString(random, digit_chars, 5));
					RETURN_IF_ERROR(value.AppendFormat(UNI_L("600%s"), random.CStr()));
					break;
				}
				default:
					OP_ASSERT(FALSE); // if new meanings have been added then add them to this switch
					return OpStatus::ERR;
			}
			RETURN_IF_ERROR(item.SetValue(i, j, value.CStr()));
		}
	}
	return OpStatus::OK;
}

/* static */ BOOL
AddressBookSelftestHelper::AddressBookItemsEqual(OpAddressBookItem& arg1, OpAddressBookItem& arg2)
{
	if (arg1.GetAddressBook() != arg2.GetAddressBook())
		return FALSE;
	OpString arg1_val, arg2_val;
	RETURN_VALUE_IF_ERROR(arg1.GetId(&arg1_val), FALSE);
	RETURN_VALUE_IF_ERROR(arg2.GetId(&arg2_val), FALSE);
	if (!(arg1_val == arg2_val))
		return FALSE;

	const OpAddressBook::OpAddressBookFieldInfo* field_infos;
	unsigned int infos_count;
	arg1.GetAddressBook()->GetFieldInfos(&field_infos, &infos_count);
	for (unsigned int i = 0; i < infos_count; ++i)
	{
		unsigned int arg1_val_count, arg2_val_count;
		RETURN_VALUE_IF_ERROR(arg1.GetValueCount(i, &arg1_val_count), FALSE);
		RETURN_VALUE_IF_ERROR(arg2.GetValueCount(i, &arg2_val_count), FALSE);
		if (arg1_val_count != arg2_val_count)
			return FALSE;
		for (unsigned int j = 0; j < arg1_val_count; ++j)
		{
			RETURN_VALUE_IF_ERROR(arg1.GetValue(i, j, &arg1_val), FALSE);
			RETURN_VALUE_IF_ERROR(arg2.GetValue(i, j, &arg2_val), FALSE);
			if (!(arg1_val == arg2_val))
				return FALSE;
		}
	}
	return TRUE;
}

class TestGetAddressBookItemCountCallbackImpl : public OpGetAddressBookItemCountCallback
{
public:
	TestGetAddressBookItemCountCallbackImpl(AddressBookSelftestHelper::AddressBookAsyncTest* test, UINT32* count_ptr)
		: m_count_ptr(count_ptr)
		, m_test(test)
	{
		OP_ASSERT(m_count_ptr);
		OP_ASSERT(m_test);
	}

	virtual void OnSuccess(UINT32 count)
	{
		*m_count_ptr = count;
		m_test->m_last_operation_result = OpStatus::OK;
		m_test->Continue();
	}

	virtual void OnFailed(OP_STATUS error)
	{
		m_test->m_last_operation_result = error;
		m_test->Continue();
	}
private:
	UINT32* m_count_ptr;
	AddressBookSelftestHelper::AddressBookAsyncTest* m_test;
};

class TestAddressBookItemModificationCallbackImpl : public OpAddressBookItemModificationCallback
{
public:
	TestAddressBookItemModificationCallbackImpl(AddressBookSelftestHelper::AddressBookAsyncTest* test, OpString* id_ptr)
		: m_id_ptr(id_ptr)
		, m_test(test)
	{
		OP_ASSERT(m_test);
	}

	virtual void OnSuccess(const uni_char* id)
	{
		OP_STATUS error = OpStatus::OK;
		if (m_id_ptr)
			error = m_id_ptr->Set(id);
		m_test->m_last_operation_result = error;
		m_test->Continue();
	}

	virtual void OnFailed(const uni_char* id, OP_STATUS error)
	{
		m_test->m_last_operation_result = error;
		if (m_id_ptr)
			m_id_ptr->Set(id);
		m_test->Continue();
	}
private:
	OpString* m_id_ptr;
	AddressBookSelftestHelper::AddressBookAsyncTest* m_test;
};

class TestAddressBookIterationCallbackImpl : public OpAddressBookIterationCallback
{
public:
	TestAddressBookIterationCallbackImpl(AddressBookSelftestHelper::AddressBookAsyncTest* test, OpVector<OpAddressBookItem>* address_book_items_ptr)
		: m_address_book_items_ptr(address_book_items_ptr)
		, m_test(test)
	{
		OP_ASSERT(m_address_book_items_ptr);
		OP_ASSERT(m_test);
		m_address_book_items_ptr->Clear();
	}

	virtual BOOL OnItem(OpAddressBookItem* item)
	{
		m_test->m_last_operation_result = OpStatus::ERR_NO_MEMORY; // default error - will be set to OK in OnFinished or to any other error if it occurs
		RETURN_VALUE_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(item), FALSE);
		OP_STATUS error =  m_address_book_items_ptr->Add(item);
		if (OpStatus::IsError(error))
		{
			m_test->m_last_operation_result = error;
			m_test->Continue();
			return FALSE;
		}
		return TRUE;
	}

	virtual void OnFinished()
	{
		m_test->m_last_operation_result = OpStatus::OK;
		m_test->Continue();
	}

	virtual void OnError(OP_ADDRESSBOOKSTATUS error)
	{
		m_test->m_last_operation_result = error;
		m_test->Continue();
	}
private:
	OpVector<OpAddressBookItem>* m_address_book_items_ptr;
	AddressBookSelftestHelper::AddressBookAsyncTest* m_test;
};


AddressBookSelftestHelper::AddressBookAsyncTest::AddressBookAsyncTest(OpAddressBook* address_book)
	: m_last_operation_result(OpStatus::OK)
	, m_address_book(address_book)
	, m_async_operation_in_progress(FALSE)
{
	OP_ASSERT(m_address_book);
}

OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::GetItemCount(UINT32* count_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestGetAddressBookItemCountCallbackImpl* cb = OP_NEW(TestGetAddressBookItemCountCallbackImpl, (this, count_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->GetItemCount(cb);
	return OpStatus::OK;
}
OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::AddItem(OpAddressBookItem* item, OpString* id_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestAddressBookItemModificationCallbackImpl* cb = OP_NEW(TestAddressBookItemModificationCallbackImpl, (this, id_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->AddItem(cb, item);
	return OpStatus::OK;
}
OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::RemoveItem(const uni_char* id, OpString* id_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestAddressBookItemModificationCallbackImpl* cb = OP_NEW(TestAddressBookItemModificationCallbackImpl, (this, id_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->RemoveItem(cb, id);
	return OpStatus::OK;
}
OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::CommitItem(OpAddressBookItem* item, OpString* id_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestAddressBookItemModificationCallbackImpl* cb = OP_NEW(TestAddressBookItemModificationCallbackImpl, (this, id_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->CommitItem(cb, item);
	return OpStatus::OK;
}
OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::GetItem(const uni_char* id, OpVector<OpAddressBookItem>* items_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestAddressBookIterationCallbackImpl* cb = OP_NEW(TestAddressBookIterationCallbackImpl, (this, items_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->GetItem(cb, id);
	return OpStatus::OK;
}

OP_STATUS AddressBookSelftestHelper::AddressBookAsyncTest::GetAllItems(OpVector<OpAddressBookItem>* items_ptr)
{
	OP_ASSERT(!m_async_operation_in_progress);
	m_async_operation_in_progress = TRUE;

	TestAddressBookIterationCallbackImpl* cb = OP_NEW(TestAddressBookIterationCallbackImpl, (this, items_ptr));
	RETURN_IF_ERROR(g_selftest.utils->GetMemoryCleanupUtil()->DeleteAfterTest(cb));
	m_address_book->Iterate(cb);
	return OpStatus::OK;
}

/* virtual */ void
AddressBookSelftestHelper::AddressBookAsyncTest::OnContinue()
{
	AsyncTest::OnContinue();
	m_async_operation_in_progress = FALSE;
}

#endif // defined(SELFTEST) && defined(PI_ADDRESSBOOK)
