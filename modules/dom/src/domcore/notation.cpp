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
#include "modules/dom/src/domcore/notation.h"
#include "modules/dom/src/domcore/doctype.h"

#ifdef DOM_SUPPORT_NOTATION

DOM_Notation::DOM_Notation(DOM_DocumentType *doctype, XMLDoctype::Notation *notation)
	: DOM_Node(NOTATION_NODE),
	  doctype(doctype),
	  notation(notation)
{
	SetOwnerDocument(doctype->GetOwnerDocument());
}

/* static */ OP_STATUS
DOM_Notation::Make(DOM_Notation *&notation, DOM_DocumentType *doctype, XMLDoctype::Notation *xml_notation)
{
	DOM_Runtime *runtime = doctype->GetRuntime();
	return DOMSetObjectRuntime(notation = OP_NEW(DOM_Notation, (doctype, xml_notation)), runtime, runtime->GetPrototype(DOM_Runtime::NODE_PROTOTYPE), "Notation");
}

/* virtual */
ES_GetState
DOM_Notation::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
		DOMSetString(value, notation->GetName());
		return GET_SUCCESS;

	case OP_ATOM_publicId:
		if (notation->GetPubid())
			DOMSetString(value, notation->GetPubid());
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_systemId:
		if (notation->GetSystem())
			DOMSetString(value, notation->GetSystem());
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_textContent:
		DOMSetNull(value);
		return GET_SUCCESS;
	}

	return DOM_Node::GetName(property_name, value, origining_runtime);
}

/* virtual */
ES_PutState
DOM_Notation::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_publicId:
	case OP_ATOM_systemId:
		return PUT_READ_ONLY;

	case OP_ATOM_textContent:
		return PUT_SUCCESS;
	}

	return DOM_Node::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_Notation::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);
	GCMark(doctype);
}

DOM_NotationsMapImpl::DOM_NotationsMapImpl(DOM_DocumentType *doctype, XMLDoctype *xml_doctype)
	: doctype(doctype),
	  notations(NULL),
	  xml_doctype(xml_doctype)
{
}

void
DOM_NotationsMapImpl::SetNotations(DOM_NamedNodeMap *new_notations)
{
	notations = new_notations;
}

/* virtual */ void
DOM_NotationsMapImpl::GCTrace()
{
	DOM_Object::GCMark(doctype);
	DOM_Object::GCMark(notations);
}

/* virtual */ int
DOM_NotationsMapImpl::Length()
{
	return xml_doctype ? xml_doctype->GetNotationsCount() : 0;
}

/* virtual */ int
DOM_NotationsMapImpl::Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	if (xml_doctype && index >= 0)
		if (XMLDoctype::Notation *xml_notation = xml_doctype->GetNotation(index))
			if (!notations->GetInternalValue(xml_notation->GetName(), return_value))
			{
				DOM_Notation *notation;
				CALL_FAILED_IF_ERROR(DOM_Notation::Make(notation, doctype, xml_notation));
				DOM_Object::DOMSetObject(return_value, notation);
				CALL_FAILED_IF_ERROR(notations->SetInternalValue(xml_notation->GetName(), *return_value));
			}

	return ES_VALUE;
}

/* virtual */ int
DOM_NotationsMapImpl::GetNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	if (xml_doctype && !ns_uri)
		if (XMLDoctype::Notation *xml_notation = xml_doctype->GetNotation(item_name))
			if (!notations->GetInternalValue(xml_notation->GetName(), return_value))
			{
				DOM_Notation *notation;
				CALL_FAILED_IF_ERROR(DOM_Notation::Make(notation, doctype, xml_notation));
				DOM_Object::DOMSetObject(return_value, notation);
				CALL_FAILED_IF_ERROR(notations->SetInternalValue(xml_notation->GetName(), *return_value));
			}

	return ES_VALUE;
}

/* virtual */ int
DOM_NotationsMapImpl::RemoveNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	return notations->CallDOMException(DOM_Object::NO_MODIFICATION_ALLOWED_ERR, return_value);
}

/* virtual */ int
DOM_NotationsMapImpl::SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	return notations->CallDOMException(DOM_Object::NO_MODIFICATION_ALLOWED_ERR, return_value);
}

#endif // DOM_SUPPORT_NOTATION
