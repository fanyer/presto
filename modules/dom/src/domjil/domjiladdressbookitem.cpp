/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjiladdressbookitem.h"

#include "modules/device_api/jil/JILAddressBook.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domjil/domjilpim.h"
#include "modules/dom/src/domcallstate.h"
#include "modules/dom/src/domsuspendcallback.h"

#include "modules/ecmascript/ecmascript.h"


/* virtual */ ES_GetState
DOM_JILAddressBookItem::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_address:
	case OP_ATOM_company:
	case OP_ATOM_eMail:
	case OP_ATOM_fullName:
	case OP_ATOM_homePhone:
	case OP_ATOM_mobilePhone:
	case OP_ATOM_title:
	case OP_ATOM_workPhone:
		// JIL requires that all these properties exist. So we prentend that they do
		// even if ReadAttributeValue claims that the device doesn't support them.
		if (value)
		{
			OpString attribute_name;
			GET_FAILED_IF_ERROR(attribute_name.Set(DOM_AtomToString(property_atom)));
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(ReadAttributeValue(attribute_name.CStr(), value, buffer));
		}
		return GET_SUCCESS;
	case OP_ATOM_addressBookItemId:
		if (value)
		{
			OpString id;
			GET_FAILED_IF_ERROR(m_item_data.GetId(&id));
			if (id.IsEmpty())
				DOMSetUndefined(value);
			else
			{
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(id.CStr()));
				DOMSetString(value, buffer);
			}
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILAddressBookItem::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_address:
	case OP_ATOM_company:
	case OP_ATOM_eMail:
	case OP_ATOM_fullName:
	case OP_ATOM_homePhone:
	case OP_ATOM_mobilePhone:
	case OP_ATOM_title:
	case OP_ATOM_workPhone:
		// JIL requires that all these properties exist. So we prentend that they do
		// even if WriteAttributeValue (and consequently the platform) isn't able to
		// handle all of them.
		// The unhandled ones will just not be written to - this should be the least
		// surprising to the users and easily spotted by our tests.
		{
			OpString attribute_name;
			PUT_FAILED_IF_ERROR(attribute_name.Set(DOM_AtomToString(property_atom)));
			OP_STATUS write_status = WriteAttributeValue(attribute_name.CStr(), value);
			switch (write_status)
			{
			case OpStatus::ERR_OUT_OF_RANGE:
				return PUT_NEEDS_STRING;
			default:
				return PUT_SUCCESS;
			}
		}
	case OP_ATOM_addressBookItemId:
		return PUT_SUCCESS;
	}

	return PUT_FAILED;
}

/* static */ OP_STATUS
DOM_JILAddressBookItem::Make(DOM_JILAddressBookItem*& new_obj, OpAddressBookItem* address_book_item, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(address_book_item);
	OpAutoPtr<OpAddressBookItem>ap_address_book_item(address_book_item);
	new_obj = OP_NEW(DOM_JILAddressBookItem, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::JIL_ADDRESSBOOKITEM_PROTOTYPE), "AddressBookItem"));
	new_obj->SetAddressBookItem(ap_address_book_item.release());
	return OpStatus::OK;
}

class AvailableAttributesBuilder : public JIL_AddressBookMappings::CollectionBuilder
{
public:
	AvailableAttributesBuilder(DOM_Runtime* origining_runtime) : m_runtime(origining_runtime), m_index(0), m_es_attribute_names(NULL)
	{
		OP_ASSERT(origining_runtime);
	}

	OP_STATUS Construct()
	{
		RETURN_IF_ERROR(m_runtime->CreateNativeArrayObject(&m_es_attribute_names, 0));
		return OpStatus::OK;
	}

	virtual OP_STATUS Add(const uni_char* attribute_name)
	{
		OP_ASSERT(m_es_attribute_names);
		ES_Value attr_name_val;
		DOM_Object::DOMSetString(&attr_name_val, attribute_name);
		RETURN_IF_ERROR(m_runtime->PutIndex(m_es_attribute_names, m_index, attr_name_val));
		++m_index;
		return OpStatus::OK;
	}

