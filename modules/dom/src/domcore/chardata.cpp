/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/chardata.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domevents/dommutationevent.h"

#include "modules/doc/frm_doc.h"
#include "modules/util/tempbuf.h"
#include "modules/logdoc/htm_elm.h"

DOM_CharacterData::DOM_CharacterData(DOM_NodeType type)
	: DOM_Node(type),
	  this_element(NULL)
{
}

/* static */ OP_STATUS
DOM_CharacterData::Make(DOM_CharacterData *&characterdata, DOM_Node *reference, const uni_char *contents, BOOL comment, BOOL cdata_section)
{
	DOM_EnvironmentImpl *environment = reference->GetEnvironment();

	HTML_Element *element;
	DOM_Node *node;

	RETURN_IF_ERROR(HTML_Element::DOMCreateTextNode(element, environment, contents, comment, cdata_section));

	if (OpStatus::IsMemoryError(environment->ConstructNode(node, element, reference->GetOwnerDocument())))
	{
		HTML_Element::DOMFreeElement(element, environment);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(node->IsA(DOM_TYPE_CHARACTERDATA));

	characterdata = (DOM_CharacterData *) node;
	return OpStatus::OK;
}


OP_STATUS
DOM_CharacterData::GetValue(TempBuffer *value)
{
	return this_element->DOMGetContents(GetEnvironment(), value);
}

const uni_char*
DOM_CharacterData::GetValueString(TempBuffer *buffer)
{
	return this_element->DOMGetContentsString(GetEnvironment(), buffer);
}

OP_STATUS
DOM_CharacterData::SetValue(const uni_char *value, DOM_Runtime *origining_runtime, DOM_MutationListener::ValueModificationType type, unsigned offset, unsigned length1, unsigned length2)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();

	TempBuffer buf;
	RETURN_IF_ERROR(GetValue(&buf));

	const uni_char *old_value = buf.GetStorage() ? buf.GetStorage() : UNI_L("");

	if (type == DOM_MutationListener::REPLACING_ALL)
	{
		type = DOM_MutationListener::REPLACING;
		offset = 0;
		length1 = uni_strlen(old_value);
		length2 = uni_strlen(value);
	}

#ifdef DOM2_MUTATION_EVENTS
	if (environment->IsEnabled() && environment->HasEventHandler(this_element, DOMCHARACTERDATAMODIFIED))
		RETURN_IF_ERROR(DOM_MutationEvent::SendCharacterDataModified(GetCurrentThread(origining_runtime), this, old_value, value));
#endif // DOM2_MUTATION_EVENTS

	DOM_EnvironmentImpl::CurrentState state(environment, (DOM_Runtime *) origining_runtime);
	RETURN_IF_ERROR(this_element->DOMSetContents(environment, value, (HTML_Element::ValueModificationType)type, offset, length1, length2));

	return OpStatus::OK;
}

/* virtual */
DOM_CharacterData::~DOM_CharacterData()
{
	if (this_element)
		FreeElement(this_element);
}

/* virtual */
ES_GetState
DOM_CharacterData::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeValue:
	case OP_ATOM_data:
	case OP_ATOM_length:
	case OP_ATOM_textContent:
		if (value)
		{
			TempBuffer* buf = GetEmptyTempBuf();
			const uni_char *string_val;
			if (!(string_val = GetValueString(buf)))
				return GET_NO_MEMORY;

			if (property_name == OP_ATOM_length)
				DOMSetNumber(value, uni_strlen(string_val));
			else
				DOMSetString(value, string_val);
		}
		return GET_SUCCESS;

	case OP_ATOM_previousSibling:
		return DOMSetElement(value, this_element->PredActual());

	case OP_ATOM_nextSibling:
		return DOMSetElement(value, this_element->SucActual());

	case OP_ATOM_parentNode:
		return DOMSetParentNode(value);
	}

	return DOM_Node::GetName(property_name, value, origining_runtime);
}

/* virtual */
ES_PutState
DOM_CharacterData::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_textContent:
	case OP_ATOM_data:
		if (value->type == VALUE_NULL)
			DOMSetString(value);
		// Fallthrough.
	case OP_ATOM_nodeValue:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		PUT_FAILED_IF_ERROR(SetValue(value->value.string, static_cast<DOM_Runtime *>(origining_runtime)));
		return PUT_SUCCESS;

	case OP_ATOM_length:
		return PUT_READ_ONLY;
	}

	return DOM_Node::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_CharacterData::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (!via_tree)
		TraceElementTree(this_element);
}

/* static */
int
DOM_CharacterData::appendData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(characterdata, DOM_TYPE_CHARACTERDATA, DOM_CharacterData);
	DOM_CHECK_ARGUMENTS("s");

	if (!characterdata->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	TempBuffer buf;
	CALL_FAILED_IF_ERROR(characterdata->GetValue(&buf));
	unsigned old_length = buf.Length();
	const uni_char *new_value = argv[0].value.string;
	unsigned new_length = uni_strlen(new_value);

	CALL_FAILED_IF_ERROR(buf.Append(new_value, new_length));
	CALL_FAILED_IF_ERROR(characterdata->SetValue(buf.GetStorage(), origining_runtime, DOM_MutationListener::INSERTING, old_length, new_length));

	return ES_FAILED;
}

