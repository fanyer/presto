/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHEVALUATOR
#define DOM_XPATHEVALUATOR

#ifdef DOM3_XPATH

#include "modules/dom/src/domobj.h"

class XPathNode;
class DOM_Node;

class XMLTreeAccessor;

extern OP_STATUS DOM_CreateXPathNode(XPathNode *&xpathnode, DOM_Node *domnode, XMLTreeAccessor *tree = NULL);

class DOM_XPathEvaluator
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_XPathEvaluator *&evaluator, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHEVALUATOR || DOM_Object::IsA(type); }

	enum
	{
		FUNCTIONS_LIST,
		FUNCTIONS_createNSResolver,
		FUNCTIONS_ARRAY_SIZE
	};

	enum
	{
		FUNCTIONS_WITH_DATA_LIST,
		FUNCTIONS_createExpression,
		FUNCTIONS_evaluate,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

    DOM_DECLARE_FUNCTION(createNSResolver);
	DOM_DECLARE_FUNCTION_WITH_DATA(createExpressionOrEvaluate);
};

class DOM_XPathEvaluator_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_XPathEvaluator_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::XPATHEVALUATOR_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM3_XPATH
#endif // DOM_XPATHEVALUATOR