	ES_Object* ReleaseAttributeNames()
	{
		ES_Object* result = m_es_attribute_names;
		m_es_attribute_names = NULL;
		m_index = 0;
		return result;
	}

private:
	DOM_Runtime* m_runtime;
	int m_index;
	ES_Object* m_es_attribute_names;
};

int
DOM_JILAddressBookItem::getAvailableAttributes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);

	JIL_AddressBookMappings* mappings;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_device_api.GetAddressBookMappings(mappings), DOM_JILPIM::HandleAddressBookError);

	AvailableAttributesBuilder result_builder(origining_runtime);
	CALL_FAILED_IF_ERROR(result_builder.Construct());
	CALL_FAILED_IF_ERROR_WITH_HANDLER(mappings->GetAvailableAttributes(this_->GetAddressBookItem(), &result_builder), DOM_JILPIM::HandleAddressBookError);

	DOM_Object::DOMSetObject(return_value, result_builder.ReleaseAttributeNames());
	return ES_VALUE;
}

int
DOM_JILAddressBookItem::getAttributeValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);
	DOM_CHECK_ARGUMENTS_JIL("s");

	const uni_char* property_name = argv[0].value.string;

	TempBuffer* buffer = GetEmptyTempBuf();
	OP_ADDRESSBOOKSTATUS status = this_->ReadAttributeValue(property_name, return_value, buffer);
	if (OpStatus::IsError(status))
		return DOM_JILAddressBookItem::HandleAddressBookError(status, property_name, return_value, origining_runtime);

	return ES_VALUE;
}

int
DOM_JILAddressBookItem::setAttributeValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);
	DOM_CHECK_ARGUMENTS_JIL("sS");

	const uni_char* property_name = argv[0].value.string;
	const ES_Value* property_value = &argv[1];

	OP_ADDRESSBOOKSTATUS status = this_->WriteAttributeValue(property_name, property_value);
	if (OpStatus::IsError(status))
		return DOM_JILAddressBookItem::HandleAddressBookError(status, property_name, return_value, origining_runtime);
	return ES_FAILED;
}

int
DOM_JILAddressBookItem::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(addressbook_item, DOM_TYPE_JIL_ADDRESSBOOKITEM, DOM_JILAddressBookItem);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("");
	OP_ASSERT(addressbook_item->m_item_data.HasItem());

	int can_be_saved = addressbook_item->CheckCanBeSaved(return_value, origining_runtime);
	if (can_be_saved != ES_VALUE)
		return can_be_saved;

	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		OpString id;
		CALL_FAILED_IF_ERROR(addressbook_item->GetAddressBookItem()->GetId(&id));

		if (id.IsEmpty())
			return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("AddressBookItem was not added yet"));
	}

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILPIM::AddressBookModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpAddressBook, OpAddressBookItemModificationCallback*, OpAddressBookItem*>
		function(g_op_address_book, &OpAddressBook::CommitItem, callback, addressbook_item->GetAddressBookItem());
	DOM_SUSPENDING_CALL(call, function, DOM_JILPIM::AddressBookModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleAddressBookError);
	return ES_FAILED; // success
}

OP_ADDRESSBOOKSTATUS
DOM_JILAddressBookItem::ReadAttributeValue(const uni_char* attribute_name, ES_Value* value, TempBuffer* temp_buffer)
{
	OP_ASSERT(temp_buffer);
	OpString value_string;
	RETURN_IF_ERROR(m_item_data.ReadAttributeValue(attribute_name, value ? &value_string : NULL));
	if (value)
	{
		if (!value_string.IsEmpty())
		{
			RETURN_IF_ERROR(temp_buffer->Append(value_string.CStr()));
			DOMSetString(value, temp_buffer->GetStorage());
		}
		else
			DOMSetUndefined(value);
	}

	return OpStatus::OK;

}

