/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILADDRESSBOOKITEM_H
#define DOM_DOMJILADDRESSBOOKITEM_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/device_api/jil/JILAddressBook.h"
#include "modules/pi/device_api/OpAddressBook.h"

class DOM_JILPIM;
class DOM_JILAddressBookItem_Constructor;

class DOM_JILAddressBookItem : public DOM_JILObject
{
	friend class DOM_JILAddressBookItem_Constructor;
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_ADDRESSBOOKITEM || DOM_Object::IsA(type); }

	/**
	 * Constructs a new DOM_JILAddressBookItem object
	 *
	 * @param new_obj set to a newly constructed object
	 * @param address_book_item pointer to platform implemntation of OpAddressBookItem. DOM_JILAddressBookItem takes ownership
	 *			of this object. Must not be NULL;
	 */
	static OP_STATUS Make(DOM_JILAddressBookItem*& new_obj, OpAddressBookItem* address_book_item, DOM_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(getAvailableAttributes);
	DOM_DECLARE_FUNCTION(getAttributeValue);
	DOM_DECLARE_FUNCTION(setAttributeValue);
	DOM_DECLARE_FUNCTION(update);
	enum { FUNCTIONS_ARRAY_SIZE = 5 };

	JIL_AddressBookItemData& GetAddressBookItemData() { return m_item_data; }
	OpAddressBookItem* GetAddressBookItem() const { return m_item_data.GetAddressBookItem(); }
	OP_STATUS ExportAsVCard(OpFileDescriptor* opened_file);

	/** Checks whether the item can be written to address book.
	 *
	 * Items that have their virtual fields modified cannot be saved to the
	 * device's address book because the virtual fields don't exist in the
	 * device.
	 * In this case INVALID_EXCEPTION is to be thrown. The exception is being
	 * constructed by this function.
	 *
	 * @param return_value Set to the exception object, if ES_EXCEPTION is returned.
	 * @param runtime Runtime in which exception is to be created.
	 *
	 * @return
	 * - ES_VALUE if the item may be saved (no exception is constructed)
	 * - ES_EXCEPTION if the item may not be saved (return_value is set to a new exception object)
	 * - other values indicate error: ES_NO_MEMORY, ES_FAILED.
	 */
	int CheckCanBeSaved(ES_Value* return_value, DOM_Runtime* runtime);

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	void SetAddressBookItem(OpAddressBookItem* item);
	DOM_JILAddressBookItem() {}

	/** Reads attribute value.
	 *
	 * Handles both real and virtual attribute names.
	 *
	 * @param attribute_name Name of the attribute to read.
	 * @param value Set to the value of attribute (string or undefined).
	 * May be NULL in which case the function only checks that attribute_name
	 * is correct.
	 * @param temp_buffer A temporary buffer to be used for storing the string that
	 * is set in value argument.
	 *
	 * @return
	 *  - OK if the attribute_name is correct
	 *  - ERR_NO_MEMORY
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE if there is no attribute by the name of attribute_name.
	 */
	OP_ADDRESSBOOKSTATUS ReadAttributeValue(const uni_char* attribute_name, ES_Value* value, TempBuffer* temp_buffer);
	OP_ADDRESSBOOKSTATUS ReadAttributeValue(const uni_char* attribute_name, OpString* value);

	/** Writes attribute value.
	 *
	 * Handles both real and virtual attribute names.
	 *
	 * @param attribute_name Name of the attribute to read.
	 * @param value The value of attribute to set (may be a string, null or undefined)
	 *
	 * @return
	 *  - OK if the attribute_name is correct or
	 *  - ERR_NO_MEMORY
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE if there is no attribute by the name of attribute_name.
	 *  - ERR_OUT_OF_RANGE if the value is of incorrect type (neither string, null nor undefined).
	 */
	OP_ADDRESSBOOKSTATUS WriteAttributeValue(const uni_char* attribute_name, const ES_Value* value);
	OP_ADDRESSBOOKSTATUS WriteAttributeValue(const uni_char* attribute_name, const uni_char* new_value);

	/** Handles address book error.
	 *
	 * @param error Error code.
	 * @param attribure_name Name of the attribute that is related to the error. May be used in message.
	 * @param return_value Set to an exception object if one is generated.
	 * @param runtime Runtime in which exception is to be created.
	 *
	 * @return ES_EXCEPTION, ES_NO_MEMORY or ES_FAILED.
	 */
	static int HandleAddressBookError(OP_ADDRESSBOOKSTATUS error, const uni_char* attribute_name, ES_Value* return_value, DOM_Runtime* runtime);

	JIL_AutoAddressBookItemData m_item_data;
};

class DOM_JILAddressBookItem_Constructor : public DOM_BuiltInConstructor
{
	friend class DOM_JILPIM;
public:
	DOM_JILAddressBookItem_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_ADDRESSBOOKITEM_PROTOTYPE) { }

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
private:
	static int CreateAddressBookItem(ES_Value* return_value, ES_Runtime* origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILADDRESSBOOKITEM_H
