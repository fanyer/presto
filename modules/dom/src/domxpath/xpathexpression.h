/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHEXPRESSION
#define DOM_XPATHEXPRESSION

#ifdef DOM3_XPATH

#include "modules/dom/src/domobj.h"

class DOM_Document;
class DOM_Node;
class XPathExpression;
class XPathNode;

class DOM_XPathExpression
	: public DOM_Object
{
protected:
	DOM_XPathExpression(DOM_Document *document, XPathExpression *xpathexpression);

	DOM_Document *document;
	XPathExpression *xpathexpression;

public:
	static OP_STATUS Make(DOM_XPathExpression *&domexpression, DOM_EnvironmentImpl *environment, DOM_Document *document, XPathExpression *xpathexpression);

	virtual ~DOM_XPathExpression();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHEXPRESSION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(evaluate);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // DOM3_XPATH
#endif // DOM_XPATHEXPRESSION
