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

#include "modules/dom/src/domcore/doctype.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/nodemap.h"
#include "modules/dom/src/domcore/entity.h"
#include "modules/dom/src/domcore/notation.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/util/str.h"

#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/xmlparser/xmldoctype.h"

DOM_DocumentType::DOM_DocumentType()
	: DOM_Node(DOCUMENT_TYPE_NODE),
	  this_element(NULL),
	  xml_document_info(NULL),
	  name(NULL),
	  public_id(NULL),
	  system_id(NULL)
{
}

DOM_DocumentType::~DOM_DocumentType()
{
	CleanDT();

	if (this_element)
		FreeElement(this_element);
}

/* static */ OP_STATUS
DOM_DocumentType::Make(DOM_DocumentType *&doctype, HTML_Element *this_element, DOM_Document *owner_document)
{
	DOM_Runtime *runtime = owner_document->GetRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(doctype = OP_NEW(DOM_DocumentType, ()), runtime, runtime->GetPrototype(DOM_Runtime::NODE_PROTOTYPE), "DocumentType"));

	doctype->owner_document = owner_document;
	doctype->this_element = this_element;

	const XMLDocumentInformation *docinfo = this_element->GetXMLDocumentInfo();
	if (docinfo)
	{
		if (!(doctype->xml_document_info = OP_NEW(XMLDocumentInformation, ())))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(doctype->xml_document_info->Copy(*docinfo));

		doctype->name = (uni_char *) doctype->xml_document_info->GetDoctypeName();
		doctype->public_id = (uni_char *) doctype->xml_document_info->GetPublicId();
		doctype->system_id = (uni_char *) doctype->xml_document_info->GetSystemId();
	}
	else if (!owner_document->IsXML() && this_element)
	{
		RETURN_IF_ERROR(UniSetStr(doctype->name, this_element->GetStringAttr(ATTR_NAME, NS_IDX_DEFAULT)));
		RETURN_IF_ERROR(UniSetStr(doctype->public_id, this_element->GetSpecialStringAttr(ATTR_PUB_ID, SpecialNs::NS_LOGDOC)));
		RETURN_IF_ERROR(UniSetStr(doctype->system_id, this_element->GetSpecialStringAttr(ATTR_SYS_ID, SpecialNs::NS_LOGDOC)));
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_DocumentType::Make(DOM_DocumentType *&doctype, DOM_EnvironmentImpl *environment, const uni_char *name, const uni_char *public_id, const uni_char *system_id)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(doctype = OP_NEW(DOM_DocumentType, ()), runtime, runtime->GetPrototype(DOM_Runtime::DOCUMENTTYPE_PROTOTYPE), "DocumentType"));

	if (!(doctype->xml_document_info = OP_NEW(XMLDocumentInformation, ())) ||
		OpStatus::IsMemoryError(doctype->xml_document_info->SetDoctypeDeclaration(name, public_id, system_id, NULL)))
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(HTML_Element::DOMCreateDoctype(doctype->this_element, environment, doctype->xml_document_info));
	doctype->this_element->SetESElement(doctype);

	doctype->name = (uni_char *) doctype->xml_document_info->GetDoctypeName();
	doctype->public_id = (uni_char *) doctype->xml_document_info->GetPublicId();
	doctype->system_id = (uni_char *) doctype->xml_document_info->GetSystemId();

	return OpStatus::OK;
}

void
DOM_DocumentType::CleanDT()
{
	if (xml_document_info)
	{
		OP_DELETE(xml_document_info);
		xml_document_info = NULL;
	}
	else
	{
		OP_DELETEA(name);
		OP_DELETEA(public_id);
		OP_DELETEA(system_id);
	}
	name = public_id = system_id = NULL;

}

