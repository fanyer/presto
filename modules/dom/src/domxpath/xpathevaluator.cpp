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

#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/domxpath/xpathexpression.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domxpath/xpathnsresolver.h"
#include "modules/dom/src/domxpath/xpathresult.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/attr.h"
#include "modules/dom/src/domcore/domdoc.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/util/tempbuf.h"
#include "modules/util/str.h"
#include "modules/xpath/xpath.h"
#include "modules/doc/frm_doc.h"
#include "modules/xmlutils/xmlnames.h"

/* Defined in xpathresult.cpp. */
extern unsigned DOM_ImportXPathType(unsigned type);

OP_STATUS
DOM_CreateXPathNode(XPathNode *&xpathnode, DOM_Node *domnode, XMLTreeAccessor *tree)
{
	FramesDocument *frames_doc = domnode->GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::ERR;

	HTML_Element *element = NULL;
	switch (domnode->GetNodeType())
	{
	case DOCUMENT_NODE:
	case DOCUMENT_FRAGMENT_NODE:
		element = domnode->GetPlaceholderElement();
		break;
	case ATTRIBUTE_NODE:
		if (DOM_Element *domelement = ((DOM_Attr *) domnode)->GetOwnerElement())
			element = domelement->GetThisElement();
		break;
	case XPATH_NAMESPACE_NODE:
		if (DOM_Element *domelement = ((DOM_XPathNamespace *) domnode)->GetOwnerElement())
			element = domelement->GetThisElement();
		break;
	default:
		element = domnode->GetThisElement();
	}

	if (!element)
		return OpStatus::ERR;

	BOOL owns_tree = !tree;

	if (!tree)
	{
		HTML_Element *rootelement = element;

		while (HTML_Element *parent = rootelement->Parent())
			rootelement = parent;

		XMLTreeAccessor::Node *rootnode;

		RETURN_IF_ERROR(logdoc->CreateXMLTreeAccessor(tree, rootnode, rootelement));
	}

	XMLTreeAccessor::Node *treenode = logdoc->GetElementAsNode(tree, element);

	OP_STATUS status;

	switch (domnode->GetNodeType())
	{
	case XPATH_NAMESPACE_NODE:
		status = XPathNode::MakeNamespace(xpathnode, tree, treenode, ((DOM_XPathNamespace *) domnode)->GetNsPrefix(), ((DOM_XPathNamespace *) domnode)->GetNsUri());
		break;

	default:
		status = XPathNode::Make(xpathnode, tree, treenode);
		break;

	case ATTRIBUTE_NODE:
		DOM_Attr *attr = (DOM_Attr *) domnode;
		const uni_char *name = attr->GetName(), *ns_uri = attr->GetNsUri();
		XMLCompleteName attr_name(ns_uri, ns_uri ? attr->GetNsPrefix() : NULL, name);
		status = XPathNode::MakeAttribute(xpathnode, tree, treenode, attr_name);
	}

	if (owns_tree && OpStatus::IsError(status))
		LogicalDocument::FreeXMLTreeAccessor(tree);

	return status;
}

/* static */ OP_STATUS
DOM_XPathEvaluator::Make(DOM_XPathEvaluator *&evaluator, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(evaluator = OP_NEW(DOM_XPathEvaluator, ()), runtime, runtime->GetPrototype(DOM_Runtime::XPATHEVALUATOR_PROTOTYPE), "XPathEvaluator");
}

/* static */ int
DOM_XPathEvaluator::createNSResolver(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED_2(DOM_TYPE_DOCUMENT, DOM_TYPE_XPATHEVALUATOR);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);

	DOM_XPathNSResolver *resolver;
	CALL_FAILED_IF_ERROR(DOM_XPathNSResolver::Make(resolver, node));

	DOM_Object::DOMSetObject(return_value, resolver);
	return ES_VALUE;
}