/* static */
int
DOM_CharacterData::substringData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(characterdata, DOM_TYPE_CHARACTERDATA, DOM_CharacterData);
	DOM_CHECK_ARGUMENTS("nn");

	if (!characterdata->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	TempBuffer* old_value = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(characterdata->GetValue(old_value));

	unsigned old_value_length = old_value->Length();

	if (argv[0].value.number < 0 ||
		argv[0].value.number > old_value_length ||
		argv[1].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	if (old_value_length > 0)
	{
		unsigned int offset = TruncateDoubleToUInt(argv[0].value.number);
		unsigned int count = TruncateDoubleToUInt(argv[1].value.number);

		count = MIN(count, old_value_length - offset);

		uni_char* storage = old_value->GetStorage();

		op_memmove(storage, storage + offset, sizeof(uni_char) * count);
		storage[count] = 0;
	}

	DOM_Object::DOMSetString(return_value, old_value);
	return ES_VALUE;

}

/* static */
int
DOM_CharacterData::insertData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(characterdata, DOM_TYPE_CHARACTERDATA, DOM_CharacterData);
	DOM_CHECK_ARGUMENTS("ns");

	if (!characterdata->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	TempBuffer old_value;
	CALL_FAILED_IF_ERROR(characterdata->GetValue(&old_value));

	if (argv[0].value.number < 0 || argv[0].value.number > old_value.Length())
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned int offset = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int count = old_value.Length() - offset;

	const uni_char *new_value = argv[1].value.string;
	unsigned new_length = uni_strlen(new_value);

	TempBuffer buf;
	CALL_FAILED_IF_ERROR(buf.Append(old_value.GetStorage(), offset));
	CALL_FAILED_IF_ERROR(buf.Append(new_value, new_length));
	CALL_FAILED_IF_ERROR(buf.Append(old_value.GetStorage() + offset, count));

	CALL_FAILED_IF_ERROR(characterdata->SetValue(buf.GetStorage(), origining_runtime, DOM_MutationListener::INSERTING, offset, new_length));

	return ES_FAILED;
}

/* static */
int
DOM_CharacterData::deleteData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(characterdata, DOM_TYPE_CHARACTERDATA, DOM_CharacterData);
	DOM_CHECK_ARGUMENTS("nn");

	if (!characterdata->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	TempBuffer old_value_buf;
	CALL_FAILED_IF_ERROR(characterdata->GetValue(&old_value_buf));
	unsigned int old_value_len = old_value_buf.Length();
	const uni_char* old_value = old_value_buf.GetStorage();

	if (argv[0].value.number < 0 ||
		argv[0].value.number > old_value_len ||
		argv[1].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned offset = TruncateDoubleToUInt(argv[0].value.number);
	unsigned length = TruncateDoubleToUInt(argv[1].value.number);
	unsigned offset_after_deleted = offset + length;

	TempBuffer buf;
	CALL_FAILED_IF_ERROR(buf.Append(old_value, offset));
	if (old_value_len > offset_after_deleted)
		CALL_FAILED_IF_ERROR(buf.Append(old_value + offset_after_deleted, old_value_len - offset_after_deleted));

	CALL_FAILED_IF_ERROR(characterdata->SetValue(buf.GetStorage(), origining_runtime, DOM_MutationListener::DELETING, offset, length));

	return ES_FAILED;
}

/* static */
int
DOM_CharacterData::replaceData(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(characterdata, DOM_TYPE_CHARACTERDATA, DOM_CharacterData);
	DOM_CHECK_ARGUMENTS("nns");

	if (!characterdata->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	TempBuffer old_value_buf;
	CALL_FAILED_IF_ERROR(characterdata->GetValue(&old_value_buf));
	unsigned int old_length = old_value_buf.Length();
	const uni_char* old_value = old_value_buf.GetStorage();

	if (argv[0].value.number < 0 ||
		argv[0].value.number > old_length ||
		argv[1].value.number < 0)
		return DOM_CALL_DOMEXCEPTION(INDEX_SIZE_ERR);

	unsigned int offset = TruncateDoubleToUInt(argv[0].value.number);
	unsigned int count = TruncateDoubleToUInt(argv[1].value.number);

	const uni_char *new_value = argv[2].value.string;
	unsigned new_length = uni_strlen(new_value);

	TempBuffer buf;
	CALL_FAILED_IF_ERROR(buf.Append(old_value, offset));
	CALL_FAILED_IF_ERROR(buf.Append(new_value, new_length));

	if (old_length > (offset + count))
		CALL_FAILED_IF_ERROR(buf.Append(old_value + offset + count, old_length - offset - count));

	CALL_FAILED_IF_ERROR(characterdata->SetValue(buf.GetStorage(), origining_runtime, DOM_MutationListener::REPLACING, offset, count, new_length));

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_CharacterData)
	DOM_FUNCTIONS_FUNCTION(DOM_CharacterData, DOM_CharacterData::substringData, "substringData", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOM_CharacterData, DOM_CharacterData::appendData, "appendData", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_CharacterData, DOM_CharacterData::insertData, "insertData", "ns-")
	DOM_FUNCTIONS_FUNCTION(DOM_CharacterData, DOM_CharacterData::deleteData, "deleteData", "nn-")
	DOM_FUNCTIONS_FUNCTION(DOM_CharacterData, DOM_CharacterData::replaceData, "replaceData", "nns-")
DOM_FUNCTIONS_END(DOM_CharacterData)

