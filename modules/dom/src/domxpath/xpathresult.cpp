/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_XPATH

#include "modules/dom/src/domxpath/xpathresult.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/element.h"

#include "modules/xpath/xpath.h"

#include "modules/xmlutils/xmlnames.h"
#include "modules/logdoc/logdoc.h"

unsigned
DOM_ImportXPathType(unsigned type)
{
	switch (static_cast<unsigned>(type))
	{
	default :                                           return XPathExpression::Evaluate::TYPE_INVALID;
	case DOM_XPathResult::NUMBER_TYPE:                  return XPathExpression::Evaluate::PRIMITIVE_NUMBER;
	case DOM_XPathResult::STRING_TYPE:                  return XPathExpression::Evaluate::PRIMITIVE_STRING;
	case DOM_XPathResult::BOOLEAN_TYPE:                 return XPathExpression::Evaluate::PRIMITIVE_BOOLEAN;
	case DOM_XPathResult::UNORDERED_NODE_ITERATOR_TYPE: return XPathExpression::Evaluate::NODESET_ITERATOR;
	case DOM_XPathResult::ORDERED_NODE_ITERATOR_TYPE:   return XPathExpression::Evaluate::NODESET_ITERATOR | XPathExpression::Evaluate::NODESET_FLAG_ORDERED;
	case DOM_XPathResult::UNORDERED_NODE_SNAPSHOT_TYPE: return XPathExpression::Evaluate::NODESET_SNAPSHOT;
	case DOM_XPathResult::ORDERED_NODE_SNAPSHOT_TYPE:   return XPathExpression::Evaluate::NODESET_SNAPSHOT | XPathExpression::Evaluate::NODESET_FLAG_ORDERED;
	case DOM_XPathResult::ANY_UNORDERED_NODE_TYPE:      return XPathExpression::Evaluate::NODESET_SINGLE;
	case DOM_XPathResult::FIRST_ORDERED_NODE_TYPE:      return XPathExpression::Evaluate::NODESET_SINGLE | XPathExpression::Evaluate::NODESET_FLAG_ORDERED;
	}
}

extern double
DOM_ExportXPathType(unsigned eval_type_enum)
{
	unsigned eval_type = eval_type_enum;
	if (eval_type == 0)
		return DOM_XPathResult::ANY_TYPE;

	if ((eval_type & XPathExpression::Evaluate::PRIMITIVE_NUMBER) != 0)
		return DOM_XPathResult::NUMBER_TYPE;

	if ((eval_type & XPathExpression::Evaluate::PRIMITIVE_STRING) != 0)
		return DOM_XPathResult::STRING_TYPE;

	if ((eval_type & XPathExpression::Evaluate::PRIMITIVE_BOOLEAN) != 0)
		return DOM_XPathResult::BOOLEAN_TYPE;

	if ((eval_type & XPathExpression::Evaluate::NODESET_ITERATOR) != 0)
	{
		if ((eval_type & XPathExpression::Evaluate::NODESET_FLAG_ORDERED) != 0)
			return DOM_XPathResult::ORDERED_NODE_ITERATOR_TYPE;
		return DOM_XPathResult::UNORDERED_NODE_ITERATOR_TYPE;
	}

	if ((eval_type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0)
	{
		if ((eval_type & XPathExpression::Evaluate::NODESET_FLAG_ORDERED) != 0)
			return DOM_XPathResult::ORDERED_NODE_SNAPSHOT_TYPE;
		return DOM_XPathResult::UNORDERED_NODE_SNAPSHOT_TYPE;
	}

	OP_ASSERT ((eval_type & XPathExpression::Evaluate::NODESET_SINGLE) != 0); // Or we get some unknown input
	if ((eval_type & XPathExpression::Evaluate::NODESET_FLAG_ORDERED) != 0)
		return DOM_XPathResult::FIRST_ORDERED_NODE_TYPE;

	return DOM_XPathResult::ANY_UNORDERED_NODE_TYPE;
}

DOM_XPathResult::DOM_XPathResult(DOM_Document *document, XMLTreeAccessor *tree, XPathExpression::Evaluate *xpathresult)
	: tree(tree),
	  xpathresult(xpathresult),
	  document(document),
	  invalid(FALSE),
	  check_changes(FALSE)
{
	HTML_Element *treeroot = LogicalDocument::GetNodeAsElement(tree, tree->GetRoot());
	HTML_Element *actualroot = LogicalDocument::GetNodeAsElement(tree, tree->GetFirstChild(tree->GetRoot()));

	if (treeroot && actualroot && !treeroot->IsAncestorOf(actualroot))
		/* The XMLTreeAccessor has a dummy HE_DOC_ROOT element. */
		rootelement = actualroot;
	else
		rootelement = treeroot;
}

DOM_XPathResult::~DOM_XPathResult()
{
	LogicalDocument::FreeXMLTreeAccessor(tree);
	XPathExpression::Evaluate::Free(xpathresult);

	if (check_changes)
		GetEnvironment()->RemoveMutationListener(this);
}

OP_STATUS
DOM_XPathResult::Initialize()
{
	/* Should perhaps support pausing?  (As long as we don't provide extensions
	   that block, it is only a matter of time-slice length, though.  This will
	   work.) */
	OP_BOOLEAN finished;
	do
		finished = EnsureTypeAndFirstResult();
	while (finished == OpBoolean::IS_FALSE);

	if ((result_type & XPathExpression::Evaluate::NODESET_ITERATOR) != 0)
		check_changes = TRUE;
	else
	{
		if ((result_type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0)
		{
			do
				finished = xpathresult->GetNodesCount(snapshot_length);
			while (finished == OpBoolean::IS_FALSE);
			RETURN_IF_ERROR(finished);
			for (unsigned index = 0; index < snapshot_length; ++index)
			{
				XPathNode *xpathnode;
				RETURN_IF_ERROR(xpathresult->GetNode(xpathnode, index));
				RETURN_IF_ERROR(AddNode(xpathnode));
			}
		}
		else if ((result_type & XPathExpression::Evaluate::NODESET_SINGLE) != 0)
		{
			XPathNode *xpathnode;
			do
				finished = xpathresult->GetSingleNode(xpathnode);
			while (finished == OpBoolean::IS_FALSE);
			RETURN_IF_ERROR(finished);
			RETURN_IF_ERROR(AddNode(xpathnode));
		}
		else
			return OpStatus::OK;

		LogicalDocument::FreeXMLTreeAccessor(tree);
		tree = NULL;
		XPathExpression::Evaluate::Free(xpathresult);
		xpathresult = NULL;
	}

	if (check_changes)
		GetEnvironment()->AddMutationListener(this);

	return OpStatus::OK;
}

OP_STATUS
DOM_XPathResult::AddNode(XPathNode *xpathnode)
{
	if (xpathnode)
	{
		DOM_Node *domnode;

		OP_STATUS status = GetDOMNode(domnode, xpathnode, document);
		if (OpStatus::IsSuccess(status))
			status = nodes.Add(domnode);

		return status;
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_XPathResult::Make(DOM_XPathResult *&domresult, DOM_Document *document, XMLTreeAccessor *tree, XPathExpression::Evaluate *xpathresult)
{
	DOM_Runtime *runtime = document->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(domresult = OP_NEW(DOM_XPathResult, (document, tree, xpathresult)), runtime, runtime->GetPrototype(DOM_Runtime::XPATHRESULT_PROTOTYPE), "XPathResult"));
	return domresult->Initialize();
}

OP_STATUS DOM_XPathResult::EnsureTypeAndFirstResult()
{
	if (xpathresult)
	{
		OP_BOOLEAN finished;
		do
			finished = xpathresult->CheckResultType(result_type);
		while (finished == OpBoolean::IS_FALSE);
		RETURN_IF_ERROR(finished);
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_XPathResult::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_resultType:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			DOMSetNumber(value, DOM_ExportXPathType(result_type));
		}
		return GET_SUCCESS;

	case OP_ATOM_numberValue:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			if ((result_type & XPathExpression::Evaluate::PRIMITIVE_NUMBER) != 0)
			{
				double result;
				OP_BOOLEAN finished;
				do
					finished = xpathresult->GetNumberResult(result);
				while (finished == OpBoolean::IS_FALSE);
				GET_FAILED_IF_ERROR(finished);
				DOMSetNumber(value, result);
			}
			else
				break;
		}
		return GET_SUCCESS;

	case OP_ATOM_stringValue:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			if ((result_type & XPathExpression::Evaluate::PRIMITIVE_STRING) != 0)
			{
				const uni_char* result;
				OP_BOOLEAN finished;
				do
					finished = xpathresult->GetStringResult(result);
				while (finished == OpBoolean::IS_FALSE);
				GET_FAILED_IF_ERROR(finished);
				DOMSetString(value, result);
			}
			else
				break;
		}
		return GET_SUCCESS;

	case OP_ATOM_booleanValue:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			if ((result_type & XPathExpression::Evaluate::PRIMITIVE_BOOLEAN) != 0)
			{
				BOOL result;
				OP_BOOLEAN finished;
				do
					finished = xpathresult->GetBooleanResult(result);
				while (finished == OpBoolean::IS_FALSE);
				GET_FAILED_IF_ERROR(finished);
				DOMSetBoolean(value, result);
			}
			else
				break;
		}
		return GET_SUCCESS;

	case OP_ATOM_singleNodeValue:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			if ((result_type & XPathExpression::Evaluate::NODESET_SINGLE) != 0)
				DOMSetObject(value, nodes.GetCount() == 1 ? nodes.Get(0) : NULL);
			else
				break;
		}
		return GET_SUCCESS;

	case OP_ATOM_invalidIteratorState:
		DOMSetBoolean(value, invalid);
		return GET_SUCCESS;

	case OP_ATOM_snapshotLength:
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureTypeAndFirstResult());
			if ((result_type & XPathExpression::Evaluate::NODESET_SNAPSHOT) != 0)
				DOMSetNumber(value, snapshot_length);
			else
				break;
		}
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}

	return DOM_GETNAME_XPATHEXCEPTION(TYPE_ERR);
}

/* virtual */ ES_PutState
DOM_XPathResult::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_XPathResult::GCTrace()
{
	for (unsigned index = 0, count = nodes.GetCount(); index < count; ++index)
		GCMark(nodes.Get(index));
}

/* virtual */ OP_STATUS
DOM_XPathResult::OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	if (check_changes && rootelement->IsAncestorOf(child))
	{
		invalid = TRUE;
		check_changes = FALSE;
		GetEnvironment()->RemoveMutationListener(this);
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_XPathResult::OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	if (check_changes && rootelement->IsAncestorOf(child))
	{
		invalid = TRUE;
		check_changes = FALSE;
		GetEnvironment()->RemoveMutationListener(this);
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_XPathResult::OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime)
{
	if (check_changes && rootelement->IsAncestorOf(element))
	{
		invalid = TRUE;
		check_changes = FALSE;
		GetEnvironment()->RemoveMutationListener(this);
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_XPathResult::OnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime)
{
	if (check_changes && rootelement->IsAncestorOf(element))
	{
		invalid = TRUE;
		check_changes = FALSE;
		GetEnvironment()->RemoveMutationListener(this);
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_XPathResult::SetValue(XMLTreeAccessor *new_tree, XPathExpression::Evaluate *new_xpathresult)
{
	nodes.Clear();

	LogicalDocument::FreeXMLTreeAccessor(tree);
	XPathExpression::Evaluate::Free(xpathresult);

	tree = new_tree;
	xpathresult = new_xpathresult;

	if (check_changes)
		GetEnvironment()->RemoveMutationListener(this);

	invalid = FALSE;
	check_changes = FALSE;

	return Initialize();
}

/* static */ void
DOM_XPathResult::ConstructXPathResultObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Object::PutNumericConstantL(object, "ANY_TYPE", ANY_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "NUMBER_TYPE", NUMBER_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "STRING_TYPE", STRING_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "BOOLEAN_TYPE", BOOLEAN_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "UNORDERED_NODE_ITERATOR_TYPE", UNORDERED_NODE_ITERATOR_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "ORDERED_NODE_ITERATOR_TYPE", ORDERED_NODE_ITERATOR_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "UNORDERED_NODE_SNAPSHOT_TYPE", UNORDERED_NODE_SNAPSHOT_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "ORDERED_NODE_SNAPSHOT_TYPE", ORDERED_NODE_SNAPSHOT_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "ANY_UNORDERED_NODE_TYPE", ANY_UNORDERED_NODE_TYPE, runtime);
	DOM_Object::PutNumericConstantL(object, "FIRST_ORDERED_NODE_TYPE", FIRST_ORDERED_NODE_TYPE, runtime);
}

/* static */ OP_STATUS
DOM_XPathResult::GetDOMNode(DOM_Node *&domnode, XPathNode *xpathnode, DOM_Document *document)
{
	if (xpathnode)
	{
		HTML_Element* element = LogicalDocument::GetNodeAsElement(xpathnode->GetTreeAccessor(), xpathnode->GetNode());
		RETURN_IF_ERROR(document->GetEnvironment()->ConstructNode(domnode, element, document));

		if (xpathnode->GetType() == XPathNode::ATTRIBUTE_NODE)
		{
			DOM_Attr *attrnode;
			XMLCompleteName attr_name;
			BOOL case_sensitive = document->IsXML() || element->GetNsIdx() != NS_IDX_HTML;
			xpathnode->GetNodeName(attr_name);
			RETURN_IF_ERROR(((DOM_Element *) domnode)->GetAttributeNode(attrnode, ATTR_XML, attr_name.GetLocalPart(), attr_name.GetNsIndex(), TRUE, FALSE, case_sensitive));
			domnode = attrnode;
		}
		else if (xpathnode->GetType() == XPathNode::NAMESPACE_NODE)
		{
			DOM_XPathNamespace *nsnode;
			RETURN_IF_ERROR(((DOM_Element *) domnode)->GetXPathNamespaceNode(nsnode, xpathnode->GetNamespacePrefix(), xpathnode->GetNamespaceURI()));
			domnode = nsnode;
		}
	}
	else
		domnode = NULL;

	return OpStatus::OK;
}

/* static */ int
DOM_XPathResult::getNode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(domresult, DOM_TYPE_XPATHRESULT, DOM_XPathResult);

	DOM_Node *domnode = NULL;

	if (domresult->invalid)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	CALL_FAILED_IF_ERROR(domresult->EnsureTypeAndFirstResult());

	if (data == 0)
	{
		if ((domresult->result_type & XPathExpression::Evaluate::NODESET_ITERATOR) == 0)
			return DOM_CALL_XPATHEXCEPTION(TYPE_ERR);

		if (domresult->xpathresult)
		{
			XPathNode *xpathnode;
			OP_BOOLEAN finished;

			/* FIXME: Should perhaps pause here instead. */
			do
				finished = domresult->xpathresult->GetNextNode(xpathnode);
			while (finished == OpBoolean::IS_FALSE);

			CALL_FAILED_IF_ERROR(finished);

			if (xpathnode)
				CALL_FAILED_IF_ERROR(GetDOMNode(domnode, xpathnode, domresult->document));
			else
			{
				LogicalDocument::FreeXMLTreeAccessor(domresult->tree);
				domresult->tree = NULL;
				XPathExpression::Evaluate::Free(domresult->xpathresult);
				domresult->xpathresult = NULL;
			}
		}
	}
	else
	{
		DOM_CHECK_ARGUMENTS("n");

		if ((domresult->result_type & XPathExpression::Evaluate::NODESET_SNAPSHOT) == 0)
			return DOM_CALL_XPATHEXCEPTION(TYPE_ERR);

		double dindex = argv[0].value.number;

		if (op_isintegral(dindex) && dindex >= 0 && dindex < domresult->nodes.GetCount())
			domnode = domresult->nodes.Get((unsigned) dindex);
	}

	DOMSetObject(return_value, domnode);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_XPathResult)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XPathResult, DOM_XPathResult::getNode, 0, "iterateNext", "-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_XPathResult, DOM_XPathResult::getNode, 1, "snapshotItem", "n-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_XPathResult)

#endif // DOM3_XPATH
