/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/domtokenlist.h"
#include "modules/logdoc/class_attribute.h"
#include "modules/logdoc/htm_lex.h"

#define WHITESPACE_L UNI_L(" \t\n\f\r")

#include "modules/logdoc/stringtokenlist_attribute.h"

/* static */ OP_STATUS
DOM_DOMTokenList::Make(DOM_DOMTokenList *&tokenlist, DOM_Element* elm)
{
	DOM_Runtime *runtime = elm->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(tokenlist = OP_NEW(DOM_DOMTokenList, (elm)), runtime, runtime->GetPrototype(DOM_Runtime::DOMTOKENLIST_PROTOTYPE), "DOMTokenList"));
	tokenlist->m_data = OP_NEW(DOM_DOMTokenList::DataClassAttribute, (elm));
	RETURN_OOM_IF_NULL(tokenlist->m_data);
	return OpStatus::OK;
}

DOM_DOMTokenList::~DOM_DOMTokenList()
{
	if (m_data)
		OP_DELETE(m_data);
}

/* virtual */ ES_GetState
DOM_DOMTokenList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_data->length());
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_DOMTokenList::GetIndex(int index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	TempBuffer* item_buffer = GetEmptyTempBuf();
	OP_STATUS ok = m_data->item(index, item_buffer);
	if (ok == OpStatus::OK)
	{
		DOMSetString(return_value, item_buffer);
		return GET_SUCCESS;
	}
	else if (ok == OpStatus::ERR_NO_MEMORY)
		return GET_NO_MEMORY;
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_DOMTokenList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ ES_PutState
DOM_DOMTokenList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ ES_GetState
DOM_DOMTokenList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_data->length();
	return GET_SUCCESS;
}

/* static */ int
DOM_DOMTokenList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(tokenlist, DOM_TYPE_DOMTOKENLIST, DOM_DOMTokenList);
	DOM_CHECK_ARGUMENTS("n");
	int index = TruncateDoubleToInt(argv[0].value.number);
	if (index < 0 || tokenlist->GetIndex(index, return_value, origining_runtime) == GET_FAILED)
		DOMSetNull(return_value);
	return ES_VALUE;
}

/* static */ int
DOM_DOMTokenList::toString(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(tokenlist, DOM_TYPE_DOMTOKENLIST, DOM_DOMTokenList);
	const uni_char* contents;
	CALL_FAILED_IF_ERROR(tokenlist->m_data->toString(contents));
	DOMSetString(return_value, contents);
	return ES_VALUE;
}

/* static */ int
DOM_DOMTokenList::access(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	DOM_THIS_OBJECT(tokenlist, DOM_TYPE_DOMTOKENLIST, DOM_DOMTokenList);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char* item = argv[0].value.string;
	// Item must not be an empty string.
	if (item[0] == '\0')
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
	// Item must not contain whitespace.
	if (item[uni_strcspn(item, WHITESPACE_L)] != '\0')
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);

	switch (data)
	{
		case 0: // contains
			DOMSetBoolean(return_value, tokenlist->m_data->contains(item));
			break;
		case 1: // add
			CALL_FAILED_IF_ERROR(tokenlist->m_data->add(item));
			break;
		case 2: // remove
		{
			TempBuffer* buffer_out = GetEmptyTempBuf();
			CALL_FAILED_IF_ERROR(tokenlist->m_data->remove(item, buffer_out));
			break;
		}
		case 3: // toggle
			if (tokenlist->m_data->contains(item))
			{
				TempBuffer* buffer_out = GetEmptyTempBuf();
				CALL_FAILED_IF_ERROR(tokenlist->m_data->remove(item, buffer_out));
				DOMSetBoolean(return_value, FALSE);
			}
			else
			{
				CALL_FAILED_IF_ERROR(tokenlist->m_data->add(item));
				DOMSetBoolean(return_value, TRUE);
			}
			break;
	}

	return ES_VALUE;
}