OP_STATUS
DOM_DocumentType::CopyFrom(DOM_DocumentType* dt)
{
	CleanDT();
	RETURN_IF_ERROR(UniSetStr(this->name, dt->GetName()));
	RETURN_IF_ERROR(UniSetStr(this->public_id, dt->GetPublicId()));
	RETURN_IF_ERROR(UniSetStr(this->system_id, dt->GetSystemId()));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_DocumentType::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime )
{
	switch(property_name)
	{
	case OP_ATOM_previousSibling:
		return DOMSetElement(value, this_element ? this_element->PredActual() : NULL);

	case OP_ATOM_nextSibling:
		return DOMSetElement(value, this_element ? this_element->SucActual() : NULL);

	case OP_ATOM_parentNode:
		return DOMSetParentNode(value);

	case OP_ATOM_name:
	case OP_ATOM_nodeName:
		DOMSetString(value, name);
		return GET_SUCCESS;

	case OP_ATOM_publicId:
		DOMSetString(value, public_id);
		return GET_SUCCESS;

	case OP_ATOM_systemId:
		DOMSetString(value, system_id);
		return GET_SUCCESS;

	case OP_ATOM_internalSubset:
		DOMSetNull(value);
		if (xml_document_info && xml_document_info->GetInternalSubset())
			DOMSetString(value, xml_document_info->GetInternalSubset());
		return GET_SUCCESS;

	case OP_ATOM_entities:
	case OP_ATOM_notations:
		if (value)
		{
			int private_name = property_name == OP_ATOM_entities ? DOM_PRIVATE_entities : DOM_PRIVATE_notations;

			switch (DOMSetPrivate(value, private_name))
			{
			case GET_SUCCESS:
				return GET_SUCCESS;

			case GET_NO_MEMORY:
				return GET_NO_MEMORY;

			default:
				DOM_NamedNodeMap *map;
				DOM_NamedNodeMapImpl *impl = NULL;

#ifdef DOM_SUPPORT_ENTITY
				if (property_name == OP_ATOM_entities)
				{
					impl = OP_NEW(DOM_EntitiesMapImpl, (this, GetXMLDoctype()));
					if (!impl)
						return GET_NO_MEMORY;
				}
#endif // DOM_SUPPORT_ENTITY

#ifdef DOM_SUPPORT_NOTATION
				if (property_name == OP_ATOM_notations)
				{
					impl = OP_NEW(DOM_NotationsMapImpl, (this, GetXMLDoctype()));
					if (!impl)
						return GET_NO_MEMORY;
				}
#endif // DOM_SUPPORT_ENTITY

				GET_FAILED_IF_ERROR(DOM_NamedNodeMap::Make(map, this, impl));
				GET_FAILED_IF_ERROR(PutPrivate(private_name, map));

#ifdef DOM_SUPPORT_ENTITY
				if (property_name == OP_ATOM_entities)
					((DOM_EntitiesMapImpl *) impl)->SetEntities(map);
#endif // DOM_SUPPORT_ENTITY

#ifdef DOM_SUPPORT_NOTATION
				if (property_name == OP_ATOM_notations)
					((DOM_NotationsMapImpl *) impl)->SetNotations(map);
#endif // DOM_SUPPORT_ENTITY

				DOMSetObject(value, map);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_textContent:
		DOMSetNull(value);
		return GET_SUCCESS;

	default:
		return DOM_Node::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_DocumentType::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	/* Justification for this behavior: the DOM2 Core spec says (p62)
	   that "The DOM Level 2 doesn't support editing DocumentType nodes."
	   There is a test in the W3C DOM Level 1 Core tests that tests this
	   by setting the nodeValue property on a documentType node and
	   expecting it to fail quietly.

	   This code implements that behavior, and the reason that this comment
	   is longer than the code is that that kind of absurd behavior should
	   always be well-documented. */

	return GetName(property_name, NULL, origining_runtime) == GET_SUCCESS ? PUT_SUCCESS : PUT_FAILED;
}

/* virtual */ void
DOM_DocumentType::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);

	if (this_element && !via_tree)
		TraceElementTree(this_element);
}

XMLDoctype *
DOM_DocumentType::GetXMLDoctype()
{
	return xml_document_info ? xml_document_info->GetDoctype() : NULL;
}
