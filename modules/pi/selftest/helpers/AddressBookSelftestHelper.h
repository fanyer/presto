/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PI_SELFTEST_HELPERS_ADDRESSBOOKSELFTESTHELPER_H
#define PI_SELFTEST_HELPERS_ADDRESSBOOKSELFTESTHELPER_H

#if defined(SELFTEST) && defined(PI_ADDRESSBOOK)

#include "modules/pi/device_api/OpAddressBook.h"
#include "modules/selftest/src/testutils.h"
#include "modules/selftest/src/asynctest.h"

class AddressBookSelftestHelper
{
public:
	// returns TRUE if arg1 is the same addressbook item as arg2
	static BOOL AddressBookItemsEqual(OpAddressBookItem& arg1, OpAddressBookItem& arg2);

	static OP_STATUS FillAddressBookItemWithData(OpAddressBookItem& item, BOOL add_randomness = FALSE);
	static OP_STATUS GenerateRandomString(OpString& output, const uni_char* allowed_chars, unsigned int max_length);

	class AddressBookAsyncTest : public AsyncTest
	{
	public:
		AddressBookAsyncTest(OpAddressBook* address_book);

		OP_STATUS GetItemCount(UINT32* count_ptr);
		OP_STATUS AddItem(OpAddressBookItem* item, OpString* id_ptr);
		OP_STATUS RemoveItem(const uni_char* id, OpString* id_ptr);
		OP_STATUS CommitItem(OpAddressBookItem* item, OpString* id_ptr);
		OP_STATUS GetItem(const uni_char* id, OpVector<OpAddressBookItem>* items_ptr);
		OP_STATUS GetAllItems(OpVector<OpAddressBookItem>* items_ptr);
		virtual void OnContinue();

		OP_STATUS m_last_operation_result;
	protected:
		OpAddressBook* m_address_book;
	private:
		BOOL m_async_operation_in_progress;
	};
};

#endif // defined(SELFTEST) && defined(PI_ADDRESSBOOK)

#endif // !PI_SELFTEST_HELPERS_ADDRESSBOOKSELFTESTHELPER_H