/* static */ OP_STATUS
DOM_DOMSettableTokenList::Make(DOM_DOMSettableTokenList *&tokenlist, DOM_Element* element, Markup::AttrType attribute)
{
	DOM_Runtime *runtime = element->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(tokenlist = OP_NEW(DOM_DOMSettableTokenList, (element, attribute)), runtime, runtime->GetPrototype(DOM_Runtime::DOMSETTABLETOKENLIST_PROTOTYPE), "DOMSettableTokenList"));
	// We're only using this on strings right now.
	tokenlist->m_data = OP_NEW(DOM_DOMSettableTokenList::DataStringAttribute, (element, attribute));
	RETURN_OOM_IF_NULL(tokenlist->m_data);
	return OpStatus::OK;
}

/* virtual */
DOM_DOMSettableTokenList::~DOM_DOMSettableTokenList()
{}

/* virtual */ ES_GetState
DOM_DOMSettableTokenList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_value)
	{
		if (value)
			DOMSetString(value, static_cast<DOM_DOMSettableTokenList::Data*>(m_data)->GetValue());
		return GET_SUCCESS;
	}
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, static_cast<DOM_DOMSettableTokenList::Data*>(m_data)->length());
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_DOMSettableTokenList::GetIndex(int index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	TempBuffer* item_buffer = GetEmptyTempBuf();
	OP_STATUS ok = m_data->item(index, item_buffer);
	if (ok == OpStatus::OK)
	{
		DOMSetString(return_value, item_buffer);
		return GET_SUCCESS;
	}
	else if (ok == OpStatus::ERR_NO_MEMORY)
		return GET_NO_MEMORY;
	return GET_FAILED;
}


/* virtual */ ES_PutState
DOM_DOMSettableTokenList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_value)
	{
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(static_cast<DOM_DOMSettableTokenList::Data*>(m_data)->SetValue(value->value.string));
		return PUT_SUCCESS;
	}
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ ES_PutState
DOM_DOMSettableTokenList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	return PUT_READ_ONLY;
}

/* virtual */ ES_GetState
DOM_DOMSettableTokenList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_data->length();
	return GET_SUCCESS;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_DOMTokenList)
	DOM_FUNCTIONS_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::item, "item", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::toString, "toString", 0)
DOM_FUNCTIONS_END(DOM_DOMTokenList)

DOM_FUNCTIONS_WITH_DATA_START(DOM_DOMTokenList)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::access, 0, "contains", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::access, 1, "add", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::access, 2, "remove", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_DOMTokenList, DOM_DOMTokenList::access, 3, "toggle", "s-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_DOMTokenList)

DOM_DOMTokenList::DOM_DOMTokenList(DOM_Element* element)
	: m_element(element)
	, m_data(NULL)
{
}

//---
// Implementation of DOM_DOMTokenList::DataClassAttribute
//

/* virtual */ int
DOM_DOMTokenList::DataClassAttribute::length()
{
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	return class_attrib ? class_attrib->GetLength() : 0;
}

/* virtual */ OP_STATUS
DOM_DOMTokenList::DataClassAttribute::item(int index_to_get, TempBuffer* contents)
{
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	if (!class_attrib)
		return OpStatus::ERR;
	if (IsInStrictMode())
	{
		const ReferencedHTMLClass* single_class = class_attrib->GetClassRefSafe(index_to_get);
		if (single_class)
			return contents->Append(single_class->GetString());
	}
	else
	{
		const uni_char* case_sensitive_class_string = class_attrib->GetAttributeString();
		int class_string_i = 0;
		while (*case_sensitive_class_string)
		{
			// skip whitespace
			case_sensitive_class_string += uni_strspn(case_sensitive_class_string, WHITESPACE_L);
			// find size of next token
			size_t token_len = uni_strcspn(case_sensitive_class_string, WHITESPACE_L);
			if (token_len)
			{
				if (class_string_i == index_to_get)
					return contents->Append(case_sensitive_class_string, token_len);
				case_sensitive_class_string += token_len;
				class_string_i++;
			}
		}
	}
	return OpStatus::ERR;
}