class DOM_XPathEvaluator_State
	: public DOM_Object,
	  public ES_AsyncCallback
{
public:
	DOM_XPathEvaluator_State(DOM_Document *document, DOM_Node *contextNode, ES_Object *resolver, unsigned type, ES_Thread *thread);
	~DOM_XPathEvaluator_State();

	virtual void GCTrace();
	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	DOM_Document *document;
	DOM_Node *contextNode;
	ES_Object *resolver;
	unsigned type, index;
	ES_Thread *thread;
	XPathNamespaces *namespaces;
	uni_char *expression_src;
	OP_STATUS evaluation_status;
	BOOL is_exception;
	ES_Value exception;
};

DOM_XPathEvaluator_State::DOM_XPathEvaluator_State(DOM_Document *document, DOM_Node *contextNode, ES_Object *resolver, unsigned type, ES_Thread *thread)
	: document(document),
	  contextNode(contextNode),
	  resolver(resolver),
	  type(type),
	  index(0),
	  thread(thread),
	  namespaces(NULL),
	  expression_src(NULL),
	  evaluation_status(OpStatus::OK),
	  is_exception(FALSE)
{
}

DOM_XPathEvaluator_State::~DOM_XPathEvaluator_State()
{
	XPathNamespaces::Free(namespaces);
	OP_DELETEA(expression_src);

	if (is_exception && exception.type == VALUE_STRING)
		OP_DELETEA((uni_char *) exception.value.string);
}

/* virtual */ void
DOM_XPathEvaluator_State::GCTrace()
{
	GCMark(document);
	GCMark(contextNode);
	GCMark(resolver);

	if (is_exception)
		GCMark(exception);
}