OP_ADDRESSBOOKSTATUS
DOM_JILAddressBookItem::WriteAttributeValue(const uni_char* attribute_name, const ES_Value* value)
{
	const uni_char* new_value;
	switch (value->type)
	{
	case VALUE_STRING:
		new_value = value->value.string;
		break;
	case VALUE_NULL:
	case VALUE_UNDEFINED:
		new_value = NULL;
		break;
	default:
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	return m_item_data.WriteAttributeValue(attribute_name, new_value);
}

void
DOM_JILAddressBookItem::SetAddressBookItem(OpAddressBookItem* item)
{
	OP_ASSERT(!m_item_data.GetAddressBookItem()); // no resetting of m_address_book_item allowed
	OP_ASSERT(item);
	m_item_data.SetAddressBookItem(item);
}

int
DOM_JILAddressBookItem::CheckCanBeSaved(ES_Value* return_value, DOM_Runtime* runtime)
{
#ifdef DOM_JIL_ADDRESSBOOK_IGNORE_VIRTUAL_FIELDS_SAVE_ERRORS
	return ES_VALUE;
#else
	const uni_char* modified_attribute;
	OP_BOOLEAN is_virtual_field_modified = m_item_data.IsVirtualFieldModified(&modified_attribute);
	CALL_FAILED_IF_ERROR(is_virtual_field_modified);
	if (is_virtual_field_modified == OpBoolean::IS_TRUE)
		return DOM_JILAddressBookItem::HandleAddressBookError(OpStatus::ERR_NO_ACCESS, modified_attribute, return_value, runtime);
	else
		return ES_VALUE;
#endif // DOM_JIL_ADDRESSBOOK_IGNORE_VIRTUAL_FIELDS_SAVE_ERRORS
}

/* static */ int
DOM_JILAddressBookItem::HandleAddressBookError(OP_ADDRESSBOOKSTATUS error, const uni_char* attribute_name, ES_Value* return_value, DOM_Runtime* runtime)
{
	if (attribute_name != NULL)
	{
		switch (error)
		{
		case OpAddressBookStatus::ERR_FIELD_INDEX_OUT_OF_RANGE:
			{
				OpString message;
				CALL_FAILED_IF_ERROR(message.AppendFormat(UNI_L("No such attribute: %s"), attribute_name));
				return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, message.CStr());
			}
		case OpStatus::ERR_NO_ACCESS:
			{
				OpString message;
				CALL_FAILED_IF_ERROR(message.AppendFormat(UNI_L("Attribute '%s' is virtual and it's value cannot be saved."), attribute_name));
				return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, runtime, message.CStr());
			}
		default:
			return DOM_JILPIM::HandleAddressBookError(error, return_value, runtime);
		}
	}
	else
		return DOM_JILPIM::HandleAddressBookError(error, return_value, runtime);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILAddressBookItem)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAddressBookItem, DOM_JILAddressBookItem::getAvailableAttributes , "getAvailableAttributes"	, "",	"AddressBookItem.getAvailableAttributes")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAddressBookItem, DOM_JILAddressBookItem::getAttributeValue		, "getAttributeValue"		, "s-",	"AddressBookItem.getAttributeValue")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAddressBookItem, DOM_JILAddressBookItem::setAttributeValue		, "setAttributeValue"		, "sS-","AddressBookItem.setAttributeValue")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILAddressBookItem, DOM_JILAddressBookItem::update					, "update"					, "",	"AddressBookItem.update")
DOM_FUNCTIONS_END(DOM_JILAddressBookItem)

/* virtual */
int DOM_JILAddressBookItem_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	return CreateAddressBookItem(return_value, origining_runtime);
}

/* static */
int DOM_JILAddressBookItem_Constructor::CreateAddressBookItem(ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_JILAddressBookItem* dom_address_book_item = NULL;
	OpAddressBookItem* address_book_item;
	RETURN_IF_ERROR(OpAddressBookItem::Make(&address_book_item, g_op_address_book));

	CALL_FAILED_IF_ERROR(DOM_JILAddressBookItem::Make(dom_address_book_item, address_book_item, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_address_book_item);
	return ES_VALUE;
}

#endif // DOM_JIL_API_SUPPORT

