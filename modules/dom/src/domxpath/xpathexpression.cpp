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

#include "modules/dom/src/domxpath/xpathexpression.h"
#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/domxpath/xpathresult.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domdoc.h"

#include "modules/xpath/xpath.h"
#include "modules/logdoc/logdoc.h"

/* Defined in xpathresult.cpp. */
extern unsigned DOM_ImportXPathType(unsigned type);

DOM_XPathExpression::DOM_XPathExpression(DOM_Document *document, XPathExpression *xpathexpression)
	: document(document),
	  xpathexpression(xpathexpression)
{
}

DOM_XPathExpression::~DOM_XPathExpression()
{
	XPathExpression::Free(xpathexpression);
}

/* virtual */ void
DOM_XPathExpression::GCTrace()
{
	GCMark(document);
}

/* static */ OP_STATUS
DOM_XPathExpression::Make(DOM_XPathExpression *&domexpression, DOM_EnvironmentImpl *environment, DOM_Document *document, XPathExpression *xpathexpression)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(domexpression = OP_NEW(DOM_XPathExpression, (document, xpathexpression)), runtime, runtime->GetPrototype(DOM_Runtime::XPATHEXPRESSION_PROTOTYPE), "XPathExpression");
}

/* static */ int
DOM_XPathExpression::evaluate(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(domexpression, DOM_TYPE_XPATHEXPRESSION, DOM_XPathExpression);
	DOM_CHECK_ARGUMENTS("onO");

	if (domexpression->GetFramesDocument())
	{
		DOM_ARGUMENT_OBJECT(contextNode, 0, DOM_TYPE_NODE, DOM_Node);
		double dtype = argv[1].value.number;
		DOM_XPathResult *resultwrapper = NULL;

		if (argv[2].type == VALUE_OBJECT)
			DOM_ARGUMENT_OBJECT_EXISTING(resultwrapper, 2, DOM_TYPE_XPATHRESULT, DOM_XPathResult);

		if (!contextNode->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

		if (domexpression->document)
		{
			if (!domexpression->document->OriginCheck(origining_runtime))
				return ES_EXCEPT_SECURITY;
			if (contextNode->GetOwnerDocument() != domexpression->document)
				return DOM_CALL_DOMEXCEPTION(WRONG_DOCUMENT_ERR);
		}

		DOM_Document *document = contextNode->GetOwnerDocument();

		if (dtype < 0 || dtype > 9 || !op_isintegral(dtype))
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		unsigned type = DOM_ImportXPathType(static_cast<unsigned>(dtype));
		OP_STATUS status = OpStatus::OK;

		DOM_Object::DOMException domexception = DOM_Object::DOMEXCEPTION_NO_ERR;
		DOM_Object::XPathException xpathexception = DOM_Object::XPATHEXCEPTION_NO_ERR;

		XPathNode *node = NULL;

		xpathexception = DOM_Object::XPATHEXCEPTION_NO_ERR;
		domexception = DOM_Object::NOT_SUPPORTED_ERR;
		if (OpStatus::IsError(status = DOM_CreateXPathNode(node, contextNode)))
			goto handle_error;

		xpathexception = DOM_Object::TYPE_ERR;
		domexception = DOM_Object::DOMEXCEPTION_NO_ERR;

		XPathExpression::Evaluate *result;

		if (OpStatus::IsError(status = XPathExpression::Evaluate::Make(result, domexpression->xpathexpression)))
		{
			XPathNode::Free(node);
			goto handle_error;
		}

		result->SetContext(node);
		if (type != XPathExpression::Evaluate::TYPE_INVALID)
			result->SetRequestedType(type);

		if (resultwrapper)
			status = resultwrapper->SetValue(node->GetTreeAccessor(), result);
		else
			status = DOM_XPathResult::Make(resultwrapper, document, node->GetTreeAccessor(), result);

		if (OpStatus::IsError(status))
			goto handle_error;

		DOM_Object::DOMSetObject(return_value, resultwrapper);
		return ES_VALUE;

	handle_error:
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (domexception != DOM_Object::DOMEXCEPTION_NO_ERR)
			return domexpression->CallDOMException(domexception, return_value);
		else
			return domexpression->CallXPathException(xpathexception, return_value);
	}
	else
		return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XPathExpression)
	DOM_FUNCTIONS_FUNCTION(DOM_XPathExpression, DOM_XPathExpression::evaluate, "evaluate", "onO-")
DOM_FUNCTIONS_END(DOM_XPathExpression)

#endif // DOM3_XPATH
