/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHNSRESOLVER
#define DOM_XPATHNSRESOLVER

#ifdef DOM3_XPATH

#include "modules/dom/src/domobj.h"

class DOM_Node;

class DOM_XPathNSResolver
	: public DOM_Object
{
protected:
	DOM_XPathNSResolver(DOM_Node *node);

	DOM_Node *node;

public:
	static OP_STATUS Make(DOM_XPathNSResolver *&resolver, DOM_Node *node);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHNSRESOLVER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	const uni_char *LookupNamespaceURI(const uni_char *prefix);

	DOM_DECLARE_FUNCTION(lookupNamespaceURI);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // DOM3_XPATH
#endif // DOM_XPATHNSRESOLVER