/* virtual */ OP_STATUS
DOM_XPathEvaluator_State::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	OP_STATUS stat = OpStatus::OK;
	OpStatus::Ignore(stat);

	switch (status)
	{
	case ES_ASYNC_SUCCESS:
		if (result.type == VALUE_STRING && *result.value.string)
			evaluation_status = namespaces->SetURI(index, result.value.string);
		else
			evaluation_status = OpStatus::ERR;
		break;

	case ES_ASYNC_EXCEPTION:
		is_exception = TRUE;
		exception = result;

		if (exception.type == VALUE_STRING)
		{
			uni_char *copy = NULL;
			if (OpStatus::IsSuccess(UniSetStr(copy, exception.value.string)))
				exception.value.string = copy;
			else
				exception.type = VALUE_NULL;
		}

	case ES_ASYNC_FAILURE:
	case ES_ASYNC_CANCELLED:
		evaluation_status = OpStatus::ERR;
		break;

	case ES_ASYNC_NO_MEMORY:
		evaluation_status = OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* static */ int
DOM_XPathEvaluator::createExpressionOrEvaluate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_XPathEvaluator_State *state = NULL;

	DOM_Document *document = NULL;

	const uni_char *expression_src;
	DOM_Node *contextNode = NULL;
	ES_Object *resolver;
	unsigned type = 0;
	XPathNamespaces *namespaces = NULL;
	XPathExpression *expression = NULL;
	XPathNode *node = NULL;
	XPathExpression::Evaluate *result = NULL;
	DOM_XPathResult *resultwrapper = NULL;

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING_OPTIONAL(document, DOM_TYPE_DOCUMENT, DOM_Document);
		DOM_THIS_OBJECT_OPTIONAL(evaluator, DOM_TYPE_XPATHEVALUATOR, DOM_XPathEvaluator);
		DOM_THIS_OBJECT_CHECK(document || evaluator);

		if (!this_object->GetFramesDocument())
			return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

		if (data == 0)
		{
			DOM_CHECK_ARGUMENTS("sO");

			resolver = argv[1].type == VALUE_OBJECT ? argv[1].value.object : NULL;
		}
		else
		{
			DOM_CHECK_ARGUMENTS("soOnO");

			DOM_ARGUMENT_OBJECT_EXISTING(contextNode, 1, DOM_TYPE_NODE, DOM_Node);
			resolver = argv[2].type == VALUE_OBJECT ? argv[2].value.object : NULL;
			double dtype = argv[3].value.number;

			if (argv[4].type == VALUE_OBJECT)
				DOM_ARGUMENT_OBJECT_EXISTING(resultwrapper, 4, DOM_TYPE_XPATHRESULT, DOM_XPathResult);

			if (!contextNode->OriginCheck(origining_runtime))
				return ES_EXCEPT_SECURITY;

			if (document)
			{
				if (!document->OriginCheck(origining_runtime))
					return ES_EXCEPT_SECURITY;
				if (contextNode->GetOwnerDocument() != document)
					return DOM_CALL_DOMEXCEPTION(WRONG_DOCUMENT_ERR);
			}
			else
				document = contextNode->GetOwnerDocument();

			if (dtype < 0 || dtype > 9 || !op_isintegral(dtype))
				return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

			type = DOM_ImportXPathType(static_cast<unsigned>(dtype));
		}

		expression_src = argv[0].value.string;

		status = XPathNamespaces::Make(namespaces, expression_src);

		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (OpStatus::IsError(status))
			return DOM_CALL_XPATHEXCEPTION(INVALID_EXPRESSION_ERR);

		if (namespaces->GetCount() != 0 && !resolver)
		{
			XPathNamespaces::Free(namespaces);
			return DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);
		}
	}
	else
	{
		state = DOM_VALUE2OBJECT(*return_value, DOM_XPathEvaluator_State);

		document = state->document;
		expression_src = state->expression_src;
		contextNode = state->contextNode;
		resolver = state->resolver;
		type = state->type;
		namespaces = state->namespaces;

		if (OpStatus::IsMemoryError(state->evaluation_status))
			return ES_NO_MEMORY;
		else if (OpStatus::IsError(state->evaluation_status))
			if (state->is_exception)
			{
				*return_value = state->exception;
				return ES_EXCEPTION;
			}
			else
				return DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);
	}

	DOM_HOSTOBJECT_SAFE(xpathnsresolver, resolver, DOM_TYPE_XPATHNSRESOLVER, DOM_XPathNSResolver);

	int code = ES_FAILED;

	for (unsigned index = 0, count = namespaces->GetCount(); index < count; ++index)
		if (xpathnsresolver)
		{
			const uni_char *uri = xpathnsresolver->LookupNamespaceURI(namespaces->GetPrefix(index));

			if (!uri)
				code = DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);
			else if (OpStatus::IsSuccess(status = namespaces->SetURI(index, uri)))
				continue;

			break;
		}
		else if (!namespaces->GetURI(index))
		{
			ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);

			if (!state)
			{
				if (OpStatus::IsError(DOM_Object::DOMSetObjectRuntime(state = OP_NEW(DOM_XPathEvaluator_State, (document, contextNode, resolver, type, thread)), this_object->GetRuntime())) ||
				    OpStatus::IsError(UniSetStr(state->expression_src, expression_src)))
				{
					XPathNamespaces::Free(namespaces);
					return ES_NO_MEMORY;
				}

				state->index = index;
				state->namespaces = namespaces;

				DOM_Object::DOMSetObject(return_value, state);
			}

			state->index = index;

			ES_AsyncInterface *asyncif = origining_runtime->GetEnvironment()->GetAsyncInterface();
			asyncif->SetWantExceptions();

			ES_Value arguments[1];
			DOM_Object::DOMSetString(arguments, namespaces->GetPrefix(index));

			if (op_strcmp(ES_Runtime::GetClass(resolver), "Function") == 0)
				CALL_FAILED_IF_ERROR(asyncif->CallFunction(resolver, NULL, 1, arguments, state, thread));
			else
				CALL_FAILED_IF_ERROR(asyncif->CallMethod(resolver, UNI_L("lookupNamespaceURI"), 1, arguments, state, thread));

			return ES_SUSPEND | ES_RESTART;
		}

	if (state)
		state->namespaces = NULL;

	if (OpStatus::IsMemoryError(status))
		code = ES_NO_MEMORY;
	else if (code == ES_FAILED)
		if (this_object->GetFramesDocument())
		{
			DOM_Object::DOMException domexception = DOM_Object::DOMEXCEPTION_NO_ERR;
			DOM_Object::XPathException xpathexception = DOM_Object::XPATHEXCEPTION_NO_ERR;

			xpathexception = DOM_Object::INVALID_EXPRESSION_ERR;
			XPathExpression::ExpressionData expression_data;
			expression_data.source = expression_src;
			expression_data.namespaces = namespaces;
#ifdef XPATH_ERRORS
			OpString error_message;
			expression_data.error_message = &error_message;
#endif // XPATH_ERRORS
			if (OpStatus::IsError(status = XPathExpression::Make(expression, expression_data)))
				goto handle_error;

			if (data == 0)
			{
				DOM_XPathExpression *wrapper;

				if (OpStatus::IsError(status = DOM_XPathExpression::Make(wrapper, this_object->GetEnvironment(), document, expression)))
					goto handle_error;

				DOM_Object::DOMSetObject(return_value, wrapper);
			}
			else
			{
				xpathexception = DOM_Object::XPATHEXCEPTION_NO_ERR;
				domexception = DOM_Object::NOT_SUPPORTED_ERR;
				if (OpStatus::IsError(status = DOM_CreateXPathNode(node, contextNode)))
					goto handle_error;

				xpathexception = DOM_Object::TYPE_ERR;
				domexception = DOM_Object::DOMEXCEPTION_NO_ERR;

				if (OpStatus::IsError(status = XPathExpression::Evaluate::Make(result, expression)))
				{
					XPathNode::Free(node);
					goto handle_error;
				}

				result->SetContext(node);
				if (type != XPathExpression::Evaluate::TYPE_INVALID)
					result->SetRequestedType((XPathExpression::Evaluate::Type) type);

				if (resultwrapper)
					status = resultwrapper->SetValue(node->GetTreeAccessor(), result);
				else
					status = DOM_XPathResult::Make(resultwrapper, document, node->GetTreeAccessor(), result);

				if (OpStatus::IsError(status))
					goto handle_error;

				DOM_Object::DOMSetObject(return_value, resultwrapper);
			}

			result = NULL;
			code = ES_VALUE;

		handle_error:
			if (OpStatus::IsMemoryError(status))
				code = ES_NO_MEMORY;
			else if (OpStatus::IsError(status))
				if (domexception != DOM_Object::DOMEXCEPTION_NO_ERR)
					code = this_object->CallDOMException(domexception, return_value);
				else
#ifdef XPATH_ERRORS
					code = this_object->CallXPathException(xpathexception, return_value, error_message.CStr());
#else // XPATH_ERRORS
					code = this_object->CallXPathException(xpathexception, return_value);
#endif // XPATH_ERRORS
		}
		else
			code = DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

	XPathNamespaces::Free(namespaces);

	if (data == 1)
		XPathExpression::Free(expression);

	return code;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XPathEvaluator)
	DOM_FUNCTIONS_FUNCTION(DOM_Document, DOM_XPathEvaluator::createNSResolver, "createNSResolver", "o-")
DOM_FUNCTIONS_END(DOM_Document)

DOM_FUNCTIONS_WITH_DATA_START(DOM_XPathEvaluator)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_XPathEvaluator::createExpressionOrEvaluate, 0, "createExpression", "sO-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Document, DOM_XPathEvaluator::createExpressionOrEvaluate, 1, "evaluate", "soOnO-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_Document)

/* virtual */ int
DOM_XPathEvaluator_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_XPathEvaluator *evaluator;
	CALL_FAILED_IF_ERROR(DOM_XPathEvaluator::Make(evaluator, GetEnvironment()));

	DOMSetObject(return_value, evaluator);
	return ES_VALUE;
}

#endif // DOM3_XPATH