/* virtual */ BOOL
DOM_DOMTokenList::DataClassAttribute::contains(const uni_char *string_to_check)
{
	OP_ASSERT(string_to_check);
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	if (!class_attrib)
		return FALSE;
	// ClassAttribute::Match second parameter is named "case_sensitive", but that class underlying data
	// representation (HTMLClassNames) is implemented using OpGenericStringHashTable, which is initialized
	// with case_sensitive=IsInStrictMode().
	// And when IsInStrictMode()==FALSE, the case sensitiveness of strings inside OpGenericStringHashTable
	// is lost.  In that case DataClassAttribute has to handle string tokenization by itself.
	if (IsInStrictMode())
		return class_attrib->MatchClass(string_to_check, TRUE);
	else
	{
		const uni_char* case_sensitive_class_string = class_attrib->GetAttributeString();
		unsigned int to_check_len = uni_strlen(string_to_check);
		while (*case_sensitive_class_string)
		{
			// Skip whitespace.
			case_sensitive_class_string += uni_strspn(case_sensitive_class_string, WHITESPACE_L);
			if (size_t token_len = uni_strcspn(case_sensitive_class_string, WHITESPACE_L))
			{
				if (uni_strncmp(case_sensitive_class_string, string_to_check, MAX(token_len, to_check_len)) == 0)
					return TRUE;
				case_sensitive_class_string += token_len;
			}
		}
	}
	return FALSE;
}

BOOL
DOM_DOMTokenList::DataClassAttribute::IsInStrictMode()
{
	if (m_is_in_strict_mode == MAYBE)
	{
		if (HLDocProfile* hldoc = m_attribute_owner->GetHLDocProfile())
			m_is_in_strict_mode = hldoc->IsInStrictMode() ? YES : NO;
	}

	return m_is_in_strict_mode == YES ? TRUE : FALSE;
}


static OP_STATUS AppendWithSpace(TempBuffer* buffer, const uni_char* append, size_t length=(size_t)-1)
{
	OP_ASSERT(buffer);
	if (!length)
		return OpStatus::OK;
	if (buffer->Length())
	{
		uni_char last_character = buffer->GetStorage()[buffer->Length()-1];
		if (!IsHTML5Space(last_character))
			RETURN_IF_ERROR(buffer->Append(' '));
	}
	RETURN_IF_ERROR(buffer->Append(append, length));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_DOMTokenList::DataClassAttribute::add(const uni_char *string)
{
	OP_ASSERT(string);
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	TempBuffer* buffer_value = GetEmptyTempBuf();
	if (!class_attrib)
		RETURN_IF_ERROR(buffer_value->Append(string));
	else
	{
		if (contains(string))
			return OpStatus::OK;
		RETURN_IF_ERROR(const_cast<ClassAttribute*>(class_attrib)->ToString(buffer_value));
		RETURN_IF_ERROR(AppendWithSpace(buffer_value, string));
	}

	return m_attribute_owner->GetThisElement()->DOMSetAttribute(m_attribute_owner->GetEnvironment(), ATTR_XML, UNI_L("class"), NS_IDX_ANY_NAMESPACE, buffer_value->GetStorage(), buffer_value->Length(), m_attribute_owner->HasCaseSensitiveNames());
}

/* virtual */ OP_STATUS
DOM_DOMTokenList::DataClassAttribute::remove(const uni_char *to_remove, TempBuffer* buffer_out)
{
	OP_ASSERT(to_remove);
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	if (!class_attrib)
		return OpStatus::OK;
	const uni_char* class_string = class_attrib->GetAttributeString();
	const uni_char* copy_start = class_string;
	const uni_char* copy_end = class_string;
	unsigned int to_remove_len = uni_strlen(to_remove);
	while (*class_string)
	{
		// Skip whitespace.
		class_string += uni_strspn(class_string, WHITESPACE_L);
		// Find size of next to_remove.
		if (size_t token_len = uni_strcspn(class_string, WHITESPACE_L))
		{
			if (uni_strncmp(class_string, to_remove, MAX(token_len, to_remove_len)) == 0)
			{
				// Copy what we have so far.
				RETURN_IF_ERROR(AppendWithSpace(buffer_out, copy_start, copy_end - copy_start));
				// Skip this to_remove.
				class_string += token_len;
				// Skip whitespace.
				class_string += uni_strspn(class_string, WHITESPACE_L);
				// Continue copying from next token.
				copy_start = copy_end = class_string;
			}
			else
			{
				// Keep this token, will copy later.
				class_string += token_len;
				copy_end = class_string;
			}
		}
	}
	// Now append what remains.
	RETURN_IF_ERROR(AppendWithSpace(buffer_out, copy_start, class_string - copy_start));

	return m_attribute_owner->GetThisElement()->DOMSetAttribute(m_attribute_owner->GetEnvironment(), ATTR_XML, UNI_L("class"), NS_IDX_ANY_NAMESPACE, buffer_out->GetStorage() ? buffer_out->GetStorage() : UNI_L(""), buffer_out->Length(), m_attribute_owner->HasCaseSensitiveNames());
}

/* virtual */ OP_STATUS
DOM_DOMTokenList::DataClassAttribute::toString(const uni_char *&contents)
{
	const ClassAttribute* class_attrib = m_attribute_owner->GetThisElement()->GetClassAttribute();
	if (class_attrib)
		contents = class_attrib->GetAttributeString();
	else
		contents = UNI_L("");
	return OpStatus::OK;
}


//---
// Implementation of DOM_DOMSettableTokenList::DataStringAttribute.

/* virtual */ int
DOM_DOMSettableTokenList::DataStringAttribute::length()
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	return attr ? attr->GetTokenCount() : 0;
}
/* virtual */ OP_STATUS
DOM_DOMSettableTokenList::DataStringAttribute::item(int index_to_get, TempBuffer *contents)
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	if (!attr)
		return OpStatus::ERR;
	return attr->Get(index_to_get, contents);
}
/* virtual */ BOOL
DOM_DOMSettableTokenList::DataStringAttribute::contains(const uni_char *string_to_check)
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	if (!attr)
		return FALSE;
	return attr->Contains(string_to_check);
}
/* virtual */ OP_STATUS
DOM_DOMSettableTokenList::DataStringAttribute::add(const uni_char *string)
{
	OP_ASSERT(string);
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	if (!attr)
		return m_attribute_owner->SetAttribute(m_attribute, NULL, NS_IDX_DEFAULT, string ? string : UNI_L(""), string ? uni_strlen(string) : 1, m_attribute_owner->HasCaseSensitiveNames(), m_attribute_owner->GetRuntime());
	return attr->Add(string);
}

/* virtual */ OP_STATUS
DOM_DOMSettableTokenList::DataStringAttribute::remove(const uni_char *to_remove, TempBuffer* buffer_out)
{
	OP_ASSERT(to_remove);
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	if (!attr)
		return OpStatus::OK;
	return attr->Remove(to_remove);
}

/* virtual */ OP_STATUS
DOM_DOMSettableTokenList::DataStringAttribute::toString(const uni_char *&contents)
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	contents = attr ? attr->GetAttributeString() : UNI_L("");
	return OpStatus::OK;
}

/* virtual */ const
uni_char* DOM_DOMSettableTokenList::DataStringAttribute::GetValue()
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	return attr ? attr->GetAttributeString() : UNI_L("");
}

/* virtual */ OP_STATUS
DOM_DOMSettableTokenList::DataStringAttribute::SetValue(const uni_char* str)
{
	StringTokenListAttribute* attr = static_cast<StringTokenListAttribute*>(m_attribute_owner->GetThisElement()->GetAttr(m_attribute, ITEM_TYPE_COMPLEX, NULL));
	if (!attr)
		return m_attribute_owner->SetAttribute(m_attribute, NULL, NS_IDX_DEFAULT, str ? str : UNI_L(""), str ? uni_strlen(str) : 1, m_attribute_owner->HasCaseSensitiveNames(), m_attribute_owner->GetRuntime());
	return attr->SetValue(str);
}


